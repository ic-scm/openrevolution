echo "Building decoder..."
g++ -O2 main.cpp -o brstm
echo "Building player..."
g++ -O2 rt_player/main.cpp -o rt_player/brstm_rt -lrtaudio
echo "Building encoder..."
g++ -O2 -std=c++0x encoder/encoder.cpp -o brstm_encoder
echo "Building reencoder..."
g++ -O2 -std=c++0x encoder/reencoder.cpp -o brstm_reencoder
echo "Building rebuilder..."
g++ -O2 -std=c++0x encoder/rebuilder.cpp -o brstm_rebuilder
