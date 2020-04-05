//C++ BRSTM reader
//Copyright (C) 2020 Extrasklep
//https://github.com/Extrasklep/brstm

#pragma once
#include <iostream>
#include <fstream>
#include <cstring>

//Bool endian: 0 = little endian, 1 = big endian

//Format information
const unsigned int BRSTM_formats_count = 5;
//File header magic words for each file format
const char* BRSTM_formats_str[BRSTM_formats_count] = {"    ","RSTM","CSTM","FSTM","BWAV"};
//Offset to the audio offset information in each format (32 bit integer)
const unsigned int BRSTM_formats_audio_off_off[BRSTM_formats_count] = {0x00,0x20,0x00,0x00,0x40};
//Short human readable strings
const char* BRSTM_formats_short_usr_str[BRSTM_formats_count] = {"None","BRSTM","BCSTM","BFSTM","BWAV"};
//Long human readable strings
const char* BRSTM_formats_long_usr_str [BRSTM_formats_count] = {
"Unknown format",
"Binary Revolution Stream",
"Binary CTR Stream",
"Binary Cafe Stream",
"Nintendo BWAV"
};

//Codec information
const unsigned int BRSTM_codecs_count = 3;
//Human readable strings
const char* BRSTM_codecs_usr_str[BRSTM_codecs_count] = {
"8-bit PCM",
"16-bit PCM",
"4-bit DSPADPCM"
};


//The BRSTM struct
struct Brstm {
    //Byte order mark
    bool BOM;
    //File type, 1 = BRSTM, see above for full list
    unsigned int  file_format;
    //Audio codec, 0 = PCM8, 1 = PCM16, 2 = DSPADPCM
    unsigned int  codec;
    bool          loop_flag;
    unsigned int  num_channels;
    unsigned long sample_rate;
    unsigned long loop_start;
    unsigned long total_samples;
    unsigned long audio_offset;
    unsigned long total_blocks;
    unsigned long blocks_size;
    unsigned long blocks_samples;
    unsigned long final_block_size;
    unsigned long final_block_samples;
    unsigned long final_block_size_p;
    
    //track information
    unsigned int  num_tracks;
    unsigned int  track_desc_type;
    unsigned int  track_num_channels[8] = {0,0,0,0,0,0,0,0};
    unsigned int  track_lchannel_id [8] = {0,0,0,0,0,0,0,0};
    unsigned int  track_rchannel_id [8] = {0,0,0,0,0,0,0,0};
    unsigned int  track_volume      [8] = {0,0,0,0,0,0,0,0};
    unsigned int  track_panning     [8] = {0,0,0,0,0,0,0,0};
    
    int16_t* PCM_samples[16];
    int16_t* PCM_buffer[16];
    
    unsigned char* ADPCM_data  [16];
    unsigned char* ADPCM_buffer[16]; //Not used yet
    int16_t  ADPCM_coefs [16][16];
    int16_t* ADPCM_hsamples_1   [16];
    int16_t* ADPCM_hsamples_2   [16];
    
    //Encoder
    unsigned char* encoded_file;
    unsigned long  encoded_file_size;
    
    //Things you probably shouldn't touch
    //block cache
    int16_t* PCM_blockbuffer[16];
    int PCM_blockbuffer_currentBlock = -1;
    bool getbuffer_useBuffer = true;
};


//Util functions
unsigned char* brstm_slice;
char* brstm_slicestring;

long brstm_clamp(long value, long min, long max) {
  return value <= min ? min : value >= max ? max : value;
}

//Get slice of data
unsigned char* brstm_getSlice(const unsigned char* data,unsigned long start,unsigned long length) {
    delete[] brstm_slice;
    brstm_slice = new unsigned char[length];
    for(unsigned int i=0;i<length;i++) {
        brstm_slice[i]=data[i+start];
    }
    return brstm_slice;
}

//Get slice and convert it to a number
unsigned long brstm_getSliceAsNumber(const unsigned char* data,unsigned long start,unsigned long length,bool endian) {
    if(length>4) {length=4;}
    unsigned long number=0;
    unsigned char* bytes=brstm_getSlice(data,start,length);
    unsigned char pos;
    if(endian) {
        pos=length-1; //Read as big endian
    } else {
        pos = 0; //Read as little endian
    }
    unsigned long pw=1; //Multiply by 1,256,65536...
    for(unsigned int i=0;i<length;i++) {
        if(i>0) {pw*=256;}
        number+=bytes[pos]*pw;
        if(endian) {pos--;} else {pos++;}
    }
    return number;
}

//Get slice as signed 16 bit number
signed int brstm_getSliceAsInt16Sample(const unsigned char * data,unsigned long start,bool endian) {
    unsigned int length=2;
    unsigned long number=0;
    unsigned char* bytes=brstm_getSlice(data,start,length);
    unsigned char little=bytes[endian];
    signed   char big=bytes[!endian];
    number=little+big*256;
    return number;
}

//Get slice as a null terminated string
char* brstm_getSliceAsString(const unsigned char* data,unsigned long start,unsigned long length) {
    unsigned char slicestr[length+1];
    unsigned char* bytes=brstm_getSlice(data,start,length);
    for(unsigned int i=0;i<length;i++) {
        slicestr[i]=bytes[i];
        if(slicestr[i]=='\0') {slicestr[i]=' ';}
    }
    slicestr[length]='\0';
    delete[] brstm_slice;
    brstm_slicestring = new char[length+1];
    for(unsigned int i=0;i<length+1;i++) {
        brstm_slicestring[i]=slicestr[i];
    }
    return brstm_slicestring;
}

/* 
 * Read the BRSTM file headers and optionally decode the audio data.
 * 
 * brstmi: Your BRSTM struct
 * fileData: BRSTM file
 * debugLevel:
 *    -1 = Never output anything
 *     0 = Only output errors/warnings
 *     1 = Output information about the BRSTM
 *     2 = Output all information about the BRSTM (offsets, sizes, ADPCM information etc.)
 * decodeADPCM:
 *     0 = Don't decode the audio data (Don't read the DATA chunk)
 *     1 = Decode audio data into PCM_samples
 *     2 = Write the raw ADPCM data into ADPCM_data
 * 
 * Returns error code (>127) or warning code (<128):
 *        0 = No error
 *      255 = Invalid BRSTM file (Doesn't begin with RSTM)
 *      250 = Invalid HEAD chunk (Doesn't begin with HEAD)
 *      249 = Too many channels
 *      248 = Too many tracks
 *      244 = Unknown track info type
 *      240 = Invalid ADPC chunk (Doesn't begin with ADPC)
 *      230 = Invalid DATA chunk (Doesn't begin with DATA)
 *      220 = Unsupported or unknown audio codec
 *      210 = Unsupported file format
 *      200 = Unknown error (this should never happen)
 */
unsigned char brstm_read(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeADPCM) {
    
    bool &BOM = brstmi->BOM;
    
    //Find filetype
    brstmi->file_format = 0;
    for(unsigned int t=0;t<BRSTM_formats_count;t++) {
        if(strcmp(BRSTM_formats_str[t],brstm_getSliceAsString(fileData,0,strlen(BRSTM_formats_str[t]))) == 0) {
            brstmi->file_format = t;
            break;
        }
    }
    if(brstmi->file_format == 0) {
        if(debugLevel>=0) {std::cout << "Invalid or unsupported file format.\n";}
        return 210;
    }
    
    if(debugLevel>0) std::cout << "File format: " << BRSTM_formats_short_usr_str[brstmi->file_format] << '\n';
    
    //BRSTM
    if(brstmi->file_format == 1) {
    //I know this isn't aligned properly but I won't change it for now
        
    //Useless BRSTM specific variables
    //Header
    unsigned long file_size;
    unsigned int header_size;
    unsigned int num_chunks;
    unsigned long HEAD_offset;
    unsigned long HEAD_size;
    unsigned long ADPC_offset;
    unsigned long ADPC_size;
    unsigned long DATA_offset;
    unsigned long DATA_size;
    //HEAD
    unsigned long HEAD_length;
    unsigned long HEAD1_offset;
    unsigned long HEAD2_offset;
    unsigned long HEAD3_offset;
    //HEAD1
    unsigned long HEAD1_samples_per_ADPC;
    unsigned long HEAD1_bytes_per_ADPC;
    //HEAD2
    unsigned long HEAD2_track_info_offsets[8] = {0,0,0,0,0,0,0,0};
    //HEAD3
    unsigned long HEAD3_ch_info_offsets  [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned int  HEAD3_ch_gain          [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned int  HEAD3_ch_initial_scale [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      signed int  HEAD3_ch_hsample_1     [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      signed int  HEAD3_ch_hsample_2     [16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned int  HEAD3_ch_loop_ini_scale[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      signed int  HEAD3_ch_loop_hsample_1[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      signed int  HEAD3_ch_loop_hsample_2[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    //ADPC
    unsigned long ADPC_total_length;
    unsigned long ADPC_total_entries;
    //DATA
    unsigned long DATA_total_length;
    
    
    //Create references to our struct for the variables we'll care about later
    //This is kinda ugly but it makes the variable names in this reader nice and it works perfectly fine
    unsigned int  &HEAD1_codec = brstmi->codec;
    bool          &HEAD1_loop = brstmi->loop_flag;
    unsigned int  &HEAD1_num_channels = brstmi->num_channels;
    unsigned long &HEAD1_sample_rate = brstmi->sample_rate;
    unsigned long &HEAD1_loop_start = brstmi->loop_start;
    unsigned long &HEAD1_total_samples = brstmi->total_samples;
    unsigned long &HEAD1_ADPCM_offset = brstmi->audio_offset;
    unsigned long &HEAD1_total_blocks = brstmi->total_blocks;
    unsigned long &HEAD1_blocks_size = brstmi->blocks_size;
    unsigned long &HEAD1_blocks_samples = brstmi->blocks_samples;
    unsigned long &HEAD1_final_block_size = brstmi->final_block_size;
    unsigned long &HEAD1_final_block_samples = brstmi->final_block_samples;
    unsigned long &HEAD1_final_block_size_p = brstmi->final_block_size_p;
    
    unsigned int  &HEAD2_num_tracks = brstmi->num_tracks;
    unsigned int  &HEAD2_track_type = brstmi->track_desc_type;
    
    unsigned int (&HEAD2_track_num_channels)[8] = brstmi->track_num_channels;
    unsigned int (&HEAD2_track_lchannel_id) [8] = brstmi->track_lchannel_id;
    unsigned int (&HEAD2_track_rchannel_id) [8] = brstmi->track_rchannel_id;
    
    unsigned int (&HEAD2_track_volume)      [8] = brstmi->track_volume;
    unsigned int (&HEAD2_track_panning)     [8] = brstmi->track_panning;
    
    unsigned int  &HEAD3_num_channels = brstmi->num_channels;
    
    int16_t* (&PCM_samples)[16] = brstmi->PCM_samples;
    int16_t* (&PCM_buffer)[16] = brstmi->PCM_buffer;
    
    unsigned char* (&ADPCM_data)  [16] = brstmi->ADPCM_data;
    unsigned char* (&ADPCM_buffer)[16] = brstmi->ADPCM_buffer;
    int16_t  (&HEAD3_int16_adpcm) [16][16] = brstmi->ADPCM_coefs;
    int16_t* (&ADPC_hsamples_1)   [16] = brstmi->ADPCM_hsamples_1;
    int16_t* (&ADPC_hsamples_2)   [16] = brstmi->ADPCM_hsamples_2;
    
    //Check if the header matches RSTM
    char* magicstr=brstm_getSliceAsString(fileData,0,4);
    char  emagic1[5]="RSTM";
    char  emagic2[5]="HEAD";
    char  emagic3[5]="ADPC";
    char  emagic4[5]="DATA";
    if(strcmp(magicstr,emagic1) == 0) {
        //Byte Order Mark
        if(brstm_getSliceAsInt16Sample(fileData,0x04,1) == -257) {
            BOM = 1; //Big endian
        } else {
            BOM = 0; //Little endian
        }
        if(debugLevel>1) {std::cout << "BOM: " << (BOM ? "Big endian" : "Little endian") << '\n';}
        //Start reading header
        file_size   = brstm_getSliceAsNumber(fileData,0x08,4,BOM);
        header_size = brstm_getSliceAsNumber(fileData,0x0C,2,BOM);
        num_chunks  = brstm_getSliceAsNumber(fileData,0x0E,2,BOM);
        HEAD_offset = brstm_getSliceAsNumber(fileData,0x10,4,BOM);
        HEAD_size   = brstm_getSliceAsNumber(fileData,0x14,4,BOM);
        ADPC_offset = brstm_getSliceAsNumber(fileData,0x18,4,BOM);
        ADPC_size   = brstm_getSliceAsNumber(fileData,0x1C,4,BOM);
        DATA_offset = brstm_getSliceAsNumber(fileData,0x20,4,BOM);
        DATA_size   = brstm_getSliceAsNumber(fileData,0x24,4,BOM);
        
        if(debugLevel>1) {std::cout << "File size: " << file_size << "\nHeader size: " << header_size << "\nChunks: " << num_chunks << "\nHEAD offset: " << HEAD_offset << "\nHEAD size: " << HEAD_size << "\nADPC offset: " << ADPC_offset << "\nADPC size: " << ADPC_size << "\nDATA offset: " << DATA_offset << "\nDATA size: " << DATA_size << "\n\n";}
        
        //HEAD
        magicstr=brstm_getSliceAsString(fileData,HEAD_offset,4);
        if(strcmp(magicstr,emagic2) == 0) {
            //Start reading HEAD
            HEAD_length  = brstm_getSliceAsNumber(fileData,HEAD_offset+0x04,4,BOM);
            HEAD1_offset = brstm_getSliceAsNumber(fileData,HEAD_offset+0x0C,4,BOM);
            HEAD2_offset = brstm_getSliceAsNumber(fileData,HEAD_offset+0x14,4,BOM);
            HEAD3_offset = brstm_getSliceAsNumber(fileData,HEAD_offset+0x1C,4,BOM);
            
            if(debugLevel>1) {std::cout << "HEAD length: " << HEAD_length << "\nHEAD1 offset: " << HEAD1_offset << "\nHEAD2 offset: " << HEAD2_offset << "\nHEAD3 offset: " << HEAD3_offset << "\n\n";}
            
            //HEAD1
            HEAD1_offset+=8;
            HEAD1_codec               = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x00,1,BOM);
            HEAD1_loop                = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x01,1,BOM);
            HEAD1_num_channels        = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x02,1,BOM);
            HEAD1_sample_rate         = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x04,2,BOM);
            HEAD1_loop_start          = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x08,4,BOM);
            HEAD1_total_samples       = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x0C,4,BOM);
            HEAD1_ADPCM_offset        = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x10,4,BOM);
            HEAD1_total_blocks        = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x14,4,BOM);
            HEAD1_blocks_size         = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x18,4,BOM);
            HEAD1_blocks_samples      = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x1C,4,BOM);
            HEAD1_final_block_size    = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x20,4,BOM);
            HEAD1_final_block_samples = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x24,4,BOM);
            HEAD1_final_block_size_p  = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x28,4,BOM);
            HEAD1_samples_per_ADPC    = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x2C,4,BOM);
            HEAD1_bytes_per_ADPC      = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD1_offset+0x30,4,BOM);
            HEAD1_offset-=8;
            
            if(debugLevel>0) {std::cout << "Codec: " << HEAD1_codec << "\nLoop: " << HEAD1_loop << "\nChannels: " << HEAD1_num_channels << "\nSample rate: " << HEAD1_sample_rate << "\nLoop start: " << HEAD1_loop_start << "\nTotal samples: " << HEAD1_total_samples << "\nOffset to ADPCM data: " << HEAD1_ADPCM_offset << "\nTotal blocks: " << HEAD1_total_blocks << "\nBlock size: " << HEAD1_blocks_size << "\nSamples per block: " << HEAD1_blocks_samples << "\nFinal block size: " << HEAD1_final_block_size << "\nFinal block samples: " << HEAD1_final_block_samples << "\nFinal block size with padding: " << HEAD1_final_block_size_p << "\nSamples per entry in ADPC: " << HEAD1_samples_per_ADPC << "\nBytes per entry in ADPC: " << HEAD1_bytes_per_ADPC << "\n\n";}
            
            //safety
            if(HEAD1_num_channels>16) { if(debugLevel>=0) {std::cout << "Too many channels. Max supported is 16.\n";} return 249;}
            
            //HEAD2
            HEAD2_offset+=8;
            HEAD2_num_tracks = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_offset+0x00,1,BOM);
            HEAD2_track_type = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_offset+0x01,1,BOM);
            
            //safety
            if(HEAD2_num_tracks>8) { if(debugLevel>=0) {std::cout << "Too many tracks. Max supported is 8.\n";} return 248;}
            
            //read info for each track
            for(unsigned char i=0;i<HEAD2_num_tracks;i++) {
                unsigned int readOffset = HEAD_offset+HEAD2_offset+0x04+4+(i*8);
                unsigned int infoOffset = brstm_getSliceAsNumber(fileData,readOffset,4,BOM);
                HEAD2_track_info_offsets[i]=infoOffset+8;
                if(HEAD2_track_type==0) {
                    HEAD2_track_num_channels[i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x00,1,BOM);
                    HEAD2_track_lchannel_id [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x01,1,BOM);
                    HEAD2_track_rchannel_id [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x02,1,BOM);
                } else if(HEAD2_track_type==1) {
                    HEAD2_track_num_channels[i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x08,1,BOM);
                    HEAD2_track_lchannel_id [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x09,1,BOM);
                    HEAD2_track_rchannel_id [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x0A,1,BOM);
                    //type 1 stuff
                    HEAD2_track_volume      [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x00,1,BOM);
                    HEAD2_track_panning     [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD2_track_info_offsets[i]+0x01,1,BOM);
                } else { if(debugLevel>=0) {std::cout << "Unknown track type.\n";} return 244;}
                HEAD2_track_info_offsets[i]-=8;
            }
            
            if(debugLevel>0) {std::cout << "Tracks: " << HEAD2_num_tracks << "\nTrack type: " << HEAD2_track_type << "\n";}
            if(debugLevel>1) {for(unsigned char i=0;i<HEAD2_num_tracks;i++) {
                std::cout << "\nTrack " << i+1 << "\nOffset: " << HEAD2_track_info_offsets[i] << "\nVolume: " << HEAD2_track_volume[i] << "\nPanning: " << HEAD2_track_panning[i] << "\nChannels: " << HEAD2_track_num_channels[i] << "\nLeft channel ID:  " << HEAD2_track_lchannel_id[i] << "\nRight channel ID: " << HEAD2_track_rchannel_id[i] << "\n\n";
            }}
            
            HEAD2_offset-=8;
            
            //HEAD3
            HEAD3_offset+=8;
            HEAD3_num_channels = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD3_offset+0x00,1,BOM);
            
            //safety
            if(HEAD3_num_channels>16) { if(debugLevel>=0) {std::cout << "Too many channels. Max supported is 16.\n";} return 249;}
            
            for(unsigned char i=0;i<HEAD3_num_channels;i++) {
                unsigned int readOffset = HEAD_offset+HEAD3_offset+0x04+4+(i*8);
                unsigned int infoOffset = brstm_getSliceAsNumber(fileData,readOffset,4,BOM);
                HEAD3_ch_info_offsets[i]=infoOffset+8;
                for(unsigned char x=0;x<32;x+=2) {
                    HEAD3_int16_adpcm[i][x/2] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x08+x,BOM);
                }
                HEAD3_ch_gain          [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x28,2,BOM);
                HEAD3_ch_initial_scale [i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x2A,2,BOM);
                HEAD3_ch_hsample_1     [i] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x2C,BOM);
                HEAD3_ch_hsample_2     [i] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x2E,BOM);
                HEAD3_ch_loop_ini_scale[i] = brstm_getSliceAsNumber(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x30,2,BOM);
                HEAD3_ch_loop_hsample_1[i] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x32,BOM);
                HEAD3_ch_loop_hsample_2[i] = brstm_getSliceAsInt16Sample(fileData,HEAD_offset+HEAD3_ch_info_offsets[i]+0x34,BOM);
                HEAD3_ch_info_offsets[i]-=8;
            }
            
            if(debugLevel>0) {std::cout << "Channels: " << HEAD3_num_channels << "\n";}
            if(debugLevel>1) {for(unsigned char i=0;i<HEAD3_num_channels;i++) {
                std::cout << "\nChannel " << i+1 << "\nOffset: " << HEAD3_ch_info_offsets[i] << "\nGain: " << HEAD3_ch_gain[i] << "\nInitial scale: " << HEAD3_ch_initial_scale[i] << "\nHistory sample 1: " << HEAD3_ch_hsample_1[i] << "\nHistory sample 2: " << HEAD3_ch_hsample_2[i] << "\nLoop initial scale: " << HEAD3_ch_loop_ini_scale[i] << "\nLoop history sample 1: " << HEAD3_ch_loop_hsample_1[i] << "\nLoop history sample 2: " << HEAD3_ch_loop_hsample_2[i] << "\nADPCM coefficients: ";
                for(unsigned char x=0;x<16;x++) {
                    std::cout << HEAD3_int16_adpcm[i][x] << ' ';
                }
                std::cout << "\n\n";
            }}
            
            HEAD3_offset-=8;
            
            //ADPC chunk
            magicstr=brstm_getSliceAsString(fileData,ADPC_offset,4);
            if(strcmp(magicstr,emagic3) == 0) {
                //Start reading ADPC
                ADPC_total_length  = brstm_getSliceAsNumber(fileData,ADPC_offset+0x04,4,BOM);
                ADPC_total_entries = (ADPC_total_length-8)/HEAD1_bytes_per_ADPC;
                for(unsigned int n=0;n<HEAD3_num_channels;n++) {
                    ADPC_hsamples_1[n] = new int16_t[ADPC_total_entries/HEAD3_num_channels];
                    ADPC_hsamples_2[n] = new int16_t[ADPC_total_entries/HEAD3_num_channels];
                    
                    if(debugLevel>1) {std::cout << "Channel " << n << ": ";}
                    
                    unsigned int it;
                    for(unsigned int i=0;i<ADPC_total_entries/HEAD3_num_channels;i++) {
                        unsigned int offset=ADPC_offset+8+(n*HEAD1_bytes_per_ADPC)+((i*HEAD1_bytes_per_ADPC)*HEAD3_num_channels);
                        ADPC_hsamples_1[n][i] = brstm_getSliceAsInt16Sample(fileData,offset,BOM);
                        ADPC_hsamples_2[n][i] = brstm_getSliceAsInt16Sample(fileData,offset+2,BOM);
                        it=i+1;
                    }
                    if(debugLevel>1) {std::cout << it << " history sample pairs read.\n";}
                }
                
                if(debugLevel>1) {std::cout << "ADPC length: " << ADPC_total_length << "\nTotal entries: " << ADPC_total_entries << "\n\n";}
                
                //DATA chunk
                magicstr=brstm_getSliceAsString(fileData,DATA_offset,4);
                if(strcmp(magicstr,emagic4) == 0) {
                    //Start reading DATA
                    DATA_total_length = brstm_getSliceAsNumber(fileData,DATA_offset+0x04,4,BOM);
                    
                    if(debugLevel>1) {std::cout << "DATA length: " << DATA_total_length << '\n';}
                    
                    if(HEAD1_codec!=2) { if(debugLevel>=0) {std::cout << "Unsupported codec.\n";} return 220;}
                    
                    if(decodeADPCM) {
                        //Read the ADPCM data
                        unsigned long decoded_samples=0;
                        
                        unsigned long posOffset=0;
                        
                        for(unsigned int c=0;c<HEAD3_num_channels;c++) {
                            //Create new array of samples for the current channel
                            switch(decodeADPCM) {
                                case 1: PCM_samples[c] = new int16_t[HEAD1_total_samples]; break;
                                case 2: ADPCM_data [c] = new unsigned char[((DATA_total_length-32))/HEAD3_num_channels]; break;
                            }
                            
                            posOffset=0+(HEAD1_blocks_size*c);
                            unsigned long outputPos = 0; //position in PCM samples or ADPCM data output array
                            for(unsigned long b=0;b<HEAD1_total_blocks;b++) {
                                //Read every block
                                unsigned int currentBlockSize    = HEAD1_blocks_size;
                                unsigned int currentBlockSamples = HEAD1_blocks_samples;
                                //Final block
                                if(b==HEAD1_total_blocks-1) {
                                    currentBlockSize    = HEAD1_final_block_size;
                                    currentBlockSamples = HEAD1_final_block_samples;
                                }
                                if(b>=HEAD1_total_blocks-1 && c>0) {
                                    //Go back to the previous position
                                    posOffset-=HEAD1_blocks_size*HEAD3_num_channels;
                                    //Go to the next block in position of first channel
                                    posOffset+=HEAD1_blocks_size*(HEAD3_num_channels-c);
                                    //Jump to the correct channel in the final block
                                    posOffset+=HEAD1_final_block_size_p*c;
                                }
                                //Get data from just the current block
                                unsigned char* blockData = brstm_getSlice(fileData,HEAD1_ADPCM_offset+posOffset,currentBlockSize);
                                
                                if(decodeADPCM == 1) {
                                    //Decode 4 bit ADPCM
                                    const unsigned char ps = blockData[0];
                                    const   signed int  yn1 = ADPC_hsamples_1[c][b], yn2 = ADPC_hsamples_2[c][b];
                                    
                                    //Magic adapted from brawllib's ADPCMState.cs
                                    signed int 
                                    cps = ps,
                                    cyn1 = yn1,
                                    cyn2 = yn2;
                                    unsigned long dataIndex = 0;
                                    
                                    for (unsigned int sampleIndex=0;sampleIndex<currentBlockSamples;) {
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
                                        
                                        outSample = (0x400 + ((scale * outSample) << 11) + HEAD3_int16_adpcm[c][brstm_clamp(cIndex, 0, 15)] * cyn1 + HEAD3_int16_adpcm[c][brstm_clamp(cIndex + 1, 0, 15)] * cyn2) >> 11;
                                        
                                        cyn2 = cyn1;
                                        cyn1 = brstm_clamp(outSample, -32768, 32767);
                                        
                                        PCM_samples[c][outputPos++] = cyn1;
                                        decoded_samples++;
                                    }
                                } else {
                                    //Write raw data to ADPCM_data
                                    for(unsigned int i=0; i<currentBlockSize; i++) {
                                        ADPCM_data[c][outputPos++] = blockData[i];
                                    }
                                }
                                
                                posOffset+=HEAD1_blocks_size*HEAD3_num_channels;
                            }
                        }
                        if(debugLevel>0) {std::cout << "Decoded PCM samples: " << decoded_samples << '\n';}
                    }
                    //end
                    
                } else { if(debugLevel>=0) {std::cout << "Invalid DATA chunk.\n";} return 230;}
                
            } else { if(debugLevel>=0) {std::cout << "Invalid ADPC chunk.\n";} return 240;}
            
        } else { if(debugLevel>=0) {std::cout << "Invalid HEAD chunk.\n";} return 250;}
        
    } else { if(debugLevel>=0) {std::cout << "Invalid BRSTM file.\n";} return 255;}
    
    //END OF BRSTM
    
    }
    //BCSTM
    else if(brstmi->file_format == 2) {
        if(debugLevel>=0) {std::cout << "Unsupported file format.\n";}
        return 210;
    }
    //BFSTM
    else if(brstmi->file_format == 3) {
        if(debugLevel>=0) {std::cout << "Unsupported file format.\n";}
        return 210;
    }
    //BWAV
    else if(brstmi->file_format == 4) {
        //Byte Order Mark
        if(brstm_getSliceAsInt16Sample(fileData,0x04,1) == -257) {
            BOM = 1; //Big endian
        } else {
            BOM = 0; //Little endian
        }
        //BWAV is weird and stupid
        brstmi->num_channels = brstm_getSliceAsNumber(fileData,0x0E,2,BOM);
        brstmi->codec = brstm_getSliceAsNumber(fileData,0x10,2,BOM) + 1; //add 1 so the codec number is like BRSTM's codec number
        brstmi->sample_rate = brstm_getSliceAsNumber(fileData,0x14,4,BOM);
        brstmi->total_samples = brstm_getSliceAsNumber(fileData,0x18,4,BOM);
        brstmi->loop_flag = ( (int32_t)brstm_getSliceAsNumber(fileData,0x4C,4,BOM) != -1 );
        brstmi->loop_start = brstm_getSliceAsNumber(fileData,0x50,4,BOM);
        brstmi->audio_offset = brstm_getSliceAsNumber(fileData,0x40,4,BOM);
        brstmi->total_blocks = 1;
        brstmi->blocks_size = brstmi->num_channels > 1 ? (brstm_getSliceAsNumber(fileData,0x8C,4,BOM) - brstmi->audio_offset) : (brstmi->codec == 1 ? brstmi->total_samples*2 : brstmi->total_samples/1.75);
        brstmi->blocks_samples = brstmi->total_samples;
        brstmi->final_block_size = brstmi->blocks_size;
        brstmi->final_block_samples = brstmi->blocks_samples;
        brstmi->final_block_size_p = brstmi->final_block_size;
        
        //Write BRSTM standard track data
        brstmi->num_tracks = (brstmi->num_channels > 1 && brstmi->num_channels%2 == 0) ? brstmi->num_channels/2 : brstmi->num_channels;
        unsigned char track_num_channels = brstmi->num_tracks*2 == brstmi->num_channels ? 2 : 1;
        brstmi->track_desc_type = 0;
        for(unsigned char c=0; c<brstmi->num_channels; c++) {
            brstmi->track_num_channels[c/track_num_channels] = track_num_channels;
            if(track_num_channels == 1 || c%2 == 0) brstmi->track_lchannel_id[c/track_num_channels] = c;
            if(track_num_channels == 2 && c%2 == 1) brstmi->track_rchannel_id[c/track_num_channels] = c;
            
            //Read coefs
            if(brstmi->codec == 2) {
                for(unsigned int i=0;i<16;i++) {
                    brstmi->ADPCM_coefs[c][i] = brstm_getSliceAsInt16Sample(fileData,0x20+i*2+c*0x4C,BOM);
                }
            }
        }
        if(brstmi->num_tracks>1 && debugLevel>=0) {std::cout << "Warning: Tracks are assumed\n";}
        
        //Audio
        //This is stupid copied code but works for now and eventually I plan to move the decoder into a function
        if(decodeADPCM) {
            //Read the ADPCM data
            unsigned long decoded_samples=0;
            
            unsigned long posOffset=0;
            
            for(unsigned int c=0;c<brstmi->num_channels;c++) {
                //Create new array of samples for the current channel
                switch(decodeADPCM) {
                    case 1: brstmi->PCM_samples[c] = new int16_t[brstmi->total_samples]; break;
                    case 2: brstmi->ADPCM_data [c] = new unsigned char[brstmi->blocks_size]; break;
                }
                
                posOffset=0+(brstmi->blocks_size*c);
                unsigned long outputPos = 0; //position in PCM samples or ADPCM data output array
                for(unsigned long b=0;b<brstmi->total_blocks;b++) {
                    //Read every block
                    unsigned int currentBlockSize    = brstmi->blocks_size;
                    unsigned int currentBlockSamples = brstmi->blocks_samples;
                    //Final block
                    if(b==brstmi->total_blocks-1) {
                        currentBlockSize    = brstmi->final_block_size;
                        currentBlockSamples = brstmi->final_block_samples;
                    }
                    if(b>=brstmi->total_blocks-1 && c>0) {
                        //Go back to the previous position
                        posOffset-=brstmi->blocks_size*brstmi->num_channels;
                        //Go to the next block in position of first channel
                        posOffset+=brstmi->blocks_size*(brstmi->num_channels-c);
                        //Jump to the correct channel in the final block
                        posOffset+=brstmi->final_block_size_p*c;
                    }
                    //Get data from just the current block
                    unsigned char* blockData = brstm_getSlice(fileData,brstmi->audio_offset+posOffset,currentBlockSize);
                    
                    if(decodeADPCM == 1) {
                        if(brstmi->codec == 2) {
                            //Decode 4 bit ADPCM
                            const unsigned char ps = blockData[0];
                            const   signed int  yn1 = 0, yn2 = 0;
                            
                            //Magic adapted from brawllib's ADPCMState.cs
                            signed int 
                            cps = ps,
                            cyn1 = yn1,
                            cyn2 = yn2;
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
                                
                                cyn2 = cyn1;
                                cyn1 = brstm_clamp(outSample, -32768, 32767);
                                
                                brstmi->PCM_samples[c][outputPos++] = cyn1;
                                decoded_samples++;
                            }
                        } else if(brstmi->codec == 1) {
                            for(unsigned long s=0;s<currentBlockSamples;s++) {
                                brstmi->PCM_samples[c][outputPos++] = brstm_getSliceAsInt16Sample(fileData,brstmi->audio_offset+posOffset+s*2,BOM);
                                decoded_samples++;
                            }
                        } else {
                            if(debugLevel>=0) {std::cout << "Unsupported codec.\n";}
                            return 220;
                        }
                    } else {
                        if(brstmi->codec == 2) {
                            //Write raw data to ADPCM_data
                            for(unsigned int i=0; i<currentBlockSize; i++) {
                                brstmi->ADPCM_data[c][outputPos++] = blockData[i];
                            }
                        } else {
                            if(debugLevel>=0) {std::cout << "Codec is not ADPCM.\n";}
                            return 220;
                        }
                    }
                    
                    posOffset+=brstmi->blocks_size*brstmi->num_channels;
                }
            }
            if(debugLevel>0) {std::cout << "Decoded PCM samples: " << decoded_samples << '\n';}
        } else {
            if(brstmi->codec == 2) {
                if(debugLevel>=0) {std::cout << "Warning: Realtime decoding works like full decoding for this format\n";}
                for(unsigned char c=0;c<brstmi->num_channels;c++) {
                    brstmi->ADPCM_hsamples_1[c] = new int16_t[1];
                    brstmi->ADPCM_hsamples_2[c] = new int16_t[1];
                    brstmi->ADPCM_hsamples_1[c][0] = 0;
                    brstmi->ADPCM_hsamples_1[c][0] = 1;
                }
            } else {
                if(debugLevel>=0) {std::cout << "Realtime decoding is not supported for this format and codec.\n";}
                return 210;
            }
        }
        
        //END OF BWAV
    }
    
    //Log details
    //BRSTM reader has it's own logger
    if(brstmi->file_format != 1) {
        if(debugLevel>0) {std::cout
            << "Codec: " << brstmi->codec
            << "\nLoop: " << brstmi->loop_flag
            << "\nChannels: " << brstmi->num_channels
            << "\nSample rate: " << brstmi->sample_rate
            << "\nLoop start: " << brstmi->loop_start
            << "\nTotal samples: " << brstmi->total_samples
            << "\nOffset to audio data: " << brstmi->audio_offset
            << "\nTotal blocks: " << brstmi->total_blocks
            << "\nBlock size: " << brstmi->blocks_size
            << "\nSamples per block: " << brstmi->blocks_samples
            << "\nFinal block size: " << brstmi->final_block_size
            << "\nFinal block samples: " << brstmi->final_block_samples
            << "\nFinal block size with padding: " << brstmi->final_block_size_p
            << "\n\n";
        }
        if(debugLevel>0) {std::cout
            << "Tracks: " << brstmi->num_tracks
            << "\nTrack type: " << brstmi->track_desc_type
            << "\n";
        }
        if(debugLevel>1) {
            for(unsigned char i=0;i<brstmi->num_tracks;i++) {
                std::cout
                << "\nTrack " << i+1
                << "\nVolume: " << brstmi->track_volume[i]
                << "\nPanning: " << brstmi->track_panning[i]
                << "\nChannels: " << brstmi->track_num_channels[i]
                << "\nLeft channel ID:  " << brstmi->track_lchannel_id[i]
                << "\nRight channel ID: " << brstmi->track_rchannel_id[i] << "\n\n";
            }
        }
        if(debugLevel>0) {std::cout 
            << "Channels: " << brstmi->num_channels
            << "\n";
        }
        if(debugLevel>1) {
            for(unsigned char i=0;i<brstmi->num_channels;i++) {
                std::cout
                << "\nChannel " << i+1
                << "\nADPCM coefficients: ";
                for(unsigned char x=0;x<16;x++) {
                    std::cout << brstmi->ADPCM_coefs[i][x] << ' ';
                }
                std::cout << "\n\n";
            }
        }
    }
    
    return 0;
}

//This function is used by brstm_getbuffer
unsigned char* brstm_getblock(const unsigned char* fileData,bool dataType,unsigned long start,unsigned long length);

/* 
 * Get a buffer of audio data
 * 
 * brstmi: Your BRSTM struct
 * fileData: BRSTM file
 * sampleOffset: Offset to the first sample in the buffer
 * bufferSamples: Amount of samples in the buffer (don't make this more than the amount of samples per block!)
 * 
 */
void brstm_getbuffer(Brstm * brstmi,const unsigned char* fileData,unsigned long sampleOffset,unsigned int bufferSamples);

/*
 * Get a buffer of audio data (fstream mode)
 * 
 * brstmi: Your BRSTM struct
 * stream: std::ifstream with an open BRSTM file
 * sampleOffset: Offset to the first sample in the buffer
 * bufferSamples: Amount of samples in the buffer (don't make this more than the amount of samples per block!)
 * 
 */
void brstm_fstream_getbuffer(Brstm * brstmi,std::ifstream& stream,unsigned long sampleOffset,unsigned int bufferSamples);

/*
 * Main function for both memory modes
 * dataType will be 1 for disk streaming mode (fileData will be null) so brstm_getblock
 * will know to do disk streaming stuff instead of just getting a slice of fileData
 */
void brstm_getbuffer_main(Brstm * brstmi,const unsigned char* fileData,bool dataType,unsigned long sampleOffset,unsigned int bufferSamples) {
    //safety
    if(sampleOffset>brstmi->total_samples) {
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            delete[] brstmi->PCM_buffer[c];
            brstmi->PCM_buffer[c] = new int16_t[bufferSamples];
            for(unsigned int i=0;i<bufferSamples;i++) {
                brstmi->PCM_buffer[c][i] = 0;
            }
        }
        return;
    }
    //decode a new block if we don't have the current block in the blockbuffer cache
    if(brstmi->PCM_blockbuffer_currentBlock != sampleOffset/brstmi->blocks_samples) {
        //calculate block number
        unsigned long b=sampleOffset/brstmi->blocks_samples;
        //Read the ADPCM data
        unsigned long posOffset=0;
        if(brstmi->codec!=2) {exit(220);}
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Create new array of samples for the current channel
            delete[] brstmi->PCM_blockbuffer[c];
            brstmi->PCM_blockbuffer[c] = new int16_t[brstmi->blocks_samples];
            
            posOffset=0+(brstmi->blocks_size*c);
            unsigned long outputPos = 0; //position in PCM block samples output array
            
            unsigned long c_writtensamples = 0;
            
            posOffset+=b*(brstmi->blocks_size*brstmi->num_channels);
            
            //Read block
            unsigned int currentBlockSize    = brstmi->blocks_size;
            unsigned int currentBlockSamples = brstmi->blocks_samples;
            //Final block
            if(b==brstmi->total_blocks-1) {
                currentBlockSize    = brstmi->final_block_size;
                currentBlockSamples = brstmi->final_block_samples;
            }
            if(b>=brstmi->total_blocks-1 && c>0) {
                //Go back to the previous position
                posOffset-=brstmi->blocks_size*brstmi->num_channels;
                //Go to the next block in position of first channel
                posOffset+=brstmi->blocks_size*(brstmi->num_channels-c);
                //Jump to the correct channel in the final block
                posOffset+=brstmi->final_block_size_p*c;
            }
            //Get data from just the current block
            unsigned char* blockData = brstm_getblock(fileData,dataType,brstmi->audio_offset+posOffset,currentBlockSize);
            
            //4 bit ADPCM - No comments, no one knows what this code does :^) Stolen from that node module
            const unsigned char ps = blockData[0];
            const   signed int  yn1 = brstmi->ADPCM_hsamples_1[c][b], yn2 = brstmi->ADPCM_hsamples_2[c][b];
            
            //Magic adapted from brawllib's ADPCMState.cs
            signed int 
            cps = ps,
            cyn1 = yn1,
            cyn2 = yn2;
            unsigned long dataIndex = 0;
            
            int16_t* coefs = brstmi->ADPCM_coefs[c];
            
            for (unsigned int sampleIndex=0;sampleIndex<currentBlockSamples;) {
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
                
                cyn2 = cyn1;
                cyn1 = brstm_clamp(outSample, -32768, 32767);
                
                brstmi->PCM_blockbuffer[c][outputPos++] = cyn1;
                c_writtensamples++;
            }
            brstmi->PCM_blockbuffer_currentBlock = b;
            
            posOffset+=brstmi->blocks_size*brstmi->num_channels;
        }
    }
    //create the requested buffer
    if(brstmi->getbuffer_useBuffer) {
        bool blockEndReached = false;
        unsigned int blockEndReachedAt = 0;
        for(unsigned int c=0;c<brstmi->num_channels;c++) {
            //Create new array of samples for the current channel
            delete[] brstmi->PCM_buffer[c];
            brstmi->PCM_buffer[c] = new int16_t[bufferSamples];
            //offset in current block
            unsigned long dataIndex = sampleOffset-brstmi->blocks_samples*(unsigned int)(sampleOffset/brstmi->blocks_samples);
            for(unsigned int p=0;p<bufferSamples;p++) {
                if(dataIndex+p >= brstmi->blocks_samples) {
                    blockEndReached=true;
                    blockEndReachedAt=p;
                    break;
                }
                brstmi->PCM_buffer[c][p] = brstmi->PCM_blockbuffer[c][dataIndex+p];
            }
        }
        if(blockEndReached) {
            brstmi->getbuffer_useBuffer = false; //don't make a new buffer in PCM_buffer
            brstm_getbuffer_main(brstmi,fileData,dataType,sampleOffset+blockEndReachedAt,0);
            brstmi->getbuffer_useBuffer = true;
            for(unsigned int c=0;c<brstmi->num_channels;c++) {
                unsigned int dataIndex=0;
                for(unsigned int p=blockEndReachedAt;p<bufferSamples;p++) {
                    brstmi->PCM_buffer[c][p] = brstmi->PCM_blockbuffer[c][dataIndex++];
                }
            }
        }
    }
}

void brstm_getbuffer(Brstm * brstmi,const unsigned char* fileData,unsigned long sampleOffset,unsigned int bufferSamples) {
    brstm_getbuffer_main(brstmi,fileData,0,sampleOffset,bufferSamples);
}

//don't do void* kids
std::ifstream* brstm_ifstream;

void brstm_fstream_getbuffer(Brstm * brstmi,std::ifstream& stream,unsigned long sampleOffset,unsigned int bufferSamples) {
    if(!stream.is_open()) {perror("brstm_fstream_getbuffer: No file open in ifstream"); exit(255);}
    brstm_ifstream = &stream;
    brstm_getbuffer_main(brstmi,nullptr,1,sampleOffset,bufferSamples);
}

//This function is used by brstm_getbuffer
unsigned char* brstm_getblock(const unsigned char* fileData,bool dataType,unsigned long start,unsigned long length) {
    if(dataType == 0) {
        return brstm_getSlice((const unsigned char*)fileData,start,length);
    } else {
        //disk streaming
        delete[] brstm_slice;
        brstm_slice = new unsigned char[length];
        
        brstm_ifstream->seekg(start);
        
        if(brstm_ifstream->bad()) {perror("brstm_getblock: ifstream error"); exit(255);}
        
        brstm_ifstream->read((char*)brstm_slice,length);
        return brstm_slice;
    }
}

/*
 * Read BRSTM directly from an ifstream
 * 
 * stream: std::ifstream with an open BRSTM file
 * debugLevel: console debug level, same as brstm_read
 */
unsigned char brstm_fstream_read(Brstm * brstmi,std::ifstream& stream,signed int debugLevel) {
    if(!stream.is_open()) {
        if(debugLevel>=0) {std::cout << "brstm_fstream_read: no file open in std::ifstream.\n";}
        return 255;
    }
    unsigned char* brstm_header;
    unsigned int format = 0;
    
    //find file format so we can read the header size from the correct offset
    for(unsigned int t=0;t<BRSTM_formats_count;t++) {
        //read magic word from ifstream
        unsigned int emagiclen = strlen(BRSTM_formats_str[t]);
        char* magicword = new char[emagiclen+1];
        stream.seekg(0);
        stream.read(magicword,emagiclen);
        magicword[emagiclen] = '\0';
        //compare
        if(strcmp(BRSTM_formats_str[t],magicword) == 0) {
            format = t;
            break;
        }
    }
    if(format == 0) {
        if(debugLevel>=0) {std::cout << "Invalid or unsupported file format.\n";}
        return 210;
    }
    
    //read Byte Order Mark
    bool BOM;
    unsigned char bomword[2];
    stream.seekg(0x04);
    stream.read((char*)bomword,2);
    if(brstm_getSliceAsInt16Sample(bomword,0,1) == -257) {
        BOM = 1; //Big endian
    } else {
        BOM = 0; //Little endian
    }
    
    //get offset to audio data so we know how much data to read to get the full header
    stream.seekg(BRSTM_formats_audio_off_off[format]);
    unsigned char audioOff[4];
    stream.read((char*)audioOff,4);
    unsigned int headerSize = brstm_getSliceAsNumber(audioOff,0,4,BOM) + 512;
    
    if(debugLevel>1) std::cout << "Reading " << headerSize << " header bytes\n";
    
    //read header into memory
    stream.seekg(0);
    brstm_header = new unsigned char[headerSize];
    stream.read((char*)brstm_header,headerSize);
    stream.seekg(0);
    
    //call main brstm read function
    unsigned char res = brstm_read(brstmi,brstm_header,debugLevel,false);
    delete[] brstm_header;
    return res;
}

/* 
 * Close the BRSTM file (reset variables and free memory)
 */
void brstm_close(Brstm * brstmi) {
    for(unsigned char i=0;i<16;i++) {
        for(unsigned char j=0;j<16;j++) {
            brstmi->ADPCM_coefs[i][j] = 0;
        }
        delete[] brstmi->ADPCM_hsamples_1[i];
        delete[] brstmi->ADPCM_hsamples_2[i];
        delete[] brstmi->PCM_samples[i];
        delete[] brstmi->PCM_buffer[i];
        delete[] brstmi->PCM_blockbuffer[i];
        delete[] brstmi->ADPCM_data[i];
        delete[] brstmi->ADPCM_buffer[i];
    }
    delete[] brstmi->encoded_file;
    
    brstmi->file_format = 0;
    brstmi->codec = 0;
    brstmi->loop_flag = 0;
    brstmi->num_channels = 0;
    brstmi->sample_rate = 0;
    brstmi->loop_start = 0;
    brstmi->total_samples = 0;
    brstmi->audio_offset = 0;
    brstmi->total_blocks = 0;
    brstmi->blocks_size = 0;
    brstmi->blocks_samples = 0;
    brstmi->final_block_size = 0;
    brstmi->final_block_samples = 0;
    brstmi->final_block_size_p = 0;
    
    brstmi->num_tracks = 0;
    brstmi->track_desc_type = 0;
    
    for(unsigned char i=0; i<8; i++) {
        brstmi->track_num_channels[i] = 0;
        brstmi->track_lchannel_id [i] = 0;
        brstmi->track_rchannel_id [i] = 0;
        brstmi->track_volume      [i] = 0;
        brstmi->track_panning     [i] = 0;
    }
    brstmi->PCM_blockbuffer_currentBlock = -1;
}
