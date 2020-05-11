//Revolution BWAV reader
//Copyright (C) 2020 Extrasklep

#include <math.h>

unsigned char brstm_formats_read_bwav(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio) {
    bool &BOM = brstmi->BOM;
    //Byte Order Mark
    if(brstm_getSliceAsInt16Sample(fileData,0x04,1) == -257) {
        BOM = 1; //Big endian
    } else {
        BOM = 0; //Little endian
    }
    //BWAV is weird and stupid
    brstmi->num_channels = brstm_getSliceAsNumber(fileData,0x0E,2,BOM);
    //channel information chunk
    unsigned int channelPans[brstmi->num_channels];
    for(unsigned int c=0; c<brstmi->num_channels; c++) {
        unsigned int offset = 0x10 + c*0x4C;
        //history samples
        brstmi->ADPCM_hsamples_1[c] = new int16_t[1];
        brstmi->ADPCM_hsamples_2[c] = new int16_t[1];
        brstmi->ADPCM_hsamples_1[c][0] = brstm_getSliceAsInt16Sample(fileData,offset+0x46,BOM);
        brstmi->ADPCM_hsamples_2[c][0] = brstm_getSliceAsInt16Sample(fileData,offset+0x48,BOM);
        //coefs
        if(brstmi->codec == 2) {
            for(unsigned int i=0;i<16;i++) {
                brstmi->ADPCM_coefs[c][i] = brstm_getSliceAsInt16Sample(fileData,offset+0x10+i*2,BOM);
            }
        }
        
        channelPans[c] = brstm_getSliceAsNumber(fileData,offset+0x02,2,BOM);
        brstmi->codec = brstm_getSliceAsNumber(fileData,offset+0x00,2,BOM) + 1; //add 1 so the codec number is like BRSTM's codec number
        brstmi->sample_rate = brstm_getSliceAsNumber(fileData,offset+0x04,4,BOM);
        brstmi->total_samples = brstm_getSliceAsNumber(fileData,offset+0x08,4,BOM);
        brstmi->loop_flag = ( (int32_t)brstm_getSliceAsNumber(fileData,offset+0x3C,4,BOM) != -1 );
        brstmi->loop_start = brstm_getSliceAsNumber(fileData,offset+0x40,4,BOM);
    }
    brstmi->audio_offset = brstm_getSliceAsNumber(fileData,0x40,4,BOM);
    brstmi->total_blocks = 1;
    brstmi->blocks_size = brstmi->num_channels > 1 ? (brstm_getSliceAsNumber(fileData,0x8C,4,BOM) - brstmi->audio_offset) : (brstmi->codec == 1 ? brstmi->total_samples*2 : brstm_getBytesForAdpcmSamples(brstmi->total_samples));
    brstmi->blocks_samples = brstmi->total_samples;
    brstmi->final_block_size = brstmi->blocks_size;
    brstmi->final_block_samples = brstmi->blocks_samples;
    brstmi->final_block_size_p = brstmi->final_block_size;
    
    //Guess track data based on channel pans and channel count
    brstmi->num_tracks = (brstmi->num_channels > 1 && brstmi->num_channels%2 == 0 && channelPans[0] == 0 && channelPans[1] == 1) ? brstmi->num_channels/2 : brstmi->num_channels;
    unsigned char track_num_channels = brstmi->num_tracks*2 == brstmi->num_channels ? 2 : 1;
    brstmi->track_desc_type = 0;
    for(unsigned char c=0; c<brstmi->num_channels; c++) {
        brstmi->track_num_channels[c/track_num_channels] = track_num_channels;
        if(track_num_channels == 1 || c%2 == 0) brstmi->track_lchannel_id[c/track_num_channels] = c;
        if(track_num_channels == 2 && c%2 == 1) brstmi->track_rchannel_id[c/track_num_channels] = c;
    }
    if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: BWAV track information is guessed\n";}
    
    //Audio
    if(decodeAudio) {
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Create new array of samples for the current channel
            switch(decodeAudio) {
                case 1: brstmi->PCM_samples[c] = new int16_t[brstmi->total_samples]; break;
                case 2: brstmi->ADPCM_data [c] = new unsigned char[brstmi->blocks_size]; break;
            }
            
            unsigned long posOffset=0+(brstmi->blocks_size*c);
            unsigned long outputPos = 0; //position in PCM samples or ADPCM data output array
            
            for(unsigned long b=0;b<brstmi->total_blocks;b++) {
                if(decodeAudio == 1) {
                    //Decode audio normally
                    brstm_decode_block(brstmi,b,c,fileData,0,brstmi->PCM_samples,b*brstmi->blocks_samples);
                } else {
                    //Write raw data
                    //Get block size
                    unsigned int currentBlockSize    = brstmi->blocks_size;
                    unsigned int currentBlockSamples = brstmi->blocks_samples;
                    //Final block
                    if(b==brstmi->total_blocks-1) {
                        currentBlockSize    = brstmi->final_block_size;
                        currentBlockSamples = brstmi->final_block_samples;
                    }
                    //Get data from just the current block
                    unsigned char* blockData = brstm_getSlice(fileData,brstmi->audio_offset+posOffset,currentBlockSize);
                    if(brstmi->codec == 2) {
                        for(unsigned int i=0; i<currentBlockSize; i++) {
                            brstmi->ADPCM_data[c][outputPos++] = blockData[i];
                        }
                    } else {
                        if(debugLevel>=0) {std::cout << "Cannot write raw ADPCM data because the codec is not ADPCM.\n";}
                        return 220;
                    }
                }
                posOffset+=brstmi->blocks_size*brstmi->num_channels;
            }
        }
    } else {
        if(debugLevel>=0) {std::cout << "Warning: Realtime decoding works like full decoding for this format\n";}
    }
    
    return 0;
}
