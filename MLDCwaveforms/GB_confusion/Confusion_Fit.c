/* gcc -O2 -o Confusion_Fit Confusion_Fit.c arrays.c -lm */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "arrays.h"
#include "Constants.h"
#include "Detector.h"

double quickselect(double *arr, int n, int k);
void medianX(long imin, long imax, double *XP, double *Xnoise, double *Xconf);
void medianAE(long imin, long imax, double *AEP, double *AEnoise, double *AEconf);
void instrument_noise(double f, double *SAE, double *SXYZ);
double chiX(long imin, long imax, int Nfit, double *XP, double *Xfit, double *fpoints);
double Xconf(double f, int Nfit, double *Xfit, double *fpoints);
void startX(long imin, long imax, int Nfit, double *XP, double *Xfit, double *fpoints);
double ran2(long *idum);
double gasdev2(long *idum);
void KILL(char*);

int main(int argc,char **argv)
{

  double f, fdot, theta, phi, A, iota, psi, phase;
  char Gfile[50];
  double *params;
  double *XfLS, *AALS, *EELS;
  double *XLS, *AA, *EE;
  double *XP, *AEP;
  double fonfs, Sn, Sm, Acut;
  long M, N, q;
  long i, j, k, cnt, mult, imax, imin;
  long rseed;
  int kk;
  double SAE, SXYZ, sqT;
  double XR, XI, AR, AI, ER, EI;
  int Nfit;
  double dlogf;
  double beta, cx, cy, Hr, alpha;
  double *Xfity;
  double *Xfit, *Afit, *fpoints;
  double *Xnoise, *Xconf;
  double *AEnoise, *AEconf;

  FILE* Infile;
  FILE* Outfile;

  if(argc !=2) KILL("Confusion_Fit Galaxy.dat\n");

  XfLS = dvector(0,NFFT-1);  AALS = dvector(0,NFFT-1);  EELS = dvector(0,NFFT-1);


  imax = (long)ceil(1.0e-2*T);
  imin = (long)floor(1.0e-4*T);

  XfLS = dvector(0,NFFT-1);  AALS = dvector(0,NFFT-1); EELS = dvector(0,NFFT-1);

  rseed = -7584529636;

      Infile = fopen(argv[1],"r");
      for(i=1; i< imax; i++)
       {
	 fscanf(Infile,"%lf%lf%lf%lf%lf%lf%lf\n", &f, &XfLS[2*i], &XfLS[2*i+1],
                 &AALS[2*i], &AALS[2*i+1], &EELS[2*i], &EELS[2*i+1]);
       }
      fclose(Infile);

  XP = dvector(imin,imax);  AEP = dvector(imin,imax);
  Xnoise = dvector(imin,imax);  Xconf = dvector(imin,imax);
  AEnoise = dvector(imin,imax);  AEconf = dvector(imin,imax);

 rseed = -7584529636;

     for(i=imin; i< imax; i++)
       {
        f = (double)(i)/T;
        instrument_noise(f, &SAE, &SXYZ);
	 XP[i] = (2.0*(XfLS[2*i]*XfLS[2*i] + XfLS[2*i+1]*XfLS[2*i+1]));
         //XP[i] = SXYZ*0.5*(pow(gasdev2(&rseed), 2.0)+pow(gasdev2(&rseed), 2.0));
         AEP[i] = (2.0*(AALS[2*i]*AALS[2*i]+AALS[2*i+1]*AALS[2*i+1])); 
       }

     Outfile = fopen("Galaxy_XAE_Pow.dat","w");
     for(i=imin; i< imax; i++)
       {
         f = (double)(i)/T;
         instrument_noise(f, &SAE, &SXYZ);
	 fprintf(Outfile,"%e %e %e %e %e\n", f, XP[i], AEP[i], SXYZ, SAE);
       }
     fclose(Outfile);

     medianX(imin, imax, XP, Xnoise, Xconf);
     medianAE(imin, imax, AEP, AEnoise, AEconf);

      Outfile = fopen("Confusion_XAE_0.dat","w");
      for(i=imin; i<= imax; i++)
       {
	 f = (double)(i)/T;
	 fprintf(Outfile,"%e %e %e %e %e\n", f, Xnoise[i], Xconf[i], AEnoise[i], AEconf[i]);
       }
      fclose(Outfile);

     Outfile = fopen("Noise_Pow.dat","w");
     for(i=imin; i< imax; i++)
       {
         f = (double)(i)/T;
         instrument_noise(f, &SAE, &SXYZ);
	 fprintf(Outfile,"%e %e %e %e %e %e %e\n", f, Xnoise[i], Xconf[i], SXYZ, AEnoise[i], AEconf[i], SAE);
       }
     fclose(Outfile);
 
    return 0;

}

void medianX(long imin, long imax, double *XP, double *Xnoise, double *Xconf)
{
  double f, logf;
  double XC, SAE, SXYZ;
  double chi, scale;
  long i, j, k;
  long segs;
  long rseed;
  int Npoly;
  double av;
  double *XX;
  double *fdata, *mdata, *pcx, *pcy, *inst;
  double *fsams;
  double **fpows;
  double chix, chiy, fit, alpha, beta, mul, conf;
  double lfmin, lfmax, x, dlf, lf, ln4;
  FILE *Xfile;

  XX = dvector(0,100);

  rseed = -546214;

  segs = (int)((double)(imax-imin)/101.0);

  lfmin = log((double)(imin-101)/T);
  lfmax = log((double)(imin+101*(segs))/T);


  Npoly = 30;

  dlf = (lfmax-lfmin)/(double)(Npoly);
  ln4 = log(1.0e-4);

  fdata = dvector(0,segs-1);
  mdata = dvector(0,segs-1);
  inst = dvector(0,segs-1);
  pcx = dvector(0,Npoly);
  pcy = dvector(0,Npoly);

     for(i=0; i < segs; i++)
       {
	 for(j=0; j<=100; j++) XX[j] = XP[imin+101*i+j];
         f = (double)(imin+101*i-50)/T;
         instrument_noise(f, &SAE, &SXYZ);
         inst[i] = log(SXYZ*1.0e40);
         chi=quickselect(XX, 101, 51);
         //printf("%e %e\n", f, chi/0.72);
         fdata[i] = log(f);
         mdata[i] = log(chi/0.72*1.0e40);
       }

     // initialize fit
     for(i=1; i < Npoly; i++)
       {
         f = exp(lfmin+(double)(i)*dlf);
	 j = (long)((f*T-(double)(imin-50))/101.0);
         //printf("%ld %ld\n", i, j);
         pcx[i] = mdata[j];
	 //printf("%e %e\n", f, exp(pcx[i])*1.0e-40);
       }
     pcx[0] = pcx[1];
     pcx[Npoly] = pcx[Npoly-1];
     //printf("%ld\n", segs);

  
     chix = 0.0;
     for(i=0; i < segs; i++)
       {
	lf = log((double)(imin+101*i-50)/T);
        j = (long)floor((lf-lfmin)/dlf);
	fit = pcx[j]+((pcx[j+1]-pcx[j])/dlf)*(lf-(lfmin+(double)(j)*dlf));
        f = exp(-1.0*((lfmin+((double)(j)+0.5)*dlf)-ln4));
        chix += 500.0*(mdata[i] - fit)*(mdata[i] - fit)*f;
        //printf("%ld %e\n", i, (lf-(lfmin+(double)(j)*dlf))/dlf);
        //printf("%e %e %e\n", exp(lf), exp(mdata[i])*1.0e-40, exp(fit)*1.0e-40);
       }

     for(k=0; k < 10000; k++)
       {
	 beta = pow(10.0,-0.0001*(10000.0-(double)(k))/10000.0);

         for(j=0; j <= Npoly; j++) pcy[j] = pcx[j];
         j = (long)((double)(Npoly+1)*ran2(&rseed));
         mul = 1.0;
         alpha = ran2(&rseed);
         if(alpha > 0.3) mul = 10.0;
         if(alpha > 0.7) mul = 0.01;
         pcy[j] = pcx[j]+mul*0.01*gasdev2(&rseed)/sqrt(beta);

        chiy = 0.0;
        for(i=0; i < segs; i++)
         {
	lf = log((double)(imin+101*i-50)/T);
        j = (long)floor((lf-lfmin)/dlf);
	fit = pcy[j]+((pcy[j+1]-pcy[j])/dlf)*(lf-(lfmin+(double)(j)*dlf));
        f = exp(-1.0*((lfmin+((double)(j)+0.5)*dlf)-ln4));
        chiy += 500.0*(mdata[i] - fit)*(mdata[i] - fit)*f;
        }

        alpha = log(ran2(&rseed));

        if(beta*(chix-chiy) > alpha)
	  {
	    chix = chiy;
            for(j=0; j <= Npoly; j++) pcx[j] = pcy[j];
          }
	if(k%100==0) printf("%ld %.10e %.10e\n", k, chix, chiy);
       }

     //printf("%ld %.10e %.10e\n", k, chix, chiy);

     Xfile = fopen("Xfit.dat","w");
        for(i=0; i < segs; i++)
         {
	lf = log((double)(imin+101*i-50)/T);
        j = (long)floor((lf-lfmin)/dlf);
	fit = pcx[j]+((pcx[j+1]-pcx[j])/dlf)*(lf-(lfmin+(double)(j)*dlf));
        fprintf(Xfile, "%e %e %e\n", exp(lf), exp(fit)*1.0e-40, exp(mdata[i])*1.0e-40);
        }
      fclose(Xfile);

     Xfile = fopen("Xf.dat","w");
        for(i=0; i <= Npoly; i++)
         {
	   lf = lfmin+(double)(i)*dlf;
           fprintf(Xfile, "%e %e\n", exp(lf), exp(pcx[i])*1.0e-40);
        }
      fclose(Xfile);



    for(i=imin; i <= imax; i++)
       {
         f = (double)(i)/T;
         lf = log(f);
         j = (long)floor((lf-lfmin)/dlf);
	 fit = pcx[j]+((pcx[j+1]-pcx[j])/dlf)*(lf-(lfmin+(double)(j)*dlf));
         instrument_noise(f, &SAE, &SXYZ);
         alpha = exp(fit)*1.0e-40;
         conf = alpha -SXYZ;
         if(conf < SXYZ/30.0) conf = 1.0e-46;
         Xnoise[i] = alpha;
         Xconf[i] = conf;
       }

    free_dvector(XX, 0,100);
    free_dvector(fdata, 0,segs-1);
    free_dvector(mdata, 0,segs-1);
    free_dvector(pcx, 0,Npoly);
    free_dvector(pcy, 0,Npoly);

  return;
}

void medianXcheby(long imin, long imax, double *XP, double *Xnoise, double *Xconf)
{
  double f, logf;
  double XC, SAE, SXYZ;
  double chi, scale;
  long i, j, k;
  long segs;
  long rseed;
  int Npoly;
  double av;
  double *XX;
  double *fdata, *mdata, *pcx, *pcy, *inst;
  double **fpows;
  double chix, chiy, fit, alpha, beta, mul, conf;
  double lfmin, lfmax, x;
  FILE *Xfile;

  XX = dvector(0,100);

  rseed = -546214;

  segs = (int)((double)(imax-imin)/101.0);

  lfmin = log((double)(imin-50)/T);
  lfmax = log((double)(imin+101*(segs-1)-50)/T);

  Npoly = 12;

  fdata = dvector(0,segs-1);
  mdata = dvector(0,segs-1);
  inst = dvector(0,segs-1);
  fpows = dmatrix(0,segs-1,0,Npoly);
  pcx = dvector(0,Npoly);
  pcy = dvector(0,Npoly);

  //av = 0.0;
     for(i=0; i < segs; i++)
       {
	 for(j=0; j<=100; j++) XX[j] = XP[imin+101*i+j];
         f = (double)(imin+101*i-50)/T;
         instrument_noise(f, &SAE, &SXYZ);
         inst[i] = log(SXYZ*1.0e40);
         chi=quickselect(XX, 101, 51);
         //printf("%e %e\n", f, chi/0.72);
         fdata[i] = f;
         mdata[i] = log(chi/0.72*1.0e40);
         x = (log(f)-0.5*(lfmax+lfmin))/(0.5*(lfmax-lfmin));
         fpows[i][0] = 1.0;
         fpows[i][1] = x;
         for(j=2; j <= Npoly; j++) fpows[i][j] = 2.0*x*fpows[i][j-1]-fpows[i][j-2];
         //av += chi;
       }

     //printf("%f\n", av/(double)(segs));

     pcx[0] = -0.5;
     for(i=1; i < Npoly; i++) pcx[i] = 0.0;
  
     chix = 0.0;
     for(i=0; i < segs; i++)
       {
	fit = 0.0;
        for(j=0; j <= Npoly; j++) fit += pcx[j]*fpows[i][j];
        chix += 10.0*(mdata[i] - inst[i] - fit)*(mdata[i] - inst[i] - fit);
       }

     for(k=0; k < 100000; k++)
       {
	 beta = pow(10.0,-0.00001*(100000.0-(double)(k))/100000.0);

         for(j=0; j <= Npoly; j++) pcy[j] = pcx[j];
         j = (long)((double)(Npoly+1)*ran2(&rseed));
         mul = 1.0;
         alpha = ran2(&rseed);
         if(alpha > 0.3) mul = 10.0;
         if(alpha > 0.7) mul = 0.01;
         pcy[j] = pcx[j]+mul*0.001*gasdev2(&rseed)/sqrt(beta);

        chiy = 0.0;
        for(i=0; i < segs; i++)
         {
	fit = 0.0;
        for(j=0; j <= Npoly; j++) fit += pcy[j]*fpows[i][j];
        chiy += 10.0*(mdata[i] - inst[i] - fit)*(mdata[i] - inst[i] - fit);
        }

        alpha = log(ran2(&rseed));

        if(beta*(chix-chiy) > alpha)
	  {
	    chix = chiy;
            for(j=0; j <= Npoly; j++) pcx[j] = pcy[j];
          }
	// if(k%10000==0) printf("%ld %.10e %.10e\n", k, chix, chiy);
       }

     //printf("%ld %.10e %.10e\n", k, chix, chiy);

     Xfile = fopen("Xfit.dat","w");
        for(i=0; i < segs; i++)
         {
	fit = 0.0;
        for(j=0; j <= Npoly; j++) fit += pcx[j]*fpows[i][j];
        fprintf(Xfile, "%e %e %e\n", fdata[i], exp(fit+inst[i])*1.0e-40, exp(mdata[i])*1.0e-40);
        }
      fclose(Xfile);

    for(i=imin; i <= imax; i++)
       {
         f = (double)(i)/T;
         x = (log(f)-0.5*(lfmax+lfmin))/(0.5*(lfmax-lfmin));
         fpows[1][0] = 1.0;
         fpows[1][1] = x;
         for(j=2; j <= Npoly; j++) fpows[1][j] = 2.0*x*fpows[1][j-1]-fpows[1][j-2];
	 fit = 0.0;
         for(j=0; j <= Npoly; j++) fit += pcx[j]*fpows[1][j];
         instrument_noise(f, &SAE, &SXYZ);
         alpha = exp(fit+log(SXYZ*1.0e40))*1.0e-40;
         conf = alpha -SXYZ;
         if(conf < SXYZ/30.0) conf = 1.0e-46;
         Xnoise[i] = alpha;
         Xconf[i] = conf;
       }

    free_dvector(XX, 0,100);
    free_dvector(fdata, 0,segs-1);
    free_dvector(mdata, 0,segs-1);
    free_dmatrix(fpows, 0,segs-1,0,Npoly);
    free_dvector(pcx, 0,Npoly);
    free_dvector(pcy, 0,Npoly);

  return;
}

void medianAE(long imin, long imax, double *AEP, double *AEnoise, double *AEconf)
{
  double f, logf;
  double XC, SAE, SXYZ;
  double chi, scale;
  long i, j, k;
  long segs;
  long rseed;
  int Npoly;
  double av;
  double *XX;
  double *fdata, *mdata, *pcx, *pcy, *inst;
  double *fsams;
  double **fpows;
  double chix, chiy, fit, alpha, beta, mul, conf;
  double lfmin, lfmax, x, dlf, lf, ln4;
  FILE *Xfile;

  XX = dvector(0,100);

  rseed = -546214;

  segs = (int)((double)(imax-imin)/101.0);

  lfmin = log((double)(imin-101)/T);
  lfmax = log((double)(imin+101*(segs))/T);


  Npoly = 30;

  dlf = (lfmax-lfmin)/(double)(Npoly);
  ln4 = log(1.0e-4);

  fdata = dvector(0,segs-1);
  mdata = dvector(0,segs-1);
  inst = dvector(0,segs-1);
  pcx = dvector(0,Npoly);
  pcy = dvector(0,Npoly);

     for(i=0; i < segs; i++)
       {
	 for(j=0; j<=100; j++) XX[j] = AEP[imin+101*i+j];
         f = (double)(imin+101*i-50)/T;
         instrument_noise(f, &SAE, &SXYZ);
         inst[i] = log(SAE*1.0e40);
         chi=quickselect(XX, 101, 51);
         //printf("%e %e\n", f, chi/0.72);
         fdata[i] = log(f);
         mdata[i] = log(chi/0.72*1.0e40);
       }

     // initialize fit
     for(i=1; i < Npoly; i++)
       {
         f = exp(lfmin+(double)(i)*dlf);
	 j = (long)((f*T-(double)(imin-50))/101.0);
         //printf("%ld %ld\n", i, j);
         pcx[i] = mdata[j];
	 //printf("%e %e\n", f, exp(pcx[i])*1.0e-40);
       }
     pcx[0] = pcx[1];
     pcx[Npoly] = pcx[Npoly-1];
     //printf("%ld\n", segs);

  
     chix = 0.0;
     for(i=0; i < segs; i++)
       {
	lf = log((double)(imin+101*i-50)/T);
        j = (long)floor((lf-lfmin)/dlf);
	fit = pcx[j]+((pcx[j+1]-pcx[j])/dlf)*(lf-(lfmin+(double)(j)*dlf));
        f = exp(-1.0*((lfmin+((double)(j)+0.5)*dlf)-ln4));
        chix += 500.0*(mdata[i] - fit)*(mdata[i] - fit)*f;
        //printf("%ld %e\n", i, (lf-(lfmin+(double)(j)*dlf))/dlf);
        //printf("%e %e %e\n", exp(lf), exp(mdata[i])*1.0e-40, exp(fit)*1.0e-40);
       }

     for(k=0; k < 10000; k++)
       {
	 beta = pow(10.0,-0.0001*(10000.0-(double)(k))/10000.0);

         for(j=0; j <= Npoly; j++) pcy[j] = pcx[j];
         j = (long)((double)(Npoly+1)*ran2(&rseed));
         mul = 1.0;
         alpha = ran2(&rseed);
         if(alpha > 0.3) mul = 10.0;
         if(alpha > 0.7) mul = 0.01;
         pcy[j] = pcx[j]+mul*0.01*gasdev2(&rseed)/sqrt(beta);

        chiy = 0.0;
        for(i=0; i < segs; i++)
         {
	lf = log((double)(imin+101*i-50)/T);
        j = (long)floor((lf-lfmin)/dlf);
	fit = pcy[j]+((pcy[j+1]-pcy[j])/dlf)*(lf-(lfmin+(double)(j)*dlf));
        f = exp(-1.0*((lfmin+((double)(j)+0.5)*dlf)-ln4));
        chiy += 500.0*(mdata[i] - fit)*(mdata[i] - fit)*f;
        }

        alpha = log(ran2(&rseed));

        if(beta*(chix-chiy) > alpha)
	  {
	    chix = chiy;
            for(j=0; j <= Npoly; j++) pcx[j] = pcy[j];
          }
	if(k%100==0) printf("%ld %.10e %.10e\n", k, chix, chiy);
       }

     //printf("%ld %.10e %.10e\n", k, chix, chiy);

     Xfile = fopen("Afit.dat","w");
        for(i=0; i < segs; i++)
         {
	lf = log((double)(imin+101*i-50)/T);
        j = (long)floor((lf-lfmin)/dlf);
	fit = pcx[j]+((pcx[j+1]-pcx[j])/dlf)*(lf-(lfmin+(double)(j)*dlf));
        fprintf(Xfile, "%e %e %e\n", exp(lf), exp(fit)*1.0e-40, exp(mdata[i])*1.0e-40);
        }
      fclose(Xfile);

    for(i=imin; i <= imax; i++)
       {
         f = (double)(i)/T;
         lf = log(f);
         j = (long)floor((lf-lfmin)/dlf);
	 fit = pcx[j]+((pcx[j+1]-pcx[j])/dlf)*(lf-(lfmin+(double)(j)*dlf));
         instrument_noise(f, &SAE, &SXYZ);
         alpha = exp(fit)*1.0e-40;
         conf = alpha -SAE;
         if(conf < SAE/30.0) conf = 1.0e-46;
         AEnoise[i] = alpha;
         AEconf[i] = conf;
       }

    free_dvector(XX, 0,100);
    free_dvector(fdata, 0,segs-1);
    free_dvector(mdata, 0,segs-1);
    free_dvector(pcx, 0,Npoly);
    free_dvector(pcy, 0,Npoly);

  return;
}






void startX(long imin, long imax, int Nfit, double *XP, double *Xfit, double *fpoints)
{
  double f, logf;
  double XC, SAE, SXYZ;
  double chi, scale;
  long i, j, k;
  double av;

     for(i=2; i< Nfit; i++)
       {
	 f = exp(fpoints[i]);
         //instrument_noise(f, &SAE, &SXYZ);
	 k = (long)(f*T);
	 av = 0.0;
        for(j=k-50; j<=k+50; j++)
         {
	   av += log(XP[j]);
         }
	av /= 101.0;

	Xfit[i] = av;

       }

  return;
}

double chiX(long imin, long imax, int Nfit, double *XP, double *Xfit, double *fpoints)
{
  double f, logf;
  double XC, SAE, SXYZ;
  double chi, scale;
  long i;

  scale = 1.0/((double)(imax)-(double)(imin));

     for(i=imin; i< imax; i++)
       {
         f = (double)(i)/T;
         instrument_noise(f, &SAE, &SXYZ);
         XC = Xconf(f,Nfit,Xfit,fpoints);
	 chi += scale*(XP[i]-SXYZ-XC)*(XP[i]-SXYZ-XC)/(2.0*SXYZ*SXYZ);
       }

  return chi;
}

double Xconf(double f, int Nfit, double *Xfit, double *fpoints)
{
  double logf;
  int i,j,k;
  double XC;

  logf = log(f);

  k = 1;
  while(fpoints[k] < logf)
    {
      k++;
    }
  k -= 1;

  XC = exp(Xfit[k] + (Xfit[k+1]-Xfit[k])/(fpoints[k+1]-fpoints[k])*(logf-fpoints[k]));

  return XC;


}

void instrument_noise(double f, double *SAE, double *SXYZ)
{
  //Power spectral density of the detector noise and transfer frequency
  double Sn, red, confusion_noise;
  double f1, f2;
  double A1, A2, slope;
  FILE *outfile;
  
  red = (2.0*pi*1.0e-4)*(2.0*pi*1.0e-4);


  // Calculate the power spectral density of the detector noise at the given frequency

  *SAE = (16.0/3.0*pow(sin(f/fstar),2.0)*( ( (2.0+cos(f/fstar))*Sps + 2.0*(3.0+2.0*cos(f/fstar)+cos(2.0*f/fstar))*Sacc*(1.0/pow(2.0*pi*f,4)+ red/pow(2.0*pi*f,6))) / pow(2.0*L,2.0)));

  *SXYZ = (4.0*pow(sin(f/fstar),2.0)*( ( 4.0*Sps + 8.0*(1.0+pow(cos(f/fstar),2.0))*Sacc*(1.0/pow(2.0*pi*f,4)+ red/pow(2.0*pi*f,6))) / pow(2.0*L,2.0)));
	
}

	


void KILL(char* Message)
{
  printf("\a\n");
  printf("%s",Message);
  printf("Terminating the program.\n\n\n");
  exit(1);

 
  return;
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

#define SWAP(a,b) temp=(a);(a)=(b);(b)=temp;

// for n odd, median given by k = (n+1)/2. for n even, median is average of k=n/2, k=n/2+1 elements

double quickselect(double *arr, int n, int k)
 {
  unsigned long i,ir,j,l,mid;
  double a,temp;

  l=0;
  ir=n-1;
  for(;;) {
    if (ir <= l+1) { 
      if (ir == l+1 && arr[ir] < arr[l]) {
	SWAP(arr[l],arr[ir]);
      }
      return arr[k];
    }
    else {
      mid=(l+ir) >> 1; 
      SWAP(arr[mid],arr[l+1]);
      if (arr[l] > arr[ir]) {
	SWAP(arr[l],arr[ir]);
      }
      if (arr[l+1] > arr[ir]) {
	SWAP(arr[l+1],arr[ir]);
      }
      if (arr[l] > arr[l+1]) {
	SWAP(arr[l],arr[l+1]);
      }
      i=l+1; 
      j=ir;
      a=arr[l+1]; 
      for (;;) { 
	do i++; while (arr[i] < a); 
	do j--; while (arr[j] > a); 
	if (j < i) break; 
	SWAP(arr[i],arr[j]);
      } 
      arr[l+1]=arr[j]; 
      arr[j]=a;
      if (j >= k) ir=j-1; 
      if (j <= k) l=i;
    }
  }
}
