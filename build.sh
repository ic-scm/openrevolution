echo "Building decoder..."
g++ -O2 src/decoder.cpp -o brstm_decoder
echo "Building player..."
g++ -O2 src/rt_player/main.cpp -o brstm_rt -lrtaudio
echo "Building converter..."
g++ -O2 src/converter.cpp -o brstm_converter
echo "Building encoder..."
g++ -O2 -std=c++0x src/encoder.cpp -o brstm_encoder
echo "Building reencoder..."
g++ -O2 -std=c++0x src/reencoder.cpp -o brstm_reencoder
echo "Building rebuilder..."
g++ -O2 -std=c++0x src/rebuilder.cpp -o brstm_rebuilder
