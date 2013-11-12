//@HEADER
// ***********************************************************************
//
//                     Rapid Optimization Library
//
// Questions? Contact:    Drew Kouri (dpkouri@sandia.gov)
//                      Denis Ridzal (dridzal@sandia.gov)
//
// ***********************************************************************
//@HEADER

#ifndef ROL_LSR1_H
#define ROL_LSR1_H

/** \class ROL::lSR1
    \brief Provides defintions for limited-memory SR1 operators.
*/

namespace ROL {

template<class Real>
class lSR1 : public Secant<Real> {
private:

  bool updateIterate_;

public:
  lSR1(int M) : Secant<Real>(M) {
    updateIterate_ = true;
  }

  // Update Secant Approximation
  void update( const Vector<Real> &grad, const Vector<Real> &gp, const Vector<Real> &s,
               const Real snorm, const int iter ) {
    // Get Generic Secant State
    Teuchos::RCP<SecantState<Real> >& state = Secant<Real>::get_state();

    state->iter = iter;
    Teuchos::RCP<Vector<Real> > gradDiff = grad.clone();
    gradDiff->set(grad);
    gradDiff->axpy(-1.0,gp);

    Real sy = s.dot(*gradDiff);
    if (this->updateIterate_ || state->current == -1) {
      if (state->current < state->storage-1) {
        state->current++;                               // Increment Storage
      }
      else {
        state->iterDiff.erase(state->iterDiff.begin()); // Remove first element of s list 
        state->gradDiff.erase(state->gradDiff.begin()); // Remove first element of y list
        state->product.erase(state->product.begin());   // Remove first element of rho list
      }
      state->iterDiff.push_back(s.clone());
      state->iterDiff[state->current]->set(s);          // s=x_{k+1}-x_k
      state->gradDiff.push_back(s.clone());
      state->gradDiff[state->current]->set(*gradDiff);  // y=g_{k+1}-g_k
      state->product.push_back(sy);                     // ys=1/rho  
    }
    updateIterate_ = true;
  }

  // Apply Initial Secant Approximate Inverse Hessian
  void applyH0( Vector<Real> &Hv, const Vector<Real> &v, const Vector<Real> &x ) {
    Hv.set(v);
  }


  // Apply lSR1 Approximate Inverse Hessian
  void applyH( Vector<Real> &Hv, const Vector<Real> &v, const Vector<Real> &x ) { 
    // Get Generic Secant State
    Teuchos::RCP<SecantState<Real> >& state = Secant<Real>::get_state();

    // Apply initial Hessian approximation to v   
    applyH0(Hv,v,x);

    std::vector<Teuchos::RCP<Vector<Real> > > a(state->current+1);
    std::vector<Teuchos::RCP<Vector<Real> > > b(state->current+1);
    Real byi = 0.0, byj = 0.0, bv = 0.0, normbi = 0.0, normyi = 0.0;
    for (int i = 0; i <= state->current; i++) {
      // Compute Hy
      a[i] = x.clone();
      applyH0(*(a[i]),*(state->gradDiff[i]),x);
      for (int j = 0; j < i; j++) {
        byj = b[j]->dot(*(state->gradDiff[j]));
        byi = b[j]->dot(*(state->gradDiff[i]));
        a[i]->axpy(byi/byj,*(b[j]));
      }
      // Compute s - Hy
      b[i] = x.clone();
      b[i]->set(*(state->iterDiff[i]));
      b[i]->axpy(-1.0,*(a[i]));

      // Compute Hv
      byi    = b[i]->dot(*(state->gradDiff[i]));
      normbi = b[i]->norm();
      normyi = state->gradDiff[i]->norm();
      if ( i == state->current && std::abs(byi) < sqrt(Teuchos::ScalarTraits<Real>::eps())*normbi*normyi ) {
        this->updateIterate_ = false;
      }
      else {
        this->updateIterate_ = true;
        bv  = b[i]->dot(v);
        Hv.axpy(bv/byi,*(b[i]));
      }
    }
  }

  // Apply Initial Secant Approximate Hessian  
  void applyB0( Vector<Real> &Bv, const Vector<Real> &v, const Vector<Real> &x ) { 
    Bv.set(v);
  }


  // Apply lSR1 Approximate Hessian
  void applyB( Vector<Real> &Bv, const Vector<Real> &v, const Vector<Real> &x ) { 
    // Get Generic Secant State
    Teuchos::RCP<SecantState<Real> >& state = Secant<Real>::get_state();

    // Apply initial Hessian approximation to v   
    applyB0(Bv,v,x);

    std::vector<Teuchos::RCP<Vector<Real> > > a(state->current+1);
    std::vector<Teuchos::RCP<Vector<Real> > > b(state->current+1);
    Real bsi = 0.0, bsj = 0.0, bv = 0.0, normbi = 0.0, normsi = 0.0;
    for (int i = 0; i <= state->current; i++) {
      // Compute Hy
      a[i] = x.clone();
      applyB0(*(a[i]),*(state->iterDiff[i]),x);
      for (int j = 0; j < i; j++) {
        bsj = b[j]->dot(*(state->iterDiff[j]));
        bsi = b[j]->dot(*(state->iterDiff[i]));
        a[i]->axpy(bsi/bsj,*(b[j]));
      }
      // Compute s - Hy
      b[i] = x.clone();
      b[i]->set(*(state->gradDiff[i]));
      b[i]->axpy(-1.0,*(a[i]));

      // Compute Hv
      bsi    = b[i]->dot(*(state->iterDiff[i]));
      normbi = b[i]->norm();
      normsi = state->iterDiff[i]->norm();
      if ( i == state->current && std::abs(bsi) < sqrt(Teuchos::ScalarTraits<Real>::eps())*normbi*normsi ) {
        this->updateIterate_ = false;
      }
      else {
        this->updateIterate_ = true;
        bv  = b[i]->dot(v);
        Bv.axpy(bv/bsi,*(b[i]));
      }
    }
  }

};

}

#endif
