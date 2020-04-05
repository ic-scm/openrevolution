//C++ BRSTM re-encoder
//Copyright (C) 2020 Extrasklep
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>
#include <string>

#include "lib/brstm.h"
#include "lib/brstm_encode.h"

//-------------------######### STRINGS

const char* helpString0 = "BRSTM Re-encoder\nCopyright (C) 2020 Extrasklep\nThis program is free software, see the license file for more information.\nUsage:\n";
const char* helpString1 = " [file to open] [options...]\nOptions:\n-o [output file name] - If this is not used the output will not be saved.\n-v - Verbose output\n--ffmpeg \"[ffmpeg arguments]\" - Use ffmpeg in the middle of reencoding to change the audio data with the passed ffmpeg arguments (as a single argument!)\nRequires FFMPEG to be installed and it may not work on non-unix systems.\n";

//------------------ Command line arguments

const char* opts[] = {"-v","-o","--ffmpeg"};
const char* opts_alt[] = {"--verbose","--output","--ffmpeg"};
const unsigned int optcount = 3;
const bool optrequiredarg[optcount] = {0,1,1};
bool  optused  [optcount];
char* optargstr[optcount];
//____________________________________
bool verb=0;
bool saveFile=0;

//Modified functions from brstm_encode.h, used to write WAV files
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

void delete_ffmpeg_files() {
    int tmp;
    tmp = system("rm .brstm-ffmpeg-i.wav");
    tmp = system("rm .brstm-ffmpeg-o.wav");
}

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
    bool useFFMPEG = 0;
    const char* ffmpegArgs;
    if(optused[0]) verb=1;
    if(optused[1]) {outputName=optargstr[1]; saveFile=1;}
    if(optused[2]) {ffmpegArgs=optargstr[2]; useFFMPEG=1;}
    
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
    } else {if(verb) {std::cout << '\n';} perror("Unable to open input file"); exit(255);}
    
    Brstm* brstm = new Brstm;
    
    //read the brstm
    unsigned char result=brstm_read(brstm,memblock,verb+1,true);
    if(result>127) {
        std::cout << "BRSTM read error " << (int)result << ".\n";
        return result;
    }
    
    //FFMPEG: save audio to WAV file, run ffmpeg on it, and read the output
    if(useFFMPEG) {
        //Create WAV file
        std::ofstream ofile (".brstm-ffmpeg-i.wav",std::ios::out|std::ios::binary|std::ios::trunc);
        if(ofile.is_open()) {
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
            for(unsigned long s=0;s<brstm->total_samples;s++) {
                for(unsigned char c=0;c<brstm->num_channels;c++) {
                    unsigned char samplebytes[2];
                    int16_t cSample = brstm->PCM_samples[c][s];
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
            ofile.close();
        } else {perror("Unable to open FFMPEG input file"); delete_ffmpeg_files(); exit(255);}
        
        //Run FFMPEG
        std::string systemCommand = "ffmpeg -i .brstm-ffmpeg-i.wav ";
        systemCommand += ffmpegArgs;
        systemCommand += " .brstm-ffmpeg-o.wav";
        std::cout << systemCommand << '\n';
        if(system(systemCommand.c_str())) {goto ffmpegOutputError;}
        
        {
            //Read FFMPEG output WAV
            unsigned char * memblock;
            std::ifstream file (".brstm-ffmpeg-o.wav", std::ios::in|std::ios::binary|std::ios::ate);
            if (file.is_open()) {
                fsize = file.tellg();
                memblock = new unsigned char [fsize];
                file.seekg (0, std::ios::beg);
                file.read ((char*)memblock, fsize);
                file.close();
            } else {perror("Unable to open FFMPEG output file"); delete_ffmpeg_files(); exit(255);}
            
            
            //Read the WAV file data
            if(strcmp("RIFF",brstm_getSliceAsString(memblock,0,4)) != 0) {
                goto ffmpegOutputError;
            }
            unsigned long wavFileSize = brstm_getSliceAsNumber(memblock,4,4,0) + 8;
            if(strcmp("WAVEfmt ",brstm_getSliceAsString(memblock,8,8)) != 0) {
                goto ffmpegOutputError;
            }
            if(brstm_getSliceAsNumber(memblock,16,4,0) != 16 || brstm_getSliceAsNumber(memblock,20,2,0) != 1) {
                goto ffmpegOutputError;
            }
            if(brstm->num_channels != brstm_getSliceAsNumber(memblock,22,2,0)) {
                goto ffmpegOutputError;
            }
            //read new sample rate in case the file was resampled
            brstm->sample_rate = brstm_getSliceAsNumber(memblock,24,4,0);
            if(brstm_getSliceAsNumber(memblock,34,2,0) != 16) {
                goto ffmpegOutputError;
            }
            unsigned long wavAudioOffset = 36;
            for(;strcmp("data",brstm_getSliceAsString(memblock,wavAudioOffset,4)) != 0 ;wavAudioOffset++) {}
            //read new total sample count
            brstm->total_samples = brstm_getSliceAsNumber(memblock,wavAudioOffset+4,4,0) / brstm->num_channels / 2;
            wavAudioOffset += 8;
            
            { //Read audio from wav file
                std::cout << "Reading output FFMPEG audio...\n";
                unsigned int c;
                for(unsigned int i=0;i<brstm->total_samples;i++) {
                    for(c=0;c<brstm->num_channels;c++) {
                        brstm->PCM_samples[c][i] = brstm_getSliceAsInt16Sample(memblock,wavAudioOffset+i*(2*brstm->num_channels)+c*2,0);
                    }
                }
                delete[] memblock;
            }
        }
        delete_ffmpeg_files();
        goto ffmpegSuccess;
        
        ffmpegOutputError:
        delete_ffmpeg_files();
        std::cout << "Invalid FFMPEG output file.\n"; exit(255);
        ffmpegSuccess:;
    }
    
    
    //Set codec to ADPCM
    brstm->codec = 2;
    
    //encode new brstm
    if(saveFile) {
        std::cout << "Saving file to " << outputName << "...\n";
        
        std::ofstream ofile (outputName,std::ios::out|std::ios::binary|std::ios::trunc);
        if(ofile.is_open()) {
            unsigned char res = brstm_encode(brstm,1,1);
            if(res>127) {
                std::cout << "BRSTM encode error " << (int)res << ".\n";
                exit(res);
            }
            ofile.write((char*)brstm->encoded_file,brstm->encoded_file_size);
            ofile.close();
        } else {perror("Unable to open output file"); exit(255);}
    }
    
    brstm_close(brstm);
    delete brstm;
    
    return 0;
}
