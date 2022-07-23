/*
 S4GE-APP6
 
 Author: Paul Charette
 
 Last modified: 09/08/2021
 
 Copyright : Université de Sherbrooke

 Summary:
    Time-domain IIR and frequency-domain FIR filtering of audio signal
   
 GitHub repository: https://github.com/pgcharetteUdeS/S4GE-APP6-MPLAB
 
 Notes:
    - FIR filtering : filter transfer function coefficients in filterFIRcoeffs.h
    - IRR filtering : filter coefficients in filterIIRcoeffs.h
 
 Controls (DIP switches):
    - SW0 : Pass-through
    - SW1 : Calculate frequency with strongest amplitude
    - SW2 : IIR filter (bandstop, fc = 1 kHz)
    - SW3 : FIR filter H3 (high-pass, fc = 4490 Hz)
    - SW4 ; FIR filter H4 (bandpass, [2500, 4500} Hz)
    - SW5 : FIR filter H5 (bandpass, [1500, 2500] Hz)
    - SW6 : FIR filter H6 (bandpass, [500 Hz, 1500] Hz)
    - SW7 : FIR filter H7 (low-pass, fc = 500 Hz)
 
 Main variables:
   - inFFT[FFT_LEN], outFFT[FFT_LEN], Scratch[FFT_LEN]
        32 bit COMPLEX integer buffers for use with PIC32 DSP library FFT function calls
        in frequency domain FIR filtering computations (overlap & save method).
   - Htot[FFT_LEN]
        32 bit COMPLEX integer accumulator for the summation of the 5 FIR filter transfer functions
        used in the frequency domain FIR filtering.
   - inBuffer1[SIG_LEN], inBuffer2[SIG_LEN], outBuffer1[SIG_LEN], outBuffer2[SIG_LEN]:
        32 bit integer buffer pairs for input/output double-buffering.
        While one input/output buffer pair of length SIG_LEN is being filled with THREE blocks of data
		by the ADC1 interrupt handler (each block is of length H_LEN=256, SIG_LEN = 3*H_LEN),
		the other buffer pair is being processed in the main program by the overlap & save method
		for FIR filtering on FOUR blocks (FFT_LEN = SIG_LEN+H_LEN = 4*H_LEN)
        using the inFFT, outFFT, and Scratch buffers.
    - previousInBuffer, previousOutBuffer, currentInBuffer, currentOutBuffer
        Pointer pairs that designate which of the input/output buffer pairs above are currently being
        filled by the ADC1 interrupt handler versus the other pair (previous 3 blocks of data)
		being used for FIR filtering with the overlap & save method in the main program.
        When the ADC1 interrupt handler flags a buffer as full (inputBufferFull = true),
        the pointers are swapped in the main program and FIR processing begins.
    - debugBuffer1[FFT_LEN], debugBuffer2[FFT_LEN]: 32 bit integer buffers for general usage.
    - Fe: sampling frequency (20 kHz)

 */
#include "stdio.h"
#include "math.h"

// MCC-generated configuration include files
#include "mcc_generated_files/mcc.h"

// Main program include file
#include "S4-GE-APP6.h"

// PIC32 DSP library include files
#include "fftc.h"

// MX3 LibPack include files
#include "utils.h"
#include "lcd.h"
#include "swt.h"
#include "btn.h"
#include "ssd.h"

// Window and FIR filter transfer functions, scaled by 2^H_QXY_RES_NBITS for fixed-point encoding
#include "filterFIRcoeffs.h"
#include "window.h"

// Global variables
int32_t *currentInBuffer, *currentOutBuffer, bufferCount, PR2_Global;
bool inputBufferFull, IIREnabled;

// Though this is BAD PROGRAMMING PRACTICE, define these arrays as
// static (private) global variables for the MPLAB DMCI plug-in to see them
static int32_t inBuffer1[SIG_LEN], inBuffer2[SIG_LEN], outBuffer1[SIG_LEN], outBuffer2[SIG_LEN],
        debugBuffer1[FFT_LEN], debugBuffer2[FFT_LEN], Fe;
static int32c inFFT[FFT_LEN], outFFT[FFT_LEN], Htot[FFT_LEN], twiddles[FFT_LEN / 2];

// Local function prototyping
void calc_power_spectrum(int32c *, int32_t *, int);
void buildH(const int32c *, const int32c *, const int32c *, const int32c *, const int32c *, int32c *, int);
void numberInto4DigitString(int, char *);
bool switchStateChanged(bool *, bool *, bool *, bool *, bool *, bool *, bool *, bool *);
unsigned char SWT_GetValue_Local(unsigned char);

int main(void) {
    // Static declarations allow variables to be visible in debugger at all times
    static double spectralResolution=1;
    static int32_t *previousInBuffer, *previousOutBuffer;
    static int maxN, maxVal, maxAmplFreq;
    bool SW7StateChange, SW6StateChange, SW5StateChange, SW4StateChange, SW3StateChange,
            SW2StateChange, SW1StateChange, SW0StateChange, switchStateChange;
    int32_t n, m, k, log2N = LOG2FFTLEN, theAnswerTofLifeTheUniverseAndEverything, niters=0;
    int32c Scratch[FFT_LEN];
    char LCDBuf[256], freqDigits[4], clrString[] = "                ";

    // Double buffer pointer logic initializations
    bufferCount = 0;
    currentInBuffer = inBuffer1;
    currentOutBuffer = outBuffer1;
    inputBufferFull = false;

    // PIC32 hardware initializations
    SYSTEM_Initialize();
    BMXCONbits.BMXWSDRM = 0; // Data Memory SRAM wait states: Default Setting = 1, set it to 0
    CHECONbits.PFMWS = 3;    // Flash PM Wait States: MX Flash runs at 3 wait states @ 96 MHz
    CHECONbits.PREFEN = 2;   // Enable prefetch for cacheable PFM instructions

    // PR2 (PWM dynamic range) value is unreliable when read in interrupt handlers
    // for some reason, store the correct value in a global variable
    PR2_Global = PR2;

    // Calculate sampling frequency
    Fe = PeripheralClockFrequency / PR3;

    // Calculate spectral resolution, use (double) type casting for parameters
    // *** POINT A1: spectralResolution =...

    // MX3 peripherals hardware initializations
    BTN_Init();
    LCD_Init();
    SSD_Init();
    SSD_WriteDigits(-1, -1, -1, -1, 0, 0, 0, 0);
    SWT_Init();
    DelayAprox10Us(1000);

    // Show sampling frequency on LCD
    sprintf(LCDBuf, "Fe = %d Hz", Fe);
    LCD_WriteStringAtPos(LCDBuf, 0, 0);
    LCD_WriteStringAtPos("BTND to start...", 1, 0);

    // Timer starts (TMR2: PWM, TMR3: ADC)
    TMR2_Start();
    TMR3_Start();

    // Wait for user to press BTND to start
    while (BTN_GetValue('D') == 0);

    // DEBUG: copy twiddles to local buffer for viewing with DMCI
    for (k = 0; k < FFT_LEN / 2; k++) {
        twiddles[k] = fft32c1024[k];
    }

    // Main infinite loop that waits for the ADC1 interrupt handler to signal that
    // the input buffer is full, at which point the data is processed according
    // to the MX3 DIP switch settings: PASS-THROUGH, SPECTRUM ESTIMATION,
    // REAL-TIME IIR FILTERING, or BUFFERED FIR FILTERING.
    while (1) {
        if (inputBufferFull) {
            // DEBUG: Raise pin BIN1 for oscilloscope timing measurements
            BIN1(1);

            // Swap input/output buffer pointers
            if (currentInBuffer == inBuffer1) {
                currentInBuffer = inBuffer2;
                currentOutBuffer = outBuffer2;
                previousInBuffer = inBuffer1;
                previousOutBuffer = outBuffer1;
            } else {
                currentInBuffer = inBuffer1;
                currentOutBuffer = outBuffer1;
                previousInBuffer = inBuffer2;
                previousOutBuffer = outBuffer2;
            }

            // DEBUG : Useful breakpoint location to check contents of input/output buffers
            // with DMCI for FIR filtering. The "niters" counter logic ensures that
            // at least two (three in this case) buffers are acquired sequentially without interruption
            // as required for the Overlap & Save method to work properly (no glitches)
            if (++niters == 4)
                niters = 0; // (DEBUG C) 

            // Read switch states: if any switch has changed state, raise switchStateChangeflag
            switchStateChange = switchStateChanged(&SW7StateChange, &SW6StateChange, &SW5StateChange,
                    &SW4StateChange, &SW3StateChange, &SW2StateChange, &SW1StateChange, &SW0StateChange);

            // If switch state change, initialize main decision tree state: disable IIR filtering,
            // clear LCD and 7-segment display
            if (switchStateChange) {
                IIREnabled = false;
                SSD_WriteDigits(-1, -1, -1, -1, 0, 0, 0, 0);
                LCD_WriteStringAtPos(clrString, 0, 0);
                LCD_WriteStringAtPos(clrString, 1, 0);
            }

            // 
            // MAIN DECISION TREE: : PASS-THROUGH, SPECTRUM ESTIMATION, REAL-TIME IIR FILTERING, or BUFFERED FIR FILTERING
            //
            if (SWT_GetValue_Local(0) == 0) { // MAIN DECISION TREE: PASS-THROUGH
                // Copy input samples straight to output buffer (pass-through)
                for (n = 0; n < SIG_LEN; n++)
                    previousOutBuffer[n] = previousInBuffer[n];

                // If required, change to display pass-through functionality message on LCD
                if (switchStateChange)
                    LCD_WriteStringAtPos("PASS-THROUGH... ", 0, 0);

            } else if (SWT_GetValue_Local(1) == 0) { // MAIN DECISION TREE: SPECTRUM ESTIMATION
                // Copy input samples straight to output buffer (same as for pass-through)
                for (n = 0; n < SIG_LEN; n++)
                    previousOutBuffer[n] = previousInBuffer[n];

                // Window input samples, x[n], zero-pad out to FFT_LEN (next power of 2 after SIG_LEN)
                //   1) Input samples, x[n], are multiplied by the window function, w[n]
                //   2) Multiplication of FFT input vector by N (N = FFT_LEN) is required
                //      BEFORE the FFT function call because of the built-in division by N
                //      in the PIC32 DSP Library implementation of the FFT algorithm
                //      (See DS51685E, p.118), else roundoff error decreases resolution of X[k]
                //   3) Because the window function is scaled by 2^H_and_W_QXY_RES_NBITS,
                //      overflow may occur because of the required pre-multiplication by N of
                //      the FFT input vector, x[n]*w[n]*N. Therefore, the x[n]*w[n]
                //      should be first divided by 2^H_and_W_QXY_RES_NBITS then scaled by N, but this
                //      would result in loss of precision due to integer arithmetic (division
                //      followed by multiplication). Since both 2^H_and_W_QXY_RES_NBITS and N are powers
                //      of two, the division/multiplication operations can be combined without
                //      loss of resolution with an arithmetic shift by the difference of bits between
                //      the two: ">> (H_and_W_QXY_RES_NBITS - LOG2FFTLEN"
                for (n = 0; n < SIG_LEN; n++) {
                    inFFT[n].re = (previousInBuffer[n] * window[n]) >> (H_and_W_QXY_RES_NBITS - LOG2FFTLEN);
                    inFFT[n].im = 0;
                }
                for (; n < FFT_LEN; n++) {
                    inFFT[n].re = 0;
                    inFFT[n].im = 0;
                }
                
                // *** POINT A2: calculate frequency spectrum components X[k] with PIC32 DSP Library FFT function call

                // Calculate power spectrum
                calc_power_spectrum(outFFT, debugBuffer1, FFT_LEN);

                // Find index of frequency with highest power (positive frequency spectrum only)
                maxVal = -1;
                for (k = 1; k < (FFT_LEN / 2) - 1; k++) {
                    if (debugBuffer1[k] > maxVal) {
                        maxVal = debugBuffer1[k];
                        maxN = k;
                    }
                }
                
                // Calculate value in Hz of frequency with highest power 
                // *** POINT A4: maxAmplFreq = ...

                // Show frequency with highest power on 7 segment display, max-out at 4 digits (9999)
                numberInto4DigitString(maxAmplFreq, freqDigits);
                SSD_WriteDigits(freqDigits[3], freqDigits[2], freqDigits[1], freqDigits[0], 0, 0, 0, 0);

                // If required, change to display spectrum estimation functionality message on LCD
                if (switchStateChange) {
                    LCD_WriteStringAtPos("SPECTRUM ESTM...", 0, 0);
                    sprintf(LCDBuf, "      df = %d Hz", (int) spectralResolution);
                    LCD_WriteStringAtPos(LCDBuf, 1, 0);
                }
                
            } else if (SWT_GetValue_Local(2) == 0) { // MAIN DECISION TREE: REAL-TIME IIR FILTERING
                // Enable IIR filtering in ADC1 interrupt handler
                IIREnabled = true;

                // If required, change to display IIR filtering functionality message on LCD
                if (switchStateChange) {
                    LCD_WriteStringAtPos("REAL-TIME IIR...", 0, 0);
                }
                
            } else { // MAIN DECISION TREE: BUFFERED FIR FILTERING ("Overlap-and-save" method, see Lyons, p. 719)
                // If any of the switches have changed state, re-build FIR filter cumulative
                //  transfer function, Htot, according to the current SW7-SW3 settings
                if (switchStateChange) {
                    buildH(H7, H6, H5, H4, H3, Htot, FFT_LEN);
                }

                // *** POINT B0: Load inFFT buffer with FFT_LEN samples (4 blocks)
                //     of x[n] for FFT calculation (explicitely set imaginary components to 0):
                //       1) Last H_LEN samples (1 block) of x[n] from the previous input buffer pointed
                //          to by "currentInBuffer". Though this buffer is currently being filled
                //          by the A/D interrupt handler, the last H_LEN samples (last block)
                //          from the previous group of 3 blocks are still available.
                //       2) All SIG_LEN samples of x[n] from the most recently filled input buffer
                //          (3 blocks) pointed to by "previousInBuffer".
                //     IMPORTANT: Pre-multiplication of x[n] by N (FFT_LEN) is required because
                //                of the built-in division by N in the PIC32 DSP Library implementation
                //                of the FFT algorithm (See DS51685E, p.118), else roundoff error 
                //                decreases resolution of X[k] result.

                // *** POINT B1: Calculate X[k] with PIC32 DSP Library FFT function call

                // *** POINT B2: FIR Filtering, calculate Y* = (HX)*, where "*" is the complex conjugate
                // (instead of Y=HX, in preparation for inverse FFT using forward FFT library function call)

                // *** POINT B3: Inverse FFT by forward FFT library function call, no need to divide by N

                // *** POINT B4: Extract real part of the inverse FFT result and remove H QX.Y scaling,
				// discard first block as per the "Overlap-and-save" method.

                // If required, update LCD display with SW7-SW3 switch states
                if (switchStateChange) {
                    sprintf(LCDBuf, "H7:%s H6:%s H5:%s", (SWT_GetValue_Local(7) ? "+" : "-"),
                            (SWT_GetValue_Local(6) ? "+" : "-"), (SWT_GetValue_Local(5) ? "+" : "-"));
                    LCD_WriteStringAtPos(LCDBuf, 0, 0);
                    sprintf(LCDBuf, "H4:%s H3:%s", (SWT_GetValue_Local(4) ? "+" : "-"),
                            (SWT_GetValue_Local(3) ? "+" : "-"));
                    LCD_WriteStringAtPos(LCDBuf, 1, 0);
                }

            } // END MAIN DECISION TREE

            // Reset the double-buffer swap flag to wait for next input buffer full... 
            inputBufferFull = false;

            // Lower pin BIN1, oscilloscope timing measurements
            BIN1(0);
        } // if (inputBufferFull)
    } // while(1))

    return -1;
}


//
// Calculation of power spectrum in dB: 10log(|X[k]|^2)
//
// Function parameters:
//      - inbuf (*int32c, INPUT):     pointer to array of length FFT_LEN
//                                    containing complex spectrum, X[k].
//      - outbuf (*int32_t, OUTPUT):  pointer to array of length FFT_LEN
//                                    for storage of power spectrum.
//
// NOTES:
// 1) Because the multiplication of large X[k] real and imaginary components
//    will overflow the dynamic range for signed 32 bit integers, calculations
//    MUST be carried out in double-precision floating point using intermediate
//    variables. However, the final result (logarithm of |X[k]|^2) will not
//    overflow the output buffer dynamic range (signed 32 bit integers).
// 2) To avoid the risk of RAM overflow and ensure the function call is as
//    "resource-light" as possible, do not define new arrays or tables inside
//    the function (defining non-array variables, e.g. int, double,... is ok).
// 3) To avoid a NAN result in case of log10(0), i.e. when X[k] = 0,
//    add "1" to the log10() operand: log10(blah + 1).
//

void calc_power_spectrum(int32c *inbuf, int32_t *outbuf, int n) {
    double re, im;

    // *** POINT A3: Complete the calc_power_spectrum() function
}


//
// DIP switch interface wrapper
//

unsigned char SWT_GetValue_Local(unsigned char n) {
    if (n < 0)
        n = 0;
    else if (n > 7)
        n = 7;
    return (SWT_GetValue(n));
}


//
// Load "maxAmplFreq" value into a four digit character string, max out at 9999
//

void numberInto4DigitString(int maxAmplFreq, char *freqDigits) {
    if (maxAmplFreq < 9999) {
        freqDigits[0] = maxAmplFreq / 1000;
        freqDigits[1] = (maxAmplFreq - freqDigits[0]*1000) / 100;
        freqDigits[2] = (maxAmplFreq - freqDigits[0]*1000 - freqDigits[1]*100) / 10;
        freqDigits[3] = maxAmplFreq - freqDigits[0]*1000 - freqDigits[1]*100 - freqDigits[2]*10;
    } else {
        freqDigits[0] = 9;
        freqDigits[1] = 9;
        freqDigits[2] = 9;
        freqDigits[3] = 9;
    }
}


//
// Build cumulative FIR transfer function, H, by summing the transfer functions
// from the five FIR filters (H7-H3) based on the state of switches SW7-SW3.
// NOTE: the overlapping transfer functions from the five filters are designed such that,
//       when summed, the total gain at any frequency never exceeds 1
//

void buildH(const int32c *H7p, const int32c *H6p, const int32c *H5p, const int32c *H4p, const int32c *H3p, int32c *Hp, int N) {
    int n;

    // Clear H summation buffer
    for (n = 0; n < N; n++) {
        Hp[n].re = 0;
        Hp[n].im = 0;
    }

    // Add H functions for each filter according to its switch setting
    if (SWT_GetValue_Local(7)) {
        for (n = 0; n < N; n++) {
            Hp[n].re += H7p[n].re;
            Hp[n].im += H7p[n].im;
        }
    }
    if (SWT_GetValue_Local(6)) {
        for (n = 0; n < N; n++) {
            Hp[n].re += H6p[n].re;
            Hp[n].im += H6p[n].im;
        }
    }
    if (SWT_GetValue_Local(5)) {
        for (n = 0; n < N; n++) {
            Hp[n].re += H5p[n].re;
            Hp[n].im += H5p[n].im;
        }
    }
    if (SWT_GetValue_Local(4)) {
        for (n = 0; n < N; n++) {
            Hp[n].re += H4p[n].re;
            Hp[n].im += H4p[n].im;
        }
    }
    if (SWT_GetValue_Local(3)) {
        for (n = 0; n < N; n++) {
            Hp[n].re += H3p[n].re;
            Hp[n].im += H3p[n].im;
        }
    }
}

//
// Read the current state of switches SW7-SW0, return true if any switch was changed from the previous state
//

bool switchStateChanged(bool *SW7StateChange, bool *SW6StateChange, bool *SW5StateChange,
        bool *SW4StateChange, bool *SW3StateChange, bool *SW2StateChange, bool *SW1StateChange, bool * SW0StateChange) {
    static bool coldStart = true, SW7PreviousState, SW6PreviousState, SW5PreviousState,
            SW4PreviousState, SW3PreviousState, SW2PreviousState, SW1PreviousState, SW0PreviousState;

    if (coldStart) {
        SW7PreviousState = SWT_GetValue_Local(7);
        SW6PreviousState = SWT_GetValue_Local(6);
        SW5PreviousState = SWT_GetValue_Local(5);
        SW4PreviousState = SWT_GetValue_Local(4);
        SW3PreviousState = SWT_GetValue_Local(3);
        SW2PreviousState = SWT_GetValue_Local(2);
        SW1PreviousState = SWT_GetValue_Local(1);
        SW0PreviousState = SWT_GetValue_Local(9);
        *SW7StateChange = true;
        *SW6StateChange = true;
        *SW5StateChange = true;
        *SW4StateChange = true;
        *SW3StateChange = true;
        *SW2StateChange = true;
        *SW1StateChange = true;
        *SW0StateChange = true;
        coldStart = false;
        return (true);

    } else {
        // Check state of SW7
        *SW7StateChange = false;
        if (SWT_GetValue_Local(7)) {
            if (SW7PreviousState == false) {
                SW7PreviousState = true;
                *SW7StateChange = true;
            }
        } else {
            if (SW7PreviousState == true) {
                SW7PreviousState = false;
                *SW7StateChange = true;
            }
        }

        // Check state of SW6
        *SW6StateChange = false;
        if (SWT_GetValue_Local(6)) {
            if (SW6PreviousState == false) {
                SW6PreviousState = true;
                *SW6StateChange = true;
            }
        } else {
            if (SW6PreviousState == true) {
                SW6PreviousState = false;
                *SW6StateChange = true;
            }
        }

        // Check state of SW5
        *SW5StateChange = false;
        if (SWT_GetValue_Local(5)) {
            if (SW5PreviousState == false) {
                SW5PreviousState = true;
                *SW5StateChange = true;
            }
        } else {
            if (SW5PreviousState == true) {
                SW5PreviousState = false;
                *SW5StateChange = true;
            }
        }

        // Check state of SW4
        *SW4StateChange = false;
        if (SWT_GetValue_Local(4)) {
            if (SW4PreviousState == false) {
                SW4PreviousState = true;
                *SW4StateChange = true;
            }
        } else {
            if (SW4PreviousState == true) {
                SW4PreviousState = false;
                *SW4StateChange = true;
            }
        }

        // Check state of SW3
        *SW3StateChange = false;
        if (SWT_GetValue_Local(3)) {
            if (SW3PreviousState == false) {
                SW3PreviousState = true;
                *SW3StateChange = true;
            }
        } else {
            if (SW3PreviousState == true) {
                SW3PreviousState = false;
                *SW3StateChange = true;
            }
        }

        // Check state of SW2
        *SW2StateChange = false;
        if (SWT_GetValue_Local(2)) {
            if (SW2PreviousState == false) {
                SW2PreviousState = true;
                *SW2StateChange = true;
            }
        } else {
            if (SW2PreviousState == true) {
                SW2PreviousState = false;
                *SW2StateChange = true;
            }
        }

        // Check state of SW1
        *SW1StateChange = false;
        if (SWT_GetValue_Local(1)) {
            if (SW1PreviousState == false) {
                SW1PreviousState = true;
                *SW1StateChange = true;
            }
        } else {
            if (SW1PreviousState == true) {
                SW1PreviousState = false;
                *SW1StateChange = true;
            }
        }

        // Check state of SW0
        *SW0StateChange = false;
        if (SWT_GetValue_Local(0)) {
            if (SW0PreviousState == false) {
                SW0PreviousState = true;
                *SW0StateChange = true;
            }
        } else {
            if (SW0PreviousState == true) {
                SW0PreviousState = false;
                *SW0StateChange = true;
            }
        }

        // Return true if any switch has changed state
        return (*SW7StateChange || *SW6StateChange || *SW5StateChange || *SW4StateChange ||
                *SW3StateChange || *SW2StateChange || *SW1StateChange || *SW0StateChange);
    }
}
/**
 End of File
 */