//Revolution BWAV encoder
//Copyright (C) 2020 Extrasklep

//Thanks to https://gota7.github.io/Citric-Composer/specs/binaryWav.html
//And again BWAV is weird and stupid

#include <math.h>
//BWAV header requires a CRC checksum
#include "../crc/crc_32.c"

unsigned char brstm_formats_encode_bwav(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    if(debugLevel>=0) {std::cout << "BWAV encoder is not implemented yet.\n";}
    //return 210;
    
    //Check for invalid requests
    //Unsupported codec
    if(brstmi->codec != 2 && brstmi->codec != 1) {
        if(debugLevel>=0) std::cout << "Unsupported codec.\n";
        return 220;
    }
    
    //PCM16 not supported yet
    if(brstmi->codec == 1) {
        if(debugLevel>=0) std::cout << "Unsupported codec.\n";
        return 220;
    }
    
    if(brstmi->num_tracks > 1) {
        if(debugLevel>=0) std::cout << "Warning: BWAV cannot store accurate track information\n";
    }
    
    bool &BOM = brstmi->BOM;
    BOM = 0; //Little Endian
    char spinner = '/';
    uint32_t CRCsum = 0xFFFFFFFF;
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building BWAV headers...                ";
    
    delete[] brstmi->encoded_file;
    unsigned char* buffer = new unsigned char[(brstmi->total_samples*brstmi->num_channels*
    (brstmi->codec == 1 ? 2 : 1)) //For 16 bit PCM
    +((brstmi->total_samples*brstmi->num_channels/14336)*4)+brstmi->num_channels*256+8192];
    unsigned long  bufpos = 0;
    unsigned long  off; //for argument 4 of brstm_encoder_writebytes when we don't write to the end of the buffer
    
    //Header
    //Magic word
    brstm_encoder_writebytes(buffer,(unsigned char*)"BWAV",4,bufpos);
    //Byte Order Mark (LE) and version
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0xFF,0xFE,0x01,0x00},4,bufpos);
    //CRC32 hash (will be actually written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
    //Number of channels
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->num_channels,2,BOM),2,bufpos);
    
    //Channel info for each channel
    //Total samples adjusted for bytes
    unsigned long TotalBytesPerChannel = brstmi->codec == 2 ? ceil((double)brstmi->total_samples/(double)1.75) : brstmi->total_samples*2;
    unsigned long chAudioOffsets[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    for(unsigned int c=0;c<brstmi->num_channels;c++) {
        //Codec
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->codec-1,2,BOM),2,bufpos);
        //Channel pan?
        unsigned int ChannelPan = brstmi->track_num_channels[0] == 2 ? (c%2) : (2);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ChannelPan,2,BOM),2,bufpos);
        //Sample rate
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->sample_rate,4,BOM),4,bufpos);
        //Total samples (twice)
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_samples,4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_samples,4,BOM),4,bufpos);
        //ADPCM coefs 16xInt16
        if(brstmi->codec == 2) {
            for(unsigned char i=0;i<16;i++) {
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(brstmi->ADPCM_coefs[c][i],BOM),2,bufpos);
            }
        } else {
            //Write zeros for other codecs
            for(unsigned char i=0;i<8;i++) {
                brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
            }
        }
        //Offset to channel's audio data
        chAudioOffsets[c] = 
        ( c==0 ? (0x10 + 0x4C*brstmi->num_channels) : 0) + //Headers
        (c>0 ? chAudioOffsets[c-1] + TotalBytesPerChannel : 0);
        while(chAudioOffsets[c] % 0x40 != 0) chAudioOffsets[c]++; //Padding
        //Write it twice
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(chAudioOffsets[c],4,BOM),4,bufpos);
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(chAudioOffsets[c],4,BOM),4,bufpos);
        //Unknown value, always 1
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(1,4,BOM),4,bufpos);
        //Loop end/loop flag
        if(brstmi->loop_flag) {
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_samples,4,BOM),4,bufpos);
        } else {
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0xFF,0xFF,0xFF,0xFF},4,bufpos);
        }
        //Loop start point
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->loop_start,4,BOM),4,bufpos);
        //Initial predictor scale (will be written later)
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
        //2 history samples, always 0
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        //Padding
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos);
    }
    
    //audio encoding magic here
    unsigned char** ADPCMdata;
    unsigned long   ADPCMdataPos[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    if(encodeADPCM == 1) {
        //yes copied code whatever will be split later
        ADPCMdata = new unsigned char* [16];
        //Encode ADPCM for each channel
        for(unsigned char c=0;c<brstmi->num_channels;c++) {
            ADPCMdata[c] = new unsigned char[brstmi->total_samples];
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
                
                brstm_encoder_writebytes(ADPCMdata[c],block,brstm_encoder_GetBytesForAdpcmSamples(numSamples),ADPCMdataPos[c]);
                
                //console output
                if(!(p%512) && debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Encoding DSPADPCM data... (CH " << (unsigned int)c+1 << "/" << brstmi->num_channels << " " << floor(((float)p/packetCount) * 100) << "%)          ";
            }
            //Write ADPCM information to channel infos
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCMdata[c][0],2,BOM),2,off=0x54 + c*0x4C); //Initial predictor scale
            
            if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Encoding DSPADPCM data... (CH " << (unsigned int)c+1 << "/" << brstmi->num_channels << " 100%)          ";
        }
    } else {
        ADPCMdata = brstmi->ADPCM_data;
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Write ADPCM information to channel infos
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCMdata[c][0],2,BOM),2,off=0x54 + c*0x4C); //Initial predictor scale
        }
    }
    
    //Write audio data to file
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing ADPCM data...                                                                        ";
    for(unsigned int c=0;c<brstmi->num_channels;c++) {
        //Write padding until we reach the correct offset to audio data
        while(bufpos != chAudioOffsets[c]) brstm_encoder_writebyte(buffer,0x00,bufpos);
        //Write audio data
        if(brstmi->codec == 2) {
            brstm_encoder_writebytes(buffer,ADPCMdata[c],TotalBytesPerChannel,bufpos);
            //Calculate checksum
            CRCsum = crc32buf((char*)ADPCMdata[c],TotalBytesPerChannel,CRCsum);
            std::cout << "\nCurrent CRC: " << std::hex << CRCsum << " First byte: " << (int)ADPCMdata[c][0] << " Last: " << (int)ADPCMdata[c][TotalBytesPerChannel-1] << '\n';
            if(encodeADPCM == 1) delete[] ADPCMdata[c]; //delete the ADPCM data only if we made it locally
        } else {
            
        }
    }
    
    if(encodeADPCM == 1) delete[] ADPCMdata; //delete the ADPCM data only if we made it locally
    
    //Finalize file (write some things we couldn't write earlier)
    //CRC32 hash
    //CRCsum = crc32buf((char*)buffer+chAudioOffsets[0],bufpos-chAudioOffsets[0],CRCsum);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(CRCsum,4,BOM),4,off=0x08);
    
    //copy finished file to brstm_encoded_data
    brstmi->encoded_file = new unsigned char[bufpos];
    memcpy(brstmi->encoded_file,buffer,bufpos);
    delete[] buffer;
    brstmi->encoded_file_size = bufpos;
    
    if(debugLevel>0) std::cout << "\r" << "BWAV encoding done                                   \n";
    
    return 0;
}
