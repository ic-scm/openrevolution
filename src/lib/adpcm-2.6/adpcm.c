/**************************************/
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/**************************************/

//! Signed rounding 2^n division
static inline int Div2nRound(int x, size_t n) {
	return (x + (((1 << n) - (x < 0)) >> 1)) >> n;
}

//! Clip data to range
static inline int Clip(int x, int Min, int Max) {
	return (x <= Min) ? Min : (x >= Max) ? Max : x;
}
static inline int Clip16(int x) {
	return Clip(x, -0x8000, +0x7FFF);
}

/**************************************/

//! LPC coefficient table (.6fxp)
//! Derived from Cos[(i+0.5)*Pi/30] and slightly altered
static const short PredCoef[16] = {
	0x7F, 0x7E, 0x7C, 0x77,
	0x72, 0x6B, 0x63, 0x5B,
	0x51, 0x46, 0x3A, 0x2E,
	0x20, 0x13, 0x08,-0x08,
};

/**************************************/

//! Encode ADPCM frame
ADPCM_Frame_t ADPCM_Encode(struct ADPCM_State_t *State, const short *Frame) {
	//! Bruteforce the best selection
	uint8_t  n;
	uint8_t  Pred;
	uint8_t  Shift;
	uint64_t Error, ErrorBest = ~0ULL;
	       ADPCM_Frame_t FrameTemp, FrameBest = 0; //! <- This shuts gcc up
	struct ADPCM_State_t StateTemp, StateBest;
	for(Pred=0;Pred<16;Pred++) {
		for(Shift=0;Shift<16;Shift++) {
			//! Encode the frame
			Error     = 0;
			FrameTemp = 0;
			int16_t *yz1 = &StateTemp.yz1; *yz1 = State->yz1;
			int16_t *yz2 = &StateTemp.yz2; *yz2 = State->yz2;
			int16_t *Dec =  StateTemp.Dec;
			int16_t *Res =  StateTemp.Res;
			for(n=0;n<ADPCM_FRAME_SIZE;n++) {
				//! Compute prediction, quantize residual, compute output/error
				int Xn = Frame[n];
				int Pn = (*yz1 * PredCoef[Pred] >> 6) - *yz2;
				int Dn = Xn - Pn;
				int Qn = (ADPCM_RES_BITS != 1) ? Clip(Div2nRound(Dn, Shift), ADPCM_RES_MIN, ADPCM_RES_MAX) : (Dn < 0) ? (-1) : (Dn > 0) ? (+1) : ((n%2)*2-1);
				int Yn = Clip16(Pn + (Qn << Shift));
				int En = Xn - Yn;

				//! Save outputs, shuffle taps
				Dec[n] =       (Yn);
				Res[n] = Clip16(En);
				*yz2 = *yz1;
				*yz1 = Yn;

				//! Append to frame data
				if(ADPCM_RES_BITS > 1) FrameTemp = (FrameTemp << ADPCM_RES_BITS) | (Qn & ((1<<ADPCM_RES_BITS)-1));
				else                   FrameTemp = (FrameTemp <<              1) | (Qn < 0);

				//! Sum error
				Error += (int64_t)En*En;
				if(Error > ErrorBest) break;
			}

			//! Better model?
			if(Error < ErrorBest) {
				//! 64-bit bit-twiddling
				//! This places the first samples in the lowermost 32 bits
				// if(sizeof(ADPCM_Frame_t) == 8) FrameBest = (FrameBest>>32) | (FrameBest << (32 - 4 - 4));

				//! Append LPC selection, shift selection
				FrameTemp = FrameTemp<<4 | Shift;
				FrameTemp = FrameTemp<<4 | Pred;

				//! Save new state
				ErrorBest = Error;
				FrameBest = FrameTemp;
				StateBest = StateTemp;
				if(!Error) break;
			}
		}
		if(!ErrorBest) break;
	}

	//! Save state
	State->yz1 = StateBest.yz1;
	State->yz2 = StateBest.yz2;
	memcpy(State->Dec, StateBest.Dec, sizeof(StateBest.Dec));
	memcpy(State->Res, StateBest.Res, sizeof(StateBest.Res));
	return FrameBest;
}

/**************************************/
//! EOF
/**************************************/
