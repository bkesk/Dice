/*
Developed by Sandeep Sharma with contributions from James E. Smith and Adam A. Homes, 2017
Copyright (c) 2017, Sandeep Sharma

This file is part of DICE.
This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "omp.h"
#include "Determinants.h"
#include "SHCIbasics.h"
#include "SHCIgetdeterminants.h"
#include "SHCIsampledeterminants.h"
#include "SHCIrdm.h"
#include "SHCISortMpiUtils.h"
#include "input.h"
#include "integral.h"
#include <vector>
#include <algorithm>
#include "math.h"
#include "Hmult.h"
#include <tuple>
#include <map>
#include "Davidson.h"
#include "boost/format.hpp"
#include <fstream>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#ifndef SERIAL
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi.hpp>
#endif
#include "communicate.h"

using namespace std;
using namespace Eigen;
using namespace boost;
using namespace SHCISortMpiUtils;

void SHCIrdm::loadRDM(schedule& schd, MatrixXx& s2RDM, MatrixXx& twoRDM, int root) {
  int norbs = twoRDM.rows();
  int nSpatOrbs = pow(s2RDM.rows(),0.5);
  if (schd.DoSpinRDM ){
    if (mpigetrank() == 0) {
      char file [5000];
      sprintf (file, "%s/%d-spinRDM.bkp" , schd.prefix[0].c_str(), root );
      std::ifstream ifs(file, std::ios::binary);
      boost::archive::binary_iarchive load(ifs);
      load >> twoRDM;
      //ComputeEnergyFromSpinRDM(norbs, nelec, I1, I2, coreE, twoRDM);
    }
    else
      twoRDM.setZero(norbs*(norbs+1)/2, norbs*(norbs+1)/2);
  }

  if (mpigetrank() == 0) {
    char file [5000];
    sprintf (file, "%s/%d-spatialRDM.bkp" , schd.prefix[0].c_str(), root );
    std::ifstream ifs(file, std::ios::binary);
    boost::archive::binary_iarchive load(ifs);
    load >> s2RDM;
    //ComputeEnergyFromSpatialRDM(nSpatOrbs, nelec, I1, I2, coreE, s2RDM);
  }
  else
    s2RDM.setZero(nSpatOrbs*nSpatOrbs, nSpatOrbs*nSpatOrbs);

}

void SHCIrdm::load3RDM(schedule& schd, MatrixXx& s3RDM, int root) {
  // TODO 3RDM is currently only for the spatial 3RDM not spin.
  int nSpatOrbs = pow(s3RDM.rows(),1/3);
  int nSpatOrbs2 = nSpatOrbs*nSpatOrbs;

  if (mpigetrank() == 0) {
    char file [5000];
    sprintf (file, "%s/%d-spatial3RDM.bkp", schd.prefix[0].c_str(), root );
    std::ifstream ifs(file, std::ios::binary);
    boost::archive::binary_iarchive load(ifs);
    load >> s3RDM;

  }
  else
    s3RDM.setZero(nSpatOrbs*nSpatOrbs2, nSpatOrbs*nSpatOrbs2);

}

void SHCIrdm::saveRDM(schedule& schd, MatrixXx& s2RDM, MatrixXx& twoRDM, int root) {
  int nSpatOrbs = pow(s2RDM.rows(),0.5);
  if(mpigetrank() == 0) {
    char file [5000];
    sprintf (file, "%s/spatialRDM.%d.%d.txt" , schd.prefix[0].c_str(), root, root );
    std::ofstream ofs(file, std::ios::out);
    ofs << nSpatOrbs<<endl;

    for (int n1=0; n1<nSpatOrbs; n1++)
      for (int n2=0; n2<nSpatOrbs; n2++)
	for (int n3=0; n3<nSpatOrbs; n3++)
	  for (int n4=0; n4<nSpatOrbs; n4++)
	    {
	      if (abs(s2RDM(n1*nSpatOrbs+n2, n3*nSpatOrbs+n4))  > 1.e-6)
		ofs << str(boost::format("%3d   %3d   %3d   %3d   %10.8g\n") % n1 % n2 % n3 % n4 % s2RDM(n1*nSpatOrbs+n2, n3*nSpatOrbs+n4));
	    }
    ofs.close();


    if (schd.DoSpinRDM) {
      char file [5000];
      sprintf (file, "%s/%d-spinRDM.bkp" , schd.prefix[0].c_str(), root );
      std::ofstream ofs(file, std::ios::binary);
      boost::archive::binary_oarchive save(ofs);
      save << twoRDM;
      //ComputeEnergyFromSpinRDM(norbs, nelec, I1, I2, coreE, twoRDM);
    }

    {
      char file [5000];
      sprintf (file, "%s/%d-spatialRDM.bkp" , schd.prefix[0].c_str(), root );
      std::ofstream ofs(file, std::ios::binary);
      boost::archive::binary_oarchive save(ofs);
      save << s2RDM;
      //ComputeEnergyFromSpatialRDM(nSpatOrbs, nelec, I1, I2, coreE, s2RDM);
    }
  }

}

void SHCIrdm::save3RDM(schedule& schd, MatrixXx& threeRDM, MatrixXx& s3RDM,
			int root, size_t norbs ) {
  int nSpatOrbs = pow(s3RDM.rows(),1/3.0);
  int nSpatOrbs2 = nSpatOrbs*nSpatOrbs;

  if(mpigetrank() == 0) {
    {
      char file[5000];
      sprintf (file, "spatial3RDM.%d.%d.txt", root, root);
      std::ofstream ofs(file, std::ios::out);
      ofs << nSpatOrbs << endl;

      for (int n0=0; n0 < nSpatOrbs; n0++)
	for (int n1=0; n1 < nSpatOrbs; n1++)
	  for (int n2=0; n2 < nSpatOrbs; n2++)
	    for (int n3=0; n3 < nSpatOrbs; n3++)
	      for (int n4=0; n4 < nSpatOrbs; n4++)
		for (int n5=0; n5 < nSpatOrbs; n5++) {
		  if ( abs( s3RDM(n0*nSpatOrbs2+n1*nSpatOrbs+n2,
				  n3*nSpatOrbs2+n4*nSpatOrbs+n5) ) > 1.e-12 ) {
		    ofs << str(boost::format("%3d   %3d   %3d   %3d   %3d   %3d   %10.8g\n")%n0%n1%n2%n3%n4%n5%s3RDM(n0*nSpatOrbs2+n1*nSpatOrbs+n2,n3*nSpatOrbs2+n4*nSpatOrbs+n5));
		  } 
		}
      ofs.close();
    }

    if ( true ) { // TODO change to schd.DoSpinRDM
      char file [5000];
      sprintf (file, "%s/%d-spin3RDM.bkp", schd.prefix[0].c_str(), root);
      std::ofstream ofs( file, std::ios::binary );
      boost::archive::binary_oarchive save(ofs);
      save << threeRDM;
    }

    {
      char file [5000];
      sprintf (file, "%s/%d-spatial3RDM.bkp", schd.prefix[0].c_str(), root );
      std::ofstream ofs(file, std::ios::binary);
      boost::archive::binary_oarchive save(ofs);
      save << s3RDM;
    }
  }
}

void SHCIrdm::save4RDM(schedule& schd, MatrixXx& fourRDM, MatrixXx& s4RDM,
			int root, int norbs ) {
  int n = norbs/2;
  int n2 = n*n; int n3 = n2*n;

  if(mpigetrank() == 0) {
    {
      char file[5000];
      sprintf (file, "spatial4RDM.%d.%d.txt", root, root);
      std::ofstream ofs(file, std::ios::out);
      ofs << n << endl;

      for (int c0=0; c0 < n; c0++)
	for (int c1=0; c1 < n; c1++)
	  for (int c2=0; c2 < n; c2++)
	    for (int c3=0; c3 < n; c3++)
	      for (int d0=0; d0 < n; d0++)
		for (int d1=0; d1 < n; d1++) 
		  for (int d2=0; d2 < n; d2++) 
		    for (int d3=0; d3 < n; d3++) {
		      if ( abs( s4RDM( n3*c0+n2*c1+n*c2+c3, 
				       n3*d0+n2*d1+n*d2+d3) ) > 1.e-12 ) {
			ofs << str(boost::format("%3d   %3d   %3d   %3d   %3d   %3d   %3d   %3d   %10.8g\n")%c0%c1%c2%c3%d0%d1%d2%d3%s4RDM( n3*c0+n2*c1+n*c2+c3, n3*d0+n2*d1+n*d2+d3 ) );
		      } 
		    }
      ofs.close();
    }
    /*
    if ( true ) { // TODO change to schd.DoSpinRDM
      char file [5000];
      sprintf (file, "%s/%d-spin4RDM.bkp", schd.prefix[0].c_str(), root);
      std::ofstream ofs( file, std::ios::binary );
      boost::archive::binary_oarchive save(ofs);
      save << fourRDM;
    }

    {
      char file [5000];
      sprintf (file, "%s/%d-spatial4RDM.bkp", schd.prefix[0].c_str(), root );
      std::ofstream ofs(file, std::ios::binary);
      boost::archive::binary_oarchive save(ofs);
      save << s4RDM;
    }
    */
  }
}


void SHCIrdm::UpdateRDMPerturbativeDeterministic(vector<Determinant>& Dets, MatrixXx& ci, double& E0,
						 oneInt& I1, twoInt& I2, schedule& schd,
						 double coreE, int nelec, int norbs,
						 std::vector<StitchDEH>& uniqueDEH, int root,
						 MatrixXx& s2RDM, MatrixXx& twoRDM) {

  int nSpatOrbs = norbs/2;

  int num_thrds = omp_get_max_threads();
  for (int thrd = 0; thrd <num_thrds; thrd++) {

    vector<Determinant>& uniqueDets = *uniqueDEH[thrd].Det;
    vector<double>& uniqueEnergy = *uniqueDEH[thrd].Energy;
    vector<CItype>& uniqueNumerator = *uniqueDEH[thrd].Num;
    vector<vector<int> >& uniqueVarIndices = *uniqueDEH[thrd].var_indices;
    vector<vector<size_t> >& uniqueOrbDiff = *uniqueDEH[thrd].orbDifference;

    for (size_t k=0; k<uniqueDets.size();k++) {
      for (size_t i=0; i<uniqueVarIndices[k].size(); i++){
	int d0=uniqueOrbDiff[k][i]%norbs, c0=(uniqueOrbDiff[k][i]/norbs)%norbs;

	if (uniqueOrbDiff[k][i]/norbs/norbs == 0) { // single excitation
	  vector<int> closed(nelec, 0);
	  vector<int> open(norbs-nelec,0);
	  Dets[uniqueVarIndices[k][i]].getOpenClosed(open, closed);
	  for (int n1=0;n1<nelec; n1++) {
	    double sgn = 1.0;
	    int a=max(closed[n1],c0), b=min(closed[n1],c0), I=max(closed[n1],d0), J=min(closed[n1],d0);
	    if (closed[n1] == d0) continue;
	    Dets[uniqueVarIndices[k][i]].parity(min(d0,c0), max(d0,c0),sgn);
	    if (!( (closed[n1] > c0 && closed[n1] > d0) || (closed[n1] < c0 && closed[n1] < d0))) sgn *=-1.;
	    if (schd.DoSpinRDM) {
	      twoRDM(a*(a+1)/2+b, I*(I+1)/2+J) += 0.5*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]);
	      twoRDM(I*(I+1)/2+J, a*(a+1)/2+b) += 0.5*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]);
	    }
	    populateSpatialRDM(a, b, I, J, s2RDM, 0.5*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]), nSpatOrbs);
	    populateSpatialRDM(I, J, a, b, s2RDM, 0.5*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]), nSpatOrbs);
	  } // for n1
	}  // single
	else { // double excitation
	  int d1=(uniqueOrbDiff[k][i]/norbs/norbs)%norbs, c1=(uniqueOrbDiff[k][i]/norbs/norbs/norbs)%norbs ;
	  double sgn = 1.0;
	  Dets[uniqueVarIndices[k][i]].parity(d1,d0,c1,c0,sgn);
	  int P = max(c1,c0), Q = min(c1,c0), R = max(d1,d0), S = min(d1,d0);
	  if (P != c0)  sgn *= -1;
	  if (Q != d0)  sgn *= -1;

	  if (schd.DoSpinRDM) {
	    twoRDM(P*(P+1)/2+Q, R*(R+1)/2+S) += 0.5*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]);
	    twoRDM(R*(R+1)/2+S, P*(P+1)/2+Q) += 0.5*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]);
	  }

	  populateSpatialRDM(P, Q, R, S, s2RDM, 0.5*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]), nSpatOrbs);
	  populateSpatialRDM(R, S, P, Q, s2RDM, 0.5*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]), nSpatOrbs);
	}// If
      } // i in variational connections to PT det k
    } // k in PT dets
  } //thrd in num_thrds

#ifndef SERIAL
  if (schd.DoSpinRDM)
    MPI_Allreduce(MPI_IN_PLACE, &twoRDM(0,0), twoRDM.rows()*twoRDM.cols(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  MPI_Allreduce(MPI_IN_PLACE, &s2RDM(0,0), s2RDM.rows()*s2RDM.cols(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif


}


void SHCIrdm::UpdateRDMResponsePerturbativeDeterministic(vector<Determinant>& Dets, MatrixXx& ci, double& E0,
							 oneInt& I1, twoInt& I2, schedule& schd,
							 double coreE, int nelec, int norbs,
							 std::vector<StitchDEH>& uniqueDEH, int root,
							 double& Psi1Norm, MatrixXx& s2RDM, MatrixXx& twoRDM) {

  int nSpatOrbs = norbs/2;

  s2RDM *=(1.-Psi1Norm);

  int num_thrds = omp_get_max_threads();
  for (int thrd = 0; thrd <num_thrds; thrd++) {

    vector<Determinant>& uniqueDets = *uniqueDEH[thrd].Det;
    vector<double>& uniqueEnergy = *uniqueDEH[thrd].Energy;
    vector<CItype>& uniqueNumerator = *uniqueDEH[thrd].Num;
    vector<vector<int> >& uniqueVarIndices = *uniqueDEH[thrd].var_indices;
    vector<vector<size_t> >& uniqueOrbDiff = *uniqueDEH[thrd].orbDifference;

    for (size_t i=0; i<uniqueDets.size();i++)
    {
      vector<int> closed(nelec, 0);
      vector<int> open(norbs-nelec,0);
      uniqueDets[i].getOpenClosed(open, closed);

      CItype coeff = uniqueNumerator[i]/(E0-uniqueEnergy[i]);
      //<Di| Gamma |Di>
      for (int n1=0; n1<nelec; n1++) {
	for (int n2=0; n2<n1; n2++) {
	  int orb1 = closed[n1], orb2 = closed[n2];
	  if (schd.DoSpinRDM)
	    twoRDM(orb1*(orb1+1)/2 + orb2, orb1*(orb1+1)/2+orb2) += conj(coeff)*coeff;
	  populateSpatialRDM(orb1, orb2, orb1, orb2, s2RDM, conj(coeff)*coeff, nSpatOrbs);
	}
      }

    }

    for (size_t k=0; k<uniqueDets.size();k++) {
      for (size_t i=0; i<uniqueVarIndices[k].size(); i++){
	int d0=uniqueOrbDiff[k][i]%norbs, c0=(uniqueOrbDiff[k][i]/norbs)%norbs;

	if (uniqueOrbDiff[k][i]/norbs/norbs == 0) { // single excitation
	  vector<int> closed(nelec, 0);
	  vector<int> open(norbs-nelec,0);
	  Dets[uniqueVarIndices[k][i]].getOpenClosed(open, closed);
	  for (int n1=0;n1<nelec; n1++) {
	    double sgn = 1.0;
	    int a=max(closed[n1],c0), b=min(closed[n1],c0), I=max(closed[n1],d0), J=min(closed[n1],d0);
	    if (closed[n1] == d0) continue;
	    Dets[uniqueVarIndices[k][i]].parity(min(d0,c0), max(d0,c0),sgn);
	    if (!( (closed[n1] > c0 && closed[n1] > d0) || (closed[n1] < c0 && closed[n1] < d0))) sgn *=-1.;
	    if (schd.DoSpinRDM) {
	      twoRDM(a*(a+1)/2+b, I*(I+1)/2+J) += 1.0*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]);
	      twoRDM(I*(I+1)/2+J, a*(a+1)/2+b) += 1.0*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]);
	    }
	    populateSpatialRDM(a, b, I, J, s2RDM, 1.0*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]), nSpatOrbs);
	    populateSpatialRDM(I, J, a, b, s2RDM, 1.0*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]), nSpatOrbs);
	  } // for n1
	}  // single
	else { // double excitation
	  int d1=(uniqueOrbDiff[k][i]/norbs/norbs)%norbs, c1=(uniqueOrbDiff[k][i]/norbs/norbs/norbs)%norbs ;
	  double sgn = 1.0;
	  Dets[uniqueVarIndices[k][i]].parity(d1,d0,c1,c0,sgn);
	  int P = max(c1,c0), Q = min(c1,c0), R = max(d1,d0), S = min(d1,d0);
	  if (P != c0)  sgn *= -1;
	  if (Q != d0)  sgn *= -1;

	  if (schd.DoSpinRDM) {
	    twoRDM(P*(P+1)/2+Q, R*(R+1)/2+S) += 1.0*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]);
	    twoRDM(R*(R+1)/2+S, P*(P+1)/2+Q) += 1.0*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]);
	  }

	  populateSpatialRDM(P, Q, R, S, s2RDM, 1.0*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]), nSpatOrbs);
	  populateSpatialRDM(R, S, P, Q, s2RDM, 1.0*sgn*uniqueNumerator[k]*ci(uniqueVarIndices[k][i],0)/(E0-uniqueEnergy[k]), nSpatOrbs);
	}// If
      } // i in variational connections to PT det k
    } // k in PT dets
  } //thrd in num_thrds

#ifndef SERIAL
  if (schd.DoSpinRDM)
    MPI_Allreduce(MPI_IN_PLACE, &twoRDM(0,0), twoRDM.rows()*twoRDM.cols(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  MPI_Allreduce(MPI_IN_PLACE, &s2RDM(0,0), s2RDM.rows()*s2RDM.cols(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif

}


void SHCIrdm::populateSpatialRDM(int& i, int& j, int& k, int& l, MatrixXx& s2RDM,
				 CItype value, int& nSpatOrbs) {
  //we assume i != j  and  k != l
  int I = i/2, J=j/2, K=k/2, L=l/2;
  if (i%2 == l%2 && j%2 == k%2) {
    s2RDM(I*nSpatOrbs+J,L*nSpatOrbs+K) -= value;
    s2RDM(J*nSpatOrbs+I,K*nSpatOrbs+L) -= value;
  }

  if (i%2 == k%2 && l%2 == j%2 ) {
    s2RDM(I*nSpatOrbs+J,K*nSpatOrbs+L) += value;
    s2RDM(J*nSpatOrbs+I,L*nSpatOrbs+K) += value;
  }

}

void SHCIrdm::EvaluateRDM(vector<vector<int> >& connections, vector<Determinant>& Dets,
			  MatrixXx& cibra, MatrixXx& ciket,
			  vector<vector<size_t> >& orbDifference, int nelec,
			  schedule& schd, int root, MatrixXx& twoRDM, MatrixXx& s2RDM) {
#ifndef SERIAL
  boost::mpi::communicator world;
#endif

  size_t norbs = Dets[0].norbs;
  int nSpatOrbs = norbs/2;

  int num_thrds = omp_get_max_threads();

  //#pragma omp parallel for schedule(dynamic)
  for (int i=0; i<Dets.size(); i++) {
    if ((i/num_thrds)%mpigetsize() != mpigetrank()) continue;

    vector<int> closed(nelec, 0);
    vector<int> open(norbs-nelec,0);
    Dets[i].getOpenClosed(open, closed);

    //<Di| Gamma |Di>
    for (int n1=0; n1<nelec; n1++) {
      for (int n2=0; n2<n1; n2++) {
	int orb1 = closed[n1], orb2 = closed[n2];
	if (schd.DoSpinRDM)
	  twoRDM(orb1*(orb1+1)/2 + orb2, orb1*(orb1+1)/2+orb2) += conj(cibra(i,0))*ciket(i,0);
	populateSpatialRDM(orb1, orb2, orb1, orb2, s2RDM, conj(cibra(i,0))*ciket(i,0), nSpatOrbs);
      }
    }

    for (int j=1; j<connections[i].size(); j++) {
      int d0=orbDifference[i][j]%norbs, c0=(orbDifference[i][j]/norbs)%norbs ;
      if (orbDifference[i][j]/norbs/norbs == 0) { //only single excitation
	for (int n1=0;n1<nelec; n1++) {
	  double sgn = 1.0;
	  int a=max(closed[n1],c0), b=min(closed[n1],c0), I=max(closed[n1],d0), J=min(closed[n1],d0);
	  if (closed[n1] == d0) continue;
	  if (closed[n1] == c0) continue; // TODO REMOVE
	  Dets[i].parity(min(d0,c0), max(d0,c0),sgn);
	  if (!( (closed[n1] > c0 && closed[n1] > d0) || (closed[n1] < c0 && closed[n1] < d0))) sgn *=-1.;
	  if (schd.DoSpinRDM) {
	    twoRDM(a*(a+1)/2+b, I*(I+1)/2+J) += sgn*conj(cibra(connections[i][j],0))*ciket(i,0);
	    twoRDM(I*(I+1)/2+J, a*(a+1)/2+b) += sgn*conj(ciket(connections[i][j],0))*cibra(i,0);
	  }

	  populateSpatialRDM(a, b, I, J, s2RDM, sgn*conj(cibra(connections[i][j],0))*ciket(i,0), nSpatOrbs);
	  populateSpatialRDM(I, J, a, b, s2RDM, sgn*conj(ciket(connections[i][j],0))*cibra(i,0), nSpatOrbs);

	}
      }
      else {
	int d1=(orbDifference[i][j]/norbs/norbs)%norbs, c1=(orbDifference[i][j]/norbs/norbs/norbs)%norbs ;
	double sgn = 1.0;

	Dets[i].parity(d1,d0,c1,c0,sgn);

	if (schd.DoSpinRDM) {
	  twoRDM(c1*(c1+1)/2+c0, d1*(d1+1)/2+d0) += sgn*conj(cibra(connections[i][j],0))*ciket(i,0);
	  twoRDM(d1*(d1+1)/2+d0, c1*(c1+1)/2+c0) += sgn*conj(ciket(connections[i][j],0))*cibra(i,0);
	}

	populateSpatialRDM(c1, c0, d1, d0, s2RDM, sgn*conj(cibra(connections[i][j],0))*ciket(i,0), nSpatOrbs);
	populateSpatialRDM(d1, d0, c1, c0, s2RDM, sgn*conj(ciket(connections[i][j],0))*cibra(i,0), nSpatOrbs);
      }
    }

  }

#ifndef SERIAL
  if (schd.DoSpinRDM)
    MPI_Allreduce(MPI_IN_PLACE, &twoRDM(0,0), twoRDM.rows()*twoRDM.cols(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  MPI_Allreduce(MPI_IN_PLACE, &s2RDM(0,0), s2RDM.rows()*s2RDM.cols(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif
}


void SHCIrdm::EvaluateOneRDM(vector<vector<int> >& connections, vector<Determinant>& Dets,
			     MatrixXx& cibra, MatrixXx& ciket,
			     vector<vector<size_t> >& orbDifference, int nelec,
			     schedule& schd, int root, MatrixXx& s1RDM) {
#ifndef SERIAL
  boost::mpi::communicator world;
#endif
  size_t norbs = Dets[0].norbs;
  int nSpatOrbs = norbs/2;


  int num_thrds = omp_get_max_threads();

  //#pragma omp parallel for schedule(dynamic)
  for (int i=0; i<Dets.size(); i++) {
    if (i%mpigetsize() != mpigetrank()) continue;

    vector<int> closed(nelec, 0);
    vector<int> open(norbs-nelec,0);
    Dets[i].getOpenClosed(open, closed);

    //<Di| Gamma |Di>
    for (int n1=0; n1<nelec; n1++) {
      int orb1 = closed[n1];
      s1RDM(orb1, orb1) += conj(cibra(i,0))*ciket(i,0);
    }

    for (int j=1; j<connections[i].size(); j++) {
      int d0=orbDifference[i][j]%norbs, c0=(orbDifference[i][j]/norbs)%norbs ;
      if (orbDifference[i][j]/norbs/norbs == 0) { //only single excitation
	double sgn = 1.0;
	Dets[i].parity(min(c0,d0), max(c0,d0),sgn);
	s1RDM(c0, d0)+= sgn*conj(cibra(connections[i][j],0))*ciket(i,0);
      }
    }
  }

#ifndef SERIAL
  MPI_Allreduce(MPI_IN_PLACE, &s1RDM(0,0), s1RDM.rows()*s1RDM.cols(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif
}



double SHCIrdm::ComputeEnergyFromSpinRDM(int norbs, int nelec, oneInt& I1, twoInt& I2,
				       double coreE, MatrixXx& twoRDM) {

  //RDM(i,j,k,l) = a_i^\dag a_j^\dag a_l a_k
  //also i>=j and k>=l
  double energy = coreE;
  double onebody = 0.0;
  double twobody = 0.0;
  //if (mpigetrank() == 0)  cout << "Core energy= " << energy << endl;

  MatrixXx oneRDM = MatrixXx::Zero(norbs, norbs);
#pragma omp parallel for schedule(dynamic)
  for (int p=0; p<norbs; p++)
    for (int q=0; q<norbs; q++)
      for (int r=0; r<norbs; r++) {
	int P = max(p,r), R1 = min(p,r);
	int Q = max(q,r), R2 = min(q,r);
	double sgn = 1.;
	if (P != p)  sgn *= -1;
	if (Q != q)  sgn *= -1;

	oneRDM(p,q) += sgn*twoRDM(P*(P+1)/2+R1,Q*(Q+1)/2+R2)/(nelec-1.);
      }

#pragma omp parallel for reduction(+ : onebody)
  for (int p=0; p<norbs; p++)
    for (int q=0; q<norbs; q++)
#ifdef Complex
      onebody += (I1(p, q)*oneRDM(p,q)).real();
#else
  onebody += I1(p, q)*oneRDM(p,q);
#endif

#pragma omp parallel for reduction(+ : twobody)
  for (int p=0; p<norbs; p++){
    for (int q=0; q<norbs; q++){
      for (int r=0; r<norbs; r++){
	for (int s=0; s<norbs; s++){
	  //if (p%2 != r%2 || q%2 != s%2)  continue; // This line is not necessary
	  int P = max(p,q), Q = min(p,q);
	  int R = max(r,s), S = min(r,s);
	  double sgn = 1;
	  if (P != p)  sgn *= -1;
	  if (R != r)  sgn *= -1;
#ifdef Complex
	  twobody += (sgn * 0.5 * twoRDM(P*(P+1)/2+Q, R*(R+1)/2+S) * I2(p,r,q,s)).real(); // 2-body term
#else
	  twobody += sgn * 0.5 * twoRDM(P*(P+1)/2+Q, R*(R+1)/2+S) * I2(p,r,q,s); // 2-body term
#endif
	}
      }
    }
  }

  //if (mpigetrank() == 0)  cout << "One-body from 2RDM: " << onebody << endl;
  //if (mpigetrank() == 0)  cout << "Two-body from 2RDM: " << twobody << endl;

  energy += onebody + twobody;
  if (mpigetrank() == 0)  cout << "E from 2RDM: " << energy << endl;
  return energy;
}


double SHCIrdm::ComputeEnergyFromSpatialRDM(int norbs, int nelec, oneInt& I1, twoInt& I2,
					  double coreE, MatrixXx& twoRDM) {

  double energy = coreE;
  double onebody = 0.0;
  double twobody = 0.0;

  MatrixXx oneRDM = MatrixXx::Zero(norbs, norbs);
#pragma omp parallel for schedule(dynamic)
  for (int p=0; p<norbs; p++)
    for (int q=0; q<norbs; q++)
      for (int r=0; r<norbs; r++)
	oneRDM(p,q) += twoRDM(p*norbs+r, q*norbs+ r)/(1.*nelec-1.);

#pragma omp parallel for reduction(+ : onebody)
  for (int p=0; p<norbs; p++)
    for (int q=0; q<norbs; q++) {
#ifdef Complex
      onebody += (I1(2*p, 2*q)*oneRDM(p,q)).real();
#else
      onebody += I1(2*p, 2*q)*oneRDM(p,q);
#endif
    }

#pragma omp parallel for reduction(+ : twobody)
  for (int p=0; p<norbs; p++)
    for (int q=0; q<norbs; q++)
      for (int r=0; r<norbs; r++)
	for (int s=0; s<norbs; s++)
#ifdef Complex
	  twobody +=  (0.5 * twoRDM(p*norbs+q,r*norbs+s) * I2(2*p,2*r,2*q,2*s)).real(); // 2-body term
#else
  twobody +=  0.5*twoRDM(p*norbs+q,r*norbs+s) * I2(2*p,2*r,2*q,2*s); // 2-body term
#endif

  energy += onebody + twobody;
  if (mpigetrank() == 0)  cout << "E from 2RDM: " << energy << endl;
  return energy;
}


/*
*** 3-RDM Methods
 */


inline void SHCIrdm::getUniqueIndices( Determinant& bra, Determinant& ket,
  vector<int>& cs, vector<int>& ds ) {
  // Appends two lists of creation and annihilation operator indices.
  for ( int i=0; i < bra.norbs; i++ ) {
    if ( bra.getocc(i) == ket.getocc(i) ) { continue; }
    else {
      if ( ket.getocc(i) == 0 ) { cs.push_back(i); }
      else { ds.push_back(i); }
    }
  }
  return; 
}


int genIdx( int& a, int& b, int& c, size_t& norbs ) {
  return a*norbs*norbs+b*norbs+c;
} 


void popSpin3RDM( vector<int>& cs, vector<int>& ds, CItype value,
		   size_t& norbs, MatrixXx& threeRDM ) {
  // d2->c0    d1->c1    d2->c0
  int cI[] = {0,1,2}; int dI[] = {0,1,2}; 
  int ctr = 0;
  vector<double> pars (6);
  pars[0]=1.; pars[1]=-1.; pars[2]=-1.; pars[3]=1.; pars[4]=1.; pars[5]=-1.;
  double par;

  do {
    do {
      par = pars[ctr/6]*pars[ctr%6];
      ctr++;
      threeRDM( genIdx(cs[cI[0]],cs[cI[1]],cs[cI[2]], norbs),
		genIdx(ds[dI[0]],ds[dI[1]],ds[dI[2]], norbs) ) += par*value;
    } while ( next_permutation( dI,dI+3 ) );
  } while ( next_permutation( cI, cI+3 ) );
  return;
}


void SHCIrdm::Evaluate3RDM( vector<Determinant>& Dets, MatrixXx& cibra,
			    MatrixXx& ciket, int nelec, schedule& schd, int root,
			    MatrixXx & threeRDM, MatrixXx& s3RDM ) {
  /*
     TODO optimize speed and memory
     TODO make spin 3RDM optional
  */
#ifndef SERIAL
  boost::mpi::communicator world;
#endif
  int num_thrds = omp_get_max_threads();
  int nprocs = mpigetsize(), proc = mpigetrank();

  size_t norbs = Dets[0].norbs;
  int nSpatOrbs = norbs/2; size_t nSOrbs = norbs/2;
  int nSpatOrbs2 = nSpatOrbs * nSpatOrbs;

  for (int b=0; b < Dets.size(); b++ ) {
    for (int k=0; k < Dets.size(); k++ ) {

      int dist = Dets[b].ExcitationDistance( Dets[k] );
      if ( dist > 3 ) { continue; } 
      vector<int> cs (0), ds(0);
      getUniqueIndices( Dets[b], Dets[k], cs, ds );
      //double sgn = 1.0;

      if ( dist == 3 ) {
	double sgn = 1.0;
        Dets[k].parity( cs[0], cs[1], cs[2], ds[0], ds[1], ds[2], sgn );
	popSpin3RDM(cs,ds,sgn*conj(cibra(b,0))*ciket(k,0),norbs,threeRDM);       
      }

      else if ( dist == 2 ) {
        vector<int> closed(nelec, 0);
        vector<int> open(norbs-nelec,0);
        Dets[k].getOpenClosed(open, closed);

	// Initialize the final spot in operator arrays and move d ops toward
	// ket so inner two c/d operators are paired
	cs.push_back(0); ds.push_back(0); 
	ds[2] = ds[1]; ds[1] = ds[0]; 

        for ( int x=0; x < nelec; x++ ) {
          cs[2]=closed[x]; ds[0]=closed[x];
	  if ( closed[x] == ds[1] || closed[x] == ds[2] ) continue;
	  if ( closed[x] == cs[0] || closed[x] == cs[1] ) continue; // TODO CHECK W/ SS
	  
	  // Gamma = c0 c1 c2 d0 d1 d2
	  double sgn = 1.0;
	  Dets[k].parity( ds[2], ds[1], cs[0], cs[1], sgn ); // TOOD	  
	  
	  // TODO Make spinRDM optional
	  popSpin3RDM(cs,ds,sgn*conj(cibra(b,0))*ciket(k,0),norbs,threeRDM);       
	}
      }
    

      else if ( dist == 1 ) {
	vector<int> closed(nelec,0);
	vector<int> open(norbs-nelec,0);
	Dets[k].getOpenClosed(open,closed);
	cs.push_back(0); cs.push_back(0);
	ds.push_back(0); ds.push_back(0);
	ds[2] = ds[0];
	
	for ( int x=0; x<nelec; x++ ) {
	  cs[1] = closed[x]; ds[1] = closed[x];
	  if ( closed[x] == cs[0] || closed[x] == ds[2] ) continue;
	  for ( int y=0; y<x; y++ ) {
	    cs[2] = closed[y]; ds[0] = closed[y];
	    if ( closed[y] == ds[1] || closed[y] == ds[2] ) continue;
	    if ( closed[y] == cs[0] || closed[y] == cs[1] ) continue; // TODO CHECK W/ SS

	    // Gamma = c0 c1 c2 d0 d1 d2
	    double sgn = 1.0;
	    Dets[k].parity( min(cs[0],ds[2]), max(cs[0],ds[2]), sgn ); // TODO Update repop order

	    popSpin3RDM(cs,ds,sgn*conj(cibra(b,0))*ciket(k,0),norbs,threeRDM);       
	  }
	}
      }

      else if ( dist == 0 ) {
	vector<int> closed(nelec, 0);
	vector<int> open(norbs-nelec,0);
	Dets[k].getOpenClosed(open, closed);
	cs.push_back(0); cs.push_back(0); cs.push_back(0);
	ds.push_back(0); ds.push_back(0); ds.push_back(0);
	for ( int x=0; x < nelec; x++ ) {
	  cs[0]=closed[x]; ds[2]=closed[x];
	  for ( int y=0; y < x; y++ ) {
	    cs[1]=closed[y]; ds[1]=closed[y];
	    for ( int z=0; z < y; z++ ) {
	      cs[2]=closed[z]; ds[0]=closed[z];	      

	      popSpin3RDM(cs,ds,conj(cibra(b,0))*ciket(k,0),norbs,threeRDM);       

	    }	
	  }
	}
      }
    }
  }

  for ( int c0=0; c0 < nSpatOrbs; c0++ )
    for ( int c1=0; c1 < nSpatOrbs; c1++ )
      for ( int c2=0; c2 < nSpatOrbs; c2++ )
	for ( int d0=0; d0 < nSpatOrbs; d0++ )
	  for ( int d1=0; d1 < nSpatOrbs; d1++ )
	    for ( int d2=0; d2 < nSpatOrbs; d2++ ) 
	      for ( int i=0; i < 2; i++ )
		for ( int j=0; j < 2; j++ )
		  for ( int k=0; k < 2; k++ ) {
		    
		    int c0p=2*c0+i, c1p=2*c1+j, c2p=2*c2+k;
		    int d0p=2*d0+k, d1p=2*d1+j, d2p=2*d2+i;
		    
		    // Gamma = i j k l m n
		    s3RDM(genIdx(c0,c1,c2,nSOrbs),genIdx(d0,d1,d2,nSOrbs)) +=
			  threeRDM(genIdx(c0p,c1p,c2p,norbs),genIdx(d0p,d1p,d2p,norbs));
		  }
}


/*
*** 4-RDM Methods
 */

int gen4Idx(int& a, int& b, int& c, int& d, int& norbs) {
  return a*norbs*norbs*norbs+b*norbs*norbs+c*norbs+d;
}

void popSpin4RDM( vector<int>& cs, vector<int>& ds, CItype value, int& norbs,
		  MatrixXx& fourRDM ){

  int cI[] = {0,1,2,3}; int dI[] = {0,1,2,3}; 
  int ctr = 0;
  double pars[] = {1,-1,-1,1,1,-1,-1,1,1,-1,-1,1,1,-1,-1,1,1,-1,-1,1,1,-1,-1,1};
  double par= 1.0;

  do {
    do {
      par = pars[ctr/24]*pars[ctr%24];
      fourRDM( gen4Idx(cs[cI[0]],cs[cI[1]],cs[cI[2]],cs[cI[3]], norbs),
		gen4Idx(ds[dI[0]],ds[dI[1]],ds[dI[2]],ds[dI[3]], norbs) ) += par*value;
      ctr++;
    } while ( next_permutation( dI,dI+4 ) );
  } while ( next_permutation( cI, cI+4 ) );
  return;
}

void SHCIrdm::Evaluate4RDM( vector<Determinant>& Dets, MatrixXx& cibra,
			    MatrixXx& ciket, int nelec, schedule& schd,
			    int root, MatrixXx& fourRDM, MatrixXx& s4RDM ){

  /*
     TODO optimize speed and memory
     TODO make spin 4RDM optional
  */
#ifndef SERIAL
  boost::mpi::communicator world;
#endif
  int num_thrds = omp_get_max_threads();
  int nprocs = mpigetsize(), proc = mpigetrank();

  int norbs = Dets[0].norbs;
  int nSOs = norbs/2; // Number of spatial orbitals

  for (int b=0; b<Dets.size(); b++) {
    for (int k=0; k<Dets.size(); k++) {

      int dist = Dets[b].ExcitationDistance( Dets[k] );
      if ( dist > 4 ) { continue; } 
      vector<int> cs (0), ds(0);
      getUniqueIndices( Dets[b], Dets[k], cs, ds );

      if ( dist == 4 ) {
	double sgn = 1.0;
	Dets[k].parity(cs[0],cs[1],cs[2],cs[3],ds[0],ds[1],ds[2],ds[3],sgn);
	popSpin4RDM(cs,ds,sgn*conj(cibra(b,0))*ciket(k,0),norbs,fourRDM);
      }

      else if ( dist == 3 ) {
	vector<int> closed(nelec, 0);
        vector<int> open(norbs-nelec,0);
        Dets[k].getOpenClosed(open, closed);
	cs.push_back(0); ds.push_back(0); 
	ds[3]=ds[2]; ds[2]=ds[1]; ds[1]=ds[0]; //Keep notation

	for ( int w=0; w<nelec; w++) {
	  cs[3]=closed[w]; ds[0]=closed[w];
	  if ( closed[w]==cs[0] || closed[w]==cs[1] || closed[w]==cs[2] ||
	       closed[w]==ds[3] || closed[w]==ds[2] || closed[w]==ds[1] ) {
	    continue;
	  }

	  double sgn = 1.0;
	  Dets[k].parity(cs[0],cs[1],cs[2],ds[1],ds[2],ds[3],sgn);
	  popSpin4RDM(cs,ds,sgn*conj(cibra(b,0))*ciket(k,0),norbs,fourRDM);
	}
      }
      
      else if ( dist == 2 ) {
        vector<int> closed(nelec, 0);
        vector<int> open(norbs-nelec,0);
        Dets[k].getOpenClosed(open, closed);
	cs.push_back(0); ds.push_back(0); 
	cs.push_back(0); ds.push_back(0); 
	ds[3]=ds[1]; ds[2]=ds[0]; //Keep notation

	for (int w=0; w<nelec; w++){
	  cs[2]=closed[w]; ds[1]=closed[w]; 
	  if (closed[w]==cs[0] || closed[w]==cs[1]) continue;
	  if (closed[w]==ds[3] || closed[w]==ds[2]) continue;

	  for (int x=0; x<w; x++) {
	    cs[3]=closed[x]; ds[0]=closed[x];
	    if (closed[x]==cs[0] || closed[x]==cs[1]) continue;
	    if (closed[x]==ds[3] || closed[x]==ds[2]) continue; 

	    double sgn = 1.0;
	    Dets[k].parity(ds[3],ds[2],cs[0],cs[1],sgn); // SS notation
	    popSpin4RDM(cs,ds,sgn*conj(cibra(b,0))*ciket(k,0),norbs,fourRDM);	    
	  }
	}

      }

      else if ( dist == 1 ) {
        vector<int> closed(nelec, 0);
        vector<int> open(norbs-nelec,0);
        Dets[k].getOpenClosed(open, closed);
	cs.push_back(0); ds.push_back(0); 
	cs.push_back(0); ds.push_back(0);
	cs.push_back(0); ds.push_back(0); 

	ds[3]=ds[0]; //Keep notation

	for (int w=0; w<nelec; w++){
	  cs[1]=closed[w]; ds[2]=closed[w]; 
	  if (closed[w]==cs[0] || closed[w]==ds[3]) continue;

	  for (int x=0; x<w; x++) {
	    cs[2]=closed[x]; ds[1]=closed[x];
	    if (closed[x]==cs[0] || closed[x]==ds[3]) continue;

	    for (int y=0; y<x; y++) {
	      cs[3]=closed[y]; ds[0]=closed[y];
	      if (closed[y]==cs[0] || closed[y]==ds[3]) continue;

	      double sgn = 1.0;
	      Dets[k].parity( min(ds[3],cs[0]), max(ds[3],cs[0]), sgn); // SS notation
	      popSpin4RDM(cs,ds,sgn*conj(cibra(b,0))*ciket(k,0),norbs,fourRDM);	    
	    }
	  }
	}
      }

      else if ( dist == 0 ) {
        vector<int> closed(nelec, 0);
        vector<int> open(norbs-nelec,0);
        Dets[k].getOpenClosed(open, closed);
	cs.push_back(0); ds.push_back(0); 
	cs.push_back(0); ds.push_back(0); 
	cs.push_back(0); ds.push_back(0);
	cs.push_back(0); ds.push_back(0); 

	for (int w=0; w<nelec; w++){
	  cs[0]=closed[w]; ds[3]=closed[w]; 	  
	  for (int x=0; x<w; x++) {
	    cs[1]=closed[x]; ds[2]=closed[x];	   
	    for (int y=0; y<x; y++) {
	      cs[2]=closed[y]; ds[1]=closed[y];
	      for (int z=0; z<y; z++ ){
		cs[3]=closed[z]; ds[0]=closed[z];

		popSpin4RDM(cs,ds,conj(cibra(b,0))*ciket(k,0),norbs,fourRDM);	    
	      }
	    }
	  }
	}
      }
    }
  }
  cout << "Populating spatial 4-RDM" << endl; // TODO
  // Pop. Spatial 4RDM
  for ( int c0=0; c0 < nSOs; c0++ )
    for ( int c1=0; c1 < nSOs; c1++ )
      for ( int c2=0; c2 < nSOs; c2++ )
	for ( int c3=0; c3 < nSOs; c3++ )
	  for ( int d0=0; d0 < nSOs; d0++ )
	    for ( int d1=0; d1 < nSOs; d1++ )
	      for ( int d2=0; d2 < nSOs; d2++ ) 
		for ( int d3=0; d3 < nSOs; d3++ ) 
		  for ( int i=0; i < 2; i++ )
		    for ( int j=0; j < 2; j++ )
		      for ( int k=0; k < 2; k++ )
			for ( int l=0; l < 2; l++ ) {
			  
			  int c0p=2*c0+i, c1p=2*c1+j, c2p=2*c2+k, c3p=2*c3+l;
			  int d0p=2*d0+l, d1p=2*d1+k, d2p=2*d2+j, d3p=2*d3+i;

			  // Gamma = c0 c1 c2 c3 d0 d1 d2 d3
			  s4RDM(gen4Idx(c0,c1,c2,c3,nSOs),
				gen4Idx(d0,d1,d2,d3,nSOs)) +=
			    fourRDM(gen4Idx(c0p,c1p,c2p,c3p,norbs),
				    gen4Idx(d0p,d1p,d2p,d3p,norbs));
			}
  return;
}
