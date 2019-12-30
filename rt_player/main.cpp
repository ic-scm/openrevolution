//C++ BRSTM reader
//Made by extrasklep copyright license bla bla bla
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

int16_t* PCM_samples[16];

int16_t* PCM_buffer[16];

unsigned long written_samples=0;

#include "../brstm.h" //must be included after this stuff

unsigned char* memblock;

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
unsigned int Channels=0;
int current_track=0;
unsigned long playback_seconds=0;
unsigned long total_seconds=0;
bool stop_playing=0;
bool paused=0;
unsigned int update_display_i=0;

//RtAudio callback
int RtAudioCb( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void *userData) {
    unsigned int i, c;
    double *buffer = (double *) outputBuffer;
    if (status) {std::cout << "Stream underflow detected!\n";}
    
    //Display
    playback_seconds=playback_current_sample/HEAD1_sample_rate;
    //std::cout << "\rCurrent time: " << secondsToMString(playback_seconds) << " Buffer frames: " << nBufferFrames << " Channels: " << Channels << " Played samples: " << playback_current_sample << "              ";
    if(update_display_i>0) {
        update_display_i=0; 
        std::cout << '\r';
        if(paused) {std::cout << "Paused ";}
        std::cout /*<< "\e[?25l"*/ << "(" << secondsToMString(playback_seconds) << "/" << secondsToMString(total_seconds) << " Track: " << current_track+1 << ") (< >:Seek /\\ \\/:Switch track):\033[0m                           \r" /*<< "\e[?25h"*/;
    }
    update_display_i++;
    
    //Get buffer and write data
    if(!paused) {
        brstm_getbuffer(memblock,playback_current_sample,nBufferFrames,true);
        int ioffset=0;
        for (i=0;i<nBufferFrames;i+=1) {
            for(c=0;c<Channels;c++) {
                unsigned char tchannel;
                if(c==0) {tchannel=HEAD2_track_lchannel_id[current_track];}
                else {tchannel=HEAD2_track_rchannel_id[current_track];}
                *buffer++ = (double)PCM_buffer[tchannel][i+ioffset]/(double)32768;
            }
            playback_current_sample++;
            if(playback_current_sample>HEAD1_total_samples) {
                if(HEAD1_loop) {
                    playback_current_sample=HEAD1_loop_start;
                    //refill buffer
                    brstm_getbuffer(memblock,playback_current_sample,nBufferFrames,true);
                    ioffset-=i;
                } else {stop_playing=1; return 1;}
            }
        }
    } else {
            for(i=0;i<nBufferFrames;i+=1) {
                for(c=0;c<Channels;c++) {
                    *buffer++ = 0;
                }
            }
        }
    return 0;
}

//-------------------######### STRINGS

const char* helpString0 = "Usage:\n";
const char* helpString1 = " [file to open] [options...]\nOptions:\n-v - Verbose output\n";

const char* opts[] = {"-v"};
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
        for(unsigned int o=0;o<1;o++) {
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
    unsigned char result=readBrstm(memblock,verb,false);
    if(result>127) {
        std::cout << "Error.\n";
        return result;
    }
    
    //initialize rtaudio
    RtAudio dac;
    if (dac.getDeviceCount()<1) {
        std::cout << "No audio device found.\n";
        exit(255);
    }
    RtAudio::StreamParameters parameters;
    RtAudio::StreamOptions options;
    parameters.deviceId = dac.getDefaultOutputDevice();
    if(HEAD3_num_channels<2) {Channels=1;}
    else {Channels=2;}
    parameters.nChannels = Channels;
    parameters.firstChannel = 0;
    options.streamName="BRSTM";
    unsigned int sampleRate = HEAD1_sample_rate;
    unsigned int bufferFrames = 256; // 256 sample frames
    double data[2];
    if(verb) {std::cout << "Playback channels: " << parameters.nChannels << '\n';}
    try {
        dac.openStream( &parameters, NULL, RTAUDIO_FLOAT64, sampleRate, &bufferFrames, &RtAudioCb, (void *)&data, &options);
        dac.startStream();
    } catch (RtAudioError& e) {
        e.printMessage();
        exit(255);
    }
    
    //User input
    char input;
    total_seconds=HEAD1_total_samples/HEAD1_sample_rate;
    
    while(stop_playing==0) {
        input=getch();
        if(input=='\033') {
        getch();
        input=getch();
        switch(input) {
            case 'A': /*U*/ current_track++; if(current_track>=HEAD2_num_tracks) {current_track=HEAD2_num_tracks-1;} /*std::cout << "UP\n";*/ break;
            case 'B': /*D*/ current_track--; if(current_track<0) {current_track=0;} /*std::cout << "DOWN\n";*/ break;
            case 'C': /*R*/ playback_current_sample+=HEAD1_sample_rate; if(playback_current_sample>HEAD1_total_samples) {playback_current_sample=HEAD1_total_samples;} /*std::cout << "RIGHT\n";*/ break;
            case 'D': /*L*/ playback_current_sample-=HEAD1_sample_rate; if(playback_current_sample<0) {playback_current_sample=0;} /*std::cout << "LEFT\n";*/ break;
        }
        } else switch(input) {
            /*case 'w': case 'W': std::cout << "W UP\n"; break;
            case 's': case 'S': std::cout << "S DOWN\n"; break;
            case 'a': case 'A': std::cout << "A LEFT\n"; break;
            case 'd': case 'D': std::cout << "D RIGHT\n"; break;*/
            case ' ': paused=!paused; /*std::cout << "SPACE\n";*/ break;
        }
        //std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::cout << '\n';
    //std::cin.get( input );
    
    
    try {
        // Stop the stream
        dac.stopStream();
    } catch (RtAudioError& e) {
        e.printMessage();
    }
    if (dac.isStreamOpen()) dac.closeStream();
    
    return 0;
}
