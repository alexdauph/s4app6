void __ISR ( _ADC_VECTOR, IPL1AUTO ) ADC_1 (void)
{
   int x, y, nSOS;
    
    // Read A/D input (ALWAYS, otherwise program hangs)
    x = ADC1BUF0;

    // Copy the input sample to the current input buffer
    currentInBuffer[bufferCount] = x;

    // If IIR filtering is enabled, real-time calculation of the next output sample
    if (IIREnabled) {
        y = x;
         for (nSOS = 0; nSOS < N_SOS_SECTIONS; nSOS++) {
            // *** POINT C1
            
			// y[n] = 
			
			// v[n] = 
			
			// u[n] = 
            
            // Update the input for the next SOS section
            x = y;
        }

        // Copy the resulting filtered sample to the current output buffer
        currentOutBuffer[bufferCount] = y;
    }

    // Write next output sample from the current output buffer to the PWM 
    // NOTE: The PWM duty cycle is programmed from 0 to 100% in units
	//       of "peripheral clock (PBCLK) ticks" contained within one PWM period,
    //       where PR2 is the number of 48 MHz PBCLK clock ticks in one PWM period,
	//       i.e the "resolution" of the PWM amplitude. The audio PWM is implemented with OC1,
	//       programmed here to be driven by TMR2 (f = 142,9 kHz, 7 us period).
    //       Therefore, PR2 = 48 MHz / 142,9 kHz = 336, so the PWM resolution
	//       is slightly better than 8 bits (2^8 = 256). Since the output sample
	//       is a signed integer in the range [-512, 511], it must be offset
	//       by +512 (ADC1_DYN_RANGE_HALF) and scaled by PR2/1024 to the PWM duty cycle
	//       dynamic range [0, PR2], where the division by 1024 is implemented
	//       with a right-shift (>> ADC1_NBITS)
    OC1_PWMPulseWidthSet(((currentOutBuffer[bufferCount] + ADC1_DYN_RANGE_HALF) * PR2_Global) >> ADC1_NBITS);

    // If end of buffers reached, flag the main while() loop to begin processing, reset sample counter, and pulse BIN2 (DEBUG)
    if (++bufferCount >= SIG_LEN) {
        inputBufferFull = true;
        bufferCount = 0;
        BIN2(1);
        DelayAprox10Us(5);
        BIN2(0);
    }
    
    // clear ADC interrupt flag
    IFS0CLR= 1 << _IFS0_AD1IF_POSITION;
}

