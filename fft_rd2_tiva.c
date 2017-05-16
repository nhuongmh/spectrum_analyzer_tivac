/*
  fft_rd2_tiva.c
  fft function definition for tiva c
  refer to Rulph Chassing's code 
  re-writen by Minh Nhuong using DIT algorithm and run in tiva c
  date: Apr 23rd, 2017
*/
#include "fft_tiva.h"


void twiddle_array(int16_t fftlen, COMPLEX *twiddle)
{// tinh he so W cho fft W = e^(-j2pi/N)
  int16_t n;
  float expon;
  for(n=0; n<fftlen; n++)
  {
    expon = (float) (PI*n/fftlen) ;
    twiddle[n].real  =  cosf(expon) ;
    twiddle[n].imag = -sinf(expon) ;
  }
}

// FFT radix-2 Decimation-in-time
void fft(COMPLEX *Y, int16_t fftlen, COMPLEX *w)
{
  COMPLEX temp, temp1 ; //temp2 ;
  int i , j , k ;
  int upper_leg, lower_leg ;  //canh tren va canh xuong cua butterfly
  int leg_diff ;              			// ( nhin hinh tu hieu)
  int num_stages = 0 ;        // so tung thuc hien fft
  int index ;                 			// index la so mu cua W tuong ung.
  int step,stem   ;                //stem va step la 2 bien de tinh gia tri nap lai vong lap trong cung
  i = 1 ;
//============================================================================//
//tinh so tung cho fft. num_stages = log2(fftlen)
  do
  {
    num_stages ++ ;
    i *= 2 ;
  } while(i!=fftlen) ;
//============================================================================//
  leg_diff = fftlen/2;
  step  = fftlen ;
/*
//============================================================================//
//                  DIF FFT Algorithm implementation                          //
	  for (i=0;i<num_stages;i++)      //for M-point FFT                 
  {
    index=0;
    for (j=0;j<leg_diff;j++)
    {
      for (upper_leg=j;upper_leg<fftlen;upper_leg+=(2*leg_diff))
      {
        lower_leg			=		upper_leg+leg_diff;
				
        temp1.real		=	(Y[upper_leg]).real 	+ (Y[lower_leg]).real;
        temp1.imag	=	(Y[upper_leg]).imag 	+ (Y[lower_leg]).imag;
				
        temp2.real		=	(Y[upper_leg]).real 	- 	(Y[lower_leg]).real;
        temp2.imag	=	(Y[upper_leg]).imag 	- 	(Y[lower_leg]).imag;
				
        (Y[lower_leg]).real		=	temp2.real	*	(w[index]).real		-		temp2.imag	*	(w[index]).imag;
        (Y[lower_leg]).imag	=	temp2.real	*	(w[index]).imag	+	temp2.imag	*	(w[index]).real;
				
        (Y[upper_leg]).real		=	temp1.real;
        (Y[upper_leg]).imag	=	temp1.imag;
      }
      index+=step;
    }
    leg_diff=leg_diff/2;
    step*=2;
  }
	//============================================================================//
  j=0;
  for (i=1;i<(fftlen-1);i++)           //dao bit lai cho dung thu tu
  {
    k=fftlen/2;
    while (k<=j)
    {
      j=j-k;
      k=k/2;
    }
    j=j+k;
    if (i<j)
    {
      temp1.real=(Y[j]).real;
      temp1.imag=(Y[j]).imag;
      (Y[j]).real=(Y[i]).real;
      (Y[j]).imag=(Y[i]).imag;
      (Y[i]).real=temp1.real;
      (Y[i]).imag=temp1.imag;
    }
  }
  return;
}                       

     */
//============================================================================//
//                  DIT FFT Algorithm implementation                          //
	
  for(i=0;i<num_stages;i++)
  {
    index = 0 ; 
    for(j=0; j<fftlen; j+=(2*leg_diff))
    {
			stem = (j==0)?0:(stem+step);		// kho giai thich :))
      for(upper_leg=stem ; upper_leg<leg_diff ; upper_leg++)
      {

        //(a+ib)*(c+id) = (ac-bd) + i(ad+bc)
        lower_leg = upper_leg + leg_diff ;
				
        (Y[lower_leg]).real 		= (Y[lower_leg]).real*(w[index]).real - (Y[lower_leg]).imag*(w[index]).imag ;
        (Y[lower_leg]).imag 	= (Y[lower_leg]).real*(w[index]).imag + (Y[lower_leg]).imag*(w[index]).real ;
				
        temp.real = (Y[upper_leg]).real + (Y[lower_leg]).real ;
        temp.imag = (Y[upper_leg]).imag + (Y[lower_leg]).imag ;
				
        (Y[lower_leg]).real = (Y[upper_leg]).real - (Y[lower_leg]).real ;
        (Y[lower_leg]).imag = (Y[upper_leg]).imag - (Y[lower_leg]).imag ;
				
        (Y[upper_leg]).real = temp.real ;
        (Y[upper_leg]).imag = temp.imag ;
				
      }
      index += leg_diff ;		// cho nay phai nghien cuu flowchart cua fft moi hieu duoc
    }
    leg_diff = leg_diff/2 ;
		step = step/2;

  }
	  j=0;
  for (i=1;i<(fftlen-1);i++)           //bit reversal for resequencing data
  {
    k=fftlen/2;
    while (k<=j)
    {
      j=j-k;
      k=k/2;
    }
    j=j+k;
    if (i<j)
    {
      temp1.real=(Y[j]).real;
      temp1.imag=(Y[j]).imag;
      (Y[j]).real=(Y[i]).real;
      (Y[j]).imag=(Y[i]).imag;
      (Y[i]).real=temp1.real;
      (Y[i]).imag=temp1.imag;
    }
  }
//============================================================================//
  return ;
}
