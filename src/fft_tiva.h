#ifndef	__FFT_TIVA_H__
#define	__FFT_TIVA_H__
#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include "driverlib/fpu.h"


#define N   												128             // N-point FFT
#define FS  												20000           // tan so lay mau
#define TEST_FREQ1 						5000
#define TEST_FREQ2 						2500
#define PI    											3.14159265358979
#define M													16 		// size of central buffer

typedef struct
{
  float real;
  float imag;
}     COMPLEX;

	

//uint8_t ProcessdResult[N] ;  // need to edit if solution to plot float type is found
																	//need to edit, do not use this . copy directly result from ProcessingArray to BuffA/B

void gpio_config(void);
void twiddle_array(int16_t fftlen, COMPLEX *twiddle);
void fft(COMPLEX *Y, int16_t fftlen, COMPLEX *w);


#endif
