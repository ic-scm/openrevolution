//C++ BRSTM reader
//Made by extrasklep copyright license bla bla bla
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

signed   int* PCM_samples[16];

unsigned long written_samples=0;

#include "brstm.h" //must be included after this stuff

//-------------------######### STRINGS

const char* helpString0 = "Usage:\n";
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
    
    //file data
    
    //read the file
    if(verb) {std::cout << "Reading file " << args[1];}
    std::streampos fsize;
    char * memblocksigned;
    std::ifstream file (args[1], std::ios::in|std::ios::binary|std::ios::ate);
    if (file.is_open()) {
        fsize = file.tellg();
        memblocksigned = new char [fsize];
        file.seekg (0, std::ios::beg);
        file.read (memblocksigned, fsize);
        if(verb) {std::cout << " size " << fsize << '\n';}
        //file.close();
        //delete[] memblock;
    } else {std::cout << "\nUnable to open file\n"; return 255;}
    
    //unsigned data array
    unsigned char* memblock;
    memblock = new unsigned char[fsize];
    for(unsigned int i=0;i<fsize;i++) {memblock[i]=memblocksigned[i];}
    delete[] memblocksigned;
    
    /*//show some of the first bytes for debugging
    for(unsigned char t=0;t<3;t++) {
    for(unsigned int i=0;i<40;i++) {
        unsigned int byte=memblock[i];
        if(t==0) {std::cout << std::hex << byte;}
        else if(t==1) {std::cout << std::dec << byte;}
        else {std::cout << memblock[i];}
        std::cout << ' ';
        if(t==0) {std::cout << ' ';}
        else if(t==2) {std::cout << "  ";}
        else {
            if(byte<10) {std::cout << ' ';}
            else if(byte<100) {std::cout << ' ';}
        }
    }
    std::cout << '\n';
    }*/
    
    //read the brstm
    unsigned char result=readBrstm(memblock,verb+1);
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
            signed int* rawAudioData;
            rawAudioData = new signed int[TotalPCMSampleCount];
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
            ofile.write(rawFileData,TotalPCMSampleCount*2);
            ofile.close();
        } else {std::cout << "\nUnable to open file\n"; return 255;}
    }
    return 0;
}
