//C++ BRSTM wav encoder
//Copyright (C) 2020 Extrasklep
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>

//brstm stuff

unsigned int  HEAD1_codec; //char
unsigned int  HEAD1_loop;  //char
unsigned int  HEAD1_num_channels; //char
unsigned int  HEAD1_sample_rate;
unsigned long HEAD1_loop_start;
unsigned long HEAD1_total_samples;
unsigned long HEAD1_ADPCM_offset;
unsigned long HEAD1_total_blocks;
unsigned long HEAD1_blocks_size;
unsigned long HEAD1_blocks_samples;
unsigned long HEAD1_final_block_size;
unsigned long HEAD1_final_block_samples;
unsigned long HEAD1_final_block_size_p;
unsigned long HEAD1_samples_per_ADPC;
unsigned long HEAD1_bytes_per_ADPC;

unsigned int  HEAD2_num_tracks;
unsigned int  HEAD2_track_type;

unsigned int  HEAD2_track_num_channels[8] = {0,0,0,0,0,0,0,0};
unsigned int  HEAD2_track_lchannel_id [8] = {0,0,0,0,0,0,0,0};
unsigned int  HEAD2_track_rchannel_id [8] = {0,0,0,0,0,0,0,0};
//type 1 only
unsigned int  HEAD2_track_volume      [8] = {0,0,0,0,0,0,0,0};
unsigned int  HEAD2_track_panning     [8] = {0,0,0,0,0,0,0,0};
//HEAD3
unsigned int  HEAD3_num_channels;

int16_t* PCM_samples[16];

int16_t* PCM_buffer[16]; //unused in this program

unsigned long written_samples=0;

#include "../brstm.h" //must be included after this stuff

unsigned char* brstm_encoded_data;
unsigned long  brstm_encoded_data_size;
#include "../brstm_encode.h" //must be included after this stuff

//Get slice and convert it to a number (LE)
unsigned long getSliceAsNumber(const unsigned char* data,unsigned long start,unsigned long length) {
    if(length>4) {length=4;}
    unsigned long number=0;
    unsigned char* bytes=brstm_getSlice(data,start,length);
    unsigned char pos=0; //Read as little endian
    unsigned long pw=1; //Multiply by 1,256,65536...
    //std::cout << length << '\n';
    for(unsigned int i=0;i<length;i++) {
        if(i>0) {pw*=256;}
        number+=bytes[pos]*pw;
        pos++;
    }
    return number;
}

//Get slice as signed 16 bit number (LE)
signed int getSliceAsInt16Sample(const unsigned char * data,unsigned long start) {
    unsigned int length=2;
    unsigned long number=0;
    unsigned char* bytes=brstm_getSlice(data,start,length);
    unsigned char big=bytes[1];
    signed   char little=bytes[0];
    number=little+big*256;
    return number;
}

//-------------------######### STRINGS

const char* helpString0 = "WAV to BRSTM encoder\nCopyright (C) 2020 Extrasklep\nThis program is free software, see the license file for more information.\nUsage:\n";
const char* helpString1 = " [.wav input file] [options...]\nOptions:\n-o [output file name] - If this is not used the output will not be saved.\n-v - Verbose output\n-l --loop [loop point] - Creating looping BRSTM\n-c --track-channels [1 or 2] - Number of channels for each track (default is 2)\n";

//------------------ Command line arguments

const char* opts[] = {"-v","-o","-l","-c"};
const char* opts_alt[] = {"--verbose","--output","--loop","--track-channels"};
const unsigned int optcount = 4;
const bool optrequiredarg[optcount] = {0,1,1,1};
bool  optused  [optcount];
char* optargstr[optcount];
//____________________________________
bool verb=0;
bool saveFile=0;

int main( int argc, char* args[] ) {
    if(argc<2) {
        std::cout << helpString0 << args[0] << helpString1;
        return 0;
    }
    //Parse command line args
    for(unsigned int a=2;a<argc;a++) {
        int vOpt = -1;
        //Compare cmd arg against each known option
        for(unsigned int o=0;o<optcount;o++) {
            if( strcmp(args[a], opts[o]) == 0 || strcmp(args[a], opts_alt[o]) == 0 ) {
                //Matched
                vOpt = o;
                break;
            }
        }
        //No match
        if(vOpt < 0) {std::cout << "Unknown option '" << args[a] << "'.\n"; exit(255);}
        //Mark the options as used
        optused[vOpt] = 1;
        //Read the argument for the option if it requires it
        if(optrequiredarg[vOpt]) {
            if(a+1 < argc) {
                optargstr[vOpt] = args[++a];
            } else {
                std::cout << "Option " << opts[vOpt] << " requires an argument\n";
                exit(255);
            }
        }
    }
    //Apply the options
    const char* outputName;
    if(optused[0]) verb=1;
    if(optused[1]) {outputName=optargstr[1]; saveFile=1;}
    if(optused[2]) {
        HEAD1_loop = 1;
        HEAD1_loop_start = atoi(optargstr[2]);
    } else {
        HEAD1_loop = 0;
        HEAD1_loop_start = 0;
    }
    bool brstmStereoTracks = 1;
    if(optused[3]) {
        unsigned int tmp = atoi(optargstr[3]);
        if(!(tmp >= 1 && tmp <= 2)) {std::cout << "Track channel count must be 1 or 2.\n"; exit(255);}
        brstmStereoTracks = (tmp - 1);
    }
    
    //Read input file
    if(verb) {std::cout << "Reading file " << args[1];}
    std::streampos fsize;
    unsigned char * memblock;
    std::ifstream file (args[1], std::ios::in|std::ios::binary|std::ios::ate);
    if (file.is_open()) {
        fsize = file.tellg();
        memblock = new unsigned char [fsize];
        file.seekg (0, std::ios::beg);
        file.read ((char*)memblock, fsize);
        file.close();
        if(verb) {std::cout << " size " << fsize << '\n';}
    } else {std::cout << "\nUnable to open file\n"; return 255;}
    
    
    //Read the WAV file data
    if(strcmp("RIFF",brstm_getSliceAsString(memblock,0,4)) != 0) {
        std::cout << "Invalid RIFF header.\n"; exit(255);
    }
    unsigned long wavFileSize = getSliceAsNumber(memblock,4,4) + 8;
    if(strcmp("WAVEfmt ",brstm_getSliceAsString(memblock,8,8)) != 0) {
        std::cout << "Invalid WAVE header.\n"; exit(255);
    }
    unsigned long wavFmtSize = getSliceAsNumber(memblock,16,4);
    if(wavFmtSize != 16 || getSliceAsNumber(memblock,20,2) != 1) {
        std::cout << "Only PCM WAVs are supported.\n"; exit(255);
    }
    HEAD1_num_channels = getSliceAsNumber(memblock,22,2);
    if(HEAD1_num_channels > 16) {
        std::cout << "Too many channels. Max supported is 16.\n"; exit(255);
    } else if(!brstmStereoTracks && HEAD1_num_channels > 8) {
        std::cout << "Too many channels. Max supported tracks is 8 and you're trying to create single channel tracks.\n"; exit(255);
    }
    HEAD1_sample_rate  = getSliceAsNumber(memblock,24,4);
    if(getSliceAsNumber(memblock,34,2) != 16) {
        std::cout << "Only 16-bit PCM WAVs are supported.\n"; exit(255);
    }
    unsigned long wavAudioOffset = 36;
    for(;strcmp("data",brstm_getSliceAsString(memblock,wavAudioOffset,4)) != 0 ;wavAudioOffset++) {}
    HEAD1_total_samples = getSliceAsNumber(memblock,wavAudioOffset+4,4) / HEAD1_num_channels / 2;
    wavAudioOffset += 8;
    
    if(verb) {
        std::cout << "WAV size: " << wavFileSize << "\nChannels: " << HEAD1_num_channels << "\nSample rate: " << HEAD1_sample_rate
        << "\nOffset: " << wavAudioOffset << "\nTotal samples: " << HEAD1_total_samples << '\n';
    }
    
    { //Read audio from wav file
        std::cout << "Reading audio data...\n";
        unsigned int c;
        for(c=0;c<HEAD1_num_channels;c++) {
            PCM_samples[c] = new int16_t[HEAD1_total_samples];
        }
        for(unsigned int i=0;i<HEAD1_total_samples;i++) {
            for(c=0;c<HEAD1_num_channels;c++) {
                PCM_samples[c][i] = getSliceAsInt16Sample(memblock,wavAudioOffset+i*(2*HEAD1_num_channels)+c*2);
            }
        }
        delete[] memblock;
    }
    
    //Set other BRSTM data
    HEAD1_codec = 2;
    //Make sure the amount of channels is valid
    if((brstmStereoTracks && HEAD1_num_channels < 2) || (brstmStereoTracks && HEAD1_num_channels%2 != 0)) {
        std::cout << "You cannot create a stereo BRSTM with " << HEAD1_num_channels << " channel" << (HEAD1_num_channels == 1 ? "" : "s") << ".\n";
        exit(255);
    }
    HEAD2_num_tracks    = brstmStereoTracks ? HEAD1_num_channels / 2 : HEAD1_num_channels;
    HEAD2_track_type    = 1;
    
    for(unsigned int t=0;t<HEAD2_num_tracks;t++) {
        HEAD2_track_num_channels[t] = brstmStereoTracks ? 2 : 1;
        
        HEAD2_track_lchannel_id [t] = brstmStereoTracks ? t*2 : t;
        HEAD2_track_lchannel_id [t] = brstmStereoTracks ? t*2+1 : 0;
        
        HEAD2_track_volume      [t] = 0x7F;
        HEAD2_track_panning     [t] = 0x40;
    }
    if(verb) std::cout << "Looping BRSTM: " << HEAD1_loop << "\nLoop point: " << HEAD1_loop_start << "\nStereo BRSTM: " << (int)brstmStereoTracks << "\nTracks: " << HEAD2_num_tracks << "\n";
    
    //Encode and save the BRSTM
    if(saveFile) {
        std::cout << "Saving file to " << outputName << "...\n";
        
        std::ofstream ofile (outputName,std::ios::out|std::ios::binary|std::ios::trunc);
        if(ofile.is_open()) {
            unsigned char res = brstm_encode(1);
            if(res>127) {
                std::cout << "BRSTM encode error.\n";
                exit(res);
            }
            
            for(unsigned int c=0;c<HEAD1_num_channels;c++) {
                delete[] PCM_samples[c];
            }
            
            ofile.write((char*)brstm_encoded_data,brstm_encoded_data_size);
            ofile.close();
        } else {std::cout << "\nUnable to open file\n"; return 255;}
    }
    
    return 0;
}
