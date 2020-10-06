//OpenRevolution audio encoders
//Copyright (C) 2020 I.C. and others (See each encoder file for more information)

#include "dspadpcm_encoder.c"

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

//2 bit ADPCM
void brstm_encode_adpcm_2bit(Brstm* brstmi, unsigned char** ADPCMdata, signed int debugLevel);
#include "adpcm-2.6/adpcmtool.c"
