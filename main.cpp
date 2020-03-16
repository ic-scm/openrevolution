//C++ BRSTM reader
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
int16_t* PCM_buffer[16]; //Unused in this program

unsigned char* ADPCM_data  [16];
unsigned char* ADPCM_buffer[16];
int16_t  HEAD3_int16_adpcm [16][16]; //Coefs
int16_t* ADPC_hsamples_1   [16];
int16_t* ADPC_hsamples_2   [16];

#include "brstm.h" //must be included after this stuff

//-------------------######### STRINGS

const char* helpString0 = "BRSTM decoder\nCopyright (C) 2020 Extrasklep\nThis program is free software, see the license file for more information.\nUsage:\n";
const char* helpString1 = " [file to open] [options...]\nOptions:\n-o [output file name] - If this is not used the output will not be saved.\n-v - Verbose output\n";

//------------------ Command line arguments

const char* opts[] = {"-v","-o"};
const char* opts_alt[] = {"--verbose","--output"};
const unsigned int optcount = 2;
const bool optrequiredarg[optcount] = {0,1};
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
    
    //read the file
    if(verb) {std::cout << "Reading file " << args[1];}
    std::streampos fsize;
    unsigned char * memblock;
    std::ifstream file (args[1], std::ios::in|std::ios::binary|std::ios::ate);
    if (file.is_open()) {
        fsize = file.tellg();
        memblock = new unsigned char [fsize];
        file.seekg (0, std::ios::beg);
        file.read ((char*)memblock, fsize);
        if(verb) {std::cout << " size " << fsize << '\n';}
        //file.close();
        //delete[] memblock;
    } else {std::cout << "\nUnable to open file\n"; return 255;}
    
    //read the brstm
    unsigned char result=brstm_read(memblock,verb+1,true);
    if(result>127) {
        std::cout << "Error.\n";
        return result;
    }
    
    //save the raw output
    if(saveFile) {
        std::cout << "Saving file to " << outputName << "...\n";
        
        std::ofstream ofile (outputName,std::ios::out|std::ios::binary|std::ios::trunc);
        if(ofile.is_open()) {
            const unsigned long TotalPCMSampleCount=HEAD1_total_samples * HEAD1_num_channels;
            const unsigned long PCMSampleCountPerChannel=HEAD1_total_samples;
            int16_t* rawAudioData;
            rawAudioData = new int16_t[TotalPCMSampleCount];
            unsigned long rawAudioPos=0;
            for(unsigned long i=0;i<PCMSampleCountPerChannel;i++) {
                for(unsigned int c=0;c<HEAD3_num_channels;c++) {
                    rawAudioData[rawAudioPos] = PCM_samples[c][i];
                    rawAudioPos++;
                }
            }
            char* rawFileData;
            rawFileData = new char[TotalPCMSampleCount*2];
            for(unsigned long i=0;i<TotalPCMSampleCount*2;i+=2) {
                int cSample = rawAudioData[i/2];
                rawFileData[i]   = cSample&0xFF;
                rawFileData[i+1] = (cSample>>8)&0xFF;
            }
            delete[] rawAudioData;
            ofile.write(rawFileData,TotalPCMSampleCount*2);
            delete[] rawFileData;
            ofile.close();
        } else {std::cout << "\nUnable to open file\n"; return 255;}
    }
    
    brstm_close();
    
    return 0;
}
