/*___________________________________________________________________________
|
|   File: tsm48k.h
|
|   Time Scale Modification
|
|   This software is the property of Comcast Cable
|
|   Unauthorized use is prohibited.
|
|   Copyright (c) 2022, Comcast Cable
|
|
|______________________________________________________________________________
*/

#ifndef _TSM48K_H
#define _TSM48K_H

#include <stdint.h>
#include "inttypes.h"

#if defined(WIN32) || defined(_WIN64)
#ifdef TSMCDLL_EXPORTS
#define TSM_API  __declspec(dllexport)
#else
#define TSM_API  __declspec(dllimport)
#endif
#else
#define TSM_API
#endif

/* The time scale modification algorithm a 16-bit input PCM stream, sampled at one sampling rate 
   to an output 16-bit PCM stream sampled at a different sampling rate.

   There are three straightforward API functions:

   TSM_COMCAST__create creates an instance of the sampling rate converter with an input and output sampling rate set
   for a specific pair of sampling rates
   TSM_COMCAST__filter performs the sampling rate conversion
   TSM_COMCAST__delete deletes an instance of sampling rate converter

   IMPORTANT NOTE - the number of output samples is not the same for each frame. Be sure to check the returned 
   number of output samples.
   

   There are a few additional functions that can be used to perform time scale modification - small changes in sampling rate that
   can be adjusted on the fly:

    TSM_COMCAST__changePlayoutSamplingRate - adjusts sampling rate by a specified delta in frequency
    TSM_COMCAST__setPlayoutSamplingRate - sets output sampling rate to an absolute frequency

   
   
   ========Sample Usage for time scale modification ==========

   This example shows how time scale modification (small sampling rate adjustments) can be made using this algorithm.
   In this example, the sampling rate converter is initialized to convert from a sampling  rate of 48000 Hz to a sampling rate of 
   48000 Hz - no change. As time goes on, the examples shows how the sampling rate can be adjusted up or down, typically by small amounts.
   Note that although the input and output sampling rates are set as integers at create/init time, the output sampling rate modifications
   are specified as floating point numbers, enabling small, precise changes in sampling rate.

   
# include <include/TSM48K.h>

...

#define INPUT_FRAME_SIZE 80

    TSMHandle_t TSMHandle;
    int16_t NOutputSamples;
    int32_t FrameCount = 0;
    int16_t InputFrame[INPUT_FRAME_SIZE];
    int16_t OutputFrame[INPUT_FRAME_SIZE+20];       //+20 to account for variations in the number of output samples per input frame
    float NewOutputSamplingRate, DeltaSamplingRate;

...
 	// Instantiate the sampling rate converter
    TSMHandle = TSM_COMCAST_create(INPUT_FRAME_SIZE);

    while (1)
    {
        //Gather a new input buffer -> InputFrame;
        ...
        //Run the filter (time scale modification. Note that there may be more or fewer output samples than input samples

        TSM_COMCAST_filter_16(TSMHandle, InputFrame, FRAME_SIZE, OutputFrame, &NOutputSamples);
        
        //Do something with the output frame. Note that NOutputSamples may not be exactly FRAME_SIZE
        
        // As necessary, increase the output sampling rate to perform time scale modification
        if (ClockAdjustRequest)
        {
            TSM_COMCAST_changePlayoutSamplingRate(TSMHandle, DeltaSamplingRate);
            // OR
            TSM_COMCAST_setPlayoutSamplingRate(TSMHandle, NewOutputSamplingRate);
        }

    }

    // Destroy the sampling rate converter
    TSM_COMCAST_delete(TSMHandle);

 */

typedef void *TSMHandle_t;

typedef enum
{
    PCM16,
    PCM32,
    PCMFLOAT
}       PCMFormat_e;


TSM_API TSMHandle_t TSM_COMCAST_create(
    int MaxInputFrameSize,      //Specified in samples
    PCMFormat_e PCMFormat);
    

TSM_API void TSM_COMCAST_filter_16(
	TSMHandle_t Handle,	/* Input - Handle to TSM instance (returned by TSM_COMCAST_create */
	int16_t *pInput,		/* Input - Pointer to input PCM */
	int16_t NInput,			/* Input - Number of input samples */
	int16_t *pOutput,		/* Output - Pointer to output PCM.*/
	int16_t *NOutput		/* Output - Pointer to location where SRDSPSW_CONV_filter will 
							place the number of computed output samples */
);
TSM_API void TSM_COMCAST_filter_32(
    TSMHandle_t Handle,	/* Input - Handle to TSM instance (returned by TSM_COMCAST_create */
    int32_t* pInput,		/* Input - Pointer to input PCM */
    int16_t NInput,			/* Input - Number of input samples */
    int32_t* pOutput,		/* Output - Pointer to output PCM.*/
    int16_t* NOutput		/* Output - Pointer to location where SRDSPSW_CONV_filter will
                            place the number of computed output samples */
);
TSM_API void TSM_COMCAST_filter_float(
    TSMHandle_t Handle,	    /* Input - Handle to TSM instance (returned by TSM_COMCAST_create */
    float* pInput,		    /* Input - Pointer to input PCM */
    int16_t NInput,			/* Input - Number of input samples */
    float* pOutput,		    /* Output - Pointer to output PCM.*/
    int16_t* NOutput		/* Output - Pointer to location where SRDSPSW_CONV_filter will
                            place the number of computed output samples */
);



TSM_API void TSM_COMCAST_adjustPlayoutClock(TSMHandle_t Handle, float DeltaTimeInMicroseconds, float SpreadDurationInSeconds);
TSM_API void TSM_COMCAST_setPlayoutSamplingRate(TSMHandle_t Handle, float NewSamplingRateInHz);
TSM_API void TSM_COMCAST_changePlayoutSamplingRate(TSMHandle_t Handle, float DeltaSamplingRateInHz);
TSM_API void TSM_COMCAST_delete(TSMHandle_t Handle);



#endif //_TSM_H

