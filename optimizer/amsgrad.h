/*
  Developed by Sandeep Sharma
  Copyright (c) 2017, Sandeep Sharma
  
  This file is part of DICE.
  
  This program is free software: you can redistribute it and/or modify it under the terms
  of the GNU General Public License as published by the Free Software Foundation, 
  either version 3 of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  See the GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License along with this program. 
  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef OPTIMIZER_HEADER_H
#define OPTIMIZER_HEADER_H
#include <Eigen/Dense>
#include <boost/serialization/serialization.hpp>
#include "iowrapper.h"
#include "global.h"
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#ifndef SERIAL
#include "mpi.h"
#endif

using namespace Eigen;
using namespace boost;

class AMSGrad
{
  private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive &ar, const unsigned int version)
    {
        ar & stepsize
           & decay_mom1
           & decay_mom2
           & iter        
           & mom1
           & mom2;
    }

  public:
    double stepsize;
    double decay_mom1;
    double decay_mom2;

    int maxIter;
    int iter;

    VectorXd mom1;
    VectorXd mom2;

    AMSGrad(double pstepsize=0.001,
             double pdecay_mom1=0.1, double pdecay_mom2=0.001,  
            int pmaxIter=1000) : stepsize(pstepsize), decay_mom1(pdecay_mom1), decay_mom2(pdecay_mom2), maxIter(pmaxIter)
    {
        iter = 0;
    }

    void write(VectorXd& vars)
    {
        if (commrank == 0)
        {
            char file[5000];
            sprintf(file, "amgrad.bkp");
            std::ofstream ofs(file, std::ios::binary);
            boost::archive::binary_oarchive save(ofs);
            save << *this;
            save << mom1;
            save << mom2;
            save << vars;
            ofs.close();
        }
    }

    void read(VectorXd& vars)
    {
        if (commrank == 0)
        {
            char file[5000];
            sprintf(file, "amgrad.bkp");
            std::ifstream ifs(file, std::ios::binary);
            boost::archive::binary_iarchive load(ifs);
            load >> *this;
            load >> mom1;
            load >> mom2;
            load >> vars;
            ifs.close();
        }
    }

   template<typename Function>
    void optimize(VectorXd &vars, Function& getGradient, bool restart)
    {
        if (restart)
        {
            if (commrank == 0)
                read(vars);
#ifndef SERIAL
            mpi::broadcast(world, *this, 0);
#endif
        }
        else if (mom1.rows() == 0)
        {
            mom1 = VectorXd::Zero(vars.rows());
            mom2 = VectorXd::Zero(vars.rows());
        }

        VectorXd grad = VectorXd::Zero(vars.rows());

        while (iter < maxIter)
        {
            double E0, stddev = 0.0, rt = 1.0;
            
            getGradient(vars, grad, E0, stddev, rt);

            if (commrank == 0)
            {
                for (int i = 0; i < vars.rows(); i++)
                {
                    mom1[i] = decay_mom1 * grad[i] + (1. - decay_mom1) * mom1[i];
                    mom2[i] = max(mom2[i], decay_mom2 * grad[i]*grad[i] + (1. - decay_mom2) * mom2[i]);   
                    vars[i] -= stepsize * mom1[i] / (pow(mom2[i], 0.5) + 1.e-8);
                }
            }

#ifndef SERIAL
            MPI_Bcast(&vars[0], vars.rows(), MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

            if (commrank == 0)
                std::cout << format("%5i %14.8f (%8.2e) %14.8f %8.1f %10i %8.2f\n") % iter % E0 % stddev % (grad.norm()) % (rt) % (schd.stochasticIter) % ((getTime() - startofCalc));
            iter++;
            write(vars);
        }
    }
};
#endif