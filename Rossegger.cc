#include "Rossegger.h"

#include <iostream>
#include <math.h>
#include "TMath.h"
#include "TFile.h"
#include <string>

//#include "/usr/local/include/complex_bessel.h"
#include <boost/math/special_functions.hpp> //covers all the special functions.

#include "TH2F.h"
#include "TH3.h"

using namespace std;
using namespace TMath;
//using namespace boost::math::special_functions

//Bessel Function J_n(x):
#define jn(order,x) boost::math::cyl_bessel_j(order,x)
//Bessel (Neumann) Function Y_n(x):
#define yn(order,x) boost::math::cyl_neumann(order,x)
//Modified Bessel Function of the first kind I_n(x):
#define in(order,x) boost::math::cyl_bessel_i(order,x)
//Modified Bessel Function of the second kind K_n(x):
#define kn(order,x) boost::math::cyl_bessel_k(order,x)
#define limu(im_order,x) Rossegger::Limu(im_order,x)
#define kimu(im_order,x) Rossegger::Kimu(im_order,x)

/*
  This is a modified/renamed copy of Carlos and Tom's "Spacecharge" class, modified to use boost instead of fortran routines, and with phi terms added.
 */


Rossegger::Rossegger(double InnerRadius, double OuterRadius, double Rdo_Z)
{
  a = InnerRadius;
  b = OuterRadius;
  L = Rdo_Z;

  verbosity = 0;
  pi = 2.0 * asin(1.0);
  cout << pi << endl;

  FindBetamn(0.0001);
  FindMunk(0.0001);

  char limufile[]="table.csv";
  //char kimufile[]="table.csv";
  LoadCsvToHist(hLimu,limufile);
  //LoadCsvToHist(hKimu,kimufile);
  

  cout << "Rossegger object initialized as follows:" << endl;
  cout << "  Inner Radius = " << a << " cm." << endl;
  cout << "  Outer Radius = " << b << " cm." << endl;
  cout << "  Half  Length = " << L << " cm." << endl;
  cout << "  Limu Dataset = " << limufile << endl;

  assert(1==2);
  return ;
}

double Rossegger::FindNextZero(double xstart,double epsilon,int order, double (Rossegger::*func)(int, double)){
    
  double previous=this.*func(order,xstart);
  double x=xstart+epsilon;
  double value=previous;
  while (! (  (value == 0) || (value<0 && previous>0) || (value>0 && previous<0)) ){
    	  //  Rossegger equation 5.12
    value = (this.*func)(order, x);
    if (value == 0) cout << "hit it exactly!  Go buy a lottery ticket!" << endl;
    if ( (value == 0) || (value<0 && previous>0) || (value>0 && previous<0))
      {
	//when we go from one sign to the other, we have bracketed the zero
	//the following is mathematically equivalent to finding the delta
	//between the x of our previous value and the point where we cross the axis
	//then returning x0=x_old+delta
	double slope = (value-previous)/epsilon;
	double intercept =  value - slope*x;
	double x0  = -intercept/slope;
	if (verbosity) cout << " " << x0 << "," << x0;
	return x0;
      }
    previous = value;
    x+=epsilon;
  }
  cout <<"logic break!\n";
  assert(1==2);
  return 0;

}


  
void Rossegger::FindBetamn(double epsilon)
{
  cout << "Now filling the Beta[m][n] Array..."<<endl;
  double l = a/b;
  for (int m=0; m<NumberOfOrders; m++)
    {
      if (verbosity) cout << endl << m;
      double x=epsilon;
      for (int n=0;n<NumberOfOrders;n++){//  !!!  Off by one from Rossegger convention  !!!
	x=FindNextZero(x,epsilon,m,&Rossegger::Rmn_for_zeroes);
	Betamn[m][n]=x/b;
	x+=epsilon;
      }
      /* the old way:

      double previous = jn(m,x)*yn(m,l*x) - jn(m,l*x)*yn(m,x);

      //step through the value of Rossegger 5.12 in epsilon steps.
      //When the sign changes, interpolate between the last two points
      //and call and call that our
      //zero
      while (n < NumberOfOrders)
	{
	  //  Rossegger equation 5.12
	  double value = jn(m,x)*yn(m,l*x) - jn(m,l*x)*yn(m,x);
	  //if (verbosity) cout << " " << value;
	  if (value == 0) cout << "hit it exactly!  Go buy a lottery ticket!" << endl;
	  if ( (value == 0) || (value<0 && previous>0) || (value>0 && previous<0))
	    {
	      //when we go from one sign to the other, we have bracketed the zero
	      //the following is mathematically equivalent to finding the delta
	      //between the x of our previous value and the point where we cross the axis
	      //then returning x0=x_old+delta
	      double slope = (value-previous)/epsilon;
	      double intercept =  value - slope*x;
	      double x0  = -intercept/slope;

	      Betamn[m][n] = x0/b;
	      if (verbosity) cout << " " << x0 << "," << Betamn[m][n];
	      n++;
	    }
	  previous = value;
	  x+=epsilon;
	}
      */
    }


  //  Now fill in the N2mn array...
  for (int m=0; m<NumberOfOrders; m++)
    {
      for (int n=0; n<NumberOfOrders; n++)
	{
	  //  Rossegger Equation 5.17
	  //  N^2_mn = 2/(pi * beta)^2 [ {Jm(beta a)/Jm(beta b)}^2 - 1 ]
	  N2mn[m][n]  = 2/(pi*pi*Betamn[m][n]*Betamn[m][n]);
	  //N2mn[m][n] *= (jn(m,Betamn[m][n]*a)*jn(m,Betamn[m][n]*a)/jn(m,Betamn[m][n]*b)/jn(m,Betamn[m][n]*b) - 1.0);
	  double jna_over_jnb=jn(m,Betamn[m][n]*a)/jn(m,Betamn[m][n]*b);
	  N2mn[m][n] *= (jna_over_jnb*jna_over_jnb-1.0);
	  //rcc note!  in eq 5.17, N2nm is set with betamn[m][n].  this is reversed here.  Not sure if important.
	  if (verbosity) cout << "m: " << m << " n: " << n << " N2[m][n]: " << N2mn[m][n];
	  double integral=0.0;
	  double step = 0.01;
	  if (verbosity)
	    {
	      for (double r=a; r<b; r+=step){
		integral += Rmn(m,n,r)*Rmn(m,n,r)*r*step;
	      }
	      cout << " Int: " << integral << endl;
	    }
	  //N2mn[m][n] = integral;
	}
    }

  cout << "Done." << endl;
}


void Rossegger::FindMunk(double epsilon)
{
  cout << "Now filling the Mu[n][k] Array..."<<endl;
  // We're looking for the zeroes of Rossegger eqn. 5.46:
  // R_nk(mu_nk;a,b)=Limu(Beta_n*a)Kimu(Beta_n*b)-Kimu(Beta_n*a)Limu(Beta_n*b)=0
  // since a and b are fixed, R_nk is a function solely of mu_nk and n.
  // for each 'n' we wish to find the a set of k mu_n's that result in R_nk=0


  cout << "Now filling the Mu[n][k] Array..."<<endl;
  double l = a/b;
  for (int n=0; n<NumberOfOrders; n++)//  !!!  Off by one from Rossegger convention  !!!
    {
      if (verbosity) cout << endl << n;
      double x=epsilon;
      for (int k=0;k<NumberOfOrders;k++){
	x=0;//FindNextZero(x,epsilon,n,Rossegger::Rnk_for_zeroes);
	Munk[n][k]=x;
	x+=epsilon;
      }
    }
  cout << "Done." << endl;
  return;
}

 double Rossegger::Limu(double mu, double x){
   //defined in Rossegger eqn 5.44, also a canonical 'satisfactory companion' to Kimu.
   //could use Griddit?
   return 0;
 }
 double Rossegger::Kimu(double mu, double x){
   //could use Griddit?
   return 0;
 }

 double Rossegger::Rmn_for_zeroes(int m, double x){
   double lx = a*x/b;
   return jn(m,x)*yn(m,lx) - jn(m,lx)*yn(m,x);
 }

double Rossegger::Rmn(int m, int n, double r){
  if (verbosity) cout << "Determine Rmn("<<m<<","<<n<<","<<r<<") = ";

  //  Check input arguments for sanity...
  int error=0;
  if (m<0 || m>NumberOfOrders) error=1;
  if (n<0 || n>NumberOfOrders) error=1;
  if (r<a || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments Rmn("<<m<<","<<n<<","<<r<<")" << endl;;
      return 0;
    }

  //  Calculate the function using C-libraries from boost
  //  Rossegger Equation 5.11:
  //         Rmn(r) = Ym(Beta_mn a)*Jm(Beta_mn r) - Jm(Beta_mn a)*Ym(Beta_mn r)
  double R=0;
  R = yn(m,Betamn[m][n]*a)*jn(m,Betamn[m][n]*r) - jn(m,Betamn[m][n]*a)*yn(m,Betamn[m][n]*r);

  if (verbosity) cout << R << endl;
  return R;
}

double Rossegger::Rmn1(int m, int n, double r)
{
 //  Check input arguments for sanity...
  int error=0;
  if (m<0 || m>NumberOfOrders) error=1;
  if (n<0 || n>NumberOfOrders) error=1;
  if (r<a || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments Rmn1("<<m<<","<<n<<","<<r<<")" << endl;;
      return 0;
    }

  //  Calculate using the TMath functions from root.
  //  Rossegger Equation 5.32
  //         Rmn1(r) = Km(BetaN a)Im(BetaN r) - Im(BetaN a) Km(BetaN r)
  double R=0;
  double BetaN = (n+1)*pi/L;
  R = kn(m,BetaN*a)*in(m,BetaN*r)-in(m,BetaN*a)*kn(m,BetaN*r);

  return R;
}

double Rossegger::Rmn2(int m, int n, double r)
{
 //  Check input arguments for sanity...
  int error=0;
  if (m<0 || m>NumberOfOrders) error=1;
  if (n<0 || n>NumberOfOrders) error=1;
  if (r<a || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments Rmn2("<<m<<","<<n<<","<<r<<")" << endl;;
      return 0;
    }

  //  Calculate using the TMath functions from root.
  //  Rossegger Equation 5.33
  //         Rmn2(r) = Km(BetaN b)Im(BetaN r) - Im(BetaN b) Km(BetaN r)
  double R=0;
  double BetaN = (n+1)*pi/L;
  R = kn(m,BetaN*b)*in(m,BetaN*r)-in(m,BetaN*b)*kn(m,BetaN*r);

  return R;
}

double Rossegger::RPrime(int m, int n, double ref, double r)
{
 //  Check input arguments for sanity...
  int error=0;
  if (m<0   || m>NumberOfOrders) error=1;
  if (n<0   || n>NumberOfOrders) error=1;
  if (ref<a || ref>b)            error=1;
  if (r<a   || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments RPrime("<<m<<","<<n<<","<<ref<<","<<r<<")" << endl;;
      return 0;
    }

  double R=0;
  //  Calculate using the TMath functions from root.
  //  Rossegger Equation 5.65
  //         Rmn2(ref,r) = BetaN/2* [   Km(BetaN ref) {Im-1(BetaN r) + Im+1(BetaN r)}
  //                                  - Im(BetaN ref) {Km-1(BetaN r) + Km+1(BetaN r)}  ]
  //  NOTE:  K-m(z) = Km(z) and I-m(z) = Im(z)... though boost handles negative orders.
  //
  // with: s -> ref,  t -> r, 
  //  NOTE:  BetaN is defined near Rossegger Equation 5.27, 5.43, and other places: BetaN= n*pi / L
  //  to match our change in definition of order 0 in " !!!  Off by one from Rossegger convention  !!!", we move n->n+1
  double BetaN = (n+1)*pi/L;
  double term1 = kn(m,BetaN*ref)*( in(m-1,BetaN*r) + in(m+1,BetaN*r) );
  double term2 = in(m,BetaN*ref)*( kn(m-1,BetaN*r) + kn(m+1,BetaN*r) );
  R = BetaN/2.0*(term1 + term2);

  return R;
}

double Rossegger::Rnk_for_zeroes(int n, double mu){

  double BetaN=(n+1)*pi/L;
    //  Rossegger Equation 5.46
  //       Rnk(r) = Limu_nk (BetaN a) Kimu_nk (BetaN b) - Kimu_nk(BetaN a) Limu_nk (BetaN b)
  
  return limu(mu,BetaN*a)*kimu(mu,BetaN*b)- kimu(mu,BetaN*a)*limu(mu,BetaN*b);
}
double Rossegger::Rnk(int n, int k, double r)
{
 //  Check input arguments for sanity...
  int error=0;
  if (n<0   || n>NumberOfOrders) error=1;
  if (k<0   || k>NumberOfOrders) error=1;
  if (r<a   || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments Rnk("<<n<<","<<k<<","<<r<<")" << endl;;
      return 0;
    }
  double BetaN=(n+1)*pi/L;
   //  Rossegger Equation 5.45
  //       Rnk(r) = Limu_nk (BetaN a) Kimu_nk (BetaN r) - Kimu_nk(BetaN a) Limu_nk (BetaN r)

  return limu(Munk[n,k],BetaN*a)*kimu(Munk[n,k],BetaN*r)- kimu(Munk[n,k],BetaN*a)*limu(Munk[n,k],BetaN*r);

}


double Rossegger::Ez(double r, double phi, double z, double r1, double phi1, double z1)
{
  //if(fByFile && fabs(r-r1)>MinimumDR && fabs(z-z1)>MinimumDZ) return ByFileEZ(r,phi,z,r1,phi1,z1);
  //  Check input arguments for sanity...
  int error=0;
  if (r<a    || r>b)         error=1;
  if (phi<0  || phi>2*pi)    error=1;
  if (z<0    || z>L)         error=1;
  if (r1<a   || r1>b)        error=1;
  if (phi1<0 || phi1>2*pi)   error=1;
  if (z1<0   || z1>L)        error=1;
  if (error)
    {
      cout << "Invalid arguments Ez(";
      cout <<r<<",";
      cout <<phi<<",";
      cout <<z<<",";
      cout <<r1<<",";
      cout <<phi1<<",";
      cout <<z1;
      cout <<")" << endl;;
      return 0;
    }

  double G=0;
  for (int m=0; m<NumberOfOrders; m++)
    {
      if (verbosity) cout << endl << m;
      for (int n=0; n<NumberOfOrders; n++)
	{
	  if (verbosity) cout << " " << n;
	  double term = 1/(2.0*pi);
	  if (verbosity) cout << " " << term; 
	  term *= (2 - ((m==0)?1:0))*cos(m*(phi-phi1));
	  if (verbosity) cout << " " << term; 
	  term *= Rmn(m,n,r)*Rmn(m,n,r1)/N2mn[m][n];
	  if (verbosity) cout << " " << term; 
	  if (z<z1)
	    {
	      term *=  cosh(Betamn[m][n]*z)*sinh(Betamn[m][n]*(L-z1))/sinh(Betamn[m][n]*L);
	    }
	  else
	    {
	      term *= -cosh(Betamn[m][n]*(L-z))*sinh(Betamn[m][n]*z1)/sinh(Betamn[m][n]*L);;
	    }
	  if (verbosity) cout << " " << term; 
	  G += term;
	  if (verbosity) cout << " " << term << " " << G << endl;
	}
    }
  if (verbosity) cout << "Ez = " << G << endl;

  return G;
}


double Rossegger::Er(double r, double phi, double z, double r1, double phi1, double z1)
{
  //field at r, phi, z due to unit charge at r1, phi1, z1;
  //if(fByFile && fabs(r-r1)>MinimumDR && fabs(z-z1)>MinimumDZ) return ByFileER(r,phi,z,r1,phi1,z1);
  //  Check input arguments for sanity...
  int error=0;
  if (r<a    || r>b)         error=1;
  if (phi<0  || phi>2*pi)    error=1;
  if (z<0    || z>L)         error=1;
  if (r1<a   || r1>b)        error=1;
  if (phi1<0 || phi1>2*pi)   error=1;
  if (z1<0   || z1>L)        error=1;
  if (error)
    {
      cout << "Invalid arguments Er(";
      cout <<r<<",";
      cout <<phi<<",";
      cout <<z<<",";
      cout <<r1<<",";
      cout <<phi1<<",";
      cout <<z1;
      cout <<")" << endl;;
      return 0;
    }

  double G=0;
  for (int m=0; m<NumberOfOrders; m++)
    {
      for (int n=0; n<NumberOfOrders; n++)
	{
	  double term = 1/(L*pi);

	  term *= (2 - ((m==0)?1:0))*cos(m*(phi-phi1));

	  double BetaN = (n+1)*pi/L;
	  term *= sin(BetaN*z)*sin(BetaN*z1);

	  if (r<r1)
	    {
	      term *= RPrime(m,n,a,r)*Rmn2(m,n,r1);
	    }
	  else
	    {
	      term *= Rmn1(m,n,r1)*RPrime(m,n,b,r);
	    }

	  term /= BesselI(m,BetaN*a)*BesselK(m,BetaN*b)-BesselI(m,BetaN*b)*BesselK(m,BetaN*a);

	  G += term;
	}
    }

  if (verbosity) cout << "Er = " << G << endl;

  return G;
}

double Rossegger::Ephi(double r, double phi, double z, double r1, double phi1, double z1)
{
  //  Check input arguments for sanity...
  int error=0;
  if (r<a    || r>b)         error=1;
  if (phi<0  || phi>2*pi)    error=1;
  if (z<0    || z>L)         error=1;
  if (r1<a   || r1>b)        error=1;
  if (phi1<0 || phi1>2*pi)   error=1;
  if (z1<0   || z1>L)        error=1;
  if (error)
    {
      cout << "Invalid arguments Ephi(";
      cout <<r<<",";
      cout <<phi<<",";
      cout <<z<<",";
      cout <<r1<<",";
      cout <<phi1<<",";
      cout <<z1;
      cout <<")" << endl;;
      return 0;
    }

  double G=0;
  //cout << "Ephi = " << G << endl;
  return G;
}

LoadCsvToHist(hLimu,limufile);
  LoadCsvToHist(hKimu,kimufile);
  

  cout << "Rossegger object initialized as follows:" << endl;
  cout << "  Inner Radius = " << a << " cm." << endl;
  cout << "  Outer Radius = " << b << " cm." << endl;
  cout << "  Half  Length = " << L << " cm." << endl;
  cout << "  Limu Dataset = " << limufile << endl;

  return ;
}

double Rossegger::FindNextZero(double xstart,double epsilon,int order, int func(int, double)){
    
  double previous=func(order,xstart);
  double x=xstart+epsilon;
  double value=previous;
  while (! (  (value == 0) || (value<0 && previous>0) || (value>0 && previous<0)) ){
    	  //  Rossegger equation 5.12
    value = func(order, x);
    if (value == 0) cout << "hit it exactly!  Go buy a lottery ticket!" << endl;
    if ( (value == 0) || (value<0 && previous>0) || (value>0 && previous<0))
      {
	//when we go from one sign to the other, we have bracketed the zero
	//the following is mathematically equivalent to finding the delta
	//between the x of our previous value and the point where we cross the axis
	//then returning x0=x_old+delta
	double slope = (value-previous)/epsilon;
	double intercept =  value - slope*x;
	double x0  = -intercept/slope;
	if (verbosity) cout << " " << x0 << "," << x0;
	return x0;
      }
    previous = value;
    x+=epsilon;
  }
  cout <<"logic break!\n";
  assert(1==2);
  return 0;

}


  
void Rossegger::FindBetamn(double epsilon)
{
  cout << "Now filling the Beta[m][n] Array..."<<endl;
  double l = a/b;
  for (int m=0; m<NumberOfOrders; m++)
    {
      if (verbosity) cout << endl << m;
      int n=0;  //  !!!  Off by one from Rossegger convention  !!!
      double x=epsilon;
      for (int n=0;n<NumberOfOrders;n++){
	x=FindNextZero(x,epsilon,m,Rossegger::Rmn_for_zeroes);
	Betamn[m][n]=x/b;
	x+=epsilon;
      }
      /* the old way:

      double previous = jn(m,x)*yn(m,l*x) - jn(m,l*x)*yn(m,x);

      //step through the value of Rossegger 5.12 in epsilon steps.
      //When the sign changes, interpolate between the last two points
      //and call and call that our
      //zero
      while (n < NumberOfOrders)
	{
	  //  Rossegger equation 5.12
	  double value = jn(m,x)*yn(m,l*x) - jn(m,l*x)*yn(m,x);
	  //if (verbosity) cout << " " << value;
	  if (value == 0) cout << "hit it exactly!  Go buy a lottery ticket!" << endl;
	  if ( (value == 0) || (value<0 && previous>0) || (value>0 && previous<0))
	    {
	      //when we go from one sign to the other, we have bracketed the zero
	      //the following is mathematically equivalent to finding the delta
	      //between the x of our previous value and the point where we cross the axis
	      //then returning x0=x_old+delta
	      double slope = (value-previous)/epsilon;
	      double intercept =  value - slope*x;
	      double x0  = -intercept/slope;

	      Betamn[m][n] = x0/b;
	      if (verbosity) cout << " " << x0 << "," << Betamn[m][n];
	      n++;
	    }
	  previous = value;
	  x+=epsilon;
	}
      */
    }


  //  Now fill in the N2mn array...
  for (int m=0; m<NumberOfOrders; m++)
    {
      for (int n=0; n<NumberOfOrders; n++)
	{
	  //  Rossegger Equation 5.17
	  //  N^2_mn = 2/(pi * beta)^2 [ {Jm(beta a)/Jm(beta b)}^2 - 1 ]
	  N2mn[m][n]  = 2/(pi*pi*Betamn[m][n]*Betamn[m][n]);
	  //N2mn[m][n] *= (jn(m,Betamn[m][n]*a)*jn(m,Betamn[m][n]*a)/jn(m,Betamn[m][n]*b)/jn(m,Betamn[m][n]*b) - 1.0);
	  double jna_over_jnb=jn(m,Betamn[m][n]*a)/jn(m,Betamn[m][n]*b);
	  N2mn[m][n] *= (jna_over_jnb*jna_over_jnb-1.0);
	  //rcc note!  in eq 5.17, N2nm is set with betamn[m][n].  this is reversed here.  Not sure if important.
	  if (verbosity) cout << "m: " << m << " n: " << n << " N2[m][n]: " << N2mn[m][n];
	  double integral=0.0;
	  double step = 0.01;
	  if (verbosity)
	    {
	      for (double r=a; r<b; r+=step){
		integral += Rmn(m,n,r)*Rmn(m,n,r)*r*step;
	      }
	      cout << " Int: " << integral << endl;
	    }
	  //N2mn[m][n] = integral;
	}
    }

  cout << "Done." << endl;
}


void Rossegger::FindMunk(double epsilon)
{
  cout << "Now filling the Mu[n][k] Array..."<<endl;
  // We're looking for the zeroes of Rossegger eqn. 5.46:
  // R_nk(mu_nk;a,b)=Limu(Beta_n*a)Kimu(Beta_n*b)-Kimu(Beta_n*a)Limu(Beta_n*b)=0
  // since a and b are fixed, R_nk is a function solely of mu_nk and n.
  // for each 'n' we wish to find the a set of k mu_n's that result in R_nk=0


  cout << "Now filling the Mu[n][k] Array..."<<endl;
  double l = a/b;
  for (int n=0; n<NumberOfOrders; n++)
    {
      if (verbosity) cout << endl << n;
      int n=0;  //  !!!  Off by one from Rossegger convention  !!!
      double x=epsilon;
      for (int k=0;k<NumberOfOrders;k++){
	x=FindNextZero(x,epsilon,n,Rossegger::Rnk_for_zeroes);
	Munk[n][k]=x;
	x+=epsilon;
      }
  cout << "Done." << endl;
}

 double Rossegger::Limu(double mu, double x){
   //defined in Rossegger eqn 5.44, also a canonical 'satisfactory companion' to Kimu.
   //could use Griddit?
   return 0;
 }
 double Rossegger::Kimu(double mu, double x){
   //could use Griddit?
   return 0;
 }

 double Rossegger::Rmn_for_zeroes(int m, double x){
   double lx = a*x/b;
   return jn(m,x)*yn(m,lx) - jn(m,lx)*yn(m,x);
 }
double Rossegger::Rmn(int m, int n, double r){
  if (verbosity) cout << "Determine Rmn("<<m<<","<<n<<","<<r<<") = ";

  //  Check input arguments for sanity...
  int error=0;
  if (m<0 || m>NumberOfOrders) error=1;
  if (n<0 || n>NumberOfOrders) error=1;
  if (r<a || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments Rmn("<<m<<","<<n<<","<<r<<")" << endl;;
      return 0;
    }

  //  Calculate the function using C-libraries from boost
  //  Rossegger Equation 5.11:
  //         Rmn(r) = Ym(Beta_mn a)*Jm(Beta_mn r) - Jm(Beta_mn a)*Ym(Beta_mn r)
  double R=0;
  R = yn(m,Betamn[m][n]*a)*jn(m,Betamn[m][n]*r) - jn(m,Betamn[m][n]*a)*yn(m,Betamn[m][n]*r);

  if (verbosity) cout << R << endl;
  return R;
}

double Rossegger::Rmn1(int m, int n, double r)
{
 //  Check input arguments for sanity...
  int error=0;
  if (m<0 || m>NumberOfOrders) error=1;
  if (n<0 || n>NumberOfOrders) error=1;
  if (r<a || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments Rmn1("<<m<<","<<n<<","<<r<<")" << endl;;
      return 0;
    }

  //  Calculate using the TMath functions from root.
  //  Rossegger Equation 5.32
  //         Rmn1(r) = Km(BetaN a)Im(BetaN r) - Im(BetaN a) Km(BetaN r)
  double R=0;
  double BetaN = (n+1)*pi/L;
  R = kn(m,BetaN*a)*in(m,BetaN*r)-in(m,BetaN*a)*kn(m,BetaN*r);

  return R;
}

double Rossegger::Rmn2(int m, int n, double r)
{
 //  Check input arguments for sanity...
  int error=0;
  if (m<0 || m>NumberOfOrders) error=1;
  if (n<0 || n>NumberOfOrders) error=1;
  if (r<a || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments Rmn2("<<m<<","<<n<<","<<r<<")" << endl;;
      return 0;
    }

  //  Calculate using the TMath functions from root.
  //  Rossegger Equation 5.33
  //         Rmn2(r) = Km(BetaN b)Im(BetaN r) - Im(BetaN b) Km(BetaN r)
  double R=0;
  double BetaN = (n+1)*pi/L;
  R = kn(m,BetaN*b)*in(m,BetaN*r)-in(m,BetaN*b)*kn(m,BetaN*r);

  return R;
}

double Rossegger::RPrime(int m, int n, double ref, double r)
{
 //  Check input arguments for sanity...
  int error=0;
  if (m<0   || m>NumberOfOrders) error=1;
  if (n<0   || n>NumberOfOrders) error=1;
  if (ref<a || ref>b)            error=1;
  if (r<a   || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments RPrime("<<m<<","<<n<<","<<ref<<","<<r<<")" << endl;;
      return 0;
    }

  double R=0;
  //  Calculate using the TMath functions from root.
  //  Rossegger Equation 5.65
  //         Rmn2(ref,r) = BetaN/2* [   Km(BetaN ref) {Im-1(BetaN r) + Im+1(BetaN r)}
  //                                  - Im(BetaN ref) {Km-1(BetaN r) + Km+1(BetaN r)}  ]
  //  NOTE:  K-m(z) = Km(z) and I-m(z) = Im(z)... though boost handles negative orders.
  //
  // with: s -> ref,  t -> r, 
  //  NOTE:  BetaN is defined near Rossegger Equation 5.27, 5.43, and other places: BetaN= n*pi / L
  //  to match our change in definition of order 0 in " !!!  Off by one from Rossegger convention  !!!", we move n->n+1
  double BetaN = (n+1)*pi/L;
  double term1 = kn(m,BetaN*ref)*( in(m-1,BetaN*r) + in(m+1,BetaN*r) );
  double term2 = in(m,BetaN*ref)*( kn(m-1,BetaN*r) + kn(m+1,BetaN*r) );
  R = BetaN/2.0*(term1 + term2);

  return R;
}

double Rossegger::Rnk_for_zeroes(int n, double mu){

  double BetaN=(n+1)*pi/L;
    //  Rossegger Equation 5.46
  //       Rnk(r) = Limu_nk (BetaN a) Kimu_nk (BetaN b) - Kimu_nk(BetaN a) Limu_nk (BetaN b)

  return limu(munk,BetaN*a)*kimu(munk,BetaN*b)- kimu(munk,BetaN*a)*limu(munk,BetaN*b);
}
double Rossegger::Rnk(int n, int k, double r)
{
 //  Check input arguments for sanity...
  int error=0;
  if (n<0   || n>NumberOfOrders) error=1;
  if (k<0   || k>NumberOfOrders) error=1;
  if (r<a   || r>b)              error=1;
  if (error)
    {
      cout << "Invalid arguments Rnk("<<n<<","<<k<<","<<r<<")" << endl;;
      return 0;
    }
  double BetaN=(n+1)*pi/L;
   //  Rossegger Equation 5.45
  //       Rnk(r) = Limu_nk (BetaN a) Kimu_nk (BetaN r) - Kimu_nk(BetaN a) Limu_nk (BetaN r)


  return limu(munk,BetaN*a)*kimu(munk,BetaN*r)- kimu(munk,BetaN*a)*limu(munk,BetaN*r);

}

void Rossegger::LoadCsvToHist(TH2F* hist, char* sourcefile){
  ifstream in(sourcefile);
  string s;
  float mu, x, val;
  delete hist; // get rid of it if we already had it around.
  hist=new TH2F(sourcefile,sourcefile,10,0.5,10.5,15,0.05,1.55);//for now hard code the limits
  while (!in.eof){
    in>>mu>>x>>val;
    printf("loaded %f,%f%f from file\n",mu,x,f);
    hist->Fill(mu,x,val);
  }
    
  return;
}


double Rossegger::Ez(double r, double phi, double z, double r1, double phi1, double z1)
{
  //if(fByFile && fabs(r-r1)>MinimumDR && fabs(z-z1)>MinimumDZ) return ByFileEZ(r,phi,z,r1,phi1,z1);
  //  Check input arguments for sanity...
  int error=0;
  if (r<a    || r>b)         error=1;
  if (phi<0  || phi>2*pi)    error=1;
  if (z<0    || z>L)         error=1;
  if (r1<a   || r1>b)        error=1;
  if (phi1<0 || phi1>2*pi)   error=1;
  if (z1<0   || z1>L)        error=1;
  if (error)
    {
      cout << "Invalid arguments Ez(";
      cout <<r<<",";
      cout <<phi<<",";
      cout <<z<<",";
      cout <<r1<<",";
      cout <<phi1<<",";
      cout <<z1;
      cout <<")" << endl;;
      return 0;
    }

  double G=0;
  for (int m=0; m<NumberOfOrders; m++)
    {
      if (verbosity) cout << endl << m;
      for (int n=0; n<NumberOfOrders; n++)
	{
	  if (verbosity) cout << " " << n;
	  double term = 1/(2.0*pi);
	  if (verbosity) cout << " " << term; 
	  term *= (2 - ((m==0)?1:0))*cos(m*(phi-phi1));
	  if (verbosity) cout << " " << term; 
	  term *= Rmn(m,n,r)*Rmn(m,n,r1)/N2mn[m][n];
	  if (verbosity) cout << " " << term; 
	  if (z<z1)
	    {
	      term *=  cosh(Betamn[m][n]*z)*sinh(Betamn[m][n]*(L-z1))/sinh(Betamn[m][n]*L);
	    }
	  else
	    {
	      term *= -cosh(Betamn[m][n]*(L-z))*sinh(Betamn[m][n]*z1)/sinh(Betamn[m][n]*L);;
	    }
	  if (verbosity) cout << " " << term; 
	  G += term;
	  if (verbosity) cout << " " << term << " " << G << endl;
	}
    }
  if (verbosity) cout << "Ez = " << G << endl;

  return G;
}


double Rossegger::Er(double r, double phi, double z, double r1, double phi1, double z1)
{
  //field at r, phi, z due to unit charge at r1, phi1, z1;
  //if(fByFile && fabs(r-r1)>MinimumDR && fabs(z-z1)>MinimumDZ) return ByFileER(r,phi,z,r1,phi1,z1);
  //  Check input arguments for sanity...
  int error=0;
  if (r<a    || r>b)         error=1;
  if (phi<0  || phi>2*pi)    error=1;
  if (z<0    || z>L)         error=1;
  if (r1<a   || r1>b)        error=1;
  if (phi1<0 || phi1>2*pi)   error=1;
  if (z1<0   || z1>L)        error=1;
  if (error)
    {
      cout << "Invalid arguments Er(";
      cout <<r<<",";
      cout <<phi<<",";
      cout <<z<<",";
      cout <<r1<<",";
      cout <<phi1<<",";
      cout <<z1;
      cout <<")" << endl;;
      return 0;
    }

  double G=0;
  for (int m=0; m<NumberOfOrders; m++)
    {
      for (int n=0; n<NumberOfOrders; n++)
	{
	  double term = 1/(L*pi);

	  term *= (2 - ((m==0)?1:0))*cos(m*(phi-phi1));

	  double BetaN = (n+1)*pi/L;
	  term *= sin(BetaN*z)*sin(BetaN*z1);

	  if (r<r1)
	    {
	      term *= RPrime(m,n,a,r)*Rmn2(m,n,r1);
	    }
	  else
	    {
	      term *= Rmn1(m,n,r1)*RPrime(m,n,b,r);
	    }

	  term /= BesselI(m,BetaN*a)*BesselK(m,BetaN*b)-BesselI(m,BetaN*b)*BesselK(m,BetaN*a);

	  G += term;
	}
    }

  if (verbosity) cout << "Er = " << G << endl;

  return G;
}

double Rossegger::Ephi(double r, double phi, double z, double r1, double phi1, double z1)
{
  //  Check input arguments for sanity...
  int error=0;
  if (r<a    || r>b)         error=1;
  if (phi<0  || phi>2*pi)    error=1;
  if (z<0    || z>L)         error=1;
  if (r1<a   || r1>b)        error=1;
  if (phi1<0 || phi1>2*pi)   error=1;
  if (z1<0   || z1>L)        error=1;
  if (error)
    {
      cout << "Invalid arguments Ephi(";
      cout <<r<<",";
      cout <<phi<<",";
      cout <<z<<",";
      cout <<r1<<",";
      cout <<phi1<<",";
      cout <<z1;
      cout <<")" << endl;;
      return 0;
    }

  double G=0;
  //cout << "Ephi = " << G << endl;