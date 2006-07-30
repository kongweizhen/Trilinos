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

#ifndef ANASAZI_BASIC_EIGENPROBLEM_H
#define ANASAZI_BASIC_EIGENPROBLEM_H

/*! \file AnasaziBasicEigenproblem.hpp
  \brief Basic implementation of the Anasazi::Eigenproblem class
*/

#include "AnasaziEigenproblem.hpp"
#include "AnasaziMultiVecTraits.hpp"
#include "AnasaziOperatorTraits.hpp"

/*! \class Anasazi::BasicEigenproblem
  \brief This provides a basic implementation for defining standard or 
  generalized eigenvalue problems.
*/

namespace Anasazi {
  
  template<class ScalarType, class MV, class OP>
  class BasicEigenproblem : public virtual Eigenproblem<ScalarType, MV, OP> {
    
  public:
    
    //! @name Constructors/Destructor
    //@{ 
    
    //! Empty constructor - allows Anasazi::BasicEigenproblem to be described at a later time through "Set Methods".
    BasicEigenproblem();
    
    //! Standard Eigenvalue Problem Constructor.
    BasicEigenproblem( const Teuchos::RefCountPtr<OP>& Op, const Teuchos::RefCountPtr<MV>& InitVec );
    
    //! Generalized Eigenvalue Problem Constructor.
    BasicEigenproblem( const Teuchos::RefCountPtr<OP>& Op, const Teuchos::RefCountPtr<OP>& B, const Teuchos::RefCountPtr<MV>& InitVec );
    
    //! Copy Constructor.
    BasicEigenproblem( const BasicEigenproblem<ScalarType, MV, OP>& Problem );
    
    //! Destructor.
    virtual ~BasicEigenproblem() {};
    //@}
    
    //! @name Set Methods
    //@{ 
    
    /*! \brief Set the operator for which eigenvalues will be computed.  

    \note This may be different from the \c A if a spectral transformation is employed. 
    For example, this operator may apply the operation \f$(A-\sigma I)^{-1}\f$ if you are
    looking for eigenvalues of \c A around \f$\sigma\f$.  
    */
    void setOperator( const Teuchos::RefCountPtr<OP>& Op ) { _Op = Op; _isSet=false; };
    
    /*! \brief Set the operator \c A of the eigenvalue problem \f$Ax=Mx\lambda\f$.
    */
    void setA( const Teuchos::RefCountPtr<OP>& A ) { _AOp = A; _isSet=false; };
    
    /*! \brief Set the operator \c M of the eigenvalue problem \f$Ax = Mx\lambda\f$.
     */
    void setM( const Teuchos::RefCountPtr<OP>& M ) { _MOp = M; _isSet=false; };
    
    /*! \brief Set the preconditioner for this eigenvalue problem \f$Ax = Mx\lambda\f$.
     */
    void setPrec( const Teuchos::RefCountPtr<OP>& Prec ) { _Prec = Prec; _isSet=false; };
    
    /*! \brief Set the initial guess.  

    This vector is required to create all the space needed 
    by Anasazi to solve the eigenvalue problem.  

    \note Even if an initial guess is not known by the user, an initial vector must be passed in.  
    */
    void setInitVec( const Teuchos::RefCountPtr<MV>& InitVec ) { _InitVec = InitVec; _isSet=false; };
    
    /*! \brief Set auxilliary vectors.

    \note This multivector can have any number of columns, and most likely will contain vectors that
    will be used by the eigensolver to orthogonalize against.
    */
    void setAuxVecs( const Teuchos::RefCountPtr<MV>& AuxVecs ) { _AuxVecs = AuxVecs; _isSet=false; };

    //! Specify the number of eigenvalues (NEV) that are requested.
    void setNEV( const int nev ){ _nev = nev; _isSet=false; };

    //! Specify the symmetry of this eigenproblem.
    /*! This knowledge may allow the solver to take advantage of the eigenproblems' symmetry.
      Some computational work can be avoided by setting this properly.
    */
    void setHermitian( const bool isSym ){ _isSym = isSym; _isSet=false; };

    /*! \brief Specify that this eigenproblem is fully defined.
     *
     * This routine serves multiple purpose:
     * <ul>
     * <li> sanity check that the eigenproblem has been fully and constently defined
     * <li> opportunity for the eigenproblem to allocate internal storage for eigenvalues
     * and eigenvectors (to be used by eigensolvers and solver managers)
     * </ul>
     *
     * This method reallocates internal storage, so that any previously retrieved references to 
     * internal storage (eigenvectors or eigenvalues) are invalidated.
     *
     * \note The user MUST call this routine before they send the eigenproblem to any solver or solver manager.
     *
     * \returns \c true signifies success, \c false signifies error.
     */
    bool setProblem();

    /*! \brief Set the solution to the eigenproblem.
     *
     * This mechanism allows an Eigensolution struct to be associated with an Eigenproblem object.
     * setSolution() is usually called by a solver manager at the end of its SolverManager::solve() 
     * routine.
     */
    void setSolution(const Eigensolution<ScalarType,MV> &sol) {_sol = sol;}

    //@}
    
    //! @name Accessor Methods
    //@{ 
    
    //! Get a pointer to the operator for which eigenvalues will be computed.
    Teuchos::RefCountPtr<OP> getOperator() const { return( _Op ); };
    
    //! Get a pointer to the operator \c A of the eigenproblem \f$Ax=\lambda Mx\f$.
    Teuchos::RefCountPtr<OP> getA() const { return( _AOp ); };
    
    //! Get a pointer to the operator \c M of the eigenproblem \f$Ax=\lambda Mx\f$.
    Teuchos::RefCountPtr<OP> getM() const { return( _MOp ); };
    
    //! Get a pointer to the preconditioner of the eigenproblem \f$Ax=\lambda Mx\f$.
    Teuchos::RefCountPtr<OP> getPrec() const { return( _Prec ); };
    
    //! Get a pointer to the initial vector
    Teuchos::RefCountPtr<const MV> getInitVec() const { return( _InitVec ); };
    
    //! Get a pointer to the auxilliary vector
    Teuchos::RefCountPtr<const MV> getAuxVecs() const { return( _AuxVecs ); };
    
    //! Get the number of eigenvalues (NEV) that are required by this eigenproblem.
    int getNEV() const { return( _nev ); }

    //! Get the symmetry information for this eigenproblem.
    bool isHermitian() const { return( _isSym ); }
    
    //! If the problem has been set, this method will return true.
    bool isProblemSet() const { return( _isSet ); }

    /*! \brief Get the solution to the eigenproblem.
     *
     * There is no computation associated with this method. It only provides a 
     * mechanism for associating an Eigensolution with a Eigenproblem.
     */
    const Eigensolution<ScalarType,MV> & getSolution() const { return(_sol); }

    //@}
    
  protected:
    
    //! Reference-counted pointer for \c A of the eigenproblem \f$Ax=\lambda Mx\f$
    Teuchos::RefCountPtr<OP> _AOp;

    //! Reference-counted pointer for \c M of the eigenproblem \f$Ax=\lambda Mx\f$
    Teuchos::RefCountPtr<OP> _MOp; 

    //! Reference-counted pointer for the operator of the eigenproblem \f$Ax=\lambda Mx\f$
    Teuchos::RefCountPtr<OP> _Op;

    //! Reference-counted pointer for the preconditioner of the eigenproblem \f$Ax=\lambda Mx\f$
    Teuchos::RefCountPtr<OP> _Prec;

    //! Reference-counted pointer for the initial vector of the eigenproblem \f$Ax=\lambda Mx\f$
    Teuchos::RefCountPtr<MV> _InitVec;

    //! Reference-counted pointer for the auxilliary vector of the eigenproblem \f$Ax=\lambda Mx\f$
    Teuchos::RefCountPtr<MV> _AuxVecs;

    //! Reference-counted pointer for the computed eigenvectors of \f$Ax=\lambda Mx\f$
    Teuchos::RefCountPtr<MV> _Evecs;

    //! Reference-counted pointer for an orthonormal basis for the computed eigenspace of \f$Ax=\lambda Mx\f$
    Teuchos::RefCountPtr<MV> _Espace;

    //! Reference-counted pointer for an index set into the eigenpairs
    Teuchos::RefCountPtr<std::vector<int> > _index;

    //! Reference-counted pointer for the computed eigenvalues of \f$Ax=\lambda Mx\f$
    /*! \note If the operator is nonsymmetric, the length of this vector is 2*NEV where the 
      real part of eigenvalue \c j is entry \c j and the imaginary part is entry \c j+NEV .
    */
    Teuchos::RefCountPtr<std::vector<ScalarType> > _Evals;

    //! Number of eigenvalues requested
    int _nev;

    //! Symmetry of the eigenvalue problem
    /*! \note A generalized eigenvalue problem \f$Ax= \lambda Mx\f$ is considered symmetric
      if the operator \c M is positive (semi) definite.
    */
    bool _isSym;

    //! Sanity Check Flag
    bool _isSet;

    //! Type-definition for the MultiVecTraits class corresponding to the \c MV type
    typedef MultiVecTraits<ScalarType,MV> MVT;
    //! Type-definition for the OperatorTraits class corresponding to the \c OP type
    typedef OperatorTraits<ScalarType,MV,OP> OPT;

    Eigensolution<ScalarType,MV> _sol;
  };
  
  //=============================================================================
  //     Implementations (Constructors / Destructors)
  //=============================================================================
  
  template <class ScalarType, class MV, class OP>
  BasicEigenproblem<ScalarType, MV, OP>::BasicEigenproblem() : 
    _nev(0), 
    _isSym(false),
    _isSet(false)
  {
  }
  
  //=============================================================================
  
  template <class ScalarType, class MV, class OP>
  BasicEigenproblem<ScalarType, MV, OP>::BasicEigenproblem( const Teuchos::RefCountPtr<OP>& Op, const Teuchos::RefCountPtr<MV>& InitVec ) :    
    _Op(Op), 
    _InitVec(InitVec), 
    _nev(0), 
    _isSym(false),
    _isSet(false)
  {
  }
  
  //=============================================================================
  
  template <class ScalarType, class MV, class OP>
  BasicEigenproblem<ScalarType, MV, OP>::BasicEigenproblem( const Teuchos::RefCountPtr<OP>& Op, const Teuchos::RefCountPtr<OP>& M,
                                                            const Teuchos::RefCountPtr<MV>& InitVec ) :
    _MOp(M), 
    _Op(Op), 
    _InitVec(InitVec), 
    _nev(0), 
    _isSym(false),
    _isSet(false)
  {
  }
  
  //=============================================================================
  
  template <class ScalarType, class MV, class OP>
  BasicEigenproblem<ScalarType, MV, OP>::BasicEigenproblem( const BasicEigenproblem<ScalarType,MV,OP>& Problem ) :
    _AOp(Problem._AOp), 
    _MOp(Problem._MOp), 
    _Op(Problem._Op), 
    _Prec(Problem._Prec), 
    _InitVec(Problem._InitVec), 
    _Evecs(Problem._Evecs),
    _Evals(Problem._Evals),
    _nev(Problem._nev), 
    _isSym(Problem._isSym),
    _isSet(Problem._isSet)
  {
  }
  
  //=============================================================================
  //    SetProblem (sanity check method)
  //=============================================================================
  
  template <class ScalarType, class MV, class OP>
  bool BasicEigenproblem<ScalarType, MV, OP>::setProblem() 
  {
    //----------------------------------------------------------------
    // Sanity Checks
    //----------------------------------------------------------------
    // If there is no operator, then we can't proceed.
    if ( !_AOp.get() && !_Op.get() ) { return false; }
    
    // If there is no initial vector, then we don't have anything to clone workspace from.
    if ( !_InitVec.get() ) { return false; }
    
    // If we don't need any eigenvalues, we don't need to continue.
    if (_nev == 0) { return false; }

    // If there is an A, but no operator, we can set them equal.
    if (_AOp.get() && !_Op.get()) { _Op = _AOp; }

    //----------------------------------------------------------------
    // Even if this eigenproblem is being reused, reallocate storage for the
    // eigenvalues / eigenvectors
    //----------------------------------------------------------------
    _Evecs = MVT::Clone( *_InitVec, _nev );
    _Evals = Teuchos::rcp( new std::vector<ScalarType>( _nev ) );
    _index = Teuchos::rcp( new std::vector<int>( _nev ) );
    if ( _isSym ) {      
      _Espace = _Evecs;
    }
    else {
      _Espace = MVT::Clone( *_InitVec, _nev );
    }
    _isSet=true;
    return true;
  }        
  
} // end Anasazi namespace
#endif

// end AnasaziBasicEigenproblem.hpp
