//C++ BRSTM converter
//Decoder, encoder, reencoder and rebuilder merged into one program
//Copyright (C) 2020 IC
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

const char* helpString = "OpenRevolution file converter\nCopyright (C) 2021 I.C.\nThis program is free software, see the license file for more information.\nUsage:\nbrstm_converter [file to open.type] [options...]\nOptions:\n\n-o [output file name.type] - If this is not used the output will not be saved.\n\n-v - Verbose output\n\n--ffmpeg \"[ffmpeg arguments]\" - Use ffmpeg in the middle of reencoding to change the audio data with the passed ffmpeg arguments (as a single argument!)\nRequires FFMPEG to be installed and it may not work on non-unix systems.\nOnly usable in BRSTM/other -> BRSTM/other conversion.\n\n--reencode - Always reencode instead of doing lossless conversion\n\n--extend [sample count] - Extend the audio to the specified sample count (for games that require an exact length)\nYou can also cut with the same option by entering a number smaller than the sample count of the input file.\n\n--mix-tracks [0/1 for all tracks] - Mix the specified tracks from the input file into a single stereo track\n(example: --mix-tracks 1010 will mix the first and third track from a 4-track file)\nIf necessary, this option can also be used to duplicate a single mono track.\n\n--mix-tracks-mono - Use together with --mix-tracks to mix into a single mono track\n\nBRSTM/other output options:\n  -l --loop [loop point] - Set loop point or -1 for no loop\n  -c --track-channels [1 or 2] - Number of channels for each track (default is 2)\n  -kc --keep-channels [0/1 repeated for all channels] - Keep only the specified channels\n    (example: -kc 1100 keeps only 2 first channels out of a 4 channel file)\n  Advanced:\n  --oCodec [number] - Output codec, supported codecs: 0 = PCM8, 1 = PCM16, 2 = DSPADPCM, or 'same' to use the same codec as the input file\n  --oEndian [number] - Custom byte order of the output file, 0 = Little endian, 1 = Big endian\n";

//------------------ Command line arguments

const char* opts[] = {"-v","-o","-l","-c","-ffmpeg","-reencode","-extend","-oCodec","-kc","-oEndian","-mix-tracks","-mix-tracks-mono"};
const char* opts_alt[] = {"--verbose","--output","--loop","--track-channels","--ffmpeg","--reencode","--extend","--oCodec","--keep-channels","--oEndian","--mix-tracks","--mix-tracks-mono"};
const unsigned int optcount = 12;
const bool optrequiredarg[optcount] = {0,1,1,1,1,0,1,1,1,1,1,0};
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

//keep channels
bool keepChannels[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

//Track mixing
bool userTrackMixing = 0;
bool userTrackMixingMono = 0;
bool userTracksToMix[8] = {0,0,0,0,0,0,0,0};

bool reencode = 0;
bool useFFMPEG = 0;
const char* ffmpegArgs;

unsigned long extendSampleCount = 0;

int userCodec = -1;

int userEndian = -1;

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
    if(fnlen <= extlen) {return 1;}
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
    //if(compareFileExt(filename,"WAV") == 0) return 0;
    for(unsigned int f=0;f<BRSTM_formats_count;f++) {
        if(compareFileExt(filename,BRSTM_formats_short_usr_str[f]) == 0) return f;
    }
    //No match
    return -1;
}

void printConversionDetails() {
    std::cout << "Conversion: ";
    if(inputFileExt > 0 && outputFileExt == 0) {
        std::cout << BRSTM_formats_short_usr_str[inputFileExt] << " -> WAV";
    } else if(inputFileExt == 0 && outputFileExt > 0) {
        std::cout << "WAV -> " << BRSTM_formats_short_usr_str[outputFileExt];
    } else if(inputFileExt == 0 && outputFileExt == 0) {
        //This should never happen
        std::cout << "WAV -> WAV";
    } else if(inputFileExt > 0 && outputFileExt > 0) {
        std::cout << BRSTM_formats_short_usr_str[inputFileExt] << " -> ";
        if(reencode) {
            std::cout << "PCM -> ";
            if(useFFMPEG) {
                std::cout << "FFMPEG -> PCM -> ";
            }
        }
        std::cout << BRSTM_formats_short_usr_str[outputFileExt];
        if(!reencode) {
            std::cout << " (Lossless)";
        }
    }
    std::cout << '\n';
}

//Extender
unsigned long getOverflowSampleNum(Brstm* brstm,unsigned long num) {
    if(num >= brstm->total_samples) {
        num -= brstm->total_samples;
        if(brstm->loop_flag) num += brstm->loop_start;
        return getOverflowSampleNum(brstm,num);
    } else return num;
}

void extendPCMSamples(Brstm* brstm,unsigned long newSampleCount) {
    if(newSampleCount < brstm->total_samples) {
        //Cut (cutting does not require removing any samples, if the total sample count is set to a lower number
        //the other samples will just be ignored.)
        //The loop point has to be changed to not be bigger than the new sample count.
        if((brstm->loop_flag && !userLoopFlag) || (userLoopFlag && userLoopPoint+1 >= newSampleCount)) {
            std::cout << "Warning: The loop point was removed when cutting.\n";
            brstm->loop_flag = 0;
            brstm->loop_start = 0;
        }
    } else if(newSampleCount > brstm->total_samples) {
        //Extend
        for(unsigned int c=0;c<brstm->num_channels;c++) {
            int16_t* newPCMsamples = new int16_t[newSampleCount];
            for(unsigned long s=0;s<newSampleCount;s++) {
                newPCMsamples[s] = brstm->PCM_samples[c][getOverflowSampleNum(brstm,s)];
            }
            delete[] brstm->PCM_samples[c];
            brstm->PCM_samples[c] = newPCMsamples;
        }
        if(brstm->loop_flag) brstm->loop_start = newSampleCount - brstm->total_samples + brstm->loop_start;
    }
    brstm->total_samples = newSampleCount;
}

void removeChannels(Brstm* brstm,bool mode) {
    unsigned int newChannelCount = 0;
    unsigned int c = 0, realc = 0;
    for(;c<brstm->num_channels;c++) {
        if(keepChannels[c] == 0) {
            if(mode == 1) {delete[] brstm->ADPCM_data[c]; brstm->ADPCM_data[c] = nullptr;}
            else {delete[] brstm->PCM_samples[c]; brstm->PCM_samples[c] = nullptr;}
        }
        else {
            newChannelCount++;
        }
    }
    if(newChannelCount < 1) {
        std::cout << "Cannot remove all channels.\n";
        exit(255);
    }
    for(c=0;realc<brstm->num_channels;realc++) {
        if(keepChannels[realc] == 1) {
            if(mode == 1) {
                brstm->ADPCM_data[c] = brstm->ADPCM_data[realc];
                //Move ADPCM coefs
                for(unsigned char i=0; i<16; i++) {
                    brstm->ADPCM_coefs[c][i] = brstm->ADPCM_coefs[realc][i];
                }
                if(c != realc) brstm->ADPCM_data[realc] = nullptr;
            } else {
                brstm->PCM_samples[c] = brstm->PCM_samples[realc];
                if(c != realc) brstm->PCM_samples[realc] = nullptr;
            }
            c++;
        }
    }
    if(brstm->num_channels != newChannelCount) {
        //make sure new track information will be written
        if(userTrackChannels == 0) userTrackChannels = brstm->track_num_channels[0];
    }
    brstm->num_channels = newChannelCount;
}

void mixTracks (Brstm* brstm) {
    //Allocate mixing buffer
    int16_t* mixbuffer0;
    int16_t* mixbuffer1;
    mixbuffer0 = new int16_t[brstm->total_samples];
    mixbuffer1 = new int16_t[brstm->total_samples];
    
    //Clear out the mixing buffer
    for(unsigned long s=0; s<brstm->total_samples; s++) {
        mixbuffer0[s] = 0;
        mixbuffer1[s] = 0;
    }
    
    //Mixer
    for(unsigned int t=0; t<brstm->num_tracks; t++) {
        if(!userTracksToMix[t]) continue;
        unsigned char ch1id = brstm->track_lchannel_id [t];
        unsigned char ch2id = brstm->track_num_channels[t] == 2 ? brstm->track_rchannel_id[t] : ch1id;
        double track_volume = (brstm->track_desc_type == 0 ? 1 : (double)brstm->track_volume[t]/127);
        for(unsigned long s=0; s<brstm->total_samples; s++) {
            mixbuffer0[s] = brstm_clamp16( ((int32_t)mixbuffer0[s] + brstm->PCM_samples[ch1id][s]*track_volume) );
            mixbuffer1[s] = brstm_clamp16( ((int32_t)mixbuffer1[s] + brstm->PCM_samples[ch2id][s]*track_volume) );
        }
    }
    
    if(userTrackMixingMono) {
        //Mix final mix into a single mono channel
        int32_t mixsample;
        for(unsigned long s=0; s<brstm->total_samples; s++) {
            mixsample = mixbuffer0[s];
            mixsample += mixbuffer1[s];
            mixsample /= 2;
            mixbuffer0[s] = mixsample;
        }
    }
    
    //Replace audio data in BRSTM struct with mixed track
    //Free old audio data
    for(unsigned int c=0; c<brstm->num_channels; c++) {
        delete[] brstm->PCM_samples[c];
        brstm->PCM_samples[c] = nullptr;
    }
    
    //Write new mixed audio data
    brstm->num_channels = (userTrackMixingMono ? 1 : 2);
    brstm->PCM_samples[0] = mixbuffer0;
    if(!userTrackMixingMono) brstm->PCM_samples[1] = mixbuffer1;
    //Free right channel buffer if mixing to mono
    if(userTrackMixingMono) delete[] mixbuffer1;
    
    //Write track information
    brstm->num_tracks = 1;
    brstm->track_desc_type = 0;
    brstm->track_num_channels[0] = brstm->num_channels;
    brstm->track_lchannel_id [0] = 0;
    brstm->track_rchannel_id [0] = (userTrackMixingMono ? 0 : 1);
    brstm->track_volume      [0] = 0;
    brstm->track_panning     [0] = 0;
}

void readWAV(Brstm* brstm,std::ifstream& stream,std::streampos fsize) {
    //Read file
    stream.seekg(0);
    unsigned char* memblock = new unsigned char[fsize];
    stream.read((char*)memblock,fsize);
    if(!stream.good()) {
        perror("WAV file read error");
        exit(255);
    }
    
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
            //TODO Does this actually work?
            #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            cSample = __builtin_bswap16(cSample);
            #endif
            samplebytes[0]   = cSample&0xFF;
            samplebytes[1] = (cSample>>8)&0xFF;
            stream.write((char*)samplebytes,2);
        }
    }
    
    if(!stream.good()) {
        perror("WAV file write error");
        exit(255);
    }
}

//Used in reencoder
void delete_ffmpeg_files() {
    int tmp;
    tmp = system("rm .brstm-ffmpeg-i.wav &> /dev/null");
    tmp = system("rm .brstm-ffmpeg-o.wav &> /dev/null");
}

int main(int argc, char** args) {
    if(argc<2 || strcmp(args[1],"--help") == 0) {
        std::cout << helpString;
        return 0;
    }
    
    if(strcmp(args[1],"--version") == 0) {
        std::cout << brstm_getVersionString() << '\n';
        exit(0);
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
    
    //Check for conflicting options
    if(optused[10] && (optused[3] || optused[8])) {
        std::cout << "--mix-tracks cannot be used together with -c (--track-channels) or -kc (--keep-channels).\n";
        exit(255);
    }
    if(optused[11] && !optused[10]) {
        std::cout << "--mix-tracks-mono can only be used together with --mix-tracks.\n";
        exit(255);
    }
    
    //Apply the options
    //Input file
    inputFileName = args[1];
    //Verbose mode
    if(optused[0]) verb=2;
    //Output file
    if(optused[1]) {outputFileName=optargstr[1]; saveFile=1;}
    //Loop point
    if(optused[2]) {
        unsigned long lp = atoi(optargstr[2]);
        if(lp == (unsigned long)(-1)) {
            userLoop = 1;
            userLoopFlag  = 0;
            userLoopPoint = 0;
        } else {
            userLoop = 1;
            userLoopFlag  = 1;
            userLoopPoint = lp;
        }
    }
    //Track channel count
    if(optused[3]) {
        unsigned int tc = atoi(optargstr[3]);
        if(!(tc >= 1 && tc <= 2)) {std::cout << "Track channel count must be 1 or 2.\n"; exit(255);}
        userTrackChannels = tc;
    }
    //FFMPEG
    if(optused[4]) {ffmpegArgs=optargstr[4]; useFFMPEG=1; reencode = 1;}
    //Reencode
    if(optused[5]) reencode = 1;
    //Extend / cut
    if(optused[6]) {extendSampleCount = atoi(optargstr[6]);}
    //Output file codec
    if(optused[7]) {
        if(strcmp(optargstr[7], "same") == 0) {
            userCodec = -2;
        } else {
            userCodec = atoi(optargstr[7]);
        }
    }
    //Keep channels
    if(optused[8]) {
        unsigned char len = strlen(optargstr[8]);
        if(len > 16) len = 16;
        for(unsigned char i=0;i<len;i++) {
            if(optargstr[8][i] == '0') keepChannels[i] = 0;
        }
    }
    //Custom byte order
    if(optused[9]) {
        userEndian = (bool)atoi(optargstr[9]);
    }
    //Track mixer
    if(optused[10]) {
        userTrackMixing = 1;
        unsigned char len = strlen(optargstr[10]);
        if(len > 8) len = 8;
        for(unsigned char i=0;i<len;i++) {
            if(optargstr[10][i] == '1') userTracksToMix[i] = 1;
        }
        //Track mixing is done on PCM
        reencode = 1;
    }
    //Mono track mixing
    if(optused[11]) {
        userTrackMixingMono = 1;
    }
    
    
    //Safety
    if(extendSampleCount > 80000000) {
        std::cout << "Extensions longer than 80000000 samples are not supported for safety reasons.\n";
        exit(255);
    }
    //The extender / cutter won't run when the input count is set to 0
    if(extendSampleCount > 0 && extendSampleCount < 128) {
        std::cout << "Cuts shorter than 128 samples are not supported for safety reasons.\n";
        exit(255);
    }
    //Enable reencoding if we want to extend or cut samples
    if(extendSampleCount) reencode = 1;
    
    //Check file extensions
    inputFileExt = getFileExt(inputFileName);
    //Assume BRSTM if the input extension does not match any known formats
    if(inputFileExt == -1) inputFileExt = 1;
    
    if(saveFile) {
        outputFileExt = getFileExt(outputFileName);
        if(outputFileExt == -1) {std::cout << "Unsupported output file extension.\n"; exit(255);}
    }
    
    //Create BRSTM struct and open files
    Brstm* brstm = new Brstm;
    brstm_init(brstm);
    std::ifstream ifile;
    std::ofstream ofile;
    std::streampos ifsize;
    
    //Check if output file can be opened for writing
    if(saveFile) {
        ofile.open(outputFileName, std::ios::out|std::ios::binary|std::ios::app);
        if(!ofile.is_open()) {perror(outputFileName); exit(255);}
        ofile.close();
    }
    
    //Open and read input file
    ifile.open(inputFileName,std::ios::in|std::ios::binary|std::ios::ate);
    if(!ifile.is_open()) {
        perror(inputFileName);
        exit(255);
    }
    ifsize = ifile.tellg();
    ifile.seekg(0);
    
    //Read input file base information
    if(inputFileExt > 0) {
        unsigned char res = brstm_fstream_getBaseInformation(brstm,ifile,0);
        if(res>127) {
            std::cout << "Input file error. (" << (int)res << ")\n";
            exit(res);
        }
        //Change from rebuilding to reencoding if the codec is not ADPCM
        if(outputFileExt > 0 && brstm->codec != 2) reencode=1;
    }
    
    //Enable reencoding if the user requested an output codec that isn't ADPCM (the rebuilder is meant for ADPCM data)
    if( userCodec != -1 && !(userCodec == 2 || (userCodec == -2 && brstm->codec == 2)) ) reencode = 1;
    
    //Run conversions
    //Decoder BRSTM/other -> WAV
    if(inputFileExt > 0 && outputFileExt == 0) {
        //check for unsupported opts
        if(userLoop)  {std::cout << "You cannot use the loop option in decoding mode.\n"; exit(255);}
        if(useFFMPEG) {std::cout << "You cannot use the FFMPEG option in decoding mode.\n"; exit(255);}
        if(userTrackChannels) {std::cout << "You cannot use the track channel count option in decoding mode.\n"; exit(255);}
        if(userCodec != -1) {std::cout << "You cannot use the output codec option in decoding mode.\n"; exit(255);}
        if(userEndian != -1) {std::cout << "You cannot use the output endian option in decoding mode.\n"; exit(255);}
        //print conversion details
        if(saveFile) printConversionDetails();
        
        //Read file data
        unsigned char * memblock = new unsigned char[ifsize];
        ifile.read((char*)memblock,ifsize);
        if(!ifile.good()) {
            perror(inputFileName);
            exit(255);
        }
        
        //Read the BRSTM
        unsigned char result = brstm_read(brstm,memblock,verb,true);
        if(result>127) {
            std::cout << "File read error. (" << (int)result << ")\n";
            return result;
        }
        
        delete[] memblock;
        
        if(saveFile) {
            //Remove channels
            removeChannels(brstm,0);
            
            //Mix tracks
            if(userTrackMixing) {
                if(verb) std::cout << "Mixing tracks...\n";
                mixTracks(brstm);
            }
            
            //Extender / cutter
            if(extendSampleCount) {
                if(verb) std::cout << (brstm->total_samples >= extendSampleCount ? "Cutting" : "Extending")
                    << " from " << brstm->total_samples << " samples to " << extendSampleCount << " samples...\n";
                extendPCMSamples(brstm,extendSampleCount);
            }
            
            //Open output file
            ofile.open(outputFileName,std::ios::out|std::ios::binary|std::ios::trunc);
            if(!ofile.is_open()) {perror(outputFileName); exit(255);}
            
            //Write output WAV file
            writeWAV(brstm,ofile);
            
            std::cout << "Saved file to " << outputFileName << '\n';
        }
    }
    
    //Encoder WAV -> BRSTM/other
    else if(inputFileExt == 0 && outputFileExt > 0) {
        //check for unsupported opts
        if(useFFMPEG) {std::cout << "You cannot use the FFMPEG option in encoding mode.\n"; exit(255);}
        if(userTrackMixing) {std::cout << "Track mixing cannot be done on WAV files.\n"; exit(255);};
        //print conversion details
        if(saveFile) printConversionDetails();
        
        //Read input WAV file
        brstmStereoTracks = userTrackChannels > 0 ? userTrackChannels-1 : 1;
        readWAV(brstm,ifile,ifsize);
        
        if(saveFile) {
            //Set other BRSTM data
            brstm->file_format = outputFileExt;
            brstm->codec = 2;
            
            if(userCodec != -1) brstm->codec = userCodec;
            if(userCodec == -2) brstm->codec = 1;
            
            if(userLoop) {
                brstm->loop_flag  = userLoopFlag;
                brstm->loop_start = userLoopPoint;
            }
            //Remove channels
            removeChannels(brstm,0);
            //Set channels to 1 if we have only 1 channel
            if(brstmStereoTracks && brstm->num_channels < 2) {
                brstmStereoTracks = false;
            }
            //Make sure the amount of channels is valid
            if(brstmStereoTracks && brstm->num_channels%2 != 0) {
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
            //Extender / cutter
            if(extendSampleCount) {
                if(verb) std::cout << (brstm->total_samples >= extendSampleCount ? "Cutting" : "Extending")
                    << " from " << brstm->total_samples << " samples to " << extendSampleCount << " samples...\n";
                extendPCMSamples(brstm,extendSampleCount);
            }
            
            if(verb) std::cout
                << "Looping BRSTM: " << brstm->loop_flag
                << "\nLoop point: " << brstm->loop_start
                << "\nTracks: " << brstm->num_tracks << "\n";
            
            //Encode
            unsigned char res;
            //Use default byte order
            if(userEndian == -1) res = brstm_encode(brstm,1,1);
            //Use custom byte order
            else res = brstm_encode(brstm,1,1,userEndian);
            
            if(res>127) {
                std::cout << "Encoding error. (" << (int)res << ")\n";
                exit(res);
            } else {
                //Write output file
                ofile.open(outputFileName,std::ios::out|std::ios::binary|std::ios::trunc);
                if(!ofile.is_open()) {perror(outputFileName); exit(255);}
                ofile.write((char*)brstm->encoded_file,brstm->encoded_file_size);
                if(!ofile.good()) {perror(outputFileName); exit(255);}
            }
        }
    }
    
    //Lossless rebuilder
    else if(inputFileExt > 0 && outputFileExt > 0 && reencode == 0) {
        //check for unsupported opts
        if(useFFMPEG) {std::cout << "You cannot use the FFMPEG option in rebuilder mode.\n"; exit(255);}
        if( userCodec != -1 && !(userCodec == 2 || (userCodec == -2 && brstm->codec == 2)) ) {std::cout << "You cannot use the output codec option in rebuilder mode.\n"; exit(255);}
        if(userTrackMixing) {std::cout << "You cannot mix tracks when losslessly converting files.\n"; exit(255);}
        //print conversion details
        if(saveFile) printConversionDetails();
        
        //Read file data
        unsigned char * memblock = new unsigned char[ifsize];
        ifile.read((char*)memblock,ifsize);
        if(!ifile.good()) {
            perror(inputFileName);
            exit(255);
        }
        
        //Read the BRSTM
        unsigned char result=brstm_read(brstm,memblock,verb,2);
        if(result>127) {
            std::cout << "File read error. (" << (int)result << ")\n";
            return result;
        }
        delete[] memblock;
        //Set output format
        brstm->file_format = outputFileExt;
        
        if(saveFile) {
            //Apply new loop setting
            if(userLoop) {
                brstm->loop_flag = userLoopFlag;
                brstm->loop_start = userLoopPoint;
            }
            
            //Remove channels
            removeChannels(brstm,1);
            
            //Apply new track settings
            if(userTrackChannels != 0) {
                brstmStereoTracks = userTrackChannels - 1;
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
            }
            
            if(verb) std::cout
                << "Output:"
                << "\n  Looping BRSTM: " << brstm->loop_flag
                << "\n  Loop point: " << brstm->loop_start
                << "\n  Tracks: " << brstm->num_tracks
                << "\n";
            
            //Build new file
            unsigned char res;
            //Use default byte order
            if(userEndian == -1) res = brstm_encode(brstm,1,0);
            //Use custom byte order
            else res = brstm_encode(brstm,1,0,userEndian);
            
            if(res>127) {
                std::cout << "Encoding error. (" << (int)res << ")\n";
                exit(res);
            } else {
                //Write output file
                ofile.open(outputFileName,std::ios::out|std::ios::binary|std::ios::trunc);
                if(!ofile.is_open()) {perror(outputFileName); exit(255);}
                ofile.write((char*)brstm->encoded_file,brstm->encoded_file_size);
                if(!ofile.good()) {perror(outputFileName); exit(255);}
            }
        }
    }
    
    //Reencoder
    else if(inputFileExt > 0 && outputFileExt > 0 && reencode == 1) {
        //print conversion details
        if(saveFile) printConversionDetails();
        
        //Read file data
        unsigned char * memblock = new unsigned char[ifsize];
        ifile.read((char*)memblock,ifsize);
        if(!ifile.good()) {
            perror(inputFileName);
            exit(255);
        }
        
        //Read the BRSTM
        unsigned char result=brstm_read(brstm,memblock,verb,true);
        if(result>127) {
            std::cout << "File read error. (" << (int)result << ")\n";
            return result;
        }
        delete[] memblock;
        
        //Remove channels
        removeChannels(brstm,0);
        
        //Mix tracks
        if(userTrackMixing) {
            if(verb) std::cout << "Mixing tracks...\n";
            mixTracks(brstm);
        }
        
        //FFMPEG: save audio to WAV file, run ffmpeg on it, and read the output
        if(useFFMPEG) {
            delete_ffmpeg_files();
            //Create WAV file
            std::ofstream ffmpeg_ofile (".brstm-ffmpeg-i.wav",std::ios::out|std::ios::binary|std::ios::trunc);
            if(!ffmpeg_ofile.is_open()) {
                perror("Unable to open FFMPEG input file");
                delete_ffmpeg_files();
                exit(255);
            }
            writeWAV(brstm,ffmpeg_ofile);
            ffmpeg_ofile.close();
            
            //Run FFMPEG
            std::string systemCommand = "ffmpeg -i .brstm-ffmpeg-i.wav ";
            systemCommand += ffmpegArgs;
            systemCommand += " .brstm-ffmpeg-o.wav";
            std::cout << systemCommand << '\n';
            if(system(systemCommand.c_str())) {goto ffmpegOutputError;}
            
            {
                //Read FFMPEG output WAV
                std::ifstream ffmpeg_ifile (".brstm-ffmpeg-o.wav", std::ios::in|std::ios::binary|std::ios::ate);
                if (ffmpeg_ifile.is_open()) {
                    std::streampos ffmpeg_fsize = ffmpeg_ifile.tellg();
                    memblock = new unsigned char [ffmpeg_fsize];
                    ffmpeg_ifile.seekg (0, std::ios::beg);
                    ffmpeg_ifile.read ((char*)memblock, ffmpeg_fsize);
                    if(!ffmpeg_ifile.good()) {
                        perror("FFMPEG output file read error");
                        exit(255);
                    }
                    ffmpeg_ifile.close();
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
                unsigned long oldSampleRate = brstm->sample_rate;
                brstm->sample_rate = brstm_getSliceAsNumber(memblock,24,4,0);
                //calculate new loop point if audio was resampled
                if(oldSampleRate != brstm->sample_rate) {
                    std::cout << "Calculating new loop point for resampled audio\n";
                    brstm->loop_start = brstm->loop_start * (brstm->sample_rate / (double)oldSampleRate);
                }
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
                    //Reallocate memory
                    for(c=0;c<brstm->num_channels;c++) {
                        delete[] brstm->PCM_samples[c];
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
            delete_ffmpeg_files();
            goto ffmpegSuccess;
            
            ffmpegOutputError:
            delete_ffmpeg_files();
            std::cout << "Invalid FFMPEG output file.\n"; exit(255);
            ffmpegSuccess:;
        }
        
        //Set format and codec
        brstm->file_format = outputFileExt;
        
        if(userCodec == -1) brstm->codec = 2;
        if(userCodec != -1 && userCodec != -2) brstm->codec = userCodec;
        
        if(saveFile) {
            //Apply new loop setting
            if(userLoop) {
                brstm->loop_flag = userLoopFlag;
                brstm->loop_start = userLoopPoint;
            }
            
            //Apply new track settings
            if(userTrackChannels != 0) {
                brstmStereoTracks = userTrackChannels - 1;
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
            }
            
            //Extender / cutter
            if(extendSampleCount) {
                if(verb) std::cout << (brstm->total_samples >= extendSampleCount ? "Cutting" : "Extending")
                    << " from " << brstm->total_samples << " samples to " << extendSampleCount << " samples...\n";
                extendPCMSamples(brstm,extendSampleCount);
            }
            
            if(verb) std::cout
                << "Output:"
                << "\n  Looping BRSTM: " << brstm->loop_flag
                << "\n  Loop point: " << brstm->loop_start
                << "\n  Tracks: " << brstm->num_tracks
                << "\n";
            
            //Encode new file
            unsigned char res;
            //Use default byte order
            if(userEndian == -1) res = brstm_encode(brstm,1,1);
            //Use custom byte order
            else res = brstm_encode(brstm,1,1,userEndian);
            
            if(res>127) {
                std::cout << "Encoding error. (" << (int)res << ")\n";
                exit(res);
            } else {
                //Write output file
                ofile.open(outputFileName,std::ios::out|std::ios::binary|std::ios::trunc);
                if(!ofile.is_open()) {perror(outputFileName); exit(255);}
                ofile.write((char*)brstm->encoded_file,brstm->encoded_file_size);
                if(!ofile.good()) {perror(outputFileName); exit(255);}
            }
        }
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
