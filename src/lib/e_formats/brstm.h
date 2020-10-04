//OpenRevolution BRSTM encoder
//Copyright (C) 2020 IC

#include <math.h>

unsigned char brstm_formats_encode_brstm(Brstm* brstmi,signed int debugLevel,uint8_t encodeADPCM) {
    //Check for invalid requests
    //Unsupported codec
    if(brstmi->codec > 7 || brstmi->codec == 4 || brstmi->codec == 3) {
        if(debugLevel>=0) std::cout << "Unsupported codec.\n";
        return 220;
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
    brstmi->blocks_samples;
    switch(brstmi->codec) {
        case 0: brstmi->blocks_samples = 8192; break;
        case 1: brstmi->blocks_samples = 4096; break;
        case 2: brstmi->blocks_samples = 14336; break;
        case 5: brstmi->blocks_samples = 16384; break;
        case 6: brstmi->blocks_samples = 32768; break;
    }
    brstmi->blocks_size = 8192;
    brstmi->total_blocks = brstmi->total_samples / brstmi->blocks_samples;
    if(brstmi->total_samples % brstmi->blocks_samples != 0) brstmi->total_blocks++;
    //Final block
    brstmi->final_block_samples = brstmi->total_samples % brstmi->blocks_samples;
    if(brstmi->final_block_samples == 0) brstmi->final_block_samples = brstmi->blocks_samples;
    brstmi->final_block_size = (
        brstmi->codec == 2 ? brstm_getBytesForAdpcmSamples(brstmi->final_block_samples)
        : brstmi->codec == 1 ? brstmi->final_block_samples * 2
        : brstmi->codec == 0 ? brstmi->final_block_samples
        : brstmi->codec == 5 ? ceil((double)brstmi->final_block_samples / 2)
        : brstmi->codec == 6 ? ceil((double)brstmi->final_block_samples / 4)
        : 8192
    );
    brstmi->final_block_size_p = brstmi->final_block_size;
    while(brstmi->final_block_size_p % 0x20 != 0) {brstmi->final_block_size_p++;}
    
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
    //TODO this is copied code, split this into a separate utils function?
    int16_t LoopHS1[brstmi->num_channels];
    int16_t LoopHS2[brstmi->num_channels];
    int16_t HS1[brstmi->num_channels][brstmi->total_blocks];
    int16_t HS2[brstmi->num_channels][brstmi->total_blocks];
    if(brstmi->codec == 2) {
        if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Calculating history samples...       " << std::flush;
        if(encodeADPCM) {
            //Get history samples
            for(unsigned char c=0;c<brstmi->num_channels;c++) {
                LoopHS1[c] = brstmi->loop_start > 0 ? brstmi->PCM_samples[c][brstmi->loop_start-1] : 0;
                LoopHS2[c] = brstmi->loop_start > 1 ? brstmi->PCM_samples[c][brstmi->loop_start-2] : 0;
                for(unsigned long b=0;b<brstmi->total_blocks;b++) {
                    if(b==0) {
                        //First block history samples are always zero
                        HS1[c][b] = 0;
                        HS2[c][b] = 0;
                        continue;
                    }
                    HS1[c][b] = brstmi->PCM_samples[c][(b*brstmi->blocks_samples)-1];
                    HS2[c][b] = brstmi->PCM_samples[c][(b*brstmi->blocks_samples)-2];
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
                            HS1[c][b] = 0;
                            HS2[c][b] = 0;
                        } else {
                            HS1[c][b] = cyn1;
                            HS2[c][b] = cyn2;
                        }
                        b++;
                    }
                    if(currentSample == brstmi->loop_start) {
                        if(currentSample > 0) {LoopHS1[c] = cyn1;} else {LoopHS1[c] = 0;}
                        if(currentSample > 1) {LoopHS2[c] = cyn2;} else {LoopHS2[c] = 0;}
                    }
                    
                    cyn2 = cyn1;
                    cyn1 = brstm_clamp(outSample, -32768, 32767);
                    
                    currentSample++;
                }
            }
        }
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
            //This information exists only in PCM files
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
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(LoopHS1[i],BOM),2,bufpos); //Loop HS1
            brstm_encoder_writebytes  (buffer,brstm_encoder_getByteInt16(LoopHS2[i],BOM),2,bufpos); //Loop HS2
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
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(HS1[c][b],BOM),2,bufpos); //HS1
                brstm_encoder_writebytes(buffer,brstm_encoder_getByteInt16(HS2[c][b],BOM),2,bufpos); //HS2
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
    unsigned int DATAchunkoffset = bufpos;
    brstm_encoder_writebytes(buffer,(unsigned char*)"DATA",4,bufpos);
    //DATA chunk size (will be written later)
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);
    //Padding
    brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x18},4,bufpos);
    for(unsigned int i=0;i<5;i++) {brstm_encoder_writebytes_i(buffer,new unsigned char[4]{0x00,0x00,0x00,0x00},4,bufpos);}
    
    unsigned char** ADPCMdata;
    if(brstmi->codec == 2) {
        if(encodeADPCM == 1) {
            ADPCMdata = new unsigned char* [brstmi->num_channels];
            for(unsigned int c=0;c<brstmi->num_channels;c++) {
                ADPCMdata[c] = new unsigned char[brstm_getBytesForAdpcmSamples(brstmi->total_samples)];
            }
            brstm_encode_adpcm(brstmi,ADPCMdata,debugLevel);
        } else {
            ADPCMdata = brstmi->ADPCM_data;
        }
        //Write ADPCM information to HEAD3
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Write ADPCM information to HEAD3
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCMdata[c][0],2,BOM),2,off=HEAD3_ch_info_offsets[c] + 8 + HEADchunkoffset + 42); //Initial scale
            brstm_encoder_writebytes(buffer,brstm_encoder_getByteUint(ADPCMdata[c][(unsigned long)(brstmi->loop_start / 1.75)],2,BOM),2,off=HEAD3_ch_info_offsets[c] + 8 + HEADchunkoffset + 48); //Loop initial scale
        }
    }
    
    if(debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing audio data...                                                                        " << std::flush;
    
    //Write audio data to output file buffer
    for(unsigned long b=0;b<brstmi->total_blocks-1;b++) {
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            switch(brstmi->codec) {
                //4-bit DSPADPCM
                case 2: {
                    brstm_encoder_writebytes(buffer,&ADPCMdata[c][b*brstmi->blocks_size],brstmi->blocks_size,bufpos);
                    break;
                }
                //8-bit PCM
                case 0: {
                    for(unsigned int i=0; i<brstmi->blocks_samples; i++) {
                        brstm_encoder_writebyte(buffer,(brstmi->PCM_samples[c][b*brstmi->blocks_samples+i])/256,bufpos);
                    }
                    break;
                }
                //16-bit PCM
                case 1: {
                    for(unsigned int i=0; i<brstmi->blocks_samples; i++) {
                        brstm_encoder_writebytes(
                            buffer,brstm_encoder_getByteInt16(brstmi->PCM_samples[c][b*brstmi->blocks_samples+i],BOM),2,bufpos
                        );
                        if(!(b%4) && debugLevel>0) std::cout << "\r" << brstm_encoder_nextspinner(spinner) << " Writing PCM data... ("
                        << floor(((float)(b*brstmi->blocks_samples+i)/brstmi->total_samples) * 100) << "%)            ";
                    }
                    break;
                }
                //4-bit unsigned PCM
                case 5: {
                    uint8_t byte_tmp;
                    for(unsigned int i=0; i<brstmi->blocks_samples; i+=2) {
                        byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i] + 32768) / 4096) << 4);
                        byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+1] + 32768) / 4096);
                        brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                    }
                    break;
                }
                //2-bit unsigned PCM
                case 6: {
                    uint8_t byte_tmp;
                    for(unsigned int i=0; i<brstmi->blocks_samples; i+=4) {
                        byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i] + 32768) / 16384) << 6);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+1] + 32768) / 16384) << 4);
                        byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+2] + 32768) / 16384) << 2);
                        byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][b*brstmi->blocks_samples+i+3] + 32768) / 16384);
                        brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                    }
                    break;
                }
            }
        }
    }
    //Final block
    for(unsigned int c=0;c<brstmi->num_channels;c++) {
        unsigned int i=brstmi->final_block_size;
        switch(brstmi->codec) {
            //4-bit DSPADPCM
            case 2: {
                brstm_encoder_writebytes(buffer,&ADPCMdata[c][(brstmi->total_blocks-1)*brstmi->blocks_size],brstmi->final_block_size,bufpos);
                break;
            }
            //8-bit PCM
            case 0: {
                for(unsigned int i=0; i<brstmi->final_block_samples; i++) {
                    brstm_encoder_writebyte(buffer,(brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i])/256,bufpos);
                }
                break;
            }
            //16-bit PCM
            case 1: {
                for(unsigned int i=0; i<brstmi->final_block_samples; i++) {
                    brstm_encoder_writebytes(
                        buffer,brstm_encoder_getByteInt16(brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i],BOM),2,bufpos
                    );
                }
                break;
            }
            //4-bit unsigned PCM
            case 5: {
                uint8_t byte_tmp;
                for(unsigned int i=0; i<brstmi->final_block_samples; i+=2) {
                    byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i] + 32768) / 4096) << 4);
                    if(i+1 < brstmi->final_block_samples) {
                        byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+1] + 32768) / 4096);
                    }
                    brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                }
                break;
            }
            //2-bit unsigned PCM
            case 6: {
                uint8_t byte_tmp;
                for(unsigned int i=0; i<brstmi->final_block_samples; i+=4) {
                    byte_tmp = (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i] + 32768) / 16384) << 6);
                    if(i+1 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+1] + 32768) / 16384) << 4);}
                    if(i+2 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)((((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+2] + 32768) / 16384) << 2);}
                    if(i+3 < brstmi->final_block_samples) {byte_tmp = byte_tmp | (uint8_t)(((int32_t)brstmi->PCM_samples[c][(brstmi->total_blocks-1)*brstmi->blocks_samples+i+3] + 32768) / 16384);}
                    brstm_encoder_writebyte(buffer,byte_tmp,bufpos);
                }
                break;
            }
        }
        if(brstmi->codec == 2 && encodeADPCM == 1) delete[] ADPCMdata[c]; //delete the ADPCM data only if we made it locally
        //padding
        while(i<brstmi->final_block_size_p) {
            brstm_encoder_writebyte(buffer,0x00,bufpos);
            i++;
        }
    }
    if(brstmi->codec == 2 && encodeADPCM == 1) delete[] ADPCMdata; //delete the ADPCM data only if we made it locally
    
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
    
    if(debugLevel>0) std::cout << "\r" << "BRSTM encoding done                                                     \n" << std::flush;
    
    return 0;
}
