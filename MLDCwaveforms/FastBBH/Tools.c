#include <stdio.h>
#include <stdlib.h>
#include <math.h>
// #include <fftw3.h>

#include "Declarations.h"
#include "Constants.h"
#include "numrec.h"

void SBH_Barycenter(SBH_structure SBH, double *hp, double *hc)
{
  double m1, m2, Mtot, mu, Mchirp, DL, tc, eta, beta, sigma, chi1, chi2, dm; 
  double costheta, sintheta, costhetaL0, sinthetaL0;
  double costhetaS10, sinthetaS10, costhetaS20, sinthetaS20;
  double phi, phiL0, phiS10, phiS20, phic, Phase, psi;
  double LdotS1, LdotS2, LdotN, S1dotS2, fold;
  double Phi0, Phi10, Phi15, Phi20;
  double f, t, td, dt;
  double Amp, p15, p20, p150, p200;
  double fac, etain;
  double costhetaL, sinthetaL, phiL, thomas;
  double tcurrent, tfinal, hcurrent;
  double N[3], LSvals[9], updatedvals[9], fourthordervals[9], updatedthomas;
  double derivvals[9];
  double thomasderiv;
  double LcrossN[3];
  double cPhi[7], sPhi[7];
  double thomasval, maxerr, oldh, finalthomas;
  double A, B, C, D, deltat;
  double Ax, ci, si, ci2, si2, ci4, si4, ci6, OPhase;
  double x, xrt, taper;
  double tmax, fmax, VAmp, r;
  double shp, shc, c2psi, s2psi;

  int i, j, n, idxm;
  int index, laststep;

  // Vectors for storing all the data.  We don't know how many points we need, so pick an arbitrary length.

  double timevec[VECLENGTH];
  double mulvec[VECLENGTH];
  double philvec[VECLENGTH];
  double betavec[VECLENGTH];
  double sigmavec[VECLENGTH];
  double thomasvec[VECLENGTH];

  // For the spline:

  double muly2[VECLENGTH];
  double phily2[VECLENGTH];
  double betay2[VECLENGTH];
  double sigmay2[VECLENGTH];
  double thomasy2[VECLENGTH];

  double *tdvals;
  double *interpmul;
  double *interpphil;
  double *interpbeta;
  double *interpsigma;
  double *interpthomas;

  FILE *Out;

  dt = 15.0;
  n = (int)((Tobs + 2.0*Tpad)/dt);  // note, this should be made more general.
  VAmp = 2.0*SBH.AmplPNorder;

  tdvals = dvector(0, n-1);
  interpmul = dvector(0, n-1);
  interpphil = dvector(0, n-1);
  interpbeta = dvector(0, n-1);
  interpsigma = dvector(0, n-1);
  interpthomas = dvector(0, n-1);

  // Double Kerr Parameters: m1/Msun, m2/Msun, tc/sec, DL/Gpc, chi1, chi2, cos(theta), cos(thetaL(0)), cos(thetaS1(0)), cos(thetaS2(0)), phi, phiL(0), phiS1(0), phiS2(0), phic.  These are arranged into those with fixed ranges, and those without. Note: I am using redshifted masses. 

  m1 = SBH.Mass1;
  m2 = SBH.Mass1;
  tc = SBH.CoalescenceTime;
  DL = SBH.Distance/1.0e6;
  chi1 = SBH.Spin1;
  chi2 = SBH.Spin2;
  costheta = sin(SBH.EclipticLatitude);
  costhetaL0 = cos(SBH.InitialPolarAngleL);
  costhetaS10 = cos(SBH.PolarAngleOfSpin1);
  costhetaS20 = cos(SBH.PolarAngleOfSpin2);
  phi = SBH.EclipticLongitude;
  phiL0 = SBH.InitialAzimuthalAngleL;
  phiS10 = SBH.AzimuthalAngleOfSpin1;
  phiS20 = SBH.AzimuthalAngleOfSpin2;
  phic = SBH.PhaseAtCoalescence;

  // Useful functions of these parameters:

  Mtot = m1 + m2;
  dm = (m1-m2)/Mtot;
  mu = m1*m2/Mtot;
  eta = mu/Mtot;
  etain = 1.0/eta;
  Mchirp = pow(mu, 0.6)*pow(Mtot, 0.4);

  Amp = pow(Mchirp*TSUN,5./3.)/(DL*GPC/CLIGHT);

  fac = eta/(5.0*Mtot*TSUN);

  Phi0 = -2.*pow(fac,0.625);
  Phi10 = -pow(fac,0.375)*(3715./4032.+55./48.*eta);
  p15 = 0.375*pow(fac,0.25);
  p150 = p15*(4.*PI);
  p20 = -pow(fac,0.125);
  p200 = p20*(9275495./7225344.+284875./129024.*eta+1855./1024.*eta*eta);

  // Sky position:
  
  sintheta = sqrt(1.0 - costheta*costheta);
  N[0] = sintheta*cos(phi);
  N[1] = sintheta*sin(phi);
  N[2] = costheta;
  
  // LSvals: 0-2 are L, 3-5 S1, and 6-8 S2 (all unit vectors)

  sinthetaL0 = sqrt(1.0-(costhetaL0*costhetaL0));
  LSvals[0] = sinthetaL0*cos(phiL0);
  LSvals[1] = sinthetaL0*sin(phiL0);
  LSvals[2] = costhetaL0;
 
  sinthetaS10 =  sqrt(1.0-(costhetaS10*costhetaS10));
  LSvals[3] = sinthetaS10*cos(phiS10);
  LSvals[4] = sinthetaS10*sin(phiS10);
  LSvals[5] = costhetaS10;

  sinthetaS20 =  sqrt(1.0-(costhetaS20*costhetaS20));
  LSvals[6] = sinthetaS20*cos(phiS20);
  LSvals[7] = sinthetaS20*sin(phiS20);
  LSvals[8] = costhetaS20;

  LdotS1 = calcLdotS1(LSvals);
  LdotS2 = calcLdotS2(LSvals);
  S1dotS2 = calcSdotS(LSvals);

  // Initial beta and sigma:

  beta = (LdotS1*chi1/12.)*(113.*(m1/Mtot)*(m1/Mtot) + 75.*eta) + (LdotS2*chi2/12.)*(113.*(m2/Mtot)*(m2/Mtot) + 75.*eta);
      
  sigma = eta*(721.*LdotS1*chi1*LdotS2*chi2 - 247.*S1dotS2*chi1*chi2)/48.;    

  // Set up range of integration.  The extra Tpad=900 s is more than enough for the light travel time across the LISA orbit.

  tcurrent = -Tpad/TSUN;
  tmax = Tobs + Tpad;  // will decrease if fmax lies within integration range
  if(tc > tmax) tmax = tc;
  tfinal = tmax/TSUN;

  // Initial orbital radius
  r = pow(Mtot,1./3.)/pow(PI*Freq(0.0, Mtot, Mchirp, eta, beta, sigma, tc)*TSUN,2./3.);

  calcderivvals(derivvals, LSvals, r, m1, m2, Mtot, mu, chi1, chi2);
  LdotN = calcLdotN(LSvals, N);
  calcLcrossN(LcrossN, LSvals, N);
  thomasderiv = -2.*LdotN/(1.-LdotN*LdotN)*(LcrossN[0]*derivvals[0]+LcrossN[1]*derivvals[1]+LcrossN[2]*derivvals[2]);

  for(i = 0; i < 9; i++) LSvals[i] -= Tpad*derivvals[i];  // back up the initial values to time -Tpad
  thomasval = -Tpad*thomasderiv;

  index = 0;  // index for the various storage vectors
  hcurrent = RK_H;
  laststep = ENDNO;

  LdotS1 = calcLdotS1(LSvals);
  LdotS2 = calcLdotS2(LSvals);
  S1dotS2 = calcSdotS(LSvals);

  //  beta and sigma at time -Tpad

  beta = (LdotS1*chi1/12.)*(113.*(m1/Mtot)*(m1/Mtot) + 75.*eta) + (LdotS2*chi2/12.)*(113.*(m2/Mtot)*(m2/Mtot) + 75.*eta);     
  sigma = eta*(721.*LdotS1*chi1*LdotS2*chi2 - 247.*S1dotS2*chi1*chi2)/48.;    

  timevec[index] = tcurrent*TSUN;
  mulvec[index] = LSvals[2];
  philvec[index] = atan2(LSvals[1],LSvals[0]); 
  betavec[index] = beta;
  sigmavec[index] = sigma;
  thomasvec[index] = thomasval; 
  
  fold = 0.0;

  // Start the integration:
  
  while(tcurrent < tfinal)
    {	

         f = Freq(tcurrent*TSUN, Mtot, Mchirp, eta, betavec[index], sigmavec[index], tc);
         x = pow(PI*Mtot*f*TSUN,2./3.);

      if ((f >= fold) && (x < xmax)) {  // Check to make sure we're chirping and PN valid
	
	if (tcurrent+hcurrent-tfinal > 0.)
	  {
	    hcurrent = tfinal-tcurrent;
	    laststep = ENDYES;
	  }
	
	do
	  {	    
	    rkckstep(updatedvals, fourthordervals, &updatedthomas, hcurrent, LSvals, thomasval, tcurrent, m1, m2, Mtot, Mchirp, mu, eta, chi1, chi2, N, tc);
	  
	    maxerr = 0.0;
	  
	    for(i = 0; i <= 8; i++)
	      {
		double errori = fabs((updatedvals[i]-fourthordervals[i])/RK_EPS);
		maxerr = (errori > maxerr) ? errori : maxerr;
	      }
	  
	    oldh = hcurrent;
	    
	    if (maxerr > 1.) 
	      {
		hcurrent *= 0.9*pow(maxerr,-0.25);
		laststep = ENDNO;
	      }
	    else if (maxerr <= 0.0006)
	      hcurrent *= 4.;
	    else
	      hcurrent *= 0.9*pow(maxerr,-0.2);
	  }
	while(maxerr > 1.);
	
	for (i = 0; i <= 8; i++)
	  LSvals[i] = updatedvals[i];
      
	thomasval = updatedthomas;
	
	if (laststep == ENDYES)
	  tcurrent = tfinal;
	else
	  tcurrent += oldh;

        index++;

	timevec[index] = tcurrent*TSUN;
	
	LdotS1 = calcLdotS1(LSvals);
	LdotS2 = calcLdotS2(LSvals);
	S1dotS2 = calcSdotS(LSvals);
      
	mulvec[index] = LSvals[2];
	philvec[index] = atan2(LSvals[1],LSvals[0]);
      
	betavec[index] =  (LdotS1*chi1/12.)*(113.*(m1/Mtot)*(m1/Mtot) + 75.*eta) + (LdotS2*chi2/12.)*(113.*(m2/Mtot)*(m2/Mtot) + 75.*eta);
	
	sigmavec[index] = eta*(721.*LdotS1*chi1*LdotS2*chi2 - 247.*S1dotS2*chi1*chi2)/48.;    
	
	thomasvec[index] = thomasval;

        // printf("%d %e %e %e %f %f %f %f\n", index, timevec[index], betavec[index], sigmavec[index], LdotS1, LdotS2, S1dotS2, thomasval);

        fold = f;
       
      }
      else {
	// If PN gone bad go to tfinal and keep everything the same.
	
	tmax = tcurrent*TSUN;  
	// At tmax, we've just barely gone beyond xmax

	tcurrent = tfinal;
	timevec[index] = tcurrent*TSUN;
	mulvec[index] = mulvec[index-1];
	philvec[index] = philvec[index-1];
	betavec[index] = betavec[index-1];
	sigmavec[index] = sigmavec[index-1];
	
	thomasval = thomasvec[index-1];
	thomasvec[index] = thomasval;
        // printf("%d %e %e %e %f %f\n", index, timevec[index], betavec[index], sigmavec[index], thomasval);

      }
    }
  
  if (index >= VECLENGTH) {
    fprintf(stderr, "VECLENGTH is too small--fix and recompile (%d,%d).\n",index,VECLENGTH);
    exit(1);
  }

  idxm = index;

  spline(timevec, mulvec, index, 1.e30, 1.e30, muly2);
  spline(timevec, philvec, index, 1.e30, 1.e30, phily2);
  spline(timevec, betavec, index, 1.e30, 1.e30, betay2);
  spline(timevec, sigmavec, index, 1.e30, 1.e30, sigmay2); 
  spline(timevec, thomasvec, index, 1.e30, 1.e30, thomasy2);
  
  // End precession. 
  
  t = -Tpad;
  dt = (Tobs+2.0*Tpad)/(double)(n);
  
  index = 0;

  // Now interpolate.

  for(i = 0; i < n; i++)
    {   

      tdvals[i] = t;  
      
      if (tdvals[i] <= tmax) // Make sure we have data for the point.
	{
	  while(tdvals[i] > timevec[index+1])
	     index++;
	
	  deltat = timevec[index+1]-timevec[index];

	  A = (timevec[index+1]-tdvals[i])/deltat;
	  B = 1.0-A;
	  C = (pow(A,3.0)-A)*deltat*deltat/6.0;
	  D = (pow(B,3.0)-B)*deltat*deltat/6.0;
	  
	  interpmul[i] = A*mulvec[index] + B*mulvec[index+1] + C*muly2[index] + D*muly2[index+1];  
	
	  interpphil[i] = A*philvec[index] + B*philvec[index+1] + C*phily2[index] + D*phily2[index+1];  

	  interpbeta[i] = A*betavec[index] + B*betavec[index+1] + C*betay2[index] + D*betay2[index+1];
  
	  interpsigma[i] = A*sigmavec[index] + B*sigmavec[index+1] + C*sigmay2[index] + D*sigmay2[index+1];

	  interpthomas[i] = A*thomasvec[index] + B*thomasvec[index+1] + C*thomasy2[index] + D*thomasy2[index+1];
	  
	  f = Freq(tdvals[i], Mtot, Mchirp, eta, interpbeta[i], interpsigma[i], tc);
	  
	  finalthomas = interpthomas[i];  /* Note: current version can not handle tc > Tobs as we don't continue
                                             integration to find thomas phase at merger */
	 
	}
      t += dt;

      // printf("%e %e %e %e %e %e\n", tdvals[i], interpmul[i], interpphil[i], interpbeta[i], interpsigma[i], interpthomas[i]);
    }

  if(tc > Tobs) finalthomas = thomasvec[idxm];
  
  // Now calculate the waveform:

  for(i = 0; i < n; i++)
    { 
      td = tdvals[i];

      if (td <= tmax) // Make sure we have data for the point.
	{
	  costhetaL = interpmul[i];
	  sinthetaL = sqrt(1.-costhetaL*costhetaL);
	  phiL = interpphil[i];
	  beta = interpbeta[i];
	  sigma = interpsigma[i];
	  thomas = finalthomas-interpthomas[i];

	  f = Freq(td, Mtot, Mchirp, eta, beta, sigma, tc);
	    
	    x = pow(PI*Mtot*f*TSUN,2./3.);

	    if(x < xmax)   // should always be true
	      {
	    
	    Phi15 = p150 - p15*beta;
	    Phi20 = p200 - p20*(15./32.*sigma);
	    
	    Phase = 2.0*phic + etain*(Phi0*pow(tc-td,0.625) + Phi10*pow(tc-td,0.375) + Phi15*pow(tc-td,0.25) + Phi20*pow(tc-td,0.125)); 
	    
	    LdotN = costhetaL*costheta + sinthetaL*sintheta*cos(phiL - phi);
	  
	    taper = 0.5*(1.+tanh(150.*(1./SBH.TaperApplied-x)));

            Ax = 2.*Amp*pow(PI*f,2./3.)*taper;

            ci = LdotN;
            ci2 = ci*ci;
            ci4 = ci2*ci2;
            ci6 = ci2*ci4;
            si2 = 1.0-ci2;
            si4 = si2*si2;
            si = sqrt(si2);
            xrt = sqrt(x);
   
            OPhase = (Phase+thomas)/2.0;

            cPhi[1] = cos(OPhase);
            sPhi[1] = sin(OPhase);

           for(j = 2; j <= 6; j++)
            {
	      cPhi[j] = cPhi[j-1]*cPhi[1]-sPhi[j-1]*sPhi[1];
	      sPhi[j] = sPhi[j-1]*cPhi[1]+cPhi[j-1]*sPhi[1];
            }

	   /* dominant harmonic */
	    shp = Ax*(1.0+ci2)*cPhi[2];
	    shc = -2.0*Ax*ci*sPhi[2];


            if(VAmp > 0.0)
	      {
              /* 0.5 PN correction */
	      shp += xrt*Ax*si*dm/8.0*((5.0+ci2)*cPhi[1]-9.0*(1.0+ci2)*cPhi[3]);
	      shc += -xrt*Ax*3.0*si*ci*dm/4.0*(sPhi[1]-3.0*sPhi[3]); 
	      }

           if(VAmp > 1.0)
	      {
              /* 1 PN correction */
	      shp += -x*Ax*(cPhi[2]*((19.0+9.0*ci2-2.0*ci4)-eta*(19.0-11.0*ci2-6.0*ci4))/6.0
                             -4.0*cPhi[4]*si2*(1.0+ci2)*(1.0-3.0*eta)/3.0);
	      shc += x*Ax*(sPhi[2]*ci*((17.0-4.0*ci2)-eta*(13.0-12.0*ci2))/3.0
	                    -8.0*sPhi[4]*(1.0-3.0*eta)*ci*si2/3.0);
              }

           if(VAmp > 2.0)
	      {
              /* 1.5 PN correction */
		shp += -xrt*x*Ax*((dm*si/192.0)*(cPhi[1]*((57.0+60.0*ci2-ci4)-2.0*eta*(49.0-12.0*ci2-ci4))
						    -27.0*cPhi[3]*((73.0+40.0*ci2-9.0*ci4)-2.0*eta*(25.0-8.0*ci2-9.0*ci4))/2.0
                                                    +625.0*cPhi[5]*(1.0-2.0*eta)*si2*(1.0+ci2)/2.0)
                                     -TPI*(1.0+ci2)*cPhi[2]);
	        shc += xrt*x*Ax*((dm*si*ci/96.0)*(sPhi[1]*((63.0-5.0*ci2)-2.0*eta*(23.0-5.0*ci2))
                                                     -27.0*sPhi[3]*((67.0-15.0*ci2)-2.0*eta*(19.0-15.0*ci2))/2.0
                                                     +625.0*sPhi[5]*(1.0-2.0*eta)*si2/2.0)
				    -4.0*PI*ci*sPhi[2]);
              }

           if(VAmp > 3.0)
	      {
              /* 2 PN correction */
		shp += -x*x*Ax*(cPhi[2]*((22.0 + (396.0 * ci2) + (145.0 * ci4) - (5.0 * ci6))
                                            +5.0*eta*(706.0 - (216.0 * ci2) - (251.0 * ci4) + (15.0 * ci6))/3.0
                                            -5.0*eta*eta*(98.0 - (108.0 * ci2) + (7.0 * ci4) + (5.0 * ci6)))/120.0
                                  +cPhi[4]*2.0*si2*((59.0 + (35.0 * ci2) - (8.0 * ci4)) 
						    -5.0*eta*(131.0 + (59.0 * ci2) - (24.0 * ci4))/3.0
						    +5.0*eta*eta*(21.0 - (3.0 * ci2) - (8.0 * ci4)))/15.0
				  -cPhi[6]*81.0*si4*(1.0+ci2)*(1.0-5.0*eta+5.0*eta*eta)/40.0
				   +(dm*si/40.0)*(sPhi[1]*(11.0 + (7.0 * ci2) + (10.0 * (5.0+ci2)*ln2))
                                                 -cPhi[1]*5.0*PI*(5.0+ci2)
						 -sPhi[3]*27.0*(7.0-10.0*ln32)*(1.0+ci2)
						  +cPhi[3]*135.0*PI*(1.0+ci2)));
	        shc += x*x*Ax*(sPhi[2]*ci*(68.0 + (226.0 * ci2) - (15.0 * ci4)
					      +5.0*eta*(572.0 - (490.0 * ci2) + (45.0 * ci4))/3.0
                                              -5.0*eta*eta*(56.0 - (70.0 * ci2) + (15.0 * ci4)))/60.0
				  +sPhi[4]*4.0*ci*si2*((55.0-12.0*ci2)-5.0*eta*(119.0-36.0*ci2)/3.0
                                                       +5.0*eta*eta*(17.0-12.0*ci2))/15.0
                                  -sPhi[6]*81.0*ci*si4*(1.0-5.0*eta+5.0*eta*eta)/20.0
                                  -(dm*3.0*si*ci/20.0)*(cPhi[1]*(3.0+10.0*ln2)+sPhi[1]*5.0*PI
							-cPhi[3]*9.0*(7.0-10.0*ln32)-sPhi[3]*45.0*PI));        
              }


	    
	    psi = atan2(costheta*cos(phi-phiL)*sinthetaL-costhetaL*sintheta, sinthetaL*sin(phi-phiL));  
	    
	    c2psi = cos(2.*psi);
	    s2psi = sin(2.*psi);

            hp[i] = shp*c2psi + shc*s2psi;
            hc[i] = -shp*s2psi + shc*c2psi;

	      }
           else {  // x > xmax

        	hp[i] = 0.;
	        hc[i] = 0.;
              }

	}
      else {  // t > tmax
	
	 hp[i] = 0.;
	 hc[i] = 0.;
      }

    }

  free_dvector(tdvals, 0, n-1);
  free_dvector(interpmul, 0, n-1);
  free_dvector(interpphil, 0, n-1);
  free_dvector(interpbeta, 0, n-1);
  free_dvector(interpsigma, 0, n-1);
  free_dvector(interpthomas, 0, n-1);


}

double Freq(double t, double Mtot, double Mchirp, double eta, double beta, double sigma, double tc)  
{
  double fac, f10, f15, f20, f;

  fac = eta*(tc-t)/(5.*Mtot*TSUN);

  f10 = 743./2688.+11./32.*eta;
  f15 = -3.*(4.*PI-beta)/40.;
  f20 = 1855099./14450688.+56975./258048.*eta+371./2048.*eta*eta-3./64.*sigma;

  f = (pow(fac,-3./8.) + f10*pow(fac,-5./8.) + f15*pow(fac,-3./4.) + f20*pow(fac,-7./8.))/8./PI/Mtot/TSUN;

  return(f);
}

void rkckstep(double outputvals[], double fourthorderoutputvals[], double *outputthomas, double h, double currentvals[], double currentthomas, double t, double m1, double m2, double Mtot, double Mchirp, double mu, double eta, double chi1, double chi2, double N[], double tc)
{
  int i;
  
  double **k;
  double intvals[9];

  double kthomas[6];

  k = dmatrix(0,8,0,5);
  
  update(0, 0.0, h, currentvals, t, m1, m2, Mtot, Mchirp, mu, eta, chi1, chi2, N, tc, k, kthomas);
  
  for(i = 0; i < 9; i++) intvals[i] = currentvals[i] + B21*k[i][0];
  
  update(1, A2, h, intvals, t, m1, m2, Mtot, Mchirp, mu, eta, chi1, chi2, N, tc, k, kthomas);
  
  for(i = 0; i < 9; i++) intvals[i] = currentvals[i] + B31*k[i][0] + B32*k[i][1];
  
  update(2, A3, h, intvals, t, m1, m2, Mtot, Mchirp, mu, eta, chi1, chi2, N, tc, k, kthomas);
  
  for(i = 0; i < 9; i++) intvals[i] = currentvals[i] + B41*k[i][0] + B42*k[i][1] + B43*k[i][2];
  
  update(3, A4, h, intvals, t, m1, m2, Mtot, Mchirp, mu, eta, chi1, chi2, N, tc, k, kthomas);
  
  for(i = 0; i < 9; i++) intvals[i] = currentvals[i] + B51*k[i][0] + B52*k[i][1] + B53*k[i][2] + B54*k[i][3];

  update(4, A5, h, intvals, t, m1, m2, Mtot, Mchirp, mu, eta, chi1, chi2, N, tc, k, kthomas);
  
  for(i = 0; i < 9; i++) intvals[i] = currentvals[i] + B61*k[i][0] + B62*k[i][1] + B63*k[i][2] + B64*k[i][3] + B65*k[i][4];
  
  update(5, A6, h, intvals, t, m1, m2, Mtot, Mchirp, mu, eta, chi1, chi2, N, tc, k, kthomas);
  
  for(i = 0; i < 9; i++) {
    outputvals[i] = currentvals[i] + C1*k[i][0] + C2*k[i][1] + C3*k[i][2] + C4*k[i][3] + C5*k[i][4] + C6*k[i][5];
    fourthorderoutputvals[i] = currentvals[i] + D1*k[i][0] + D2*k[i][1] + D3*k[i][2] + D4*k[i][3] + D5*k[i][4] + D6*k[i][5];
  }
  
  *outputthomas = currentthomas + C1*kthomas[0] + C2*kthomas[1] + C3*kthomas[2] + C4*kthomas[3] + C5*kthomas[4] + C6*kthomas[5]; 
  
  free_dmatrix(k,0,8,0,5);
}

void update(int j, double A, double h, double intvals[], double t, double m1, double m2, double Mtot, double Mchirp, double mu, double eta, double chi1, double chi2, double N[], double tc, double **k, double kthomas[])
{
  
  double r;
  int i;
  double derivvals[9];
  double LcrossN[3];
  double LdotS1;
  double LdotS2;
  double S1dotS2;
  double LdotN;
  double thomasderiv;
  double beta;
  double sigma;

  LdotS1 = calcLdotS1(intvals);
  LdotS2 = calcLdotS2(intvals);
  S1dotS2 = calcSdotS(intvals);
  
  beta =  (LdotS1*chi1/12.)*(113.*(m1/Mtot)*(m1/Mtot) + 75.*eta) + (LdotS2*chi2/12.)*(113.*(m2/Mtot)*(m2/Mtot) + 75.*eta);
  sigma = eta*(721.*LdotS1*chi1*LdotS2*chi2 - 247.*S1dotS2*chi1*chi2)/48.;    
 
  r = pow(Mtot,1./3.)/pow(PI*Freq((t+A*h)*TSUN, Mtot, Mchirp, eta, beta, sigma, tc)*TSUN,2./3.);

  if (r == r) // catches NaNs
  {
  calcderivvals(derivvals, intvals, r, m1, m2, Mtot, mu, chi1, chi2);
  }
  else
  {
    for(i = 0; i < 9; i++) derivvals[i] = 0.0;
  }
  
  LdotN = calcLdotN(intvals, N);
  
  calcLcrossN(LcrossN, intvals, N);
  
  thomasderiv = -2.*LdotN/(1.-LdotN*LdotN)*(LcrossN[0]*derivvals[0]+LcrossN[1]*derivvals[1]+LcrossN[2]*derivvals[2]);
    
  for(i = 0; i < 9; i++) 
    k[i][j] = h*derivvals[i];
  
  kthomas[j] = h*thomasderiv; 
}

void calcderivvals(double derivvals[], double inputs[], double r, double m1, double m2, double Mtot, double mu, double chi1, double chi2)
  // Calculates the L and S derivatives
{
  derivvals[3] = S1xdot(inputs, r, m1, m2, Mtot, mu, chi1, chi2);
  derivvals[4] = S1ydot(inputs, r, m1, m2, Mtot, mu, chi1, chi2);
  derivvals[5] = S1zdot(inputs, r, m1, m2, Mtot, mu, chi1, chi2);
  derivvals[6] = S2xdot(inputs, r, m1, m2, Mtot, mu, chi1, chi2);
  derivvals[7] = S2ydot(inputs, r, m1, m2, Mtot, mu, chi1, chi2);
  derivvals[8] = S2zdot(inputs, r, m1, m2, Mtot, mu, chi1, chi2);
  derivvals[0] = (-chi1*pow(m1,2.)*derivvals[3]-chi2*pow(m2,2.)*derivvals[6])/(mu*sqrt(Mtot*r));
  derivvals[1] = (-chi1*pow(m1,2.)*derivvals[4]-chi2*pow(m2,2.)*derivvals[7])/(mu*sqrt(Mtot*r));
  derivvals[2] = (-chi1*pow(m1,2.)*derivvals[5]-chi2*pow(m2,2.)*derivvals[8])/(mu*sqrt(Mtot*r));
}

double S1xdot(double inputs[], double r, double m1, double m2, double Mtot, double mu, double chi1, double chi2)
{
  return (((2.+1.5*m2/m1)*mu*sqrt(Mtot*r)-1.5*chi2*pow(m2,2.)*calcLdotS2(inputs))*(inputs[1]*inputs[5]-inputs[2]*inputs[4])+0.5*chi2*pow(m2,2.)*(inputs[7]*inputs[5]-inputs[8]*inputs[4]))/(pow(r,3.));
}

double S1ydot(double inputs[], double r, double m1, double m2, double Mtot, double mu, double chi1, double chi2)
{
  return (((2.+1.5*m2/m1)*mu*sqrt(Mtot*r)-1.5*chi2*pow(m2,2.)*calcLdotS2(inputs))*(inputs[2]*inputs[3]-inputs[0]*inputs[5])+0.5*chi2*pow(m2,2.)*(inputs[8]*inputs[3]-inputs[6]*inputs[5]))/(pow(r,3.));
}

double S1zdot(double inputs[], double r, double m1, double m2, double Mtot, double mu, double chi1, double chi2)
{
  return (((2.+1.5*m2/m1)*mu*sqrt(Mtot*r)-1.5*chi2*pow(m2,2.)*calcLdotS2(inputs))*(inputs[0]*inputs[4]-inputs[1]*inputs[3])+0.5*chi2*pow(m2,2.)*(inputs[6]*inputs[4]-inputs[7]*inputs[3]))/(pow(r,3.));
}

double S2xdot(double inputs[], double r, double m1, double m2, double Mtot, double mu, double chi1, double chi2)
{
  return (((2.+1.5*m1/m2)*mu*sqrt(Mtot*r)-1.5*chi1*pow(m1,2.)*calcLdotS1(inputs))*(inputs[1]*inputs[8]-inputs[2]*inputs[7])+0.5*chi1*pow(m1,2.)*(inputs[4]*inputs[8]-inputs[5]*inputs[7]))/(pow(r,3.));
}

double S2ydot(double inputs[], double r, double m1, double m2, double Mtot, double mu, double chi1, double chi2)
{
  return (((2.+1.5*m1/m2)*mu*sqrt(Mtot*r)-1.5*chi1*pow(m1,2.)*calcLdotS1(inputs))*(inputs[2]*inputs[6]-inputs[0]*inputs[8])+0.5*chi1*pow(m1,2.)*(inputs[5]*inputs[6]-inputs[3]*inputs[8]))/(pow(r,3.));
}

double S2zdot(double inputs[], double r, double m1, double m2, double Mtot, double mu, double chi1, double chi2)
{
  return (((2.+1.5*m1/m2)*mu*sqrt(Mtot*r)-1.5*chi1*pow(m1,2.)*calcLdotS1(inputs))*(inputs[0]*inputs[7]-inputs[1]*inputs[6])+0.5*chi1*pow(m1,2.)*(inputs[3]*inputs[7]-inputs[4]*inputs[6]))/(pow(r,3.));
}

double calcLdotS1(double inputs[])
{
  return inputs[0]*inputs[3]+inputs[1]*inputs[4]+inputs[2]*inputs[5];
}

double calcLdotS2(double inputs[])
{
  return inputs[0]*inputs[6]+inputs[1]*inputs[7]+inputs[2]*inputs[8];
}

double calcSdotS(double inputs[])
{
  return inputs[3]*inputs[6]+inputs[4]*inputs[7]+inputs[5]*inputs[8];
}

double calcLdotN(double inputs[], double nvec[])
{
  return inputs[0]*nvec[0] + inputs[1]*nvec[1] + inputs[2]*nvec[2];
}

void calcLcrossN(double output[], double inputs[], double nvec[])
{
  output[0] = inputs[1]*nvec[2]-inputs[2]*nvec[1];
  output[1] = inputs[2]*nvec[0]-inputs[0]*nvec[2];
  output[2] = inputs[0]*nvec[1]-inputs[1]*nvec[0];
}


/* ********************************************************************************** */
/*                                                                                    */
/*                  Vector/Matrix memory (de)allocation routines                      */
/*                                                                                    */
/* ********************************************************************************** */


int *ivector(long nl, long nh)
/* allocate an int vector with subscript range v[nl..nh] */
{
    int *v;

    v=(int *)malloc((size_t) ((nh-nl+1+NR_END)*sizeof(int)));
    if (!v) fprintf(stderr,"allocation failure in ivector()");
    return v-nl+NR_END;
}

void free_ivector(int *v, long nl, long nh)
/* free an int vector allocated with ivector() */
{
    free((FREE_ARG) (v+nl-NR_END));
}

double *dvector(long nl, long nh)
/* allocate a double vector with subscript range v[nl..nh] */
{
    double *v=0;

    v=(double *)malloc((size_t) ((nh-nl+1+NR_END)*sizeof(double)));
    if (!v) fprintf(stderr,"allocation failure in dvector()");
    return v-nl+NR_END;
}

void free_dvector(double *v, long nl, long nh)
/* free a double vector allocated with dvector() */
{
    free((FREE_ARG) (v+nl-NR_END));
}

unsigned long *lvector(long nl, long nh)
/* allocate an unsigned long vector with subscript range v[nl..nh] */
{
    unsigned long *v;

    v=(unsigned long *)malloc((size_t) ((nh-nl+1+NR_END)*sizeof(long)));
    if (!v) fprintf(stderr,"allocation failure in lvector()");
    return v-nl+NR_END;
}

void free_lvector(unsigned long *v, long nl, long nh)
/* free an unsigned long vector allocated with lvector() */
{
    free((FREE_ARG) (v+nl-NR_END));
}

double **dmatrix(long nrl, long nrh, long ncl, long nch)
/* allocate a double matrix with subscript range m[nrl..nrh][ncl..nch] */
{
    long i, nrow=nrh-nrl+1,ncol=nch-ncl+1;
    double **m;

    /* allocate pointers to rows */
    m=(double **) malloc((size_t)((nrow+NR_END)*sizeof(double*)));
    if (!m) fprintf(stderr, "allocation failure 1 in matrix()");
    m += NR_END;
    m -= nrl;

    /* allocate rows and set pointers to them */
    m[nrl]=(double *) malloc((size_t)((nrow*ncol+NR_END)*sizeof(double)));
    if (!m[nrl]) fprintf(stderr,"allocation failure 2 in matrix()");
    m[nrl] += NR_END;
    m[nrl] -= ncl;

    for(i=nrl+1;i<=nrh;i++) m[i]=m[i-1]+ncol;

    /* return pointer to array of pointers to rows */
    return m;
}

void free_dmatrix(double **m, long nrl, long nrh, long ncl, long nch)
/* free a double matrix allocated by dmatrix() */
{
    free((FREE_ARG) (m[nrl]+ncl-NR_END));
    free((FREE_ARG) (m+nrl-NR_END));
}


double ***d3tensor(long nrl, long nrh, long ncl, long nch, long ndl, long ndh)
/* allocate a double 3tensor with range t[nrl..nrh][ncl..nch][ndl..ndh] */
{
    long i,j,nrow=nrh-nrl+1,ncol=nch-ncl+1,ndep=ndh-ndl+1;
    double ***t;

    /* allocate pointers to pointers to rows */
    t=(double ***) malloc((size_t)((nrow+NR_END)*sizeof(double**)));
    if (!t) fprintf(stderr,"allocation failure 1 in d3tensor()");
    t += NR_END;
    t -= nrl;

    /* allocate pointers to rows and set pointers to them */
    t[nrl]=(double **) malloc((size_t)((nrow*ncol+NR_END)*sizeof(double*)));
    if (!t[nrl]) fprintf(stderr,"allocation failure 2 in d3tensor()");
    t[nrl] += NR_END;
    t[nrl] -= ncl;

    /* allocate rows and set pointers to them */
    t[nrl][ncl]=(double *) malloc((size_t)((nrow*ncol*ndep+NR_END)*sizeof(double)));
    if (!t[nrl][ncl]) fprintf(stderr,"allocation failure 3 in f3tensor()");
    t[nrl][ncl] += NR_END;
    t[nrl][ncl] -= ndl;

    for(j=ncl+1;j<=nch;j++) t[nrl][j]=t[nrl][j-1]+ndep;
    for(i=nrl+1;i<=nrh;i++) {
        t[i]=t[i-1]+ncol;
        t[i][ncl]=t[i-1][ncl]+ncol*ndep;
        for(j=ncl+1;j<=nch;j++) t[i][j]=t[i][j-1]+ndep;
    }

    /* return pointer to array of pointers to rows */
    return t;
}

void free_d3tensor(double ***t, long nrl, long nrh, long ncl, long nch,
    long ndl, long ndh)
/* free a float d3tensor allocated by d3tensor() */
{
    free((FREE_ARG) (t[nrl][ncl]+ndl-NR_END));
    free((FREE_ARG) (t[nrl]+ncl-NR_END));
    free((FREE_ARG) (t+nrl-NR_END));
}

double gasdev2(long *idum)
{
  double ran2(long *idum);
  static int iset=0;
  static double gset;
  double fac, rsq, v1, v2;

  if(*idum < 0) iset = 0;
  if(iset == 0){
    do{
      v1 = 2.0 * ran2(idum)-1.0;
      v2 = 2.0 * ran2(idum)-1.0;
      rsq = v1*v1+v2*v2;
    } while (rsq >= 1.0 || rsq == 0.0);
    fac=sqrt(-2.0*log(rsq)/rsq);
    gset = v1 * fac;
    iset = 1;
    return(v2*fac);
  } else {
    iset = 0;
    return (gset);
  }
}

double ran2(long *idum)
{
    int j;
    long k;
    static long idum2=123456789;
    static long iy=0;
    static long iv[NTAB];
    double temp;

    if (*idum <= 0) {
        if (-(*idum) < 1) *idum=1;
        else *idum = -(*idum);
        idum2=(*idum);
        for (j=NTAB+7;j>=0;j--) {
            k=(*idum)/IQ1;
            *idum=IA1*(*idum-k*IQ1)-k*IR1;
            if (*idum < 0) *idum += IM1;
            if (j < NTAB) iv[j] = *idum;
        }
        iy=iv[0];
    }
    k=(*idum)/IQ1;
    *idum=IA1*(*idum-k*IQ1)-k*IR1;
    if (*idum < 0) *idum += IM1;
    k=idum2/IQ2;
    idum2=IA2*(idum2-k*IQ2)-k*IR2;
    if (idum2 < 0) idum2 += IM2;
    j=iy/NDIV;
    iy=iv[j]-idum2;
    iv[j] = *idum;
    if (iy < 1) iy += IMM1;
    if ((temp=AM*iy) > RNMX) return RNMX;
    else return temp;
}

void spline(double *x, double *y, int n, double yp1, double ypn, double *y2)
// Unlike NR version, assumes zero-offset arrays.  CHECK THAT THIS IS CORRECT.
{
  int i, k;
  double p, qn, sig, un, *u;

  u = dvector(0, n-2);
  
  // Boundary conditions: Check which is best.  
  if (yp1 > 0.99e30)
    y2[0] = u[0] = 0.0;
  else {
    y2[0] = -0.5;
    u[0] = (3.0/(x[1]-x[0]))*((y[1]-y[0])/(x[1]-x[0])-yp1);
  }

  for(i = 1; i < n-1; i++) {
    sig = (x[i]-x[i-1])/(x[i+1]-x[i-1]);
    p = sig*y2[i-1] + 2.0;
    y2[i] = (sig-1.0)/p;
    u[i] = (y[i+1]-y[i])/(x[i+1]-x[i]) - (y[i]-y[i-1])/(x[i]-x[i-1]);
    u[i] = (6.0*u[i]/(x[i+1]-x[i-1])-sig*u[i-1])/p;
  }

  // More boundary conditions.
  if (ypn > 0.99e30)
    qn = un = 0.0;
  else {
    qn = 0.5;
    un = (3.0/(x[n-1]-x[n-2]))*(ypn-(y[n-1]-y[n-2])/(x[n-1]-x[n-2]));
  }
  
  y2[n-1] = (un-qn*u[n-2])/(qn*y2[n-2]+1.0);
  
  for (k = n-2; k >= 0; k--)
    y2[k] = y2[k]*y2[k+1]+u[k];
  
  free_dvector(u, 0, n-2);
}
    
  
void KILL(char* Message)
{
  printf("\a\n");
  printf(Message);
  printf("Terminating the program.\n\n\n");
  exit(1);
 
  return;
}