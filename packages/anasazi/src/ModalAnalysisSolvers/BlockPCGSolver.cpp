//**************************************************************************
//
//                                 NOTICE
//
// This software is a result of the research described in the report
//
// " A comparison of algorithms for modal analysis in the absence 
//   of a sparse direct method", P. Arbenz, R. Lehoucq, and U. Hetmaniuk,
//  Sandia National Laboratories, Technical report SAND2003-1028J.
//
// It is based on the Epetra, AztecOO, and ML packages defined in the Trilinos
// framework ( http://software.sandia.gov/trilinos/ ).
//
// The distribution of this software follows also the rules defined in Trilinos.
// This notice shall be marked on any reproduction of this software, in whole or
// in part.
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// Code Authors: U. Hetmaniuk (ulhetma@sandia.gov), R. Lehoucq (rblehou@sandia.gov)
//
//**************************************************************************

#include "BlockPCGSolver.h"


BlockPCGSolver::BlockPCGSolver(const Epetra_Comm &_Comm, const Epetra_Operator *KK,
                               double _tol, int _iMax, int _verb)
               : MyComm(_Comm),
                 callBLAS(),
                 callLAPACK(),
                 callFortran(),
                 K(KK),
                 Prec(0),
                 mlPrec(false),
                 ml_handle(0),
                 ml_agg(0),
                 vectorPCG(0),
                 tolCG(_tol),
                 iterMax(_iMax),
                 verbose(_verb),
                 AMG_NLevels(0),
                 workSpace(0),
                 numSolve(0),
                 maxIter(0),
                 sumIter(0),
                 minIter(10000)
               {

}


BlockPCGSolver::BlockPCGSolver(const Epetra_Comm &_Comm, const Epetra_Operator *KK,
                               Epetra_Operator *PP, 
                               double _tol, int _iMax, int _verb)
               : MyComm(_Comm),
                 callBLAS(),
                 callLAPACK(),
                 callFortran(),
                 K(KK),
                 Prec(PP),
                 mlPrec(false),
                 ml_handle(0),
                 ml_agg(0),
                 vectorPCG(0),
                 tolCG(_tol),
                 iterMax(_iMax),
                 verbose(_verb),
                 AMG_NLevels(0),
                 workSpace(0),
                 numSolve(0),
                 maxIter(0),
                 sumIter(0),
                 minIter(10000)
               {

}


BlockPCGSolver::~BlockPCGSolver() {

  if ((mlPrec == true) && (Prec)) {
    delete Prec;
    Prec = 0;
    mlPrec = false;
    ML_Destroy(&ml_handle);
    ML_Aggregate_Destroy(&ml_agg);
  }

  if (vectorPCG) {
    delete vectorPCG;
    vectorPCG = 0;
  }

  if (workSpace) {
    delete[] workSpace;
    workSpace = 0;
  }

}


void BlockPCGSolver::setAMGPreconditioner(int smoother, int degree, int numDofs,
                                          const Epetra_MultiVector *Z) {

  // Note the constness is cast away for ML
  Epetra_RowMatrix *KK = dynamic_cast<Epetra_RowMatrix*>(const_cast<Epetra_Operator*>(K));
  if (KK == 0) {
    cerr << endl;
    cerr << " !!! For AMG preconditioner, the matrix must be 'Epetra_RowMatrix' object !!!\n";
    cerr << endl;
    return;
  }

  // Generate an ML multilevel preconditioner

  AMG_NLevels = 10;

  ML_Set_PrintLevel(verbose);
  ML_Create(&ml_handle, AMG_NLevels);

  EpetraMatrix2MLMatrix(ml_handle, 0, KK);

  ML_Aggregate_Create(&ml_agg);
  ML_Aggregate_Set_MaxCoarseSize(ml_agg, 1);
  ML_Aggregate_Set_Threshold(ml_agg, 0.0);

  if (Z) {
    ML_Aggregate_Set_NullSpace(ml_agg, numDofs, Z->NumVectors(), Z->Values(), Z->MyLength());
  }

  AMG_NLevels = ML_Gen_MGHierarchy_UsingAggregation(ml_handle, 0, ML_INCREASING, ml_agg);

  // Set a smoother for the MG method
  // Ray recommends to use MLS (polynomial)
  if (smoother == 1) {
    int j;
    for (j = 0; j < AMG_NLevels-1; j++) {
      ML_Gen_Smoother_MLS(ml_handle, j, ML_BOTH, 30., degree);
    }
#if defined(SUPERLU) || defined(DSUPERLU)
    ML_Gen_CoarseSolverSuperLU(ml_handle, AMG_NLevels-1);
#endif
  }

  // Note that Symmetric Gauss Seidel does not parallelize well
  if (smoother == 2) {
    ML_Gen_Smoother_SymGaussSeidel(ml_handle, ML_ALL_LEVELS, ML_BOTH, 1, ML_DEFAULT);
  }

  ML_Gen_Solver(ml_handle, ML_MGV, 0, AMG_NLevels-1);

  mlPrec = true;
  Prec = new Epetra_ML_Operator(ml_handle, MyComm, K->OperatorDomainMap(), 
                                K->OperatorDomainMap());

}


void BlockPCGSolver::setPreconditioner(Epetra_Operator *PP) {

  Prec = PP;
  mlPrec = false;

}


int BlockPCGSolver::Apply(const Epetra_MultiVector& X, Epetra_MultiVector& Y) const {

  return K->Apply(X, Y);

}


int BlockPCGSolver::ApplyInverse(const Epetra_MultiVector& X, Epetra_MultiVector& Y) const {

  int xcol = X.NumVectors();
  int info = 0;

  if (Y.NumVectors() < xcol)
    return -1;

  // Use AztecOO's PCG for one right-hand side
  if (xcol == 1) {

    // Define the AztecOO object 
    if (vectorPCG == 0) {
      vectorPCG = new AztecOO();

      // Cast away the constness for AztecOO
      Epetra_Operator *KK = const_cast<Epetra_Operator*>(K);
      if (dynamic_cast<Epetra_RowMatrix*>(KK) == 0)
        vectorPCG->SetUserOperator(KK);
      else
        vectorPCG->SetUserMatrix(dynamic_cast<Epetra_RowMatrix*>(KK));

      vectorPCG->SetAztecOption(AZ_max_iter, iterMax);
      vectorPCG->SetAztecOption(AZ_kspace, iterMax);
      if (verbose < 3)
        vectorPCG->SetAztecOption(AZ_output, AZ_last);
      if (verbose < 2)
        vectorPCG->SetAztecOption(AZ_output, AZ_none);

      vectorPCG->SetAztecOption(AZ_solver, AZ_cg);

      ////////////////////////////////////////////////
      //   if (K->HasNormInf()) {
      //     vectorPCG->SetAztecOption(AZ_precond, AZ_Neumann);
      //     vectorPCG->SetAztecOption(AZ_poly_ord, 3);
      //    }
      ////////////////////////////////////////////////

      if (Prec)
        vectorPCG->SetPrecOperator(Prec);

    }

    double *valX = X.Values();
    double *valY = Y.Values();

    int xrow = X.MyLength();

    bool allocated = false;
    if (valX == valY) {
      valX = new double[xrow];
      allocated = true;
      // Copy valY into valX
      memcpy(valX, valY, xrow*sizeof(double));
    }

    Epetra_MultiVector rhs(View, X.Map(), valX, xrow, xcol);
    vectorPCG->SetRHS(&rhs);

    Y.PutScalar(0.0);
    vectorPCG->SetLHS(&Y);

    vectorPCG->Iterate(iterMax, tolCG);

    numSolve += xcol;

    int iter = vectorPCG->NumIters();
    maxIter = (iter > maxIter) ? iter : maxIter;
    minIter = (iter < minIter) ? iter : minIter;
    sumIter += iter;

    if (allocated == true)
      delete[] valX;

    return info;

  } // if (xcol == 1)

  // Use block PCG for multiple right-hand sides
  info = Solve(X, Y, xcol);

  return info;

}


int BlockPCGSolver::Solve(const Epetra_MultiVector &X, Epetra_MultiVector &Y, int blkSize) const {

  int xrow = X.MyLength();
  int xcol = X.NumVectors();
  int ycol = Y.NumVectors();

  int info = 0;
  int localVerbose = verbose*(MyComm.MyPID() == 0);

  // Machine epsilon to check singularities
  double eps = 0.0;
  callLAPACK.LAMCH('E', eps);

  double *valX = X.Values();

  int NB = 3 + callFortran.LAENV(1, "dsytrd", "u", blkSize, -1, -1, -1, 6, 1);
  int lworkD = (blkSize > NB) ? blkSize*blkSize : NB*blkSize; 

  int wSize = 4*blkSize*xrow + 3*blkSize + 2*blkSize*blkSize + lworkD;

  bool useY = true;
  if (ycol % blkSize != 0) {
    // Allocate an extra block to store the solutions
    wSize += blkSize*xrow;
    useY = false;
  }

  if (workSpace == 0){
    workSpace = new (nothrow) double[wSize];
    if (workSpace == 0) {
      info = -1;
      return info;
    }
  } // if (workSpace == 0)

  double *pointer = workSpace;

  // Array to store the matrix PtKP
  double *PtKP = pointer;
  pointer = pointer + blkSize*blkSize;

  // Array to store coefficient matrices
  double *coeff = pointer;
  pointer = pointer + blkSize*blkSize;

  // Workspace array
  double *workD = pointer;
  pointer = pointer + lworkD; 

  // Array to store the eigenvalues of P^t K P
  double *da = pointer;
  pointer = pointer + blkSize;

  // Array to store the norms of right hand sides
  double *initNorm = pointer;
  pointer = pointer + blkSize;

  // Array to store the norms of residuals
  double *resNorm = pointer;
  pointer = pointer + blkSize;
  
  // Array to store the residuals
  double *valR = pointer;
  pointer = pointer + xrow*blkSize;
  Epetra_MultiVector R(View, X.Map(), valR, xrow, blkSize);

  // Array to store the preconditioned residuals
  double *valZ = pointer;
  pointer = pointer + xrow*blkSize;
  Epetra_MultiVector Z(View, X.Map(), valZ, xrow, blkSize);

  // Array to store the search directions
  double *valP = pointer;
  pointer = pointer + xrow*blkSize;
  Epetra_MultiVector P(View, X.Map(), valP, xrow, blkSize);

  // Array to store the image of the search directions
  double *valKP = pointer;
  pointer = pointer + xrow*blkSize;
  Epetra_MultiVector KP(View, X.Map(), valKP, xrow, blkSize);

  // Pointer to store the solutions
  double *valSOL = (useY == true) ? Y.Values() : pointer;

  int iRHS;
  for (iRHS = 0; iRHS < xcol; iRHS += blkSize) {

    int numVec = (iRHS + blkSize < xcol) ? blkSize : xcol - iRHS;

    // Set the initial residuals to the right hand sides
    if (numVec < blkSize) {
      R.Random();
    }
    memcpy(valR, valX + iRHS*xrow, numVec*xrow*sizeof(double));

    // Set the initial guess to zero
    valSOL = (useY == true) ? Y.Values() + iRHS*xrow : valSOL;
    Epetra_MultiVector SOL(View, X.Map(), valSOL, xrow, blkSize);
    SOL.PutScalar(0.0);

    int ii;
    int iter;
    int nFound;

    R.Norm2(initNorm);

    if (localVerbose > 1) {
      cout << endl;
      cout << " Vectors " << iRHS << " to " << iRHS + numVec - 1 << endl;
      if (localVerbose > 2) {
        fprintf(stderr,"\n");
        for (ii = 0; ii < numVec; ++ii) {
          cout << " ... Initial Residual Norm " << ii << " = " << initNorm[ii] << endl;
        }
        cout << endl;
      }
    }

    // Iteration loop
    for (iter = 1; iter <= iterMax; ++iter) {

      // Apply the preconditioner
      if (Prec)
        Prec->ApplyInverse(R, Z);
      else
        Z = R;

      // Define the new search directions
      if (iter == 1) {
        P = Z;
      }
      else {
        // Compute P^t K Z
        callBLAS.GEMM('T', 'N', blkSize, blkSize, xrow, 1.0, KP.Values(), xrow, Z.Values(), xrow,
                      0.0, workD, blkSize);
        MyComm.SumAll(workD, coeff, blkSize*blkSize);

        // Compute the coefficient (P^t K P)^{-1} P^t K Z
        callBLAS.GEMM('T', 'N', blkSize, blkSize, blkSize, 1.0, PtKP, blkSize, coeff, blkSize,
                      0.0, workD, blkSize);
        for (ii = 0; ii < blkSize; ++ii)
          callFortran.SCAL_INCX(blkSize, da[ii], workD + ii, blkSize);
        callBLAS.GEMM('N', 'N', blkSize, blkSize, blkSize, 1.0, PtKP, blkSize, workD, blkSize,
                      0.0, coeff, blkSize);

        // Update the search directions 
        // Note: Use KP as a workspace
        memcpy(KP.Values(), P.Values(), xrow*blkSize*sizeof(double));
        callBLAS.GEMM('N', 'N', xrow, blkSize, blkSize, 1.0, KP.Values(), xrow, coeff, blkSize,
                      0.0, P.Values(), xrow);

        P.Update(1.0, Z, -1.0);

      } // if (iter == 1)

      K->Apply(P, KP);

      // Compute P^t K P
      callBLAS.GEMM('T', 'N', blkSize, blkSize, xrow, 1.0, P.Values(), xrow, KP.Values(), xrow,
                    0.0, workD, blkSize);
      MyComm.SumAll(workD, PtKP, blkSize*blkSize);

      // Eigenvalue decomposition of P^t K P
      callFortran.SYEV('V', 'U', blkSize, PtKP, blkSize, da, workD, lworkD, &info);
      if (info) {
        // Break the loop as spectral decomposition failed
        break;
      } // if (info)

      // Compute the pseudo-inverse of the eigenvalues
      for (ii = 0; ii < blkSize; ++ii) {
        if (da[ii] < 0.0) {
          if (MyComm.MyPID() == 0) {
            cerr << endl << endl;
            cerr << " !!! Negative eigenvalue for P^tKP (" << da[ii] << ") !!!";
            cerr << endl << endl;
          }
          exit(-1);
        }
        else {
          da[ii] = (da[ii] == 0.0) ? 0.0 : 1.0/da[ii];
        }
      } // for (ii = 0; ii < blkSize; ++ii)

      // Compute P^t R
      callBLAS.GEMM('T', 'N', blkSize, blkSize, xrow, 1.0, P.Values(), xrow, R.Values(), xrow,
                    0.0, workD, blkSize);
      MyComm.SumAll(workD, coeff, blkSize*blkSize);

      // Compute the coefficient (P^t K P)^{-1} P^t R
      callBLAS.GEMM('T', 'N', blkSize, blkSize, blkSize, 1.0, PtKP, blkSize, coeff, blkSize,
                    0.0, workD, blkSize);
      for (ii = 0; ii < blkSize; ++ii)
        callFortran.SCAL_INCX(blkSize, da[ii], workD + ii, blkSize);
      callBLAS.GEMM('N', 'N', blkSize, blkSize, blkSize, 1.0, PtKP, blkSize, workD, blkSize,
                    0.0, coeff, blkSize);

      // Update the solutions
      callBLAS.GEMM('N', 'N', xrow, blkSize, blkSize, 1.0, P.Values(), xrow, coeff, blkSize,
                    1.0, valSOL, xrow);

      // Update the residuals
      callBLAS.GEMM('N', 'N', xrow, blkSize, blkSize, -1.0, KP.Values(), xrow, coeff, blkSize,
                    1.0, R.Values(), xrow);

      // Check convergence 
      R.Norm2(resNorm);
      nFound = 0;
      for (ii = 0; ii < numVec; ++ii) {
        if (resNorm[ii] <= tolCG*initNorm[ii])
          nFound += 1;
      }

      if (localVerbose > 1) {
        cout << " Vectors " << iRHS << " to " << iRHS + numVec - 1;
        cout << " -- Iteration " << iter << " -- " << nFound << " converged vectors\n"; 
        if (localVerbose > 2) {
          cout << endl;
          for (ii = 0; ii < numVec; ++ii) {
            cout << " ... ";
            cout.width(5);
            cout << ii << " ... Residual = ";
            cout.precision(2);
            cout.setf(ios::scientific, ios::floatfield);
            cout << resNorm[ii] << " ... Right Hand Side = " << initNorm[ii] << endl;
          }
          cout << endl;
        }
      }

      if (nFound == numVec) {
        break;
      }

    }  // for (iter = 1; iter <= maxIter; ++iter)

    if (useY == false) {
      // Copy the solutions back into Y
      memcpy(Y.Values() + xrow*iRHS, valSOL, numVec*xrow*sizeof(double));
    }

    numSolve += nFound;

    if (nFound == numVec) {
      minIter = (iter < minIter) ? iter : minIter;
      maxIter = (iter > maxIter) ? iter : maxIter;
      sumIter += iter;
    }

  } // for (iRHS = 0; iRHS < xcol; iRHS += blkSize)

  return info;

}


