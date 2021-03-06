/*
 * shell_matop.cpp
 *
 *  Created on: Aug 26, 2013
 *      Author: helmes
 */
#include "shell_matop.h"

static IO* const pars = IO::getInstance();
static Tslice* const ts = Tslice::getInstance();
static Nav* const lookup = Nav::getInstance();
/*tv copies the arrays pointed to by *x and *y to std::vectors iks, yps of type
Eigen::Vector3cd, respectively
After this the Multiplication of the Laplace takes place. Result is stored
in yps, which then is written to the array at *y again. */
static void tv2(int nx,const PetscScalar *x,PetscScalar *y) {
  const int V3 = pars -> get_int("V3");
  const double LAM_L = pars -> get_float("lambda_l");
  const double LAM_C = pars -> get_float("lambda_c");
  //define vectors
  std::vector<Eigen::Vector3cd> iks(V3, Eigen::Vector3cd::Zero());
  std::vector<Eigen::Vector3cd> yps(V3, Eigen::Vector3cd::Zero());
  //iks.clear();
  //yps.clear();
  //Eigen::Vector3cd tmp_x, tmp_y;
  omp_set_num_threads(pars -> get_int("OMP_THRDS"));
  //std::cout << "Calculating with " << omp_get_num_threads() << " threads" << std::endl;
  #pragma omp parallel
  {
  Eigen::Vector3cd tmp_x, tmp_y;
  //copy read in vectors x and y to vectors of 3cd vectors
  #pragma omp for
  for(unsigned i = 0; i < V3; ++i) {
    tmp_x << x[3*i], x[3*i+1], x[3*i+2];
    tmp_y << y[3*i], y[3*i+1], y[3*i+2];
    iks[i] = tmp_x;
    yps[i] = tmp_y;
  }
//  for (unsigned i = 0; (i+3) <= 3*V3 ; i += 3){
//    //Eigen::Vector3cd tmp_x;
//    //Eigen::Vector3cd tmp_y;
//    tmp_x << x[i], x[i+1], x[i+2];
//    tmp_y << y[i], y[i+1], y[i+2];
//    iks.push_back(tmp_x);
//    yps.push_back(tmp_y);
//  }
  //constants used often: c := 2/ (lambda_L - lambda_C) 
  //a := 1+2*lambda_C / (lambda_L - lambda_C)
  register const double c = 2./(LAM_L - LAM_C);
  register const double a = 1 + c * LAM_C;
  //these disable chebyshev acceleration:
  //register const double c = 1;
  //register const double a = 0;
  //Laplace times vector in terms of Eigen::3cd
  //for ( int k = 0; k < V3; ++k ) yps.at(k) = Eigen::Vector3cd::Zero();
  #pragma omp for
//  {
  for ( int k = 0; k < V3; ++k) {
    /*if (k == 0) {
      yps.at(k) = -(U[k][0]*iks.at( up_3d[k][0] ) + (U[ down_3d[k][0] ][0].adjoint())
                  * iks.at( down_3d[k][0] ) + U[k][1] * iks.at( up_3d[k][1] )
                  + (U[ down_3d[k][1] ][1].adjoint()) * iks.at( down_3d[k][1] )
                  + U[k][2] * iks.at( up_3d[k][2] )
                  + (U[ down_3d[k][2] ][2].adjoint()) * iks.at( down_3d[k][2] )
                  - 200.0 * (iks.at(k)));
    }*/
    //else {
    yps[k] = c * ( (ts -> get_gauge(k,0)) * iks.at( lookup -> get_up(k,0) )
             + ( (ts -> get_gauge( lookup -> get_dn(k,0), 0)).adjoint())
             * iks.at( lookup -> get_dn(k,0) ) 
             + (ts -> get_gauge(k,1)) * iks.at( lookup -> get_up(k,1) )
             + (ts -> get_gauge( lookup -> get_dn(k,1),1).adjoint()) 
             * iks.at( lookup -> get_dn(k,1) )
             + ts -> get_gauge(k,2) * iks.at( lookup -> get_up(k,2) )
             + (ts -> get_gauge( lookup -> get_dn(k, 2), 2).adjoint())
             * iks.at( lookup -> get_dn(k,2) )
             - 6.0 * (iks.at(k))) + a * (iks.at(k));
//    yps.at(k) = c * (eigen_timeslice[k][0]*iks.at( up_3d[k][0] ) + (eigen_timeslice[ down_3d[k][0] ][0].adjoint())
//              * iks.at( down_3d[k][0] ) + eigen_timeslice[k][1] * iks.at( up_3d[k][1] )
//              + (eigen_timeslice[ down_3d[k][1] ][1].adjoint()) * iks.at( down_3d[k][1] )
//              + eigen_timeslice[k][2] * iks.at( up_3d[k][2] )
//              + (eigen_timeslice[ down_3d[k][2] ][2].adjoint()) * iks.at( down_3d[k][2] )
//              - 6.0 * (iks.at(k))) + a * (iks.at(k));
//    std::cout << U[k][0] << " " << down_3d[k][0] << " " << up_3d[k][1] << " " << down_3d[k][1] << " " << up_3d[k][2] << " " << down_3d[k][2] << std::endl;
    //}
  }
//  }
  //copy vectors back to Petsc-arrays
  #pragma omp for
  for(unsigned j = 0; j < V3; ++j) {
    y[3*j] = (yps[j])(0);
    y[3*j+1] = (yps[j])(1);
    y[3*j+2] = (yps[j])(2);
  }
  }
//  int k = 0;
//  for (int j = 0; j < V3; j++) {
//    y[k] = (yps.at(j))(0);
//    y[k+1] = (yps.at(j))(1);
//    y[k+2] = (yps.at(j))(2);
//    k += 3;
//  }
}

static void scale_array(PetscScalar factor, const PetscScalar *in, PetscScalar *out) {
  const int MAT_ENTRIES = pars -> get_int("MAT_ENTRIES");
  for (int k = 0; k < MAT_ENTRIES; ++k) {
    out[k] = factor * in[k];
  }
}

//calculates y = a+b
static void add_arrays(const PetscScalar *b, const PetscScalar *a, PetscScalar *y) {
  const int MAT_ENTRIES = pars -> get_int("MAT_ENTRIES");
  for (int k = 0; k < MAT_ENTRIES; ++k) {
    y[k] = a[k] + b[k];
  }
}

//calculates y = a-b
static void subtract_arrays(const PetscScalar *b, const PetscScalar *a, PetscScalar *y) {
  const int MAT_ENTRIES = pars -> get_int("MAT_ENTRIES");
  for (int k = 0; k < MAT_ENTRIES; ++k) {
    y[k] = a[k] - b[k];
  }
}

//equals y to input
static void equal_arrays(const PetscScalar *a, PetscScalar *y){
  const int MAT_ENTRIES = pars -> get_int("MAT_ENTRIES");
  for (int k = 0; k < MAT_ENTRIES; ++k) {
    y[k] = a[k];
  }
}

static void tv_iter(int nx, const PetscScalar *x, PetscScalar *y){
  // deg: degree of polynomials
  // nx: some context variable from Petsc interface
  // x: vector with which matrix A is multiplied
  // y = A*x
  const int MAT_ENTRIES = pars -> get_int("MAT_ENTRIES");
  //hard coded atm, move to parameters later
  const int DEG = pars -> get_int("DEG");
  //const int DEG = 8;
  std::vector<PetscScalar> Told(MAT_ENTRIES); 
  std::vector<PetscScalar> Tcur(MAT_ENTRIES); 
  std::vector<PetscScalar> Tnew(MAT_ENTRIES);
  std::vector<PetscScalar> tmp(MAT_ENTRIES);
  // initialize chebyshev polynomials
  tv2( nx, &x[0], &Tcur[0]);
  equal_arrays(&x[0], &Told[0]);
  // if deg is 1 or less return initialised values
  if (DEG >=1){
  // else iteratively calculate chebyshev polynomials up to deg
  // T_n(B) = 2*B(T_{n-1}(B))-T_{n-2}(B)
    for (int n = 2; n <= DEG; n++){
      // store B(Tcur(Bx)) in tmp1
      tv2(nx, &Tcur[0], &tmp[0]);
      // scale tmp1
      scale_array(2., &tmp[0], &tmp[0]);
      //calculate new polynomial
      subtract_arrays(&Told[0], &tmp[0], &y[0]);
      // overwrite new variables
      equal_arrays(&Tcur[0], &Told[0]);
      equal_arrays(&y[0], &Tcur[0]);
    }
  }
}

//Calculating Chebyshev-Polynomial T8 of B acting on x in a 4-Step process
//static void tv( int nx, const PetscScalar *x,PetscScalar *y) {
//  const int MAT_ENTRIES = pars -> get_int("MAT_ENTRIES");
//  //PetscScalar tmp[MAT_ENTRIES];
//  //PetscScalar tmp1[MAT_ENTRIES];
//
//  std::vector<PetscScalar> tmp(MAT_ENTRIES);
//  std::vector<PetscScalar> tmp1(MAT_ENTRIES);
//  //Step 1 
//  //storing B*x in tmp
//  tv2( nx, &x[0], &tmp[0]);
//  //Calculating B(Bx);
//  tv2( nx, &tmp[0], &tmp[0]);
//  //Scale tmp with 128 and px with 256
//  scale_array(128, &tmp[0], &tmp[0]);
//  scale_array(256, &x[0], &tmp1[0]);
//  //subtract tmp1 from tmp yielding y
//  subtract_arrays(&tmp1[0], &tmp[0], &y[0]);
//
//  //Step 2
//  tv2( nx, &y[0], &tmp[0]);
//  tv2( nx, &tmp[0], &tmp[0]);
//  scale_array(160, &x[0], &tmp1[0]);
//  add_arrays(&tmp1[0], &tmp[0], &y[0]);
//
//  //Step 3
//  tv2( nx, &y[0], &tmp[0]);
//  tv2( nx, &tmp[0], &tmp[0]);
//  scale_array(32, &x[0], &tmp1[0]);
//  subtract_arrays(&tmp1[0], &tmp[0], &y[0]);
//  
//  //Step 4
//  tv2( nx, &y[0], &tmp[0]);
//  tv2( nx, &tmp[0], &tmp[0]);
//  add_arrays(&tmp[0],&x[0],&y[0]);
//
//}

#undef __FUNCT__
#define __FUNCT__ "MatMult_Laplacian2D"
/*
    Matrix-vector product subroutine for the 2D Laplacian.

    The matrix used is the 2 dimensional discrete Laplacian on unit square with
    zero Dirichlet boundary condition.

    Computes y <-- A*x, where A is the block tridiagonal matrix

                 | T -I          |
                 |-I  T -I       |
             A = |   -I  T       |
                 |        ...  -I|
                 |           -I T|

    The subroutine TV is called to compute y<--T*x.
 */
PetscErrorCode MatMult_Laplacian2D(Mat A,Vec x,Vec y) {
  void              *ctx;
  int               nx;
  const PetscScalar *px;
  PetscScalar       *py;
  PetscErrorCode    ierr;

  PetscFunctionBeginUser;
  ierr = MatShellGetContext(A,&ctx);CHKERRQ(ierr);
  nx = *(int*)ctx;
  ierr = VecGetArrayRead(x,&px);CHKERRQ(ierr);
  ierr = VecGetArray(y,&py);CHKERRQ(ierr);
  //choose tv instead of tv2 to enable chebyshev acceleration
  tv_iter( nx,&px[0],&py[0]);

  ierr = VecRestoreArrayRead(x,&px);CHKERRQ(ierr);
  ierr = VecRestoreArray(y,&py);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatGetDiagonal_Laplacian2D"
PetscErrorCode MatGetDiagonal_Laplacian2D(Mat A,Vec diag)
{
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  ierr = VecSet(diag, -6.0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}




