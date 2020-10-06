//2-bit ADPCM encoder written by Aikku
//Released unlicensed to public domain
//https://gbatemp.net/download/adpcm-2-codec-demo.36156/
//Modified by I.C. for OpenRevolution

/**************************************/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
/**************************************/
#include "adpcm.h"
/**************************************/

#define BUFFER_MAX_CHANS 16
#define BUFFER_FRAMES  256
#define BUFFER_SAMPLES (BUFFER_FRAMES * ADPCM_FRAME_SIZE)

//TODO This should be optimized to be allocated only when the code is run
struct ADPCM_State_t ADPCMState[BUFFER_MAX_CHANS];
static ADPCM_Frame_t EncBuf[BUFFER_MAX_CHANS][BUFFER_FRAMES];
static int16_t       SmpBuf[BUFFER_MAX_CHANS][BUFFER_SAMPLES];

static void brstm_encoder_adpcm_2bit_BufferDump(Brstm* brstmi, size_t &outputDataPos, bool Endian, unsigned char** ADPCMdata, size_t nFrames, size_t nChan) {
    //size_t nSmp = nFrames * ADPCM_FRAME_SIZE;
    uint32_t Byte;
    for(size_t Chan=0;Chan<nChan;Chan++) {
        for(size_t Frame=0;Frame<nFrames;Frame++) {
            for(uint8_t b=0;b<4;b++) {
                if(Endian) {
                    Byte = EncBuf[Chan][Frame] & 0x000000FF;
                    EncBuf[Chan][Frame] = EncBuf[Chan][Frame] >> 8;
                } else {
                    Byte = EncBuf[Chan][Frame] & 0xFF000000;
                    Byte = Byte >> 24;
                    EncBuf[Chan][Frame] = EncBuf[Chan][Frame] << 8;
                }
                ADPCMdata[Chan][outputDataPos+Frame*4+b] = Byte;
            }
        }
    }
    outputDataPos += nFrames*4;
    //if(adFile) fwrite(EncBuf, sizeof(ADPCM_Frame_t)*nChan, nFrames, adFile);
}

/**************************************/

void brstm_encode_adpcm_2bit(Brstm* brstmi, unsigned char** ADPCMdata, signed int debugLevel) {
    size_t Chan, Frame, n, outputDataPos, inputDataPos, Endian;
    
    outputDataPos = 0;
    inputDataPos = 0;
    
    size_t nChan = brstmi->num_channels;
    
    { // Detect system endianness
        uint16_t number = 0x1;
        uint8_t *numPtr = (uint8_t*)&number;
        Endian = (numPtr[1] == 1);
    }
    
    //! Get number of samples
    size_t nSmp, nFrames; {
        nSmp    = brstmi->total_samples;
        nFrames = (nSmp - 1) / ADPCM_FRAME_SIZE + 1;
    }
    
    //! Begin compression
    double RMSE = 0.0;
    for(Chan=0;Chan<nChan;Chan++) {
        ADPCM_Reset(&ADPCMState[Chan]);
        //Copy coefs to BRSTM struct coefs
        for(uint8_t coe=0;coe<16;coe++) {
            brstmi->ADPCM_coefs[Chan][coe] = PredCoef[coe];
        }
    }
    for(Frame=0;Frame<nFrames;Frame++) {
        if(Frame % 4096 == 0 && debugLevel > 0) {
            size_t FramePercent = Frame*100ULL*100/nFrames;
            printf("\rFrame %u/%u (%u.%2u%%)...", Frame+1, nFrames, FramePercent/100, FramePercent%100); fflush(stdout);
        }
        
        size_t FrameBufferIdx = Frame%BUFFER_FRAMES;
        if(FrameBufferIdx == 0) {
            if(Frame != 0) brstm_encoder_adpcm_2bit_BufferDump(brstmi, outputDataPos, Endian, ADPCMdata, BUFFER_FRAMES, nChan);
            //Copy PCM_samples into buffer
            size_t nRead = (BUFFER_SAMPLES <= brstmi->total_samples-inputDataPos ? BUFFER_SAMPLES : brstmi->total_samples-inputDataPos);
            for(size_t C=0;C<nChan;C++) {
                for(size_t S=0;S<nRead;S++) {
                    SmpBuf[C][S] = brstmi->PCM_samples[C][inputDataPos+S];
                }
            }
            inputDataPos += nRead;
            //Fill remaining samples with 0 if the buffer was not fully filled
            while(nRead < BUFFER_SAMPLES) {
                for(size_t C=0;C<nChan;C++) {
                    SmpBuf[C][nRead] = 0;
                }
                nRead++;
            }
        }
        
        for(Chan=0;Chan<nChan;Chan++) {
            int16_t *SmpSrc = SmpBuf[Chan] + FrameBufferIdx*ADPCM_FRAME_SIZE;
            
            EncBuf[Chan][FrameBufferIdx] = ADPCM_Encode(&ADPCMState[Chan], SmpSrc);
            for(n=0;n<ADPCM_FRAME_SIZE;n++) {
                SmpSrc[n] = ADPCMState[Chan].Dec[n];
                RMSE += ADPCMState[Chan].Res[n]*ADPCMState[Chan].Res[n];
            }
        }
    }
    
    if(nFrames%BUFFER_FRAMES) brstm_encoder_adpcm_2bit_BufferDump(brstmi, outputDataPos, Endian, ADPCMdata, nFrames%BUFFER_FRAMES, nChan);
    RMSE = sqrt(RMSE / nSmp);
    if(debugLevel > 0) {
        printf("\rFrame %u/%u (%u.%2u%%)...", nFrames, nFrames, 100, 0); fflush(stdout);
        printf("\nOk (PSNR = %.5fdB)\n", -20.0*log10(RMSE / 32768.0));
    }
    
    //! Done
}

/**************************************/
//! EOF
/**************************************/
