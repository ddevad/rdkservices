/*___________________________________________________________________________
|
|   File: tsm48k.c
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
#include <stdlib.h>
#include <stdio.h>
#include "tsm48k.h"

typedef struct
{
	void *pState;               //could be 16-bit or 32 bit state
	float **ppMiniFIRCoef;
	uint32_t FilterPhaseQ;
	int16_t InterpolationRate;	//Interpolation Rate
	int16_t MiniFIRLen;
	int16_t NMiniFIRs;
	int16_t DecimationRate;
	//	int16_t ShiftCount;
	float FilterSamplingRateHz;
	float InputSamplingRateHz;
	float OutputSamplingRateHz;		//Initial setting
	float CurrentSamplingRateHz;	//Current setting - in case of subsequent adjustment

	uint32_t FiltIncQ;
	int16_t NNewSamplesNeeded;
	int16_t Q;					//Q point for filter phase/inc
	int8_t BufferingEnabled;
	int8_t boolMemWasAllocated;
	int8_t boolInstanceWasAllocated;
	int32_t MaxInputFrameSizeSamples;

	int32_t SampleClockPerSampleFiltPhaseInc;
	int32_t SampleClockAdjustSampleCount;

}		TSMInstance_t;

typedef struct 
{
	float *pCoef;
	int16_t NCoef;
	int32_t HighSampRate;
	int32_t LowSampRate;
}		Filter_t;

#define FIR_LENGTH 173
#define FILTER_SAMPLING_RATE 19200
#define INPUT_SAMPLING_RATE 48000
#define OUTPUT_SAMPLING_RATE 48000
#define INTERPOLATION_RATE (OUTPUT_SAMPLING_RATE/INPUT_SAMPLING_RATE)
#define DECIMATION_RATE INTERPOLATION_RATE

#define MAX(x,y) (((x)>(y))? (x):(y))

float FIR_COMCAST_P192_48[173] = {
0.0001354492852f,
-0.0000778480535f,
-0.0002954693628f,
-0.0003880631411f,
-0.0002736192255f,
0.0000231807153f,
0.0003646710829f,
0.0005606022896f,
0.0004708892084f,
0.0000967615561f,
-0.0003956880537f,
-0.0007445456577f,
-0.0007280418649f,
-0.0002985562605f,
0.0003634425229f,
0.0009185712552f,
0.0010387930088f,
0.0005953475484f,
-0.0002406870772f,
-0.0010543751996f,
-0.0013883779757f,
-0.0009945812635f,
0.0000000000000f,
0.0011173767271f,
0.0017525727162f,
0.0014957968378f,
0.0003838751290f,
-0.0010679987026f,
-0.0020972448401f,
-0.0020886531565f,
-0.0009315251373f,
0.0008634261321f,
0.0023784628138f,
0.0027513441164f,
0.0016564178513f,
-0.0004596399958f,
-0.0025430778041f,
-0.0034494732972f,
-0.0025627359282f,
-0.0001865574450f,
0.0025295596570f,
0.0041353823617f,
0.0036437255330f,
0.0011159516871f,
-0.0022686650045f,
-0.0047477111220f,
-0.0048807538114f,
-0.0023663432803f,
0.0016831716057f,
0.0052107134834f,
0.0062432247214f,
0.0039738980122f,
-0.0006853873492f,
-0.0054322876967f,
-0.0076894178055f,
-0.0059779603034f,
-0.0008301106864f,
0.0052986550145f,
0.0091682784259f,
0.0084323771298f,
0.0030037632678f,
-0.0046610040590f,
-0.0106220319867f,
-0.0114303100854f,
-0.0060508796014f,
0.0033028221224f,
0.0119894761592f,
0.0151609843597f,
0.0103583615273f,
-0.0008557070978f,
-0.0132097527385f,
-0.0200560111552f,
-0.0167504400015f,
-0.0034469591919f,
0.0142262084410f,
0.0272508244962f,
0.0273939855397f,
0.0116813806817f,
-0.0149901835248f,
-0.0406178422272f,
-0.0503229051828f,
-0.0325296632946f,
0.0154642900452f,
0.0850360170007f,
0.1582340300083f,
0.2137117236853f,
0.2343750000000f,
0.2137117236853f,
0.1582340300083f,
0.0850360170007f,
0.0154642900452f,
-0.0325296632946f,
-0.0503229051828f,
-0.0406178422272f,
-0.0149901835248f,
0.0116813806817f,
0.0273939855397f,
0.0272508244962f,
0.0142262084410f,
-0.0034469591919f,
-0.0167504400015f,
-0.0200560111552f,
-0.0132097527385f,
-0.0008557070978f,
0.0103583615273f,
0.0151609843597f,
0.0119894761592f,
0.0033028221224f,
-0.0060508796014f,
-0.0114303100854f,
-0.0106220319867f,
-0.0046610040590f,
0.0030037632678f,
0.0084323771298f,
0.0091682784259f,
0.0052986550145f,
-0.0008301106864f,
-0.0059779603034f,
-0.0076894178055f,
-0.0054322876967f,
-0.0006853873492f,
0.0039738980122f,
0.0062432247214f,
0.0052107134834f,
0.0016831716057f,
-0.0023663432803f,
-0.0048807538114f,
-0.0047477111220f,
-0.0022686650045f,
0.0011159516871f,
0.0036437255330f,
0.0041353823617f,
0.0025295596570f,
-0.0001865574450f,
-0.0025627359282f,
-0.0034494732972f,
-0.0025430778041f,
-0.0004596399958f,
0.0016564178513f,
0.0027513441164f,
0.0023784628138f,
0.0008634261321f,
-0.0009315251373f,
-0.0020886531565f,
-0.0020972448401f,
-0.0010679987026f,
0.0003838751290f,
0.0014957968378f,
0.0017525727162f,
0.0011173767271f,
0.0000000000000f,
-0.0009945812635f,
-0.0013883779757f,
-0.0010543751996f,
-0.0002406870772f,
0.0005953475484f,
0.0010387930088f,
0.0009185712552f,
0.0003634425229f,
-0.0002985562605f,
-0.0007280418649f,
-0.0007445456577f,
-0.0003956880537f,
0.0000967615561f,
0.0004708892084f,
0.0005606022896f,
0.0003646710829f,
0.0000231807153f,
-0.0002736192255f,
-0.0003880631411f,
-0.0002954693628f,
-0.0000778480535f,
0.0001354492852f
};

static void** malloc_2D(int rows, int columns, int element_size)
{
    int i;
    void** pp = (void**)malloc(rows * sizeof(void*));
    if (pp == NULL)
        return (NULL);
    for (i = 0; i < rows; i++)
    {
        pp[i] = malloc(columns * element_size);
        if (pp[i] == NULL)
            return(NULL);
    }
    return(pp);
}
static void free_2D(void** pp, int nrows)
{
    int i;
    if (pp != NULL)
    {
        {
            for (i = 0; i < nrows; i++)
            {
                if (pp[i] != NULL)
                    free(pp[i]);
            }
        }
    }
    free(pp);
}
TSMHandle_t TSM_COMCAST_create(int MaximumInputFrameSize, PCMFormat_e PCMFormat)
{
    TSMInstance_t *pInstance = 0;
    int32_t i;
    int16_t FIRIndex;
    int16_t FIRCount;
    int16_t Index;
    float* pCoef;
    double TempOutputSamplingRate, SamplingRateErrorFactor;
    int16_t* pState16;
    int32_t* pState32;
    float* pStateFloat;

    pInstance = (TSMInstance_t*)malloc(sizeof(TSMInstance_t));


    pInstance->InterpolationRate = INTERPOLATION_RATE;
    pInstance->DecimationRate = DECIMATION_RATE;
    pInstance->NMiniFIRs = INTERPOLATION_RATE;
    if ((FIR_LENGTH % INTERPOLATION_RATE) == 0)
        pInstance->MiniFIRLen = FIR_LENGTH / INTERPOLATION_RATE;
    else
        pInstance->MiniFIRLen = FIR_LENGTH / INTERPOLATION_RATE + 1;
    pInstance->ppMiniFIRCoef = (float**)malloc_2D(pInstance->NMiniFIRs, pInstance->MiniFIRLen, sizeof(float));
    if (PCMFormat == PCM16)
        pInstance->pState = malloc(pInstance->MiniFIRLen * MaximumInputFrameSize * sizeof(int16_t));
    else
        pInstance->pState = malloc(pInstance->MiniFIRLen * MaximumInputFrameSize * sizeof(int32_t));

    
	pInstance->MaxInputFrameSizeSamples = MaximumInputFrameSize;
	pInstance->Q = 25;
	pInstance->FilterPhaseQ = 0;
	pInstance->FilterSamplingRateHz = 192000;
    pInstance->InputSamplingRateHz = 48000;
    pInstance->OutputSamplingRateHz = 48000;
	pInstance->CurrentSamplingRateHz = (float)pInstance->OutputSamplingRateHz;
    switch (PCMFormat)
    {
    case(PCM32):
    {
        pState32 = (int32_t*)pInstance->pState;
        for (i = 0; i < pInstance->MiniFIRLen + pInstance->MaxInputFrameSizeSamples; i++)
            pState32[i] = 0;
        break;
    }
    case(PCMFLOAT):
    {
        pStateFloat = (float*)pInstance->pState;
        for (i = 0; i < pInstance->MiniFIRLen + pInstance->MaxInputFrameSizeSamples; i++)
            pStateFloat[i] = 0.0f;
        break;
    }
    case(PCM16):
    default:
    {
        pState16 = (int16_t*)pInstance->pState;
        for (i = 0; i < pInstance->MiniFIRLen + pInstance->MaxInputFrameSizeSamples; i++)
            pState16[i] = 0;
        break;
    }
    }
	//	Gain = DecimationRate;
	for (FIRCount = 0; FIRCount < pInstance->NMiniFIRs; FIRCount++)
	{
		for (FIRIndex = 0; FIRIndex < pInstance->MiniFIRLen; FIRIndex++)
			pInstance->ppMiniFIRCoef[FIRCount][FIRIndex] = 0;
	}
    pCoef = FIR_COMCAST_P192_48;

	pInstance->NNewSamplesNeeded = pInstance->DecimationRate / pInstance->InterpolationRate;
	TempOutputSamplingRate = (double)pInstance->InputSamplingRateHz * (double)pInstance->InterpolationRate / (double)pInstance->DecimationRate;
	SamplingRateErrorFactor = (double)TempOutputSamplingRate / (double) pInstance->OutputSamplingRateHz;
	pInstance->FiltIncQ = (uint32_t)((double)(1L << pInstance->Q) * (double)pInstance->DecimationRate * SamplingRateErrorFactor);

	for (FIRCount = 0; FIRCount < pInstance->NMiniFIRs; FIRCount++)
	{
		for (FIRIndex = 0; FIRIndex < pInstance->MiniFIRLen; FIRIndex++)
		{
			Index = FIRIndex*pInstance->NMiniFIRs + FIRCount;
            if (Index < FIR_LENGTH)
                pInstance->ppMiniFIRCoef[FIRCount][FIRIndex] = pCoef[Index] * pInstance->InterpolationRate;
            else
                pInstance->ppMiniFIRCoef[FIRCount][FIRIndex] = 0.0f;
		}
	}
	pInstance->SampleClockAdjustSampleCount = 0;
    return((TSMHandle_t)pInstance);
}

void TSM_COMCAST_filter_16(TSMHandle_t Handle, int16_t *pInput, int16_t NInput, int16_t *pOutput, int16_t *pNOutput)
{
    float Sum;

    float* Coef_p;
	int16_t *State_p;
	int16_t i;
	TSMInstance_t *pChannel = (TSMInstance_t *)Handle;
	int16_t InputSampleCount;
	int16_t OutputSampleCount = 0;
	int16_t NInputSamplesRemaining = NInput;
	int16_t ShiftCount;
	uint16_t NMiniFIRs;
	int16_t InputSampleCountForFiltering = 0;
	NMiniFIRs = pChannel->InterpolationRate;
	InputSampleCount = 0;
        int16_t* pState16 = (int16_t*)pChannel->pState;
	while (InputSampleCount < NInput)
	{
		Sum = 0;
		ShiftCount = 0;
		if (pChannel->NNewSamplesNeeded > 0)
		{
			NInputSamplesRemaining = NInput - InputSampleCount;
			ShiftCount = pChannel->NNewSamplesNeeded;
			if (NInputSamplesRemaining < ShiftCount)
				ShiftCount = NInputSamplesRemaining;
			for (i = 0; i < pChannel->MiniFIRLen - ShiftCount; i++)
				pState16[i] = pState16[i + ShiftCount];
			for (i = 0; i < ShiftCount; i++)
				pState16[pChannel->MiniFIRLen - ShiftCount + i] = pInput[InputSampleCount++];
			pChannel->NNewSamplesNeeded -= ShiftCount;
		}
		InputSampleCountForFiltering += ShiftCount;
		while (pChannel->NNewSamplesNeeded == 0)
		{
			int16_t OldFilterPhase;
			State_p = &pState16[pChannel->MiniFIRLen - 1];
			Coef_p = &pChannel->ppMiniFIRCoef[pChannel->FilterPhaseQ >> pChannel->Q][0];
			Sum = 0;
			for (i = 0; i < pChannel->MiniFIRLen; i++)
			{
				Sum += (float)(*State_p--) * *Coef_p++;
			}
			OldFilterPhase = (int16_t)(pChannel->FilterPhaseQ >> pChannel->Q);
			pChannel->FilterPhaseQ = pChannel->FilterPhaseQ + pChannel->FiltIncQ;


			if (pChannel->SampleClockAdjustSampleCount != 0)
			{
				pChannel->FilterPhaseQ += pChannel->SampleClockPerSampleFiltPhaseInc;
				pChannel->SampleClockAdjustSampleCount -= 1;
			}
			if ((pChannel->FilterPhaseQ >> pChannel->Q) >= NMiniFIRs)
			{
				pChannel->NNewSamplesNeeded = (int16_t)((pChannel->FilterPhaseQ >> pChannel->Q) / NMiniFIRs - OldFilterPhase / NMiniFIRs);
				pChannel->FilterPhaseQ = pChannel->FilterPhaseQ % ((int32_t)NMiniFIRs << pChannel->Q);
			}
			else
				pChannel->NNewSamplesNeeded = 0;
			if (Sum > INT16_MAX)
				Sum = INT16_MAX;
			if (Sum < INT16_MIN)
				Sum = INT16_MIN;
            pOutput[OutputSampleCount++] = (int16_t) Sum;
		}
	}
	*pNOutput = OutputSampleCount;

}

void TSM_COMCAST_filter_32(TSMHandle_t Handle, int32_t *pInput, int16_t NInput, int32_t* pOutput, int16_t* pNOutput)
{
    float Sum;

    float* Coef_p;
    int32_t* State_p;
    int16_t i;
    TSMInstance_t* pChannel = (TSMInstance_t*)Handle;
    int16_t InputSampleCount;
    int16_t OutputSampleCount = 0;
    int16_t NInputSamplesRemaining = NInput;
    int16_t ShiftCount;
    uint16_t NMiniFIRs;
    int16_t InputSampleCountForFiltering = 0;
    NMiniFIRs = pChannel->InterpolationRate;
    InputSampleCount = 0;
    int32_t* pState32 = (int32_t*) pChannel->pState;
    while (InputSampleCount < NInput)
    {
        Sum = 0;
        ShiftCount = 0;
        if (pChannel->NNewSamplesNeeded > 0)
        {
            NInputSamplesRemaining = NInput - InputSampleCount;
            ShiftCount = pChannel->NNewSamplesNeeded;
            if (NInputSamplesRemaining < ShiftCount)
                ShiftCount = NInputSamplesRemaining;
            for (i = 0; i < pChannel->MiniFIRLen - ShiftCount; i++)
                pState32[i] = pState32[i + ShiftCount];
            for (i = 0; i < ShiftCount; i++)
                pState32[pChannel->MiniFIRLen - ShiftCount + i] = pInput[InputSampleCount++];
            pChannel->NNewSamplesNeeded -= ShiftCount;
        }
        InputSampleCountForFiltering += ShiftCount;
        while (pChannel->NNewSamplesNeeded == 0)
        {
            int16_t OldFilterPhase;
            State_p = &pState32[pChannel->MiniFIRLen - 1];
            Coef_p = &pChannel->ppMiniFIRCoef[pChannel->FilterPhaseQ >> pChannel->Q][0];
            Sum = 0;
            for (i = 0; i < pChannel->MiniFIRLen; i++)
            {
                Sum += (float)(*State_p--) * *Coef_p++;
            }
            OldFilterPhase = (int16_t)(pChannel->FilterPhaseQ >> pChannel->Q);
            pChannel->FilterPhaseQ = pChannel->FilterPhaseQ + pChannel->FiltIncQ;


            if (pChannel->SampleClockAdjustSampleCount != 0)
            {
                pChannel->FilterPhaseQ += pChannel->SampleClockPerSampleFiltPhaseInc;
                pChannel->SampleClockAdjustSampleCount -= 1;
            }
            if ((pChannel->FilterPhaseQ >> pChannel->Q) >= NMiniFIRs)
            {
                pChannel->NNewSamplesNeeded = (int16_t)((pChannel->FilterPhaseQ >> pChannel->Q) / NMiniFIRs - OldFilterPhase / NMiniFIRs);
                pChannel->FilterPhaseQ = pChannel->FilterPhaseQ % ((int32_t)NMiniFIRs << pChannel->Q);
            }
            else
                pChannel->NNewSamplesNeeded = 0;
            if (Sum > (float) INT32_MAX)
                Sum = (float) INT32_MAX;
            if (Sum < (float) INT32_MIN)
                Sum = (float) INT32_MIN;
            pOutput[OutputSampleCount++] = (int32_t)Sum;
        }
    }
    *pNOutput = OutputSampleCount;
}
void TSM_COMCAST_filter_float(TSMHandle_t Handle, float* pInput, int16_t NInput, float* pOutput, int16_t* pNOutput)
{
    float Sum;

    float* Coef_p;
    float* State_p;
    int16_t i;
    TSMInstance_t* pChannel = (TSMInstance_t*)Handle;
    int16_t InputSampleCount;
    int16_t OutputSampleCount = 0;
    int16_t NInputSamplesRemaining = NInput;
    int16_t ShiftCount;
    uint16_t NMiniFIRs;
    int16_t InputSampleCountForFiltering = 0;
    NMiniFIRs = pChannel->InterpolationRate;
    InputSampleCount = 0;
    float* pStateFloat = (float*)pChannel->pState;
    while (InputSampleCount < NInput)
    {
        Sum = 0;
        ShiftCount = 0;
        if (pChannel->NNewSamplesNeeded > 0)
        {
            NInputSamplesRemaining = NInput - InputSampleCount;
            ShiftCount = pChannel->NNewSamplesNeeded;
            if (NInputSamplesRemaining < ShiftCount)
                ShiftCount = NInputSamplesRemaining;
            for (i = 0; i < pChannel->MiniFIRLen - ShiftCount; i++)
                pStateFloat[i] = pStateFloat[i + ShiftCount];
            for (i = 0; i < ShiftCount; i++)
                pStateFloat[pChannel->MiniFIRLen - ShiftCount + i] = pInput[InputSampleCount++];
            pChannel->NNewSamplesNeeded -= ShiftCount;
        }
        InputSampleCountForFiltering += ShiftCount;
        while (pChannel->NNewSamplesNeeded == 0)
        {
            int16_t OldFilterPhase;
            State_p = &pStateFloat[pChannel->MiniFIRLen - 1];
            Coef_p = &pChannel->ppMiniFIRCoef[pChannel->FilterPhaseQ >> pChannel->Q][0];
            Sum = 0;
            for (i = 0; i < pChannel->MiniFIRLen; i++)
            {
                Sum += (float)(*State_p--) * *Coef_p++;
            }
            OldFilterPhase = (int16_t)(pChannel->FilterPhaseQ >> pChannel->Q);
            pChannel->FilterPhaseQ = pChannel->FilterPhaseQ + pChannel->FiltIncQ;


            if (pChannel->SampleClockAdjustSampleCount != 0)
            {
                pChannel->FilterPhaseQ += pChannel->SampleClockPerSampleFiltPhaseInc;
                pChannel->SampleClockAdjustSampleCount -= 1;
            }
            if ((pChannel->FilterPhaseQ >> pChannel->Q) >= NMiniFIRs)
            {
                pChannel->NNewSamplesNeeded = (int16_t)((pChannel->FilterPhaseQ >> pChannel->Q) / NMiniFIRs - OldFilterPhase / NMiniFIRs);
                pChannel->FilterPhaseQ = pChannel->FilterPhaseQ % ((int32_t)NMiniFIRs << pChannel->Q);
            }
            else
                pChannel->NNewSamplesNeeded = 0;
            pOutput[OutputSampleCount++] = Sum;
        }
    }
    *pNOutput = OutputSampleCount;
}

void TSM_COMCAST_delete(TSMHandle_t Handle)
{
	TSMInstance_t *pInstance = (TSMInstance_t *)Handle;
    free_2D((void**)pInstance->ppMiniFIRCoef, pInstance->NMiniFIRs);
    free(pInstance->pState);
    free(pInstance);
}

 void TSM_COMCAST_adjustPlayoutClock(TSMHandle_t Handle, float DeltaTimeInSeconds, float SpreadDurationInSeconds)
{
	TSMInstance_t *pInstance = (TSMInstance_t *)Handle;
	int64_t i64FiltPhaseInc;

	pInstance->SampleClockAdjustSampleCount = MAX(1, (int32_t)(SpreadDurationInSeconds * pInstance->OutputSamplingRateHz));
	i64FiltPhaseInc = (int64_t)((float)DeltaTimeInSeconds * pInstance->FilterSamplingRateHz * (float)(1LL << pInstance->Q));
	i64FiltPhaseInc /= pInstance->SampleClockAdjustSampleCount;
	pInstance->SampleClockPerSampleFiltPhaseInc = (int32_t)i64FiltPhaseInc;

}



 void TSM_COMCAST_setPlayoutSamplingRate(TSMHandle_t Handle, float NewSamplingRateInHz)
{
	TSMInstance_t *pInstance = (TSMInstance_t *)Handle;
	if (NewSamplingRateInHz != pInstance->CurrentSamplingRateHz)
	{
		TSM_COMCAST_changePlayoutSamplingRate(Handle, NewSamplingRateInHz - pInstance->CurrentSamplingRateHz);
		pInstance->CurrentSamplingRateHz = NewSamplingRateInHz;
	}
}

 void TSM_COMCAST_changePlayoutSamplingRate(TSMHandle_t Handle, float DeltaSamplingRateInHz)
{
	TSMInstance_t *pInstance = (TSMInstance_t *)Handle;
	int32_t TempOutputSamplingRate;
	double SamplingRateErrorFactor;
	int32_t NewFiltIncQ;

	TempOutputSamplingRate = (int32_t) ((float)pInstance->InputSamplingRateHz * (float)pInstance->InterpolationRate / (float)pInstance->DecimationRate);
	SamplingRateErrorFactor = (float)TempOutputSamplingRate / (pInstance->OutputSamplingRateHz + DeltaSamplingRateInHz);
	SamplingRateErrorFactor -= 1.0f;
	NewFiltIncQ = pInstance->FiltIncQ + (int32_t)((double)(1LL << pInstance->Q) * (double)pInstance->DecimationRate * SamplingRateErrorFactor);

	if (NewFiltIncQ > 0)
		pInstance->FiltIncQ = NewFiltIncQ;

}

