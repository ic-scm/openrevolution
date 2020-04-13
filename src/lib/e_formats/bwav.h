//Revolution BWAV encoder
//Copyright (C) 2020 Extrasklep

//Thanks to https://gota7.github.io/Citric-Composer/specs/binaryWav.html
//And again BWAV is weird and stupid

#include <math.h>
//BWAV header requires a CRC checksum
#include "../crc/crc_32.c"

unsigned char brstm_formats_encode_bwav(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    //Check for invalid requests
    //Unsupported codec
    if(brstmi->codec != 2 && brstmi->codec != 1) {
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
    if(brstmi->codec == 2) {
        if(encodeADPCM == 1) {
            ADPCMdata = new unsigned char* [brstmi->num_channels];
            for(unsigned char c=0;c<brstmi->num_channels;c++) {
                ADPCMdata[c] = new unsigned char[brstm_encoder_GetBytesForAdpcmSamples(brstmi->total_samples)];
            }
            brstm_encode_adpcm(brstmi,ADPCMdata,debugLevel);
        } else {
            ADPCMdata = brstmi->ADPCM_data;
        }
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Write ADPCM information to channel infos
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCMdata[c][0],2,BOM),2,off=0x54 + c*0x4C); //Initial predictor scale
        }
    } //Else do nothing because the PCM audio is already in PCM_samples
    
    //I had to do this because calculating it the simpler way just didn't work with multi channel files for some reason
    unsigned char* CRCbuf = new unsigned char[TotalBytesPerChannel*brstmi->num_channels];
    unsigned long CRCbufpos = 0;
    //Write audio data to file
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing ADPCM data...                                                                        ";
    for(unsigned int c=0;c<brstmi->num_channels;c++) {
        //Write padding until we reach the correct offset to audio data
        while(bufpos != chAudioOffsets[c]) brstm_encoder_writebyte(buffer,0x00,bufpos);
        //Write audio data
        if(brstmi->codec == 2) {
            brstm_encoder_writebytes(buffer,ADPCMdata[c],TotalBytesPerChannel,bufpos);
            //Write checksum calculation buffer
            brstm_encoder_writebytes(CRCbuf,ADPCMdata[c],TotalBytesPerChannel,CRCbufpos);
            if(encodeADPCM == 1) delete[] ADPCMdata[c]; //delete the ADPCM data only if we made it locally
        } else {
            //PCM16
            //TODO This is slow because of the brstm_encoder_getByteInt16 function, make those functions faster while keeping support for both big endian and little endian processors
            for(unsigned long s=0;s<brstmi->total_samples;s++) {
                unsigned char* samplebytes = brstm_encoder_getByteInt16(brstmi->PCM_samples[c][s],BOM);
                brstm_encoder_writebytes(buffer,samplebytes,2,bufpos);
                brstm_encoder_writebytes(CRCbuf,samplebytes,2,CRCbufpos);
                //Console output, if PCM writing wasn't so slow then this wouldn't have to be here
                if(!(s%8192) && debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing PCM data... (CH " << (unsigned int)c+1 << "/" << brstmi->num_channels << " " << floor(((float)s/brstmi->total_samples) * 100) << "%)          ";
            }
        }
    }
    
    if(encodeADPCM == 1 && brstmi->codec == 2) delete[] ADPCMdata; //delete the ADPCM data only if we made it locally
    
    //Finalize file (write some things we couldn't write earlier)
    //CRC32 hash
    CRCsum = crc32buf((char*)CRCbuf,TotalBytesPerChannel*brstmi->num_channels,CRCsum);
    delete[] CRCbuf;
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(CRCsum,4,BOM),4,off=0x08);
    
    //copy finished file to brstm_encoded_data
    brstmi->encoded_file = new unsigned char[bufpos];
    memcpy(brstmi->encoded_file,buffer,bufpos);
    delete[] buffer;
    brstmi->encoded_file_size = bufpos;
    
    if(debugLevel>0) std::cout << "\r" << "BWAV encoding done                                   \n";
    
    return 0;
}
