# brstm_rt
C++ RtAudio BRSTM player

## Usage
./brstm_rt file.brstm

-v - Verbose output

**Memory modes**

-m - Load the entire file into memory (default for <15MB files)

-s - Stream the file from disk (default for >15MB files)

-d - Decode all audio data into memory before playing

**Controls**

Left and right arrow keys: Seek 1 second

Up and down arrow keys: Switch track (for multi track BRSTMs)

Space: Pause

Q: Quit
