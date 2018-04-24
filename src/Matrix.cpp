//=======================================================================
// Copyright 2014-2015 David Simmons-Duffin.
// Distributed under the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================


#include <vector>
#include "Matrix.h"

#include "Timers.h"

// Most of the routines below are helpfully-named wrappers for
// functions in MBLAS or MLAPACK.  See Matrix.h for a more detailed
// description of the various input/output parameters.
//
// For a list of MBLAS routines with documentation, see
// http://mplapack.sourceforge.net/mblas_routines.html
//
// For a list of MLAPACK routines with documentation, see
// http://mplapack.sourceforge.net/mlapack_routines.html
//
// We have chosen not to parallelize operations that are used in
// BlockDiagonalMatrix, since there parallelism can be achieved by
// parallelizing loops over blocks.


ostream& operator<<(ostream& os, const Matrix& a) {
  os << "{";
  for (int r = 0; r < a.rows; r++) {
    os << "{";
    for (int c = 0; c < a.cols; c++) {
      os << a.elt(r, c);
      if (c < a.cols-1)
        os << ", ";
    }
    os << "}";
    if (r < a.rows-1)
      os << ", ";
  }
  os << "}";
  return os;
}

// C := alpha*A + beta*C
void matrixScaleAdd(Real alpha, Matrix &A,
		    Real beta, Matrix &C){
  std::transform(A.elements.begin(),A.elements.end(),C.elements.begin(),C.elements.begin(),[alpha,beta](Real a, Real c){return alpha*a+beta*c;});
}

// C := alpha*A + beta*C
void matrixScaleAdd(Real alpha, Real * A,
		    Real beta, Matrix &C){
  //std::transform(C.elements.begin(),C.elements.end(),A,C.elements.begin(),[alpha,beta](Real c, Real a){return alpha*a+beta*c;});
  for (int r = 0; r < C.rows; ++r)
    for (int c = 0; c < C.cols; ++c)
      C.elt(r,c) = alpha*A[r+C.rows*c] + beta*C.elt(r,c);
}

// C := alpha*A*B + beta*C
void matrixScaleMultiplyAdd(Real alpha, Matrix &A, Matrix &B,
                            Real beta, Matrix &C) {
  assert(A.cols == B.rows);
  assert(A.rows == C.rows);
  assert(B.cols == C.cols);

  Rgemm("N", "N", A.rows, B.cols, A.cols, alpha,
        &A.elements[0], A.rows,
        &B.elements[0], B.rows,
        beta,
        &C.elements[0], C.rows);
}

//
void matrixScaleMultiplyAddMpmat(mpmat &myWorkspace, Real alpha, Matrix &A,
				 Matrix &B, Real beta, Matrix &C
#ifdef __SDPB_CUDA__
				 ,bool gpu
#endif
) {
  assert(A.cols == B.rows);
  assert(A.rows == C.rows);
  assert(B.cols == C.cols);

  mpf_set_default_prec(mpf_get_default_prec()+512);
  Real * Ctmp = new Real[C.cols*C.rows];
  mpf_set_default_prec(mpf_get_default_prec()-512);

#ifdef __SDPB_CUDA__
  if(gpu) myWorkspace.gemm_reduced_gpu(CblasRowMajor,CblasNoTrans,CblasNoTrans,
				 A.rows, B.cols, A.cols,
				 A.elements.data(), B.elements.data(),
				 Ctmp);
  else
  myWorkspace.gemm_reduced(CblasRowMajor,CblasNoTrans,CblasNoTrans,
			   A.rows, B.cols, A.cols,
			   A.elements.data(), B.elements.data(),
			   Ctmp);
#else
				 myWorkspace.gemm_reduced(CblasRowMajor,CblasNoTrans,CblasNoTrans,
			   A.rows, B.cols, A.cols,
			   A.elements.data(), B.elements.data(),
			   Ctmp);
#endif
  matrixScaleAdd(alpha,Ctmp,beta,C);

  delete [] Ctmp;
}

void matrixScaleTransMultiplyAddMpmat(mpmat &myWorkspace, char ta, char tb, Real alpha, Matrix &A, int a_offset,
         Matrix &B, int b_offset, Real beta, Matrix &C, int c_offset
#ifdef __SDPB_CUDA__
         ,bool gpu
#endif
) {
  assert(A.cols == B.rows);
  assert(A.rows == C.rows);
  assert(B.cols == C.cols);

  mpf_set_default_prec(mpf_get_default_prec()+512);
  Real * Ctmp = new Real[C.cols*C.rows];
  mpf_set_default_prec(mpf_get_default_prec()-512);

#ifdef __SDPB_CUDA__
  if(gpu) myWorkspace.gemm_reduced_gpu(CblasRowMajor,ta == 't' ? CblasTrans : CblasNoTrans,tb == 't' ? CblasTrans : CblasNoTrans,
         A.rows, B.cols, A.cols,
         A.elements.data()+a_offset, B.elements.data()+b_offset,
         Ctmp+c_offset);
  else
  myWorkspace.gemm_reduced(CblasRowMajor,ta == 't' ? CblasTrans : CblasNoTrans,tb == 't' ? CblasTrans : CblasNoTrans,
         A.rows, B.cols, A.cols,
         A.elements.data()+a_offset, B.elements.data()+b_offset,
         Ctmp+c_offset);
#else
         myWorkspace.gemm_reduced(CblasRowMajor,ta == 't' ? CblasTrans : CblasNoTrans,tb == 't' ? CblasTrans : CblasNoTrans,
         A.rows, B.cols, A.cols,
         A.elements.data()+a_offset, B.elements.data()+b_offset,
         Ctmp+c_offset);
#endif
  matrixScaleAdd(alpha,Ctmp,beta,C);

  delete [] Ctmp;
}

// C := A*B
void matrixMultiply(Matrix &A, Matrix &B, Matrix &C) {
  matrixScaleMultiplyAdd(1, A, B, 0, C);
}

#ifdef __SDPB_CUDA__
void matrixMultiplyMpmat(mpmat &myWorkspace, Matrix &A, Matrix &B, Matrix &C, bool gpu) {
  matrixScaleMultiplyAddMpmat(myWorkspace,1, A, B, 0, C,gpu);
}
#else
void matrixMultiplyMpmat(mpmat &myWorkspace, Matrix &A, Matrix &B, Matrix &C) {
  matrixScaleMultiplyAddMpmat(myWorkspace,1, A, B, 0, C);
}
#endif

// Set block starting at (bRow, bCol) of B to A^T A
void matrixSquareIntoBlock(Matrix &A, Matrix &B, int bRow, int bCol) {
  assert(bRow + A.cols <= B.rows);
  assert(bCol + A.cols <= B.cols);

  // This operation is not used within a BlockDiagonalMatrix, so it is
  // worthwhile to parallelize.  In fact, this function, used in
  // computing TopLeft(Q) = SchurOffDiagonal^T SchurOffDiagonal (see
  // SDPSolver.cpp) is one of the main performance bottlenecks in the
  // solver.


  #pragma omp parallel for schedule(dynamic)
  for (int c = 0; c < A.cols; c++) {
    for (int r = 0; r <= c; r++) {
      Real tmp = 0;
      for (int p = 0; p < A.rows; p++)
        tmp += A.elt(p, r) * A.elt(p, c);
      B.elt(bRow + r, bCol + c) = tmp;
      if (r != c)
        B.elt(bRow + c, bCol + r) = tmp;
    }
  }
}

// Set block starting at (bRow, bCol) of B to A^T A
#ifdef __SDPB_CUDA__
  void matrixSquareIntoBlockMpmat(mpmat &myWorkspace, Matrix &A, Matrix &B, int bRow, int bCol, bool gpu){
#else
  void matrixSquareIntoBlockMpmat(mpmat &myWorkspace, Matrix &A, Matrix &B, int bRow, int bCol){
#endif 
  assert(bRow + A.cols <= B.rows);
  assert(bCol + A.cols <= B.cols);

  // This operation is not used within a BlockDiagonalMatrix, so it is
  // worthwhile to parallelize.  In fact, this function, used in
  // computing TopLeft(Q) = SchurOffDiagonal^T SchurOffDiagonal (see
  // SDPSolver.cpp) is one of the main performance bottlenecks in the
  // solver.
  
  timers["matrixSquareIntoBlockMpmat.complete"].resume();

  mpf_set_default_prec(mpf_get_default_prec()+256);
  Real * block = new Real[A.cols*A.cols*2];
  
  mpf_set_default_prec(mpf_get_default_prec()-256);

  
  //Matrix B2(A.cols,A.cols);

  for (int c = 0; c < A.cols; ++c){
    for (int r = 0; r < A.cols; ++r){
      block[r*A.cols+c] = B.elt(bRow + r, bCol + c);
    }
   }
  
#ifdef __SDPB_CUDA__
  if (gpu)
    myWorkspace.syrk_reduced_gpu(CblasRowMajor,CblasTrans,A.cols,A.rows,A.elements.data(),block);
  else
    myWorkspace.syrk_reduced(CblasRowMajor,CblasTrans,A.cols,A.rows,A.elements.data(),block);
#else
  myWorkspace.syrk_reduced(CblasRowMajor,CblasTrans,A.cols,A.rows,A.elements.data(),block);
#endif
  
  // matrixSquareIntoBlock(A,B2,0,0);
 

  for (int c = 0; c < A.cols; ++c){
  for (int r = 0; r < A.cols; ++r){
  /*if (((block[r*A.cols+c] != 0)&&(B2.elt(r,c)/block[r*A.cols+c] < .9 || B2.elt(r,c)/block[r*A.cols+c] > 1.1))|| ((B2.elt(r,c) != 0)&&(block[r*A.cols+c]/B2.elt(r,c) < .9 || block[r*A.cols+c]/B2.elt(r,c) > 1.1)) ){
  std::cerr << "Error: the answers are very inconsistent. \nAt location (" << r<< ", " << c << "), expected " << B2.elt(r,c) << ", got " << block[r*A.cols+c] << ".\n";
  compare_mpf_bits(block[r*A.cols+c],B2.elt(r,c));

  std::cout << "The doubles at this point are: \n";
  std::cout << "{";
      for (int i = 0; i < 34; ++i){
  std::cout << myWorkspace.tmp[i+(r+c*A.cols)*34] << ",";
      }
      std::cout << "}\n";
      print_mpmat_double_array(&myWorkspace.tmp[(r+c*A.cols)*34],34);
      std::cout << "{";
      for (int i = 0; i < 34; ++i){
  std::cout << myWorkspace.tmp[i+(c+r*A.cols)*34] << ",";
      }
      std::cout << "}\n";
      print_mpmat_double_array(&myWorkspace.tmp[(c+r*A.cols)*34],34);
      }*/
  /*if (block[r*A.cols+c] < 0){
  std::cout << "This result is negative at location (" << r<< ", " << c << ").\n";
  compare_mpf_bits(block[r*A.cols+c],B2.elt(r,c));
  print_mpmat_double_array(&myWorkspace.tmp[(r+c*A.cols)*34],34);
  std::cout << "\n\n\n";
  }*/
    B.elt(bRow + r, bCol + c) = block[r*A.cols+c];
}
}

  delete [] block;

  timers["matrixSquareIntoBlockMpmat.complete"].stop();
  
}


// A := L^{-1} A L^{-T}
void lowerTriangularInverseCongruence(Matrix &A, Matrix &L) {
  int dim = A.rows;
  assert(A.cols == dim);
  assert(L.rows == dim);
  assert(L.cols == dim);

  // A := A L^{-T}
  Rtrsm("Right", "Lower", "Transpose", "NonUnitDiagonal", dim, dim, 1,
        &L.elements[0], dim,
        &A.elements[0], dim);

  // A := L^{-1} A
  Rtrsm("Left", "Lower", "NoTranspose", "NonUnitDiagonal", dim, dim, 1,
        &L.elements[0], dim,
        &A.elements[0], dim);
}

// y := alpha A x + beta y
void vectorScaleMatrixMultiplyAdd(Real alpha, Matrix &A, Vector &x,
                                  Real beta, Vector &y) {
  assert(A.cols <= static_cast<int>(x.size()));
  assert(A.rows <= static_cast<int>(y.size()));

  #pragma omp parallel for schedule(static)
  for (int p = 0; p < A.rows; p++) {
    Real tmp = 0;
    for (int n = 0; n < A.cols; n++)
      tmp += A.elt(p, n)*x[n];
    y[p] = alpha*tmp + beta*y[p];
  }
}

// y := alpha A^T x + beta y
void vectorScaleMatrixMultiplyTransposeAdd(Real alpha, Matrix &A, Vector &x,
                                           Real beta, Vector &y) {
  assert(A.cols <= static_cast<int>(y.size()));
  assert(A.rows <= static_cast<int>(x.size()));

  #pragma omp parallel for schedule(static)
  for (int n = 0; n < A.cols; n++) {
    Real tmp = 0;
    for (int p = 0; p < A.rows; p++)
      tmp += A.elt(p, n)*x[p];
    y[n] = alpha*tmp + beta*y[n];
  }
}

// Tr(A B), where A and B are symmetric
Real frobeniusProductSymmetric(const Matrix &A, const Matrix &B) {
  assert(A.rows == B.rows);
  assert(A.cols == B.cols);
  assert(A.rows == A.cols);

  Real result = 0;
  for (int c = 0; c < A.cols; c++)
    for (int r = 0; r < c ; r++)
      result += A.elt(r, c)*B.elt(r, c);
  result *= 2;

  for (int r = 0; r < A.rows; r++)
    result += A.elt(r, r)*B.elt(r, r);

  return result;
}

// (X + dX) . (Y + dY), where X, dX, Y, dY are symmetric Matrices and
// '.' is the Frobenius product.
//
Real frobeniusProductOfSums(const Matrix &X, const Matrix &dX,
                            const Matrix &Y, const Matrix &dY) {
  Real result = 0;

  for (int c = 0; c < X.cols; c++)
    for (int r = 0; r < c; r++)
      result += (X.elt(r, c) + dX.elt(r, c)) * (Y.elt(r, c) + dY.elt(r, c));
  result *= 2;

  for (int r = 0; r < X.rows; r++)
    result += (X.elt(r, r) + dX.elt(r, r)) * (Y.elt(r, r) + dY.elt(r, r));

  return result;
}

// Compute the eigenvalues of A, via the QR method
void matrixEigenvalues(Matrix &A, Vector &workspace, Vector &eigenvalues) {
  assert(A.rows == A.cols);
  assert(static_cast<int>(eigenvalues.size()) == A.rows);
  assert(static_cast<int>(workspace.size()) == 3*A.rows - 1);

  Integer info;
  Integer workSize = workspace.size();
  Rsyev("NoEigenvectors", "LowerTriangular", A.rows, &A.elements[0],
        A.rows, &eigenvalues[0], &workspace[0], workSize, &info);
  assert(info == 0);
}

// Minimum eigenvalue of A, via the QR method
Real minEigenvalue(Matrix &A, Vector &workspace, Vector &eigenvalues) {
  matrixEigenvalues(A, workspace, eigenvalues);
  return eigenvalues[0];
}

// Compute an in-place LU decomposition of A, with pivots, suitable
// for use with 'solveWithLUDecomposition'
void LUDecomposition(Matrix &A, vector<Integer> &pivots) {
  int dim = A.rows;
  assert(A.cols == dim);

  Integer info;
  Rgetrf(dim, dim, &A.elements[0], dim, &pivots[0], &info);
  assert(info == 0);
}

// b := A^{-1} b, where LU and pivots encode the LU decomposition of A
void solveWithLUDecomposition(Matrix &LU, vector<Integer> &pivots, Vector &b) {
  Integer info;
  Rgetrs("NoTranspose", LU.rows, 1, &LU.elements[0], LU.rows,
         &pivots[0], &b[0], b.size(), &info);
  assert(info == 0);
}

// L (lower triangular) such that A = L L^T
void choleskyDecomposition(Matrix &A, Matrix &L) {
  int dim = A.rows;
  assert(A.cols == dim);
  assert(L.rows == dim);
  assert(L.cols == dim);

  // Set lower-triangular part of L to cholesky decomposition
  L.copyFrom(A);
  Integer info;
  Rpotrf("Lower", dim, &L.elements[0], dim, &info);
  assert(info == 0);

  // Set the upper-triangular part of the L to zero
  for (int j = 0; j < dim; j++)
    for (int i = 0; i < j; i++)
      L.elements[i + j*dim] = 0;
}

// Compute L (lower triangular) such that A + U U^T = L L^T.
void choleskyDecompositionStabilized(Matrix &A, Matrix &L,
                                     vector<Integer> &stabilizeIndices,
                                     vector<Real> &stabilizeLambdas,
                                     const double stabilizeThreshold) {
  int dim = A.rows;
  assert(A.cols == dim);
  assert(L.rows == dim);
  assert(L.cols == dim);

  // Set lower-triangular part of L to cholesky decomposition
  L.copyFrom(A);
  Integer info;
  RpotrfStabilized("Lower", dim, &L.elements[0], dim, &info,
                   stabilizeIndices, stabilizeLambdas, stabilizeThreshold);
  assert(info == 0);

  // Set the upper-triangular part of the L to zero
  for (int j = 0; j < dim; j++)
    for (int i = 0; i < j; i++)
      L.elements[i + j*dim] = 0;
}

// B := L^{-1} B, where L is lower-triangular and B is a matrix
// pointed to by b
void lowerTriangularSolve(Matrix &L, Real *b, int bcols, int ldb) {
  int dim = L.rows;
  assert(L.cols == dim);

  Rtrsm("Left", "Lower", "NoTranspose", "NonUnitDiagonal",
        dim, bcols, 1, &L.elements[0], dim, b, ldb);
}

// b := L^{-1} b, where L is lower-triangular
void lowerTriangularSolve(Matrix &L, Vector &b) {
  assert(static_cast<int>(b.size()) == L.rows);
  lowerTriangularSolve(L, &b[0], 1, b.size());
}

// B := L^{-T} B, where L is lower-triangular and B is a matrix
// pointed to by b
void lowerTriangularTransposeSolve(Matrix &L, Real *b, int bcols, int ldb) {
  int dim = L.rows;
  assert(L.cols == dim);

  Rtrsm("Left", "Lower", "Transpose", "NonUnitDiagonal",
        dim, bcols, 1, &L.elements[0], dim, b, ldb);
}

// b := L^{-T} b, where L is lower-triangular
void lowerTriangularTransposeSolve(Matrix &L, Vector &b) {
  assert(static_cast<int>(b.size()) == L.rows);
  lowerTriangularTransposeSolve(L, &b[0], 1, b.size());
}

// X := ACholesky^{-1 T} ACholesky^{-1} X = A^{-1} X
void matrixSolveWithCholesky(Matrix &ACholesky, Matrix &X) {
  int dim = X.rows;
  assert(X.cols == dim);
  assert(ACholesky.rows == dim);
  assert(ACholesky.cols == dim);

  lowerTriangularSolve(ACholesky, &X.elements[0], X.cols, X.rows);
  lowerTriangularTransposeSolve(ACholesky, &X.elements[0], X.cols, X.rows);
}
