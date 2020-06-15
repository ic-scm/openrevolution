//C++ BRSTM reader
//Copyright (C) 2020 Extrasklep
//https://github.com/Extrasklep/openrevolution

#pragma once
#include <iostream>
#include <fstream>
#include <cstring>

//Bool endian: 0 = little endian, 1 = big endian

const char* BRSTM_version_str = "v2.2.0";
const char* brstm_getVersionString() {return BRSTM_version_str;}

//Format information
const unsigned int BRSTM_formats_count = 6;
//File header magic words for each file format
const char* BRSTM_formats_str[BRSTM_formats_count] = {"RIFF","RSTM","CSTM","FSTM","BWAV","OSTM"};
//Offset to the audio offset information in each format (32 bit integer)
//(doesn't have to be accurate, just enough to fit the entire file header before it)
const unsigned int BRSTM_formats_audio_off_off[BRSTM_formats_count] = {0x00,0x70,0x00,0x30,0x40,0x00};
//Offset to the codec information and their sizes in each format
const unsigned int BRSTM_formats_codec_off  [BRSTM_formats_count] = {0x00,0x60,0x00,0x60,0x10,0x00};
const unsigned int BRSTM_formats_codec_bytes[BRSTM_formats_count] = {1,1,1,1,2,1};
//Short human readable strings (equal to file extension)
const char* BRSTM_formats_short_usr_str[BRSTM_formats_count] = {"WAV","BRSTM","BCSTM","BFSTM","BWAV","ORSTM"};
//Long human readable strings
const char* BRSTM_formats_long_usr_str [BRSTM_formats_count] = {
"Waveform Audio",
"Binary Revolution Stream",
"Binary CTR Stream",
"Binary Cafe Stream",
"Nintendo BWAV",
"Open Revolution Stream"
};

//Codec information
const unsigned int BRSTM_codecs_count = 4;
//Human readable strings
const char* BRSTM_codecs_usr_str[BRSTM_codecs_count] = {
"8-bit PCM",
"16-bit PCM",
"4-bit DSPADPCM",
"IMA ADPCM"
};


//The BRSTM struct
struct Brstm {
    //Byte order mark
    bool BOM = 0;
    //File type, 1 = BRSTM, see above for full list
    unsigned int  file_format   = 0;
    //Audio codec, 0 = PCM8, 1 = PCM16, 2 = DSPADPCM
    unsigned int  codec         = 0;
    bool          loop_flag     = 0;
    unsigned int  num_channels  = 0;
    unsigned long sample_rate   = 0;
    unsigned long loop_start    = 0;
    unsigned long total_samples = 0;
    unsigned long audio_offset  = 0;
    unsigned long total_blocks  = 0;
    unsigned long blocks_size   = 0;
    unsigned long blocks_samples  = 0;
    unsigned long final_block_size  = 0;
    unsigned long final_block_samples = 0;
    unsigned long final_block_size_p  = 0;
    
    //track information
    unsigned int  num_tracks      = 0;
    unsigned int  track_desc_type = 0;
    unsigned int  track_num_channels[8] = {0,0,0,0,0,0,0,0};
    unsigned int  track_lchannel_id [8] = {0,0,0,0,0,0,0,0};
    unsigned int  track_rchannel_id [8] = {0,0,0,0,0,0,0,0};
    unsigned int  track_volume      [8] = {0,0,0,0,0,0,0,0};
    unsigned int  track_panning     [8] = {0,0,0,0,0,0,0,0};
    
    int16_t* PCM_samples[16];
    int16_t* PCM_buffer [16];
    
    unsigned char* ADPCM_data   [16];
    unsigned char* ADPCM_buffer [16]; //Not used yet
    int16_t  ADPCM_coefs    [16][16];
    int16_t* ADPCM_hsamples_1   [16];
    int16_t* ADPCM_hsamples_2   [16];
    
    //Encoder
    unsigned char* encoded_file = nullptr;
    unsigned long  encoded_file_size = 0;
    
    //Things you probably shouldn't touch
    //block cache
    int16_t* PCM_blockbuffer[16];
    int PCM_blockbuffer_currentBlock = -1;
    bool getbuffer_useBuffer = true;
};

#include "utils.h"
//This function is used by brstm_decode_block
unsigned char* brstm_getblock(const unsigned char* fileData,bool dataType,unsigned long start,unsigned long length);
#include "audio_decoder.h"
#include "d_formats/all.h"

//Get short file format string
const char* brstm_getShortFormatString(Brstm* brstmi) {
    if(brstmi->file_format >= BRSTM_formats_count) return "";
    else return BRSTM_formats_short_usr_str[brstmi->file_format];
}
//Get long file format string
const char* brstm_getLongFormatString(Brstm* brstmi) {
    if(brstmi->file_format >= BRSTM_formats_count) return "";
    else return BRSTM_formats_long_usr_str[brstmi->file_format];
}
//Get codec string
const char* brstm_getCodecString(Brstm* brstmi) {
    if(brstmi->codec >= BRSTM_codecs_count) return "";
    else return BRSTM_codecs_usr_str[brstmi->codec];
}
//Get error string from code
const char* brstm_getErrorString(unsigned char code) {
    const char* invalidfile = "Invalid file";
    switch(code) {
        case 0: return "No error";
        case 255: return invalidfile;
        case 250: return invalidfile;
        case 249: return "Too many channels";
        case 248: return "Too many tracks";
        case 244: return "Unknown track description type";
        case 240: return invalidfile;
        case 230: return invalidfile;
        case 220: return "Unsupported audio codec";
        case 210: return "Unsupported file format or invalid file";
    }
    return "Unknown error";
}

//Used by brstm_fstream_read, return standard codec number from the number in the file
unsigned int brstm_getStandardCodecNum(Brstm* brstmi,unsigned int num) {
    switch(brstmi->file_format) {
        case 0: {
            //None
            return -1;
        }
        case 1: {
            //BRSTM
            if(num >= 0 && num < 3) {
                return num;
            }
            return -1;
        }
        case 2: {
            //BCSTM (unsupported)
            return -1;
        }
        case 3: {
            //BFSTM
            if(num >= 0 && num < 4) {
                return num;
            }
            return -1;
        }
        case 4: {
            //BWAV
            if(num == 0) {
                return 1;
            } else if(num == 1) {
                return 2;
            }
            return -1;
        }
        case 5: {
            //ORSTM (doesn't exist)
            return -1;
        }
    }
    return -1;
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
 * decodeAudio:
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
unsigned char brstm_read(Brstm* brstmi,const unsigned char* fileData,signed int debugLevel,uint8_t decodeAudio) {
    //Initialize struct
    for(unsigned int c=0;c<16;c++) {
        brstmi->PCM_samples      [c] = nullptr;
        brstmi->PCM_buffer       [c] = nullptr;
        brstmi->ADPCM_data       [c] = nullptr;
        brstmi->ADPCM_buffer     [c] = nullptr;
        brstmi->ADPCM_hsamples_1 [c] = nullptr;
        brstmi->ADPCM_hsamples_2 [c] = nullptr;
        brstmi->PCM_blockbuffer  [c] = nullptr;
    }
    
    
    bool &BOM = brstmi->BOM;
    unsigned char readres = 0;
    
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
    
    if(debugLevel>0) std::cout << "File format: " << brstm_getShortFormatString(brstmi) << '\n';
    
    if(brstmi->file_format == 1) {
        //BRSTM
        readres = brstm_formats_read_brstm(brstmi,fileData,debugLevel,decodeAudio);
    } else if(brstmi->file_format == 2) {
        //BCSTM
        readres = brstm_formats_read_bcstm(brstmi,fileData,debugLevel,decodeAudio);
    } else if(brstmi->file_format == 3) {
        //BFSTM
        readres = brstm_formats_read_bfstm(brstmi,fileData,debugLevel,decodeAudio);
    } else if(brstmi->file_format == 4) {
        //BWAV
        readres = brstm_formats_read_bwav (brstmi,fileData,debugLevel,decodeAudio);
    } else if(brstmi->file_format == 5) {
        //ORSTM
        readres = brstm_formats_read_orstm(brstmi,fileData,debugLevel,decodeAudio);
    }
    
    //Return now if a read error occurred
    if(readres>127) return readres;
    
    //Log details
    //BRSTM reader has it's own logger
    if(brstmi->file_format != 1) {
        if(debugLevel>0) {std::cout
            << "Codec: " << brstm_getCodecString(brstmi)
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
    
    return readres;
}

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
        //Run decoder
        brstm_decode_block(brstmi,b,fileData,dataType);
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
            //safety, return if we are outside the last block
            if((sampleOffset+blockEndReachedAt)/brstmi->blocks_samples >= brstmi->total_blocks) return;
            //don't make a new buffer in PCM_buffer
            brstmi->getbuffer_useBuffer = false;
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

//This function is used by brstm_getbuffer/brstm_decode_block
unsigned char* brstm_getblock(const unsigned char* fileData,bool dataType,unsigned long start,unsigned long length) {
    if(dataType == 0) {
        return brstm_getSlice((const unsigned char*)fileData,start,length);
    } else {
        //disk streaming
        delete[] brstm_slice;
        brstm_slice = new unsigned char[length];
        
        brstm_ifstream->seekg(start);
        
        brstm_ifstream->read((char*)brstm_slice,length);
        return brstm_slice;
    }
}

//readAllData is 0 when called from brstm_fstream_getBaseInformation, 1 when called from brstm_fstream_read
unsigned char brstm_fstream_read_header(Brstm * brstmi,std::ifstream& stream,signed int debugLevel, bool readAllData) {
    if(!stream.is_open()) {
        if(debugLevel>=0) {std::cout << "brstm_fstream_read: no file open in std::ifstream.\n";}
        return 255;
    }
    bool &BOM = brstmi->BOM;
    unsigned char res = 0;
    unsigned char* brstm_header;
    
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
            brstmi->file_format = t;
            break;
        }
    }
    if(brstmi->file_format == 0) {
        if(debugLevel>=0) {std::cout << "Invalid or unsupported file format.\n";}
        return 210;
    }
    
    //read Byte Order Mark
    unsigned char bomword[2];
    stream.seekg(0x04);
    stream.read((char*)bomword,2);
    if(brstm_getSliceAsInt16Sample(bomword,0,1) == -257) {
        BOM = 1; //Big endian
    } else {
        BOM = 0; //Little endian
    }
    
    //get offset to audio data so we know how much data to read to get the full header
    stream.seekg(BRSTM_formats_audio_off_off[brstmi->file_format]);
    unsigned char audioOff[4];
    stream.read((char*)audioOff,4);
    brstmi->audio_offset = brstm_getSliceAsNumber(audioOff,0,4,BOM) + 64;
    
    //Get codec
    stream.seekg(BRSTM_formats_codec_off[brstmi->file_format]);
    unsigned char codecBytes[4];
    stream.read((char*)codecBytes,4);
    brstmi->codec = brstm_getStandardCodecNum(brstmi,
        brstm_getSliceAsNumber(codecBytes,0,BRSTM_formats_codec_bytes[brstmi->file_format],BOM)
    );
    
    if(readAllData) {
        if(debugLevel>1) std::cout << "Reading " << brstmi->audio_offset << " header bytes\n";
        
        //read header into memory
        stream.seekg(0);
        brstm_header = new unsigned char[brstmi->audio_offset];
        stream.read((char*)brstm_header,brstmi->audio_offset);
        
        //call main brstm read function
        res = brstm_read(brstmi,brstm_header,debugLevel,false);
        delete[] brstm_header;
    }
    
    stream.seekg(0);
    return res;
}

/*
 * Read BRSTM directly from an ifstream
 * 
 * brstmi: Your BRSTM struct pointer
 * stream: std::ifstream with an open BRSTM file
 * debugLevel: console debug level, same as brstm_read
 */
unsigned char brstm_fstream_read(Brstm * brstmi,std::ifstream& stream,signed int debugLevel) {
    return brstm_fstream_read_header(brstmi,stream,debugLevel,true);
}

/*
 * Get base file information (file format, codec, offset to audio) so you can decide how to read the file
 * This function is like brstm_fstream_read but it doesn't call the full brstm_read function
 */
unsigned char brstm_fstream_getBaseInformation(Brstm * brstmi,std::ifstream& stream,signed int debugLevel) {
    return brstm_fstream_read_header(brstmi,stream,debugLevel,false);
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
