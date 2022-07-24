
/**
  ADC1 Generated Driver File

  @Company
    Microchip Technology Inc.

  @File Name
    adc1.c

  @Summary
    This is the generated header file for the ADC1 driver using PIC32MX MCUs

  @Description
    This header file provides APIs for driver for ADC1.
    Generation Information :
        Product Revision  :  PIC32MX MCUs - pic32mx : v1.35
        Device            :  PIC32MX370F512L
        Driver Version    :  0.5
    The generated drivers are tested against the following:
        Compiler          :  XC32 1.42
        MPLAB 	          :  MPLAB X 3.55
*/

/*
    (c) 2016 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
    WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

    MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
    TERMS.
*/

/**
  Section: Included Files
*/

#include <xc.h>
#include "adc1.h"
#include "S4-GE-APP6.h"
#include "filterIIRcoeffs.h"
#include "utils.h"

/**
  Section: Driver Interface
*/

void ADC1_Initialize(void)
{
    // ASAM enabled; DONE disabled; CLRASAM disabled; FORM Signed Integer 32-bit; SAMP disabled; SSRC TMR3; SIDL disabled; ON enabled;
    AD1CON1 = 0x8544;

    // CSCNA enabled; ALTS disabled; BUFM disabled; SMPI 1; OFFCAL disabled; VCFG AVDD/AVSS;
    AD1CON2 = 0x400;

    // SAMC 0; ADRC PBCLK; ADCS 3;
    AD1CON3 = 0x3;

    // CH0SA AN0; CH0SB AN0; CH0NB Vrefl; CH0NA Vrefl;
    AD1CHS = 0x0;

    // CSSL26 disabled; CSSL25 disabled; CSSL28 disabled; CSSL27 disabled; CSSL22 disabled; CSSL21 disabled; CSSL24 disabled; CSSL9 disabled; CSSL23 disabled; CSSL20 disabled; CSSL0 disabled; CSSL8 disabled; CSSL7 disabled; CSSL6 disabled; CSSL5 disabled; CSSL4 disabled; CSSL3 disabled; CSSL29 disabled; CSSL2 disabled; CSSL1 disabled; CSSL15 disabled; CSSL14 disabled; CSSL17 enabled; CSSL16 disabled; CSSL11 disabled; CSSL10 disabled; CSSL13 disabled; CSSL12 disabled; CSSL30 disabled; CSSL19 disabled; CSSL18 disabled;
    AD1CSSL = 0x20000;

    // Enabling ADC1 interrupt.
    IEC0bits.AD1IE = 1;
}

void ADC1_Start(void)
{
    AD1CON1bits.SAMP = 1;
}

void ADC1_Stop(void)
{
    AD1CON1bits.SAMP = 0;
}

void ADC1_ConversionResultBufferGet(uint32_t *buffer)
{
    int count;
    uint32_t *ADC32Ptr;

    ADC32Ptr = (uint32_t *)&(ADC1BUF0);

    for (count = 0; count <= 1; count++)
    {
        buffer[count] = (uint32_t)*ADC32Ptr;
        ADC32Ptr = ADC32Ptr + 4;
    }
}

uint32_t ADC1_ConversionResultGet(void)
{
    return ADC1BUF0;
}

bool ADC1_IsConversionComplete(void)
{
    return AD1CON1bits.DONE; // Wait for conversion to complete
}

void ADC1_ChannelSelect(ADC1_CHANNEL channel)
{
    AD1CHS = channel << 16;
}

void __ISR(_ADC_VECTOR, IPL1AUTO) ADC_1(void)
{
    int x, y, nSOS;

    // Read A/D input (ALWAYS, otherwise program hangs)
    x = ADC1BUF0;

    // Copy the input sample to the current input buffer
    currentInBuffer[bufferCount] = x;

    // If IIR filtering is enabled, real-time calculation of the next output sample
    if (IIREnabled)
    {
        y = x;
        for (nSOS = 0; nSOS < N_SOS_SECTIONS; nSOS++)
        {
            // *** POINT C1

            y = (IIRv[nSOS] + IIRCoeffs[nSOS][0] * x) >> 13;

            IIRv[nSOS] = (IIRCoeffs[nSOS][1] * x) - (IIRCoeffs[nSOS][4] * y) + IIRu[nSOS];

            IIRu[nSOS] = (IIRCoeffs[nSOS][2] * x) - (IIRCoeffs[nSOS][5] * y);

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
    if (++bufferCount >= SIG_LEN)
    {
        inputBufferFull = true;
        bufferCount = 0;
        BIN2(1);
        DelayAprox10Us(5);
        BIN2(0);
    }

    // clear ADC interrupt flag
    IFS0CLR = 1 << _IFS0_AD1IF_POSITION;
}

/**
  End of File
*/
