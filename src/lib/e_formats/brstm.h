//OpenRevolution BRSTM encoder
//Copyright (C) 2020 IC

#include <math.h>

unsigned char brstm_formats_encode_brstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    //Check for invalid requests
    //Unsupported codec
    if(brstmi->codec > 2) {
        if(debugLevel>=0) std::cout << "Unsupported codec.\n";
        return 220;
    }
    if(brstmi->codec != 2) {
        if(debugLevel>=0) std::cout << "Warning: PCM BRSTM files are untested\n";
    }
    
    bool &BOM = brstmi->BOM;
    char spinner = '/';
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Starting BRSTM encode...                " << std::flush;
    
    delete[] brstmi->encoded_file;
    unsigned char* buffer = new unsigned char[(brstmi->total_samples*brstmi->num_channels*
    (brstmi->codec == 1 ? 2 : 1)) //For 16 bit PCM
    +((brstmi->total_samples*brstmi->num_channels/14336)*4)+brstmi->num_channels*256+8192];
    unsigned long  bufpos = 0;
    unsigned long  off; //for argument 4 of brstm_encoder_writebytes when we don't write to the end of the buffer
    
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (RSTM)             " << std::flush;
    
    
    //Header
    brstm_encoder_writebytes(buffer,(unsigned char*)"RSTM",4,bufpos);
    //Byte order mark
    switch(BOM) {
        //LE
        case 0: brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0xFF,0xFE},2,bufpos); break;
        //BE
        case 1: brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0xFE,0xFF},2,bufpos); break;
    }
    //Version
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x01,0x00},2,bufpos);
    //File size (will be actually written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Header size, number of chunks
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x40,0x00,0x02},4,bufpos);
    //Sizes and offsets of chunks (will be written later) + 24 byte zero padding
    for(unsigned int i=0;i<12;i++) { brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); }
    
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (HEAD)             " << std::flush;
    
    
    //HEAD chunk
    unsigned int HEADchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"HEAD",4,bufpos);
    //HEAD chunk size (will be written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //HEAD chunk header
    for(unsigned int i=0;i<3;i++) {
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x01,0x00,0x00,0x00},4,bufpos); //Marker
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //HEAD chunk part offsets (will be written later)
    }
    
    //HEAD1
    //Write HEAD1 offset to HEAD header
    unsigned int HEAD1offset = bufpos - HEADchunkoffset - 8;
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(HEAD1offset,4,BOM),4,off=HEADchunkoffset+0x0C);
    
    //Calculate blocks
    brstm_encoder_calculateStandardBlockInfo(brstmi);
    
    //HEAD1 data
    brstm_encoder_writebyte(buffer,brstmi->codec,bufpos);
    brstm_encoder_writebyte(buffer,brstmi->loop_flag,bufpos);
    brstm_encoder_writebyte(buffer,brstmi->num_channels,bufpos);
    brstm_encoder_writebyte(buffer,0,bufpos); //padding
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->sample_rate,2,BOM),2,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //padding
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->loop_start,4,BOM),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_samples,4,BOM),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(0,4,BOM),4,bufpos); //Audio offset, will be written later
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->total_blocks,4,BOM),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->blocks_size,4,BOM),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->blocks_samples,4,BOM),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->final_block_size,4,BOM),4,bufpos); //Final block size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->final_block_samples,4,BOM),4,bufpos);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->final_block_size_p,4,BOM),4,bufpos); //Padded final block size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->codec == 2 ? brstmi->blocks_samples : 0,4,BOM),4,bufpos);  //ADPC samples per entry
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->codec == 2 ? 4 : 0,4,BOM),4,bufpos); //ADPC bytes per entry
    
    //HEAD2
    //Write HEAD2 offset to HEAD header
    unsigned int HEAD2offset = bufpos - HEADchunkoffset - 8;
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(HEAD2offset,4,BOM),4,off=HEADchunkoffset+0x14);
    //HEAD2 header
    brstm_encoder_writebyte(buffer,brstmi->num_tracks,bufpos);
    brstm_encoder_writebyte(buffer,brstmi->track_desc_type,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //padding
    //offset table
    unsigned long HEAD2_track_info_offsets[8] = {0,0,0,0,0,0,0,0};
    for(unsigned int i=0;i<brstmi->num_tracks;i++) {
        brstm_encoder_writebyte(buffer,1,bufpos);
        brstm_encoder_writebyte(buffer,brstmi->track_desc_type,bufpos);
        brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //padding
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //Offset to track description, will be written later from HEAD2_track_info_offsets
    }
    //track descriptions
    for(unsigned int i=0;i<brstmi->num_tracks;i++) {
        //write offset to offset table
        HEAD2_track_info_offsets[i] = bufpos - HEADchunkoffset - 8;
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(HEAD2_track_info_offsets[i],4,BOM),4,off=HEADchunkoffset + HEAD2offset + 12 + 8*i + 4);
        //write additional type 1 data
        if(brstmi->track_desc_type == 1) {
            brstm_encoder_writebyte(buffer,brstmi->track_volume[i],bufpos);
            brstm_encoder_writebyte(buffer,brstmi->track_panning[i],bufpos);
            brstm_encoder_writebytes_i(buffer,new unsigned char[6]{0x00,0x00,0x00,0x00,0x00,0x00},6,bufpos); //padding
        }
        //standard data
        brstm_encoder_writebyte(buffer,brstmi->track_num_channels[i],bufpos);
        brstm_encoder_writebyte(buffer,brstmi->track_lchannel_id [i],bufpos);
        brstm_encoder_writebyte(buffer,brstmi->track_rchannel_id [i],bufpos);
        brstm_encoder_writebyte(buffer,0,bufpos); //padding
    }
    
    //Calculate history samples
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Calculating history samples...       " << std::flush;
    
    brstm_HSData_t HSData;
    //Allocate HS storage
    for(unsigned int ch=0; ch<brstmi->num_channels; ch++) {
        HSData.HS1[ch] = new int16_t[brstmi->total_blocks];
        HSData.HS2[ch] = new int16_t[brstmi->total_blocks];
    }
    if(brstmi->codec == 2) {
        brstm_encoder_adpcm_calculateAdpcmData(brstmi, encodeADPCM, &HSData);
    }
    
    //HEAD3
    //Write HEAD3 offset to HEAD header
    unsigned int HEAD3offset = bufpos - HEADchunkoffset - 8;
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(HEAD3offset,4,BOM),4,off=HEADchunkoffset+0x1C);
    //HEAD3 header
    brstm_encoder_writebyte(buffer,brstmi->num_channels,bufpos);
    brstm_encoder_writebytes_i(buffer,new unsigned char[3]{0x00,0x00,0x00},3,bufpos); //padding
    //offset table
    unsigned long HEAD3_ch_info_offsets  [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    for(unsigned int i=0;i<brstmi->num_channels;i++) {
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x01,0x00,0x00,0x00},4,bufpos); //Marker
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //Offset to channel information, will be written later from HEAD3_ch_info_offsets
    }
    //channel info
    for(unsigned int i=0;i<brstmi->num_channels;i++) {
        //write offset to offset table
        HEAD3_ch_info_offsets[i] = bufpos - HEADchunkoffset - 8;
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(HEAD3_ch_info_offsets[i],4,BOM),4,off=HEADchunkoffset + HEAD3offset + 12 + 8*i + 4);
        //write channel info
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x01,0x00,0x00,0x00},4,bufpos); //Marker
        if(brstmi->codec == 2) {
            //This information exists only in ADPCM files
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteUint(bufpos - HEADchunkoffset - 4,4,BOM),4,bufpos); //Offset to ADPCM coefs?
            //Write coefs
            for(unsigned int a=0;a<16;a++) {
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(brstmi->ADPCM_coefs[i][a],BOM),2,bufpos);
            }
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Gain, always zero
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Initial scale, will be written later
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS1, always zero
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //HS2, always zero
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Loop predictor scale, will be written later
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(HSData.LoopHS1[i],BOM),2,bufpos); //Loop HS1
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(HSData.LoopHS2[i],BOM),2,bufpos); //Loop HS2
            brstm_encoder_writebytes_i(buffer,new unsigned char[2]{0x00,0x00},2,bufpos); //Padding
        } else {
            brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos); //Padding / empty offset to ADPCM coefs for PCM files
        }
    }
    
    unsigned int HEADchunksize = bufpos - HEADchunkoffset;
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[6]{0x00,0x00,0x00,0x00,0x00,0x00},6,bufpos);
    while(bufpos % 32 != 0) {
        brstm_encoder_writebyte(buffer,0,bufpos);
        HEADchunksize = bufpos - HEADchunkoffset;
    }
    //Write HEAD chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(HEADchunksize,4,BOM),4,off=HEADchunkoffset+4);
    
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (ADPC)             " << std::flush;
    
    
    //ADPC chunk (ADPCM files only)
    unsigned int ADPCchunkoffset = 0;
    unsigned int ADPCchunksize   = 0;
    if(brstmi->codec == 2) {
        ADPCchunkoffset = bufpos;
        brstm_encoder_writebytes(buffer,(unsigned char*)"ADPC",4,bufpos);
        //ADPC chunk size (will be written later)
        brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
        //Write ADPC history samples
        for(unsigned long b=0;b<brstmi->total_blocks;b++) {
            for(unsigned char c=0;c<brstmi->num_channels;c++) {
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(HSData.HS1[c][b],BOM),2,bufpos); //HS1
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(HSData.HS2[c][b],BOM),2,bufpos); //HS2
            }
        }
        ADPCchunksize = bufpos - ADPCchunkoffset;
        //Padding
        while(bufpos % 32 != 0) {
            brstm_encoder_writebyte(buffer,0,bufpos);
            ADPCchunksize = bufpos - ADPCchunkoffset;
        }
        //Write ADPC chunk length
        brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCchunksize,4,BOM),4,off=ADPCchunkoffset+4);
    }
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Building headers... (DATA)             " << std::flush;
    
    
    //DATA chunk
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing audio data...                   " << std::flush;
    unsigned int DATAchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"DATA",4,bufpos);
    //DATA chunk size (will be written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x18},4,bufpos);
    for(unsigned int i=0;i<5;i++) {brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);}
    
    //DSPADPCM encoding
    if(brstmi->codec == 2) {
        if(encodeADPCM == 1) {
            uint32_t adpcmbytes = brstm_getBytesForAdpcmSamples(brstmi->total_samples);
            for(unsigned int c=0;c<brstmi->num_channels;c++) {
                brstmi->ADPCM_data[c] = new unsigned char[adpcmbytes];
            }
            brstm_encode_adpcm(brstmi, brstmi->ADPCM_data, debugLevel);
        }
    }
    
    //Write audio
    brstm_encoder_doStandardAudioWrite(brstmi, buffer, bufpos, debugLevel);
    
    //Finalize and clean up DSPADPCM encoding
    if(brstmi->codec == 2) {
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Write ADPCM information to HEAD3
            //Initial scale
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->ADPCM_data[c][0],2,BOM),2,off=HEAD3_ch_info_offsets[c] + 8 + HEADchunkoffset + 42);
            //Loop initial scale
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(brstmi->ADPCM_data[c][(unsigned long)(brstmi->loop_start / 1.75)],2,BOM),2,off=HEAD3_ch_info_offsets[c] + 8 + HEADchunkoffset + 48);
            
            if(encodeADPCM == 1) {
                //delete the ADPCM data only if we made it locally
                delete[] brstmi->ADPCM_data[c];
                brstmi->ADPCM_data[c] = nullptr;
            }
        }
    }
    
    unsigned int DATAchunksize = bufpos - DATAchunkoffset;
    //Write DATA chunk length
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunksize,4,BOM),4,off=DATAchunkoffset+4);
    
    
    //Finalize file (write proper filesize etc)
    //Filesize
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(bufpos,4,BOM),4,off=0x08);
    //HEAD offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(HEADchunkoffset,4,BOM),4,off=0x10);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(HEADchunksize,4,BOM),4,off=0x14);
    //ADPC offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCchunkoffset,4,BOM),4,off=0x18);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCchunksize,4,BOM),4,off=0x1C);
    //DATA offset and size
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunkoffset,4,BOM),4,off=0x20);
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunksize,4,BOM),4,off=0x24);
    //ADPCM offset in HEAD1
    brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(DATAchunkoffset+0x20,4,BOM),4,off=HEADchunkoffset + 8 + HEAD1offset + 0x10);
    
    //copy finished file to brstm_encoded_data
    brstmi->encoded_file = new unsigned char[bufpos];
    memcpy(brstmi->encoded_file,buffer,bufpos);
    delete[] buffer;
    brstmi->encoded_file_size = bufpos;
    
    //Free HS storage
    for(unsigned int ch=0; ch<brstmi->num_channels; ch++) {
        delete[] HSData.HS1[ch];
        delete[] HSData.HS2[ch];
    }
    
    if(debugLevel>0) std::cout << "\r" << "BRSTM encoding done                                                     \n" << std::flush;
    
    return 0;
}
