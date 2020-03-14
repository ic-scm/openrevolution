echo "Building decoder..."
g++ -O2 main.cpp -o brstm
echo "Building player..."
g++ -O2 rt_player/main.cpp -o rt_player/brstm_rt -lrtaudio
echo "Building encoder..."
g++ -O2 encoder/encoder.cpp -o brstm_encoder
echo "Building reencoder..."
g++ -O2 encoder/reencoder.cpp -o brstm_reencoder
