//C++ BRSTM reader
//Copyright (C) 2020 Extrasklep
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>
#include <math.h>

#include "brstm.h"

//-------------------######### STRINGS

const char* helpString0 = "BRSTM decoder\nCopyright (C) 2020 Extrasklep\nThis program is free software, see the license file for more information.\nUsage:\n";
const char* helpString1 = " [file to open] [options...]\nOptions:\n-o [output file name.wav] - If this is not used the output will not be saved.\n-v - Verbose output\n-r - Output raw file instead of WAV\n";

//------------------ Command line arguments

const char* opts[] = {"-v","-o","-r"};
const char* opts_alt[] = {"--verbose","--output","--raw"};
const unsigned int optcount = 3;
const bool optrequiredarg[optcount] = {0,1,0};
bool  optused  [optcount];
char* optargstr[optcount];
//____________________________________
bool verb=0;
bool saveFile=0;
bool rawOutput=0;

//From brstm_encode.h
void writebytes(unsigned char* buf,const unsigned char* data,unsigned int bytes,unsigned long& off) {
    for(unsigned int i=0;i<bytes;i++) {
        buf[i+off] = data[i];
    }
    off += bytes;
}
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
    if(optused[2]) rawOutput=1;
    
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
    
    //create BRSTM struct
    Brstm * brstm = new Brstm;
    
    //read the brstm
    unsigned char result=brstm_read(brstm,memblock,verb+1,true);
    if(result>127) {
        std::cout << "Error.\n";
        return result;
    }
    
    //Save output
    //TODO Use less memory and maybe add memory modes like in the player
    if(saveFile) {
        std::cout << "Saving file to " << outputName << "...\n";
        
        std::ofstream ofile (outputName,std::ios::out|std::ios::binary|std::ios::trunc);
        if(ofile.is_open()) {
            if(rawOutput) {
                const unsigned long TotalPCMSampleCount=brstm->total_samples * brstm->num_channels;
                const unsigned long PCMSampleCountPerChannel=brstm->total_samples;
                int16_t* rawAudioData;
                rawAudioData = new int16_t[TotalPCMSampleCount];
                unsigned long rawAudioPos=0;
                for(unsigned long i=0;i<PCMSampleCountPerChannel;i++) {
                    for(unsigned int c=0;c<brstm->num_channels;c++) {
                        rawAudioData[rawAudioPos] = brstm->PCM_samples[c][i];
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
            } else {
                //Create WAV file
                unsigned char* wavfiledata = new unsigned char[(brstm->total_samples*2)*brstm->num_channels+44];
                unsigned long bufpos=0;
                writebytes(wavfiledata,(unsigned char*)"RIFF",4,bufpos);
                //Size
                writebytes(wavfiledata,getLEuint((brstm->total_samples*2)*brstm->num_channels+36,4),4,bufpos);
                writebytes(wavfiledata,(unsigned char*)"WAVEfmt ",8,bufpos);
                //Subchunk size
                writebytes(wavfiledata,getLEuint(16,4),4,bufpos);
                //Format = PCM
                writebytes(wavfiledata,getLEuint(1,2),2,bufpos);
                //Number of channels
                writebytes(wavfiledata,getLEuint(brstm->num_channels,2),2,bufpos);
                //Sample rate
                writebytes(wavfiledata,getLEuint(brstm->sample_rate,4),4,bufpos);
                //Byterate
                writebytes(wavfiledata,getLEuint(brstm->sample_rate*brstm->num_channels*2,4),4,bufpos);
                //Blockalign
                writebytes(wavfiledata,getLEuint(brstm->num_channels*2,2),2,bufpos);
                //Bits per sample
                writebytes(wavfiledata,getLEuint(16,2),2,bufpos);
                //Data
                writebytes(wavfiledata,(unsigned char*)"data",4,bufpos);
                writebytes(wavfiledata,getLEuint(brstm->total_samples*brstm->num_channels*2,4),4,bufpos);
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
                        writebytes(wavfiledata,samplebytes,2,bufpos);
                    }
                }
                
                ofile.write((char*)wavfiledata,(brstm->total_samples*2)*brstm->num_channels+44);
                delete[] wavfiledata;
            }
            
            ofile.close();
        } else {std::cout << "\nUnable to open file\n"; return 255;}
    }
    
    brstm_close(brstm);
    delete brstm;
    
    return 0;
}
