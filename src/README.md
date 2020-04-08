## Converter
Decoder, encoder, rebuilder and reencoder merged into one program

Usage:
./brstm_converter input.type -o output.type

The operation mode is automatically picked depending on your file types

Run the program without any arguments to see all options.

Examples:

brstm_converter input.brstm -o output.wav

brstm_converter input.wav -o output.brstm -l 40000 -c 2 # set loop point to sample 40000, 2 channel tracks

brstm_converter input.brstm -o output.brstm -l 12345 # change loop point in an existing BRSTM

brstm_converter input.bwav -o output.brstm --ffmpeg "-af volume=10dB" # convert bwav to brstm and amplify the audio by 10dB

## RtAudio player
[See README in rt_player](https://github.com/Extrasklep/revolution/tree/master/src/rt_player)


## Old programs
Old separated tools, outdated and not recommended

If you still want to build them for some reason you can uncomment the lines for them in the build script but they will eventually be removed

### Decoder program
Usage:
./brstm_decoder input.brstm -o output.wav

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output


### Encoder program
Usage:
./brstm_encoder input.wav -o output.brstm

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

-l [loop point] - Set the loop point. If this is not used the BRSTM file will have no loop.

-c [1 or 2] - Number of channels for each track

### Reencoder
Re-encode a BRSTM (or other supported input format) into a new BRSTM.

Usage:
./brstm_reencoder input.bwav -o output.brstm --ffmpeg "-af volume=10dB"

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

--ffmpeg [ffmpeg arguments] - Use ffmpeg in the middle of re-encoding to edit the audio data with the passed ffmpeg arguments (as a single argument!)
Requires FFMPEG to be installed and it may not work on non-unix systems.

### Rebuilder
Losslessly rebuild a BRSTM (or other supported input format) into a new BRSTM.

Usage:
./brstm_rebuilder input.bwav -o output.brstm

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output
