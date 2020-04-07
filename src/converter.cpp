//C++ BRSTM converter
//Decoder, encoder, reencoder and rebuilder merger into one program
//Copyright (C) 2020 Extrasklep
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>
#include <math.h>

#include "lib/brstm.h"
#include "lib/brstm_encode.h"

//-------------------######### STRINGS

const char* helpString0 = "BRSTM/WAV/other converter\nCopyright (C) 2020 Extrasklep\nThis program is free software, see the license file for more information.\nUsage:\n";
const char* helpString1 = " [file to open.type] [options...]\nOptions:\n\n-o [output file name.type] - If this is not used the output will not be saved.\nSupported input formats:  WAV, BRSTM, BWAV\nSupported output formats: WAV, BRSTM\n\n-v - Verbose output\n\n--ffmpeg \"[ffmpeg arguments]\" - Use ffmpeg in the middle of reencoding to change the audio data with the passed ffmpeg arguments (as a single argument!)\nRequires FFMPEG to be installed and it may not work on non-unix systems.\nOnly usable in BRSTM/other -> BRSTM/other conversion.\n\n--reencode - Always reencode instead of doing lossless conversion\n\nWAV -> BRSTM/other options:\n  -l --loop [loop point] - Creating looping file or -1 for no loop\n  -c --track-channels [1 or 2] - Number of channels for each track (default is 2)\n";

//------------------ Command line arguments

const char* opts[] = {"-v","-o","-l","-c","--ffmpeg","--reencode"};
const char* opts_alt[] = {"--verbose","--output","--loop","--track-channels","--ffmpeg","--reencode"};
const unsigned int optcount = 6;
const bool optrequiredarg[optcount] = {0,1,1,1,1,0};
bool  optused  [optcount];
char* optargstr[optcount];
//____________________________________
const char* inputFileName;
const char* outputFileName;
int inputFileExt;
int outputFileExt = 1;

int  verb = 1;
bool saveFile = 0;

bool userLoop = 0; //1 if the loop option was used
signed char userLoopFlag = 0;
unsigned long userLoopPoint = 0;

bool brstmStereoTracks; //used internally later
//0 = option not used, 1 = 1ch, 2 = 2ch
unsigned char userTrackChannels = 0;

bool reencode = 0;
bool useFFMPEG = 0;
const char* ffmpegArgs;

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

//Program functions
char* strtolowerstr;
char* strtolower(const char* str) {
    delete[] strtolowerstr;
    unsigned int len = strlen(str);
    strtolowerstr = new char[len+1];
    strtolowerstr[len] = '\0';
    for(unsigned int i=0;i<len;i++) {
        strtolowerstr[i] = tolower(str[i]);
    }
    return strtolowerstr;
}

int compareFileExt(const char* filename, const char* ext) {
    unsigned int extlen = strlen(ext);
    unsigned int fnlen  = strlen(filename);
    //return no match if filename is shorter than extension
    if(fnlen < extlen) {return 1;}
    //return if extension in filename is not the same length
    if(filename[fnlen-1-extlen] != '.') {return 2;}
    char* lext = new char[extlen+1];
    memcpy(lext,strtolower(ext),extlen+1);
    //copy the end of filename
    char* fnext = new char[extlen+1];
    fnext[extlen] = '\0';
    for(unsigned int i=fnlen;i>=fnlen-extlen;i--) {
        fnext[(i-fnlen)+extlen] = tolower(filename[i]);
    }
    //compare
    int res = strcmp(fnext,lext);
    delete[] lext;
    delete[] fnext;
    return res;
}

//-1 = unsupported, 0 = WAV, 1+ = BRSTM lib formats
int getFileExt(const char* filename) {
    if(compareFileExt(filename,"WAV") == 0) return 0;
    for(unsigned int f=1;f<BRSTM_formats_count;f++) {
        if(compareFileExt(filename,BRSTM_formats_short_usr_str[f]) == 0) return f;
    }
    //No match
    return -1;
}

void printConversionDetails() {
    std::cout << "Conversion: ";
    if(inputFileExt > 0 && outputFileExt == 0) {
        std::cout << "BRSTM/other -> WAV";
    } else if(inputFileExt == 0 && outputFileExt > 0) {
        std::cout << "WAV -> BRSTM/other";
    } else if(inputFileExt == 0 && outputFileExt == 0) {
        //This should never happen
        std::cout << "WAV -> WAV";
    } else if(inputFileExt > 0 && outputFileExt > 0) {
        std::cout << "BRSTM/other -> ";
        if(reencode) {
            std::cout << "PCM -> ";
            if(useFFMPEG) {
                std::cout << "FFMPEG -> PCM -> ";
            }
        }
        std::cout << "BRSTM/other";
        if(!reencode) {
            std::cout << " (Lossless)";
        }
    }
    std::cout << '\n';
}

void readWAV(Brstm* brstm,std::ifstream& stream,std::streampos fsize) {
    //Read file
    stream.seekg(0);
    unsigned char* memblock = new unsigned char[fsize];
    stream.read((char*)memblock,fsize);
    
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
        std::cout
        << "WAV size: " << wavFileSize
        << "\nChannels: " << brstm->num_channels
        << "\nSample rate: " << brstm->sample_rate
        << "\nOffset: " << wavAudioOffset
        << "\nTotal samples: " << brstm->total_samples
        << '\n';
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
}

void writeWAV(Brstm* brstm,std::ofstream& stream) {
    //Create WAV file
    stream.write("RIFF",4);
    //Size
    stream.write((char*)getLEuint((brstm->total_samples*2)*brstm->num_channels+36,4),4);
    stream.write("WAVEfmt ",8);
    //Subchunk size
    stream.write((char*)getLEuint(16,4),4);
    //Format = PCM
    stream.write((char*)getLEuint(1,2),2);
    //Number of channels
    stream.write((char*)getLEuint(brstm->num_channels,2),2);
    //Sample rate
    stream.write((char*)getLEuint(brstm->sample_rate,4),4);
    //Byterate
    stream.write((char*)getLEuint(brstm->sample_rate*brstm->num_channels*2,4),4);
    //Blockalign
    stream.write((char*)getLEuint(brstm->num_channels*2,2),2);
    //Bits per sample
    stream.write((char*)getLEuint(16,2),2);
    //Data
    stream.write("data",4);
    stream.write((char*)getLEuint(brstm->total_samples*brstm->num_channels*2,4),4);
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
            stream.write((char*)samplebytes,2);
        }
    }
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
    //Input
    inputFileName = args[1];
    //Verbose
    if(optused[0]) verb=2;
    //Output
    if(optused[1]) {outputFileName=optargstr[1]; saveFile=1;}
    //Loop
    if(optused[2]) {
        unsigned long lp = atoi(optargstr[2]);
        if(lp == (unsigned long)(-1)) {
            userLoop = 0;
        } else {
            userLoop = 1;
            userLoopFlag  = 1;
            userLoopPoint = lp;
        }
    }
    //Track channels
    if(optused[3]) {
        unsigned int tc = atoi(optargstr[3]);
        if(!(tc >= 1 && tc <= 2)) {std::cout << "Track channel count must be 1 or 2.\n"; exit(255);}
        userTrackChannels = tc;
    }
    //FFMPEG
    if(optused[4]) {ffmpegArgs=optargstr[4]; useFFMPEG=1; reencode = 1;}
    if(optused[5]) reencode = 1;
    
    
    
    //Check file extensions
    inputFileExt = getFileExt(inputFileName);
    if(inputFileExt == -1) {std::cout << "Unsupported input file extension.\n"; exit(255);}
    if(saveFile) {
        outputFileExt = getFileExt(outputFileName);
        if(outputFileExt == -1) {std::cout << "Unsupported output file extension.\n"; exit(255);}
    }
    
    //Create BRSTM struct and open files
    Brstm* brstm = new Brstm;
    std::ifstream ifile;
    std::ofstream ofile;
    std::streampos ifsize;
    ifile.open(inputFileName,std::ios::in|std::ios::binary|std::ios::ate);
    if(!ifile.is_open()) {
        perror("Unable to open input file");
        exit(255);
    }
    ifsize = ifile.tellg();
    ifile.seekg(0);
    
    //Read input file base information
    if(inputFileExt > 0) {
        unsigned char res = brstm_fstream_getBaseInformation(brstm,ifile,0);
        if(res>127) {
            std::cout << "Input file error.\n";
            exit(res);
        }
        //Change from rebuilding to reencoding if the codec is not ADPCM
        if(outputFileExt > 0 && brstm->codec != 2) reencode=1;
    }
    
    //Run conversions
    //Decoder BRSTM/other -> WAV
    if(inputFileExt > 0 && outputFileExt == 0) {
        //check for unsupported opts
        if(userLoop)  {std::cout << "You cannot use the loop option in decoding mode.\n"; exit(255);}
        if(useFFMPEG) {std::cout << "You cannot use the FFMPEG option in decoding mode.\n"; exit(255);}
        if(userTrackChannels) {std::cout << "You cannot use the track channel count option in decoding mode.\n"; exit(255);}
        //print conversion details
        if(saveFile) printConversionDetails();
        
        //Read file data
        unsigned char * memblock = new unsigned char[ifsize];
        ifile.read((char*)memblock,ifsize);
        if(verb) {std::cout << "Read file " << inputFileName << ", size " << ifsize << '\n';}
        //Read the BRSTM
        unsigned char result = brstm_read(brstm,memblock,verb,true);
        if(result>127) {
            std::cout << "BRSTM read error " << (int)result << ".\n";
            return result;
        }
        delete[] memblock;
        if(saveFile) {
            //Open output file
            ofile.open(outputFileName,std::ios::out|std::ios::binary|std::ios::trunc);
            if(!ofile.is_open()) {perror("Unable to open output file"); exit(255);}
            //Write output WAV file
            writeWAV(brstm,ofile);
            std::cout << "Saved file to " << outputFileName << '\n';
        }
    }
    
    //Encoder WAV -> BRSTM/other
    else if(inputFileExt == 0 && outputFileExt > 0) {
        //check for unsupported opts
        if(useFFMPEG) {std::cout << "You cannot use the FFMPEG option in encoding mode.\n"; exit(255);}
        //print conversion details
        if(saveFile) printConversionDetails();
        
        //Read input WAV file
        brstmStereoTracks = userTrackChannels > 0 ? userTrackChannels-1 : 1;
        readWAV(brstm,ifile,ifsize);
        
        if(saveFile) {
            //Set other BRSTM data
            brstm->file_format = outputFileExt;
            brstm->codec = 2;
            if(userLoop) {
                brstm->loop_flag  = userLoopFlag;
                brstm->loop_start = userLoopPoint;
            }
            //Make sure the amount of channels is valid
            if((brstmStereoTracks && brstm->num_channels < 2) || (brstmStereoTracks && brstm->num_channels%2 != 0)) {
                std::cout << "You cannot create a stereo BRSTM with " << brstm->num_channels << " channel" << 
                (brstm->num_channels == 1 ? "" : "s") << ".\n";
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
            //Open output file
            ofile.open(outputFileName,std::ios::out|std::ios::binary|std::ios::trunc);
            if(!ofile.is_open()) {perror("Unable to open output file"); exit(255);}
            //Encode
            unsigned char res = brstm_encode(brstm,1,1);
            if(res>127) {
                std::cout << "BRSTM encode error " << (int)res << ".\n";
                exit(res);
            }
            ofile.write((char*)brstm->encoded_file,brstm->encoded_file_size);
        }
    }
    
    //Lossless rebuilder
    else if(inputFileExt > 0 && outputFileExt > 0 && reencode == 0) {
        //check for unsupported opts
        if(useFFMPEG) {std::cout << "You cannot use the FFMPEG option in rebuilder mode.\n"; exit(255);}
        //print conversion details
        if(saveFile) printConversionDetails();
        
        //Read file data
        unsigned char * memblock = new unsigned char[ifsize];
        ifile.read((char*)memblock,ifsize);
        if(verb) {std::cout << "Read file " << inputFileName << ", size " << ifsize << '\n';}
        //Read the BRSTM
        unsigned char result=brstm_read(brstm,memblock,verb,2);
        if(result>127) {
            std::cout << "BRSTM read error " << (int)result << ".\n";
            return result;
        }
        delete[] memblock;
        //Set output format
        brstm->file_format = outputFileExt;
        if(saveFile) {
            //Open output file
            ofile.open(outputFileName,std::ios::out|std::ios::binary|std::ios::trunc);
            if(!ofile.is_open()) {perror("Unable to open output file"); exit(255);}
            //Build new file
            unsigned char res = brstm_encode(brstm,1,0);
            if(res>127) {
                std::cout << "BRSTM encode error " << (int)res << ".\n";
                exit(res);
            }
            ofile.write((char*)brstm->encoded_file,brstm->encoded_file_size);
        }
    }
    
    //Reencoder
    else if(inputFileExt > 0 && outputFileExt > 0 && reencode == 1) {
        //print conversion details
        if(saveFile) printConversionDetails();
    }
    
    //Unsupported
    else {
        std::cout << "Unsupported conversion.\n";
        return 255;
    }
    
    //cleanup
    ifile.close();
    if(saveFile) ofile.close();
    brstm_close(brstm);
    delete brstm;
    
    return 0;
}
