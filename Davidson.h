#ifndef DAVIDSON_HEADER_H
#define DAVIDSON_HEADER_H
#include <Eigen/Dense>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <vector>

class Hmult2;
using namespace Eigen;
using namespace std;


void precondition(MatrixXd& r, MatrixXd& diag, double& e);
vector<double> davidson(Hmult2& H, vector<MatrixXd>& x0, MatrixXd& diag, int maxCopies, double tol, bool print);
double LinearSolver(Hmult2& H, MatrixXd& x0, MatrixXd& b, double tol, bool print);

#endif
