## Converter/decoder program
Usage:
./brstm_decoder input.brstm -o output.wav

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

## RtAudio player
[See README in rt_player](https://github.com/Extrasklep/revolution/tree/master/src/rt_player)

## Encoder program
Usage:
./brstm_encoder input.wav -o output.brstm

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

-l [loop point] - Set the loop point. If this is not used the BRSTM file will have no loop.

-c [1 or 2] - Number of channels for each track

## Reencoder
Re-encode a BRSTM (or other supported input format) into a new BRSTM.

Usage:
./brstm_reencoder input.bwav -o output.brstm --ffmpeg "-af volume=10dB"

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output

--ffmpeg [ffmpeg arguments] - Use ffmpeg in the middle of re-encoding to edit the audio data with the passed ffmpeg arguments (as a single argument!)
Requires FFMPEG to be installed and it may not work on non-unix systems.

## Rebuilder
Losslessly rebuild a BRSTM (or other supported input format) into a new BRSTM.

Usage:
./brstm_rebuilder input.bwav -o output.brstm

-o [output file name] - If this is not used the output will not be saved.

-v - Verbose output
