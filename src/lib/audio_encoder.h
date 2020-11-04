//OpenRevolution audio encoding/writing code
//Copyright (C) 2020 I.C.

//4-bit DSPADPCM encoder
#include "dspadpcm_encoder.c"

//Calculating history samples and coefs for 4-bit DSPADPCM
void brstm_encoder_adpcm_calculateAdpcmData(Brstm* brstmi, uint8_t encodeADPCM, brstm_HSData_t* HSData) {
    if(encodeADPCM) {
        //Get history samples
        for(unsigned char c=0;c<brstmi->num_channels;c++) {
            HSData->LoopHS1[c] = brstmi->loop_start > 0 ? brstmi->PCM_samples[c][brstmi->loop_start-1] : 0;
            HSData->LoopHS2[c] = brstmi->loop_start > 1 ? brstmi->PCM_samples[c][brstmi->loop_start-2] : 0;
            for(unsigned long b=0;b<brstmi->total_blocks;b++) {
                if(b==0) {
                    //First block history samples are always zero
                    HSData->HS1[c][b] = 0;
                    HSData->HS2[c][b] = 0;
                    continue;
                }
                HSData->HS1[c][b] = brstmi->PCM_samples[c][(b*brstmi->blocks_samples)-1];
                HSData->HS2[c][b] = brstmi->PCM_samples[c][(b*brstmi->blocks_samples)-2];
            }
            //Calculate ADPCM coefs for channel
            DSPCorrelateCoefs(brstmi->PCM_samples[c],brstmi->total_samples,brstmi->ADPCM_coefs[c]);
        }
    } else {
        for(unsigned char c=0;c<brstmi->num_channels;c++) {
            //Decode 4 bit ADPCM
            unsigned char* blockData = brstmi->ADPCM_data[c];
            unsigned long currentBlockSamples = brstmi->total_samples;
            unsigned long b = 0;
            unsigned long currentSample = 0;
            
            //Magic adapted from brawllib's ADPCMState.cs
            signed int 
            cps = blockData[0],
            cyn1 = 0,
            cyn2 = 0;
            unsigned long dataIndex = 0;
            
            int16_t* coefs = brstmi->ADPCM_coefs[c];
            
            for (unsigned long sampleIndex=0;sampleIndex<currentBlockSamples;) {
                long outSample = 0;
                if (sampleIndex % 14 == 0) {
                    cps = blockData[dataIndex++];
                }
                if ((sampleIndex++ & 1) == 0) {
                    outSample = blockData[dataIndex] >> 4;
                } else {
                    outSample = blockData[dataIndex++] & 0x0f;
                }
                if (outSample >= 8) {
                    outSample -= 16;
                }
                const long scale = 1 << (cps & 0x0f);
                const long cIndex = (cps >> 4) << 1;
                
                outSample = (0x400 + ((scale * outSample) << 11) + coefs[brstm_clamp(cIndex, 0, 15)] * cyn1 + coefs[brstm_clamp(cIndex + 1, 0, 15)] * cyn2) >> 11;
                
                if(currentSample % brstmi->blocks_samples == 0) {
                    if(b == 0) {
                        HSData->HS1[c][b] = 0;
                        HSData->HS2[c][b] = 0;
                    } else {
                        HSData->HS1[c][b] = cyn1;
                        HSData->HS2[c][b] = cyn2;
                    }
                    b++;
                }
                if(currentSample == brstmi->loop_start) {
                    if(currentSample > 0) {HSData->LoopHS1[c] = cyn1;} else {HSData->LoopHS1[c] = 0;}
                    if(currentSample > 1) {HSData->LoopHS2[c] = cyn2;} else {HSData->LoopHS2[c] = 0;}
                }
                
                cyn2 = cyn1;
                cyn1 = brstm_clamp(outSample, -32768, 32767);
                
                currentSample++;
            }
        }
    }
}

//4-bit DSPADPCM encoding
//Thanks to https://github.com/jackoalan/gc-dspadpcm-encode/
void brstm_encode_adpcm(Brstm* brstmi,unsigned char** ADPCMdata,signed int debugLevel) {
    //Encode ADPCM for each channel
    for(unsigned char c=0;c<brstmi->num_channels;c++) {
        unsigned long ADPCMdataPos = 0;
        //Store coefs like this for the DSPEncodeFrame function
        int16_t coefs[8][2]; for(unsigned int i=0;i<16;i++) {coefs[i/2][i%2] = brstmi->ADPCM_coefs[c][i];}
        
        unsigned long packetCount = brstmi->total_samples / PACKET_SAMPLES + (brstmi->total_samples % PACKET_SAMPLES != 0);
        int16_t convSamps[16] = {0};
        unsigned char block[8];
        for (unsigned long p=0;p<packetCount;p++) {
            memset(convSamps + 2, 0, PACKET_SAMPLES * sizeof(int16_t));
            int numSamples = MIN(brstmi->total_samples - p * PACKET_SAMPLES, PACKET_SAMPLES);
            for (unsigned int s=0; s<numSamples; ++s)
                convSamps[s+2] = brstmi->PCM_samples[c][p*PACKET_SAMPLES+s];
            
            DSPEncodeFrame(convSamps, PACKET_SAMPLES, block, coefs);
            
            convSamps[0] = convSamps[14];
            convSamps[1] = convSamps[15];
            
            brstm_encoder_writebytes(ADPCMdata[c],block,brstm_getBytesForAdpcmSamples(numSamples),ADPCMdataPos);
            
            //console output
            if(!(p%512) && debugLevel>0) std::cout << "\r" << "Encoding DSPADPCM data... (CH " << (unsigned int)c+1 << "/" << brstmi->num_channels << " " << floor(((float)p/packetCount) * 100) << "%)          ";
        }
        
        if(debugLevel>0) std::cout << "\r" << "Encoding DSPADPCM data... (CH " << (unsigned int)c+1 << "/" << brstmi->num_channels << " 100%)          ";
    }
}
