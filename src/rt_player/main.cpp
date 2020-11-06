//OpenRevolution RtAudio player
//Copyright (C) 2020 IC
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

#include "../lib/brstm.h"

#define OUTPUT_BUFSIZE 512

void itoa(int n, char* s) {
    std::string ss = std::to_string(n);
    strcpy(s,ss.c_str());
}

char mString[10];
char* secondsToMString(unsigned int sec) {
    for(unsigned char i=0;i<10;i++) {
        mString[i] = 0;
    }
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
    delete[] minString;
    delete[] secString;
    return mString;
}

// https://stackoverflow.com/a/16361724
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

//Console output options
//Quiet output (no player UI)
bool quietOutput = 0;
//Verbose mode
bool verb=0;
//Classic player UI
bool classic_noflush = 0;
//UI refresh timer
uint16_t ui_counter = 0;
uint16_t ui_counter_l = 0;

//Track switching and mixing
bool track_mixing_enabled = 0;
//Toggles for which tracks are playing when using the track mixer.
bool tracks_enabled[9] = {1,0,0,0,0,0,0,0,0};
//Current track in normal track switching
unsigned int current_track=0;

//Playback
unsigned long playback_current_sample=0;
unsigned long playback_seconds=0;
unsigned long total_seconds=0;
char total_seconds_string[10];
bool stop_playing=0;
bool paused=0;

std::ifstream file;
Brstm* brstm;

signed char memoryMode = -1;
//-1 - Automatically picked
// 0 - Load file into memory and decode in real time
// 1 - Stream file from disk and decode in real time
// 2 - Decode all audio data into memory before playing

//Player UI
void drawPlayerUI() {
    playback_seconds=playback_current_sample/brstm->sample_rate;
    
    if(!quietOutput) {
        
        if(classic_noflush || (ui_counter_l <= ui_counter) ) {
            ui_counter = 0;
            
            std::cout << '\r'
            // Pause
            << (paused ? "Paused " : "")
            // Time
            << "(" << secondsToMString(playback_seconds) << "/" << total_seconds_string;
            // Tracks
            if(!track_mixing_enabled) {
                std::cout << " Track: " << current_track+1;
            } else {
                std::cout << " Tracks:";
                for(unsigned int t=0; t<brstm->num_tracks; t++) {
                    std::cout << ' ' << t+1 << '[' << (tracks_enabled[t] ? 'X' : ' ') << ']';
                }
            }
            // Controls guide
            std::cout << ") (< >:Seek ";
            if(track_mixing_enabled) {
                std::cout << "1-" << brstm->num_tracks << ":Toggle tracks";
            } else {
                std::cout << "/\\ \\/:Switch track";
            }
            std::cout << "):\033[0m        "
            // End
            << (!paused ? "       " : "") << "\r";
            
            //Flush
            if(!classic_noflush) fflush(stdout);
        }
        
        ui_counter++;
    }
}

//Get the buffer in different ways depending on the memory mode
void getBufferHelper(void* userData,unsigned long sampleOffset,unsigned int bufferSize) {
    switch(memoryMode) {
        //Realtime decoding from memory
        case 0:
        brstm_getbuffer(brstm,(const unsigned char*) userData,sampleOffset,bufferSize);
        return;
        
        //Streaming data from disk and realtime decoding
        case 1:
        brstm_fstream_getbuffer(brstm,file,sampleOffset,bufferSize);
        return;
        
        //Full decoding
        case 2:
        for(unsigned int c=0;c<brstm->num_channels;c++) {
            delete[] brstm->PCM_buffer[c];
            brstm->PCM_buffer[c] = new int16_t[bufferSize];
            for(unsigned int i=0;i<bufferSize;i++) {
                brstm->PCM_buffer[c][i] = brstm->PCM_samples[c][sampleOffset+i];
            }
        }
        return;
    }
}

int16_t* brstmbuffer[2];

void mixTracks(unsigned int bufferSize) {
    //Clear out the mixing buffer
    for(unsigned int s=0; s<bufferSize; s++) {
        brstmbuffer[0][s] = 0;
        brstmbuffer[1][s] = 0;
    }
    
    for(unsigned int t=0; t<brstm->num_tracks; t++) {
        if(!tracks_enabled[t]) continue;
        unsigned char ch1id = brstm->track_lchannel_id [t];
        unsigned char ch2id = brstm->track_num_channels[t] == 2 ? brstm->track_rchannel_id[t] : ch1id;
        double track_volume = (brstm->track_desc_type == 0 ? 1 : (double)brstm->track_volume[t]/127);
        for(unsigned int s=0; s<bufferSize; s++) {
            brstmbuffer[0][s] = brstm_clamp( ((int32_t)brstmbuffer[0][s] + brstm->PCM_buffer[ch1id][s]*track_volume), -32768, 32767);
            brstmbuffer[1][s] = brstm_clamp( ((int32_t)brstmbuffer[1][s] + brstm->PCM_buffer[ch2id][s]*track_volume), -32768, 32767);
        }
    }
}

//RtAudio callback
int RtAudioCb( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void *userData) {
    unsigned int i;
    int ioffset=0;
    int16_t *buffer = (int16_t*) outputBuffer;
    //if(status) std::cout << "Stream underflow detected!\n";
    
    //Update the display
    drawPlayerUI();
    
    if(!paused) {
        //userData is the file data (unsigned char*)
        //Avoid reading garbage outside the file
        unsigned int samplesToGet = brstm->total_samples-playback_current_sample < nBufferFrames ? brstm->total_samples-playback_current_sample : nBufferFrames;
        getBufferHelper(userData,playback_current_sample,samplesToGet);
        
        //Channel IDs / track mixing
        unsigned char ch1id = 0, ch2id = 0;
        if(!track_mixing_enabled) {
            ch1id = brstm->track_lchannel_id [current_track];
            ch2id = brstm->track_num_channels[current_track] == 2 ? brstm->track_rchannel_id[current_track] : ch1id;
            brstmbuffer[0] = brstm->PCM_buffer[ch1id];
            brstmbuffer[1] = brstm->PCM_buffer[ch2id];
        } else {
            mixTracks(samplesToGet);
        }
        
        for (i=0;i<nBufferFrames;i+=1) {
            *buffer++ = brstmbuffer[0][i+ioffset];
            *buffer++ = brstmbuffer[1][i+ioffset];
            
            playback_current_sample++;
            if(playback_current_sample >= brstm->total_samples) {
                //if(brstm->loop_flag) {
                    
                    playback_current_sample=brstm->loop_start;
                    
                    //Refill buffer, using same safety as before.
                    samplesToGet = brstm->total_samples-playback_current_sample < nBufferFrames-i ? brstm->total_samples-playback_current_sample : nBufferFrames-i;
                    getBufferHelper(userData,playback_current_sample,samplesToGet);
                    
                    //Set up brstmbuffer
                    if(track_mixing_enabled) {mixTracks(samplesToGet);}
                    else {
                        brstmbuffer[0] = brstm->PCM_buffer[ch1id];
                        brstmbuffer[1] = brstm->PCM_buffer[ch2id];
                    }
                    
                    //1 is added because i will be incremented before next sample.
                    ioffset=0-(i+1);
                    
                /*} else {
                    stop_playing=1; 
                    return 1;
                }*/
            }
        }
    } else {
        //Player is paused
        for(i=0;i<nBufferFrames;i+=1) {
            *buffer++ = 0;
            *buffer++ = 0;
        }
    }
    return 0;
}

//-------------------######### STRINGS

const char* helpString = "OpenRevolution audio player\nCopyright (C) 2020 I.C.\nThis program is free software, see the license file for more information.\nUsage:\nbrstm_rt [file to open] [options...]\nOptions:\n-v - Verbose output\n-q - Quiet output (no player UI)\n--force-sample-rate [sample rate] - Force playback sample rate\n--enable-mixer - Enable track mixing for multi-track files\n--classic-ui - Classic brstm_rt appearance\n\nMemory modes:\n-m - Load the file into memory and decode it in real time\n-s - Stream the audio data from disk (lower memory usage, recommended for large files)\n-d - Decode the entire file before playing it (high memory usage, not recommended)\nDefault mode is chosen depending on the file size.\n";

const char* opts[] = {"-v","-m","-s","-d","-force-sample-rate","-q","-enable-mixer","-classic-ui"};
const char* opts_alt[] = {"--verbose","--memory","--streaming","--decode","--force-sample-rate","--quiet","--enable-mixer","--classic-ui"};
const unsigned int optcount = 8;
const bool optrequiredarg[optcount] = {0,0,0,0,1,0,0,0};
bool  optused  [optcount];
char* optargstr[optcount];
//____________________________________

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
    for(int a=2;a<argc;a++) {
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
    unsigned long forcedSampleRate = 0;
    if(optused[0]) verb=1;
    if(optused[1]) memoryMode=0;
    if(optused[2]) memoryMode=1;
    if(optused[3]) memoryMode=2;
    if(optused[4]) forcedSampleRate = atoi(optargstr[4]);
    if(optused[5]) quietOutput=1;
    if(optused[6]) track_mixing_enabled=1;
    if(optused[7]) classic_noflush=1;
    
    //BRSTM file memblock and openrevolution lib struct
    unsigned char* memblock;
    brstm = new Brstm;
    
    //Read the file
    std::streampos fsize;
    file.open(args[1], std::ios::in|std::ios::binary|std::ios::ate);
    if (file.is_open()) {
        fsize = file.tellg();
        file.seekg (0, std::ios::beg);
        //Pick default memory mode
        if(memoryMode == -1) {
            //Get base file information
            unsigned char res = brstm_fstream_getBaseInformation(brstm,file,0);
            if(res>127) exit(res);
            if(brstm->file_format == 4) {
                //Always use full decoding for BWAV
                memoryMode = 2;
            } else {
                //Streaming for >15MB files
                if(fsize > 15 * 1000000) {memoryMode = 1;}
                //Default realtime decoding mode
                else {memoryMode = 0;}
            }
        }
        //Don't read the file in mode 1 (Streaming)
        if(memoryMode != 1) {
            memblock = new unsigned char [fsize];
            file.seekg(0);
            file.read ((char*)memblock, fsize);
            if(verb) {std::cout << "Read file " << args[1] << " size " << fsize << '\n';}
            file.close();
        }
    } else {
        //File open error
        perror(args[1]);
        exit(255);
    }
    
    if(verb) switch(memoryMode) {
        case 0: std::cout << "Realtime decoding mode\n"; break;
        case 1: std::cout << "Disk stream mode\n"; break;
        case 2: std::cout << "Full decode mode\n"; break;
    }
    
    //Read the BRSTM headers
    if(memoryMode != 1) {
        //Use normal brstm read functions
        unsigned char result=brstm_read(brstm,memblock,verb,
            //Decode the audio data if memory mode is 2 (Full decoding)
            memoryMode == 2 ? true : false
        );
        //The file data will not be needed anymore in full decoding mode
        if(memoryMode == 2) {delete[] memblock;}
        if(result>127) {
            std::cout << "File read error. (" << (int)result << ")\n";
            return result;
        }
    } else {
        //Disk streaming mode
        unsigned char result = brstm_fstream_read(brstm,file,verb);
        if(result>127) {
            std::cout << "File read error. (" << (int)result << ")\n";
            return result;
        }
    }
    
    if(!brstm->loop_flag) {
        std::cout << "Warning: This file has no loop but it will be looped E to S\n";
    }
    
    if(track_mixing_enabled && brstm->num_tracks >= 9) {
        std::cout << "Too many channels for mixing.\n";
        track_mixing_enabled = 0;
    }
    if(track_mixing_enabled && brstm->num_tracks == 1) {
        track_mixing_enabled = 0;
    }
    
    //Allocate mixing buffer
    if(track_mixing_enabled) {
        brstmbuffer[0] = new int16_t[OUTPUT_BUFSIZE];
        brstmbuffer[1] = new int16_t[OUTPUT_BUFSIZE];
    }
    
    //Calculate total seconds
    total_seconds=brstm->total_samples/brstm->sample_rate;
    strcpy(total_seconds_string,secondsToMString(total_seconds));
    
    //Print basic file information in non-verbose mode
    if(verb == 0 && quietOutput == 0) {
        const char* loopstr = (brstm->loop_flag ? (brstm->loop_start > 0 ? "Looping" : "E to S") : "No loop");
        printf("%s | %s | %luHz | %uch/%utr | %s\n", brstm_getShortFormatString(brstm), brstm_getCodecString(brstm), brstm->sample_rate, brstm->num_channels, brstm->num_tracks, loopstr);
    }
    
    //Calculate UI refresh rate
    ui_counter_l = (brstm->sample_rate / 4) / OUTPUT_BUFSIZE;
    
    //Initialize RtAudio
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
    options.streamName = "OpenRevolution brstm_rt";
    unsigned int sampleRate = forcedSampleRate ? forcedSampleRate : brstm->sample_rate;
    unsigned int bufferFrames = OUTPUT_BUFSIZE;
    
    try {
        dac.openStream(&parameters, NULL, RTAUDIO_SINT16, sampleRate, &bufferFrames, &RtAudioCb, (void*)memblock, &options);
        dac.startStream();
    } catch(RtAudioError& e) {
        e.printMessage();
        exit(255);
    }
    
    //User input
    char input;
    while(stop_playing==0) {
        input = getch();
        
        if(input == '\033') {
            getch();
            input=getch();
            switch(input) {
                case 'A': /*UP - Switch track*/ {
                    if(!track_mixing_enabled) {
                        current_track++;
                        if(current_track >= brstm->num_tracks) current_track=brstm->num_tracks-1;
                    }
                    break;
                }
                case 'B': /*DOWN - Switch track*/ {
                    if(!track_mixing_enabled && current_track > 0) {
                        current_track--;
                    }
                    break;
                }
                case 'C': /*RIGHT - Fast Forward*/ {
                    playback_current_sample += brstm->sample_rate;
                    if(playback_current_sample > brstm->total_samples) playback_current_sample=brstm->total_samples;
                    break;
                }
                case 'D': /*LEFT - Rewind*/ {
                    if(playback_current_sample >= brstm->sample_rate) playback_current_sample -= brstm->sample_rate;
                    else playback_current_sample = 0;
                    break;
                }
            }
        } else switch(input) {
            //reserved for more features in the future?
            /*case 'w': case 'W': break;
            case 's': case 'S': break;
            case 'a': case 'A': break;
            case 'd': case 'D': break;*/
            //Pause
            case ' ': paused=!paused; break;
            //Quit program
            case 'q': case 'Q': stop_playing = 1; break;
            default: {
                //Track toggles
                if(input >= '0' && input <= '9' && track_mixing_enabled) {
                    uint8_t input_num = input - '0';
                    if(input_num > 0 && input_num <= brstm->num_tracks) {
                        tracks_enabled[input_num-1] = !tracks_enabled[input_num-1];
                    }
                }
                break;
            }
        }
        
        //Force display refresh to be more responsive to user input.
        ui_counter = -1;
        
        //std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    std::cout << '\n';
    
    try {
        //Stop the stream
        dac.stopStream();
    } catch(RtAudioError& e) {
        e.printMessage();
    }
    if(dac.isStreamOpen()) dac.closeStream();
    
    //Free mixing buffer
    if(track_mixing_enabled) {
        delete[] brstmbuffer[0];
        delete[] brstmbuffer[1];
    }
    
    brstm_close(brstm);
    delete brstm;
    
    return 0;
}
