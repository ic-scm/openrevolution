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

int16_t* PCM_buffer[16]; //unused in this program

unsigned long written_samples=0;

#include "brstm.h" //must be included after this stuff

//-------------------######### STRINGS

const char* helpString0 = "BRSTM decoder\nCopyright (C) 2020 Extrasklep\nThis program is free software, see the license file for more information.\nUsage:\n";
const char* helpString1 = " [file to open] [options...]\nOptions:\n-o [output file name] - If this is not used the output will not be saved.\n-v - Verbose output\n";

const char* opts[] = {"-v","-o"};
//____________________________________
bool verb=0;
bool saveFile=0;

int main( int argc, char* args[] ) {
    if(argc<2) {
        std::cout << helpString0 << args[0] << helpString1;
        return 0;
    }
    const char* outputName;
    //check user options
    for(unsigned int i=2;i<argc;i++) {
        char* currentArg=args[i];
        int vOpt=-1;
        for(unsigned int o=0;o<2;o++) {
            unsigned int matched=0;
        for(unsigned int s=0;s<strlen(currentArg);s++) {
            if(args[i][s]==opts[o][s]) {matched++;}
        }
            if(matched>=2) {
                vOpt=o;
            }
        }
        if(vOpt==0) {verb=1;}
        else if(vOpt==1) {if(argc-1>i) {outputName=args[++i]; saveFile=1;}}
    }
    
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
            const unsigned long TotalPCMSampleCount=written_samples;
            const unsigned long PCMSampleCountPerChannel=written_samples/HEAD3_num_channels;
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
