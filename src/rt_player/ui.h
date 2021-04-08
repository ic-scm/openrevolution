//brstm_rt user interface
//Copyright (C) 2020 I.C.

//UI state
struct uoutput_state_t {
    //Quiet output (no player UI)
    bool quietOutput = 0;
    //Classic player UI
    bool classic_noflush = 0;
    
    //UI refresh timer
    uint16_t ui_counter = 0;
    uint16_t ui_counter_l = 0;
    
    //Second counters and strings
    unsigned long playback_seconds = 0;
    char playback_seconds_string[10];
    unsigned long total_seconds = 0;
    char total_seconds_string[10];
    
    //Pointer to playback state
    player_state_t* playback_state;
};

//Data for input thread
struct uinput_state_t {
    bool exit = 0;
    //Pointer to playback state
    player_state_t* playback_state;
    //Pointer to output UI state
    uoutput_state_t* output_state;
};

//User input thread function
void* uinput_thread(void* arg) {
    uinput_state_t* state = (uinput_state_t*)arg;
    
    char input;
    player_state_t* pstate = state->playback_state;
    
    while(state->exit == 0) {
        input = getch();
        
        //Unlock player state before changing it's data
        while(pstate->lock) usleep(100);
        pstate->lock = 1;
        
        if(input == '\033') {
            getch();
            input=getch();
            switch(input) {
                case 'A': /*UP - Switch track*/ {
                    if(!pstate->track_mixing_enabled) {
                        pstate->current_track++;
                        if(pstate->current_track >= pstate->brstm->num_tracks) pstate->current_track = pstate->brstm->num_tracks-1;
                    }
                    break;
                }
                case 'B': /*DOWN - Switch track*/ {
                    if(!pstate->track_mixing_enabled && pstate->current_track > 0) {
                        pstate->current_track--;
                    }
                    break;
                }
                case 'C': /*RIGHT - Fast Forward*/ {
                    pstate->playback_current_sample += pstate->brstm->sample_rate;
                    if(pstate->playback_current_sample > pstate->brstm->total_samples) pstate->playback_current_sample = pstate->brstm->total_samples - 1;
                    break;
                }
                case 'D': /*LEFT - Rewind*/ {
                    if(pstate->playback_current_sample >= pstate->brstm->sample_rate) pstate->playback_current_sample -= pstate->brstm->sample_rate;
                    else pstate->playback_current_sample = 0;
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
            case ' ': pstate->paused = !pstate->paused; break;
            //Quit program
            case 'q': case 'Q': {
                pstate->stop_playing = 1;
                state->exit = 1;
                break;                
            }
            default: {
                //Track toggles
                if(input >= '0' && input <= '9' && pstate->track_mixing_enabled) {
                    uint8_t input_num = input - '0';
                    if(input_num > 0 && input_num <= pstate->brstm->num_tracks) {
                        pstate->tracks_enabled[input_num-1] = !pstate->tracks_enabled[input_num-1];
                    }
                }
                break;
            }
        }
        
        //Force display refresh to be more responsive to user input.
        state->output_state->ui_counter = -1;
        
        pstate->lock = 0;
    }
    
    return 0;
}

//Player UI
void drawPlayerUI(uoutput_state_t* state) {
    player_state_t* pstate = state->playback_state;
    state->playback_seconds = pstate->playback_current_sample / pstate->brstm->sample_rate;
    
    if(!state->quietOutput) {
        
        if(state->classic_noflush || (state->ui_counter_l <= state->ui_counter) ) {
            state->ui_counter = 0;
            
            std::cout << '\r'
            // Pause
            << (pstate->paused ? "Paused " : "")
            // Time
            << "(" << secondsToMString(state->playback_seconds_string, 10, state->playback_seconds) << "/" << state->total_seconds_string;
            // Tracks
            if(!pstate->track_mixing_enabled) {
                std::cout << " Track: " << pstate->current_track+1;
            } else {
                std::cout << " Tracks:";
                for(unsigned int t=0; t<pstate->brstm->num_tracks; t++) {
                    std::cout << ' ' << t+1 << '[' << (pstate->tracks_enabled[t] ? 'X' : ' ') << ']';
                }
            }
            // Controls guide
            std::cout << ") (< >:Seek ";
            if(pstate->track_mixing_enabled) {
                std::cout << "1-" << pstate->brstm->num_tracks << ":Toggle tracks";
            } else {
                std::cout << "/\\ \\/:Switch track";
            }
            std::cout << "):\033[0m        "
            // End
            << (!pstate->paused ? "       " : "") << "\r";
            
            //Flush
            if(!state->classic_noflush) fflush(stdout);
        }
        
        state->ui_counter++;
    }
}
