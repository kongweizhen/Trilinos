// @HEADER
// ***********************************************************************
//
//                 Anasazi: Block Eigensolvers Package
//                 Copyright (2004) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ***********************************************************************
// @HEADER
//
// This test is for LOBPCG solving a standard (Ax=xl) complex Hermitian
// eigenvalue problem.
//
// The matrix used is from MatrixMarket:
// Name: MHD1280B: Alfven Spectra in Magnetohydrodynamics
// Source: Source: A. Booten, M.N. Kooper, H.A. van der Vorst, S. Poedts and J.P. Goedbloed University of Utrecht, the Netherlands
// Discipline: Plasma physics
// URL: http://math.nist.gov/MatrixMarket/data/NEP/mhd/mhd1280b.html
// Size: 1280 x 1280
// NNZ: 22778 entries

#include "AnasaziConfigDefs.hpp"
#include "AnasaziBasicEigenproblem.hpp"
#include "AnasaziLOBPCG.hpp"
#include "AnasaziBasicSort.hpp"

#ifdef EPETRA_MPI
#include "Epetra_MpiComm.h"
#include <mpi.h>
#else
#include "Epetra_SerialComm.h"
#endif

// I/O for Harwell-Boeing files
#ifdef HAVE_ANASAZI_TRIUTILS
#include "iohb.h"
#endif

// templated multivector and sparse matrix classes
#include "MyMultiVec.hpp"
#include "MyBetterOperator.hpp"

using namespace Teuchos;

int main(int argc, char *argv[]) 
{
  int i;
  int info = 0;

#ifdef EPETRA_MPI
  // Initialize MPI
  MPI_Init(&argc,&argv);
  Epetra_MpiComm Comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm Comm;
#endif

  int MyPID = Comm.MyPID();

  bool testFailed;
  bool verbose = 0;
  std::string which("SM");
  if (argc>1) {
    if (argv[1][0]=='-' && argv[1][1]=='v') {
      verbose = true;
    }
    else {
      which = argv[1];
    }
  }
  if (argc>2) {
    if (argv[2][0]=='-' && argv[2][1]=='v') {
      verbose = true;
    }
    else {
      which = argv[2];
    }
  }

  if (verbose && MyPID == 0) {
    cout << Anasazi::Anasazi_Version() << endl << endl;
  }

#ifndef HAVE_ANASAZI_TRIUTILS
  cout << "This test requires Triutils. Please configure with --enable-triutils." << endl;
#ifdef EPETRA_MPI
  MPI_Finalize() ;
#endif
  if (verbose && MyPID==0)
    cout << "End Result: TEST FAILED" << endl;	
  return -1;
#endif

  Anasazi::ReturnType returnCode = Anasazi::Ok;  

#ifdef HAVE_COMPLEX
  typedef std::complex<double> ST;
#elif HAVE_COMPLEX_H
  typedef ::complex<double> ST;
#else
  typedef double ST;
  // no complex. quit with failure.
  if (verbose && MyPID==0) {
    cout << "Not compiled with complex support." << endl;
    if (verbose && MyPID==0) {
      cout << "End Result: TEST FAILED" << endl;
    }
#ifdef HAVE_MPI
    MPI_Finalize();
#endif
    return -1;
  }
#endif
  typedef ScalarTraits<ST>                   SCT;
  typedef SCT::magnitudeType                  MT;
  typedef Anasazi::MultiVec<ST>               MV;
  typedef Anasazi::Operator<ST>               OP;
  typedef Anasazi::MultiVecTraits<ST,MV>     MVT;
  typedef Anasazi::OperatorTraits<ST,MV,OP>  OPT;
  ST ONE  = SCT::one();
  ST ZERO = SCT::zero();

  // Create default output manager 
  RefCountPtr<Anasazi::OutputManager<ST> > MyOM 
    = rcp( new Anasazi::OutputManager<ST>( MyPID ) );
  // Set verbosity level
  if (verbose) {
    MyOM->SetVerbosity( Anasazi::FinalSummary + Anasazi::TimingDetails );
  }

  // Create the sort manager
  RefCountPtr<Anasazi::BasicSort<ST,MV,OP> > MySM = 
     rcp( new Anasazi::BasicSort<ST,MV,OP>(which) );

  // Get the data from the HB file
  int dim,dim2,nnz,nrhs;
  double *dvals;
  int *colptr,*rowind;
  ST *cvals;
  //info = readHB_newmat_double("mhd1280b.cua",&dim,&dim2,&nnz,
                              //&colptr,&rowind,&dvals);
  if (info) {
    if (verbose && MyPID==0) {
      cout << "Error reading 'mhd1280b.cua'" << endl;
      if (verbose && MyPID==0) {
        cout << "End Result: TEST FAILED" << endl;
      }
#ifdef HAVE_MPI
      MPI_Finalize();
#endif
      return -1;
    }
  }
  // Convert interleaved doubles to complex values
  cvals = new ST[nnz];
  for (int ii=0; ii<nnz; i++) {
    cvals[ii] = ST(dvals[i*2],dvals[i*2+1]);
  }
  // Build the problem matrix
  RefCountPtr< MyBetterOperator<ST> > A 
    = rcp( new MyBetterOperator<ST>(dim,colptr,nnz,rowind,cvals) );

  // Eigensolver parameters
  int nev = 4;
  int blockSize = 5;
  int maxIters = 500;
  ST tol = 1.0e-6;

  // Create parameter list to pass into solver
  ParameterList MyPL;
  MyPL.set( "Block Size", blockSize );
  MyPL.set( "Max Iters", maxIters );
  MyPL.set( "Tol", tol );

  // Create initial vectors
  RefCountPtr<MyMultiVec<ST> > ivec = rcp( new MyMultiVec<ST>(dim,blockSize) );
  ivec->MvRandom();

  // Create eigenproblem
  RefCountPtr<Anasazi::BasicEigenproblem<ST,MV,OP> > MyProblem =
    rcp( new Anasazi::BasicEigenproblem<ST,MV,OP>(A, ivec) );

  // Inform the eigenproblem that the operator A is symmetric
  MyProblem->SetSymmetric(true);

  // Set the number of eigenvalues requested and the blocksize the solver should use
  MyProblem->SetNEV( nev );

  // Inform the eigenproblem that you are done passing it information
  info = MyProblem->SetProblem();
  if (info) {
    cout << "Anasazi::BasicEigenproblem::SetProblem() returned with code : "<< info << endl;
#ifdef EPETRA_MPI
    MPI_Finalize() ;
#endif
    if (verbose && MyPID==0)
      cout << "End Result: TEST FAILED" << endl;	
    return -1;
  }

  // Create the eigensolver 
  Anasazi::LOBPCG<ST,MV,OP> MySolver(MyProblem, MySM, MyOM, MyPL);

  // Solve the problem to the specified tolerances or length
  returnCode = MySolver.solve();
  testFailed = false;
  if (returnCode != Anasazi::Ok) {
    testFailed = true; 
  }

  // Get the eigenvalues and eigenvectors from the eigenproblem
  RefCountPtr<std::vector<ST> > evals = MyProblem->GetEvals();
  RefCountPtr<MV > evecs = MyProblem->GetEvecs();
  int nevecs = MVT::GetNumberVecs(*evecs);

  // Compute the direct residual
  std::vector<MT> normV( nevecs );
  SerialDenseMatrix<int,ST> T(nevecs,nevecs);
  for (int i=0; i<nevecs; i++) {
    T(i,i) = (*evals)[i];
  }
  RefCountPtr<MV > Avecs = MVT::Clone( *evecs, nevecs );
  OPT::Apply( *A, *evecs, *Avecs );
  MVT::MvTimesMatAddMv( -ONE, *evecs, T, ONE, *Avecs );
  MVT::MvNorm( *Avecs, &normV );

  for ( i=0; i<nevecs; i++ ) {
    if ( SCT::magnitude(normV[i]/(*evals)[i]) > 5.0e-5 ) {
      testFailed = true;
    }
  }

  // Exit
#ifdef EPETRA_MPI
  MPI_Finalize() ;
#endif

  if (testFailed) {
    if (verbose && MyPID==0)
      cout << "End Result: TEST FAILED" << endl;	
    return -1;
  }
  //
  // Default return value
  //
  if (verbose && MyPID==0)
    cout << "End Result: TEST PASSED" << endl;
  return 0;

}	
