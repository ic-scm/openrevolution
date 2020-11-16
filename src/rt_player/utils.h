//Some functions used in brstm_rt
//Copyright (C) 2020 I.C. & The StackOverflow Answers

void itoa(int n, char* s) {
    std::string ss = std::to_string(n);
    strcpy(s,ss.c_str());
}

//Converts seconds to a MM:SS string
char* secondsToMString(char* mString, unsigned int mStringSize, unsigned int sec) {
    for(unsigned char i=0;i<mStringSize;i++) {
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
    
    //Safety
    if(min > 9999) min = 9999;
    
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
    if(tcgetattr(0, &old) < 0) {
        //perror("tcsetattr()");
    }
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if(tcsetattr(0, TCSANOW, &old) < 0) {
        //perror("tcsetattr ICANON");
    }
    if(read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if(tcsetattr(0, TCSADRAIN, &old) < 0) {
        //perror("tcsetattr ~ICANON");
    }
    return buf;
}

//Getch function without reading just to reset attributes.
void getch_clean() {
    char buf = 0;
    struct termios old = {0};
    fflush(stdout);
    if(tcgetattr(0, &old) < 0) {
        //perror("tcsetattr()");
    }
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if(tcsetattr(0, TCSANOW, &old) < 0) {
        //perror("tcsetattr ICANON");
    }
    if(read(0, &buf, 0) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if(tcsetattr(0, TCSADRAIN, &old) < 0) {
        //perror("tcsetattr ~ICANON");
    }
}
