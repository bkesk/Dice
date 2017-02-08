#ifndef HCI_HEADER_H
#define HCI_HEADER_H
#include <vector>
#include <Eigen/Dense>
#include <set>
#include <list>
#include <tuple>
#include <map>
#include <boost/serialization/serialization.hpp>

using namespace std;
using namespace Eigen;
class Determinant;
class HalfDet;
class oneInt;
class twoInt;
class twoIntHeatBath;
class schedule;

namespace HCIbasics {

  int sample_round(MatrixXd& ci, double eps, std::vector<int>& Sample1, std::vector<double>& newWts);
  void setUpAliasMethod(MatrixXd& ci, double& cumulative, std::vector<int>& alias, std::vector<double>& prob) ;
  int sample_N2_alias(MatrixXd& ci, double& cumulative, std::vector<int>& Sample1, std::vector<double>& newWts, std::vector<int>& alias, std::vector<double>& prob) ;
  int sample_N2(MatrixXd& ci, double& cumulative, std::vector<int>& Sample1, std::vector<double>& newWts);
  int sample_N(MatrixXd& ci, double& cumulative, std::vector<int>& Sample1, std::vector<double>& newWts);

  
  void PopulateHelperLists(std::map<HalfDet, std::vector<int> >& BetaN,
			   std::map<HalfDet, std::vector<int> >& AlphaNm1,
			   std::vector<Determinant>& Dets,
			   int StartIndex);

  void MakeHfromHelpers(std::map<HalfDet, std::vector<int> >& BetaN,
			std::map<HalfDet, std::vector<int> >& AlphaNm1,
			std::vector<Determinant>& Dets,
			int StartIndex,
			std::vector<std::vector<int> >&connections,
			std::vector<std::vector<double> >& Helements,
			int Norbs,
			oneInt& I1,
			twoInt& I2,
			double& coreE,
			std::vector<std::vector<size_t> >& orbDifference,
			bool DoRDM=false) ;


  void writeVariationalResult(int iter, vector<MatrixXd>& ci, vector<Determinant>& Dets, vector<Determinant>& SortedDets,
			      MatrixXd& diag, vector<vector<int> >& connections, vector<vector<double> >& Helements, 
			      vector<double>& E0, bool converged, schedule& schd,   
			      std::map<HalfDet, std::vector<int> >& BetaN, 
			      std::map<HalfDet, std::vector<int> >& AlphaNm1);
  void readVariationalResult(int& iter, vector<MatrixXd>& ci, vector<Determinant>& Dets, vector<Determinant>& SortedDets,
			     MatrixXd& diag, vector<vector<int> >& connections, vector<vector<double> >& Helements, 
			     vector<double>& E0, bool& converged, schedule& schd,
			     std::map<HalfDet, std::vector<int> >& BetaN, 
			     std::map<HalfDet, std::vector<int> >& AlphaNm1);
  vector<double> DoVariational(vector<MatrixXd>& ci, vector<Determinant>& Dets, schedule& schd,
			       twoInt& I2, twoIntHeatBath& I2HB, vector<int>& irrep, oneInt& I1, double& coreE, int nelec,
			       bool DoRDM=false);
  void EvaluateAndStoreRDM(vector<vector<int> >& connections, vector<Determinant>& Dets, MatrixXd& ci,
			   vector<vector<size_t> >& orbDifference, int nelec, schedule& schd, int root);



  void DoBatchDeterministic(vector<Determinant>& Dets, MatrixXd& ci, double& E0, oneInt& I1, twoInt& I2, 
			twoIntHeatBath& I2HB, vector<int>& irrep, schedule& schd, double coreE, int nelec);
  void DoPerturbativeStochastic(vector<Determinant>& Dets, MatrixXd& ci, double& E0, oneInt& I1, twoInt& I2, 
				twoIntHeatBath& I2HB, vector<int>& irrep, schedule& schd, double coreE, int nelec) ;
  void DoPerturbativeStochasticSingleList(vector<Determinant>& Dets, MatrixXd& ci, double& E0, oneInt& I1, twoInt& I2, 
					  twoIntHeatBath& I2HB, vector<int>& irrep, schedule& schd, double coreE, int nelec) ;
  void DoPerturbativeStochastic2(vector<Determinant>& Dets, MatrixXd& ci, double& E0, oneInt& I1, twoInt& I2, 
				twoIntHeatBath& I2HB, vector<int>& irrep, schedule& schd, double coreE, int nelec) ;
  void DoPerturbativeStochastic2SingleList(vector<Determinant>& Dets, MatrixXd& ci, double& E0, oneInt& I1, twoInt& I2, 
					   twoIntHeatBath& I2HB, vector<int>& irrep, schedule& schd, double coreE, int nelec, int root) ;
  double DoPerturbativeDeterministic(vector<Determinant>& Dets, MatrixXd& ci, double& E0, oneInt& I1, twoInt& I2, 
				     twoIntHeatBath& I2HB, vector<int>& irrep, schedule& schd, double coreE, int nelec,
				     bool appendPsi1ToPsi0=false) ;
  void DoPerturbativeStochastic2SingleListDoubleEpsilon2(vector<Determinant>& Dets, MatrixXd& ci, double& E0, oneInt& I1, twoInt& I2, 
							 twoIntHeatBath& I2HB, vector<int>& irrep, schedule& schd, double coreE, int nelec, int root) ;


  //used in batchdeterministic, doperturbativestochastic2, doperturbativestochastic
  //used in doperturbativestochastic2 when both the first and second list contain determinant d
  void getDeterminants(Determinant& d, double epsilon, double ci1, double ci2, oneInt& int1, twoInt& int2, twoIntHeatBath& I2hb, vector<int>& irreps, double coreE, double E0, std::map<Determinant, pair<double,double> >& dets, std::vector<Determinant>& Psi0, schedule& schd, int Nmc=0) ;
  void getDeterminants(Determinant& d, double epsilon, double ci1, double ci2, oneInt& int1, twoInt& int2, twoIntHeatBath& I2hb, vector<int>& irreps, double coreE, double E0, std::map<Determinant, tuple<double,double,double> >& Psi1, std::vector<Determinant>& Psi0, schedule& schd, int Nmc, int nelec);
  void getDeterminants2Epsilon(Determinant& d, double epsilon, double epsilonLarge, double ci1, double ci2, oneInt& int1, twoInt& int2, twoIntHeatBath& I2hb, vector<int>& irreps, double coreE, double E0, std::map<Determinant, tuple<double, double,double,double,double> >& Psi1, std::vector<Determinant>& Psi0, schedule& schd, int Nmc, int nelec);
  void updateConnections(vector<Determinant>& Dets, map<Determinant,int>& SortedDets, int norbs, oneInt& int1, twoInt& int2, double coreE, char* detArray, vector<vector<int> >& connections, vector<vector<double> >& Helements) ;

  void getDeterminants(Determinant& d, double epsilon, double ci1, double ci2, oneInt& int1, twoInt& int2, twoIntHeatBath& I2hb, vector<int>& irreps, double coreE, double E0, std::vector<Determinant>& dets, std::vector<double>& numerator, std::vector<double>& energy, schedule& schd, int Nmc, int nelec, bool mpispecific=true) ;
  void getDeterminants(Determinant& d, double epsilon, double ci1, double ci2, oneInt& int1, twoInt& int2, twoIntHeatBath& I2hb, vector<int>& irreps, double coreE, double E0, std::vector<Determinant>& dets, std::vector<double>& numerator1, std::vector<double>& numerator2, std::vector<double>& energy, schedule& schd, int Nmc, int nelec) ;

  void getDeterminants(Determinant& d, double epsilon, double ci1, double ci2, oneInt& int1, twoInt& int2, twoIntHeatBath& I2hb, vector<int>& irreps, double coreE, double E0, std::vector<Determinant>& dets, schedule& schd, int Nmc, int nelec) ;
  
  void getDeterminants2Epsilon(Determinant& d, double epsilon, double epsilonLarge, double ci1, double ci2, oneInt& int1, twoInt& int2, twoIntHeatBath& I2hb, vector<int>& irreps, double coreE, double E0, std::vector<Determinant>& dets, std::vector<double>& numerator1A, vector<double>& numerator2A, vector<bool>& present, std::vector<double>& energy, schedule& schd, int Nmc, int nelec);


}

#endif
