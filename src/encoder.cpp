//C++ BRSTM wav encoder
//Copyright (C) 2020 Extrasklep
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>

#include "lib/brstm.h"
#include "lib/brstm_encode.h"

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
    
    Brstm* brstm = new Brstm;
    
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
        brstm->loop_flag = 1;
        brstm->loop_start = atoi(optargstr[2]);
    } else {
        brstm->loop_flag = 0;
        brstm->loop_start = 0;
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
    } else {if(verb) {std::cout << '\n';} perror("Unable to open input file"); exit(255);}
    
    
    //Read the WAV file data
    if(strcmp("RIFF",brstm_getSliceAsString(memblock,0,4)) != 0) {
        std::cout << "Invalid RIFF header.\n"; exit(255);
    }
    unsigned long wavFileSize = brstm_getSliceAsNumber(memblock,4,4,0) + 8;
    if(strcmp("WAVEfmt ",brstm_getSliceAsString(memblock,8,8)) != 0) {
        std::cout << "Invalid WAVE header.\n"; exit(255);
    }
    unsigned long wavFmtSize = brstm_getSliceAsNumber(memblock,16,4,0);
    if(wavFmtSize != 16 || brstm_getSliceAsNumber(memblock,20,2,0) != 1) {
        std::cout << "Only PCM WAVs are supported.\n"; exit(255);
    }
    brstm->num_channels = brstm_getSliceAsNumber(memblock,22,2,0);
    if(brstm->num_channels > 16) {
        std::cout << "Too many channels. Max supported is 16.\n"; exit(255);
    } else if(!brstmStereoTracks && brstm->num_channels > 8) {
        std::cout << "Too many channels. Max supported tracks is 8 and you're trying to create single channel tracks.\n"; exit(255);
    }
    brstm->sample_rate = brstm_getSliceAsNumber(memblock,24,4,0);
    if(brstm_getSliceAsNumber(memblock,34,2,0) != 16) {
        std::cout << "Only 16-bit PCM WAVs are supported.\n"; exit(255);
    }
    unsigned long wavAudioOffset = 36;
    for(;strcmp("data",brstm_getSliceAsString(memblock,wavAudioOffset,4)) != 0 ;wavAudioOffset++) {}
    brstm->total_samples = brstm_getSliceAsNumber(memblock,wavAudioOffset+4,4,0) / brstm->num_channels / 2;
    wavAudioOffset += 8;
    
    if(verb) {
        std::cout << "WAV size: " << wavFileSize << "\nChannels: " << brstm->num_channels << "\nSample rate: " << brstm->sample_rate
        << "\nOffset: " << wavAudioOffset << "\nTotal samples: " << brstm->total_samples << '\n';
    }
    
    { //Read audio from wav file
        std::cout << "Reading audio data...\n";
        unsigned int c;
        for(c=0;c<brstm->num_channels;c++) {
            brstm->PCM_samples[c] = new int16_t[brstm->total_samples];
        }
        for(unsigned int i=0;i<brstm->total_samples;i++) {
            for(c=0;c<brstm->num_channels;c++) {
                brstm->PCM_samples[c][i] = brstm_getSliceAsInt16Sample(memblock,wavAudioOffset+i*(2*brstm->num_channels)+c*2,0);
            }
        }
        delete[] memblock;
    }
    
    //Set other BRSTM data
    brstm->file_format = 1;
    brstm->codec = 2;
    //Make sure the amount of channels is valid
    if((brstmStereoTracks && brstm->num_channels < 2) || (brstmStereoTracks && brstm->num_channels%2 != 0)) {
        std::cout << "You cannot create a stereo BRSTM with " << brstm->num_channels << " channel" << (brstm->num_channels == 1 ? "" : "s") << ".\n";
        exit(255);
    }
    brstm->num_tracks    = brstmStereoTracks ? brstm->num_channels / 2 : brstm->num_channels;
    brstm->track_desc_type    = 1;
    
    for(unsigned int t=0;t<brstm->num_tracks;t++) {
        brstm->track_num_channels[t] = brstmStereoTracks ? 2 : 1;
        
        brstm->track_lchannel_id [t] = brstmStereoTracks ? t*2 : t;
        brstm->track_rchannel_id [t] = brstmStereoTracks ? t*2+1 : 0;
        
        brstm->track_volume      [t] = 0x7F;
        brstm->track_panning     [t] = 0x40;
    }
    if(verb) std::cout << "Looping BRSTM: " << brstm->loop_flag << "\nLoop point: " << brstm->loop_start << "\nStereo BRSTM: " << (int)brstmStereoTracks << "\nTracks: " << brstm->num_tracks << "\n";
    
    //Encode and save the BRSTM
    if(saveFile) {
        std::cout << "Saving file to " << outputName << "...\n";
        
        std::ofstream ofile (outputName,std::ios::out|std::ios::binary|std::ios::trunc);
        if(ofile.is_open()) {
            unsigned char res = brstm_encode(brstm,1,1);
            if(res>127) {
                std::cout << "BRSTM encode error " << (int)res << ".\n";
                exit(res);
            }
            
            for(unsigned int c=0;c<brstm->num_channels;c++) {
                delete[] brstm->PCM_samples[c];
            }
            
            ofile.write((char*)brstm->encoded_file,brstm->encoded_file_size);
            ofile.close();
        } else {perror("Unable to open output file"); exit(255);}
    }
    
    return 0;
}
