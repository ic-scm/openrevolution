//C++ BRSTM reader/player
//Copyright (C) 2020 Extrasklep
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <termios.h>

#include "RtAudio.h"

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

int16_t* PCM_samples[16]; //unused here

int16_t* PCM_buffer[16];

unsigned long written_samples=0;

#include "../brstm.h" //must be included after this stuff

//BRSTM file memblock
unsigned char* memblock;

signed char memoryMode = -1;
//-1 - default
// 0 - load file into memory and decode in real time
// 1 - stream file from disk
// 2 - decode all audio data into memory before playing

void itoa(int n, char* s) {
    std::string ss = std::to_string(n);
    strcpy(s,ss.c_str());
}  

char* mString;
char* secondsToMString(unsigned int sec) {
    delete[] mString;
    mString=new char[10];
    unsigned int min=0;
    unsigned int secs=0;
    unsigned int csec=sec;
    while(csec>=60) {
        csec-=60;
        min++;
    }
    secs=csec;
    unsigned int mStringPos=0;
    char* minString = new char[5];
    itoa(min,minString);
    char* secString = new char[3];
    itoa(secs,secString);
    for(unsigned int i=0;i<strlen(minString);i++) {mString[mStringPos++]=minString[i];}
    mString[mStringPos++]=':';
    if(secs<10) {mString[mStringPos++]='0';}
    for(unsigned int i=0;i<strlen(secString);i++) {mString[mStringPos++]=secString[i];}
    mString[mStringPos++]='\0';
    return mString;
}

//stolen
char getch(void) {
    char buf = 0;
    struct termios old = {0};
    fflush(stdout);
    if(tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if(tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if(read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if(tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return buf;
}

long playback_current_sample=0;
int current_track=0;
unsigned long playback_seconds=0;
unsigned long total_seconds=0;
bool stop_playing=0;
bool paused=0;

//get the buffer in different ways depending on the memory mode
void getBufferHelper(unsigned long sampleOffset,unsigned int bufferSize) {
    switch(memoryMode) {
        case 0: brstm_getbuffer(memblock,sampleOffset,bufferSize); break;
        case 1: std::cout << "Mode 1 not implemented yet\n\n\nyour terminal is probably messed up now sorry\n"; exit(255);
        case 2:
        //full decode mode
        for(unsigned int c=0;c<HEAD3_num_channels;c++) {
            delete[] PCM_buffer[c];
            PCM_buffer[c] = new int16_t[bufferSize];
            for(unsigned int i=0;i<bufferSize;i++) {
                PCM_buffer[c][i] = PCM_samples[c][sampleOffset+i];
            }
        }
        break;
    }
}

//RtAudio callback
int RtAudioCb( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void *userData) {
    unsigned int i;
    double *buffer = (double *) outputBuffer;
    //if(status) {std::cout << "Stream underflow detected!\n";}
    
    //Update the display
    playback_seconds=playback_current_sample/HEAD1_sample_rate;
    std::cout << '\r';
    if(paused) {std::cout << "Paused ";}
    std::cout << "(" << secondsToMString(playback_seconds) << "/" << secondsToMString(total_seconds) << " Track: " << current_track+1
    << ") (< >:Seek /\\ \\/:Switch track):\033[0m                           \r";
    
    //Get buffer and write data
    unsigned char ch1id = HEAD2_track_lchannel_id[current_track];
    unsigned char ch2id = HEAD3_num_channels > 1 ? HEAD2_track_rchannel_id[current_track] : ch1id;
    
    if(!paused) {
        getBufferHelper(playback_current_sample,
                        //Avoid reading garbage outside the file
                        HEAD1_total_samples-playback_current_sample < nBufferFrames ? HEAD1_total_samples-playback_current_sample : nBufferFrames
                        );
        int ioffset=0;
        for (i=0;i<nBufferFrames;i+=1) {
            *buffer++ = (double)PCM_buffer[ch1id][i+ioffset] / 32768;
            *buffer++ = (double)PCM_buffer[ch2id][i+ioffset] / 32768;
            
            playback_current_sample++;
            if(playback_current_sample > HEAD1_total_samples) {
                if(HEAD1_loop) {
                    playback_current_sample=HEAD1_loop_start;
                    //refill buffer
                    getBufferHelper(playback_current_sample,nBufferFrames);
                    ioffset-=i;
                } else {
                    stop_playing=1; 
                    return 1;
                }
            }
        }
    } else {
        //player is paused
        for(i=0;i<nBufferFrames;i+=1) {
            *buffer++ = 0;
            *buffer++ = 0;
        }
    }
    return 0;
}

//-------------------######### STRINGS

const char* helpString0 = "Usage:\n";
const char* helpString1 = " [file to open] [options...]\nOptions:\n-v - Verbose output\n\nMemory modes:\n(default) - Load the file into memory and decode it in real time\n-s - Stream the audio data from disk (lower memory usage, recommended for large files)\n-d - Decode the entire file before playing it (high memory usage, not recommended)\n";

const char* opts[] = {"-v","-s","-d"};
//____________________________________
bool verb=0;

int main( int argc, char* args[] ) {
    if(argc<2) {
        std::cout << helpString0 << args[0] << helpString1;
        return 0;
    }
    //Check user arguments
    //TODO replace this with something better and cleaner?
    for(unsigned int i=2;i<argc;i++) {
        char* currentArg=args[i];
        int vOpt=-1;
        for(unsigned int o=0;o<3 /*replace this number with the amount of strings in opts*/;o++) {
            unsigned int matched=0;
            for(unsigned int s=0;s<strlen(currentArg);s++) {
                if(args[i][s]==opts[o][s]) {matched++;}
            }
            if(matched>=strlen(opts[o])) {
                vOpt=o;
            }
        }
        if(vOpt==0) {verb=1;}
        if(vOpt==1) {memoryMode=1;}
        if(vOpt==2) {memoryMode=2;}
    }
    
    //Read the file
    std::streampos fsize;
    std::ifstream file (args[1], std::ios::in|std::ios::binary|std::ios::ate);
    if (file.is_open()) {
        fsize = file.tellg();
        file.seekg (0, std::ios::beg);
        if(memoryMode == -1) {
            //pick default memory mode
            memoryMode = 0;
        }
        //don't read the file in mode 1
        if(memoryMode != 1) {
            memblock = new unsigned char [fsize];
            file.read ((char*)memblock, fsize);
            if(verb) {std::cout << "Read file " << args[1] << " size " << fsize << '\n';}
            file.close();
        }
    } else {std::cout << "Unable to open file \"" << args[1] << "\".\n"; return 255;}
    
    if(verb) switch(memoryMode) {
        case 0: std::cout << "Realtime decoding mode\n"; break;
        case 1: std::cout << "Disk stream mode\n"; break;
        case 2: std::cout << "Full decode mode\n"; break;
    }
    
    //Read the BRSTM headers
    if(memoryMode != 1) {
        //use normal brstm functions
        unsigned char result=brstm_read(memblock,verb,
            //decode the audio data if memory mode is 2
            memoryMode == 2 ? true : false
        );
        //the file data will not be needed anymore in memory mode 2
        if(memoryMode == 2) {delete[] memblock;}
        if(result>127) {
            std::cout << "Error.\n";
            return result;
        }
    } else {
        //disk streaming mode
        std::cout << "Mode 1 not implemented yet\n";
        exit(255);
    }
    
    //calculate total seconds
    total_seconds=HEAD1_total_samples/HEAD1_sample_rate;
    
    //Initialize rtaudio
    RtAudio dac;
    if (dac.getDeviceCount()<1) {
        std::cout << "No audio device found.\n";
        exit(255);
    }
    RtAudio::StreamParameters parameters;
    RtAudio::StreamOptions options;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = 2;
    parameters.firstChannel = 0;
    options.streamName = "BRSTM";
    unsigned int sampleRate = HEAD1_sample_rate;
    unsigned int bufferFrames = 256; // 256 sample frames
    try {
        dac.openStream( &parameters, NULL, RTAUDIO_FLOAT64, sampleRate, &bufferFrames, &RtAudioCb, (void *)&file, &options);
        dac.startStream();
    } catch (RtAudioError& e) {
        e.printMessage();
        exit(255);
    }
    
    //User input
    char input;
    while(stop_playing==0) {
        input=getch();
        if(input=='\033') {
            getch();
            input=getch();
            switch(input) {
                case 'A': /*U*/ current_track++; if(current_track>=HEAD2_num_tracks) {current_track=HEAD2_num_tracks-1;} break;
                case 'B': /*D*/ current_track--; if(current_track<0) {current_track=0;} break;
                case 'C': /*R*/ playback_current_sample+=HEAD1_sample_rate; if(playback_current_sample>HEAD1_total_samples) {playback_current_sample=HEAD1_total_samples;} break;
                case 'D': /*L*/ playback_current_sample-=HEAD1_sample_rate; if(playback_current_sample<0) {playback_current_sample=0;} break;
            }
        } else switch(input) {
            //reserved for more features in the future?
            /*case 'w': case 'W': break;
            case 's': case 'S': break;
            case 'a': case 'A': break;
            case 'd': case 'D': break;*/
            case ' ': paused=!paused; break;
            case 'q': case 'Q': stop_playing = 1; break;
        }
        //std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::cout << '\n';
    
    try {
        // Stop the stream
        dac.stopStream();
    } catch (RtAudioError& e) {
        e.printMessage();
    }
    if (dac.isStreamOpen()) dac.closeStream();
    
    brstm_close();
    
    return 0;
}
