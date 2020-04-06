//C++ BRSTM reader
//Copyright (C) 2020 Extrasklep
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>
#include <math.h>

#include "lib/brstm.h"

//-------------------######### STRINGS

const char* helpString0 = "BRSTM decoder\nCopyright (C) 2020 Extrasklep\nThis program is free software, see the license file for more information.\nUsage:\n";
const char* helpString1 = " [file to open] [options...]\nOptions:\n-o [output file name.wav] - If this is not used the output will not be saved.\n-v - Verbose output\n";

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

//From brstm_encode.h
//Returns integer as little endian bytes
unsigned char* BEint;
unsigned char* getLEuint(uint64_t num,uint8_t bytes) {
    delete[] BEint;
    BEint = new unsigned char[bytes];
    unsigned long pwr;
    unsigned char pwn = bytes - 1;
    for(unsigned char i = 0; i < bytes; i++) {
        pwr = pow(256,pwn--);
        unsigned int pos = abs(i-bytes+1);
        BEint[pos]=0;
        while(num >= pwr) {
            BEint[pos]++;
            num -= pwr;
        }
    }
    return BEint;
}

int main(int argc, char** args) {
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
        file.close();
    } else {if(verb) {std::cout << '\n';} perror("Unable to open input file"); exit(255);}
    
    //create BRSTM struct
    Brstm * brstm = new Brstm;
    
    //read the brstm
    unsigned char result=brstm_read(brstm,memblock,verb+1,true);
    if(result>127) {
        std::cout << "BRSTM error " << (int)result << ".\n";
        return result;
    }
    delete[] memblock;
    
    //Save output
    if(saveFile) {
        std::cout << "Saving file to " << outputName << "...\n";
        
        std::ofstream ofile (outputName,std::ios::out|std::ios::binary|std::ios::trunc);
        if(ofile.is_open()) {
            //Create WAV file
            ofile.write("RIFF",4);
            //Size
            ofile.write((char*)getLEuint((brstm->total_samples*2)*brstm->num_channels+36,4),4);
            ofile.write("WAVEfmt ",8);
            //Subchunk size
            ofile.write((char*)getLEuint(16,4),4);
            //Format = PCM
            ofile.write((char*)getLEuint(1,2),2);
            //Number of channels
            ofile.write((char*)getLEuint(brstm->num_channels,2),2);
            //Sample rate
            ofile.write((char*)getLEuint(brstm->sample_rate,4),4);
            //Byterate
            ofile.write((char*)getLEuint(brstm->sample_rate*brstm->num_channels*2,4),4);
            //Blockalign
            ofile.write((char*)getLEuint(brstm->num_channels*2,2),2);
            //Bits per sample
            ofile.write((char*)getLEuint(16,2),2);
            //Data
            ofile.write("data",4);
            ofile.write((char*)getLEuint(brstm->total_samples*brstm->num_channels*2,4),4);
            //Audio data
            unsigned char samplebytes[2];
            int16_t cSample;
            for(unsigned long s=0;s<brstm->total_samples;s++) {
                for(unsigned char c=0;c<brstm->num_channels;c++) {
                    cSample = brstm->PCM_samples[c][s];
                    #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                    cSample = __builtin_bswap16(cSample);
                    #endif
                    samplebytes[0]   = cSample&0xFF;
                    samplebytes[1] = (cSample>>8)&0xFF;
                    ofile.write((char*)samplebytes,2);
                }
            }
            
            ofile.close();
        } else {perror("Unable to open output file"); exit(255);}
    }
    
    brstm_close(brstm);
    delete brstm;
    
    return 0;
}
