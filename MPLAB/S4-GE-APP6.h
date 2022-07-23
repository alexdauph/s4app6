#ifndef _APP6_H    // Guard against multiple inclusion 
#define _APP6_H

// Defines for overlap-and-save filtering: FFT_LEN (4N) must equal SIG_LEN (3N) + H_LEN (1N)), where N = 256 in this case. 
#define FFT_LEN         1024
#define SIG_LEN          768
#define H_LEN            256

// Defines for DSP library FFT function call 
#define fftc       (int32c *) fft32c1024 // See fftc.h for defintion of fft32c1024
#define LOG2FFTLEN                    10 // To initialize log2N, where 2^log2N = FFT_LEN = 1024 in this case

// ADC1 parameters
#define ADC1_NBITS            10
#define ADC1_DYN_RANGE_HALF  512
#define ADC1_DYN_RANGE_FULL 1024

// Macros to address BIN1 and BIN2 pins on MX3 Analog Discovery 2 connector
#define BIN1(a);    {if(a) LATEbits.LATE9 = 1; else LATEbits.LATE9 = 0;}
#define BIN2(a);    {if(a) LATBbits.LATB5 = 1; else LATBbits.LATB5 = 0;}

// Clocks
#define PeripheralClockFrequency (48000000UL)

// Double buffering variables and other globals
extern int32_t *currentInBuffer, *currentOutBuffer, bufferCount, PR2_Global;
extern bool inputBufferFull, IIREnabled;

#endif /* _APP6_H */
