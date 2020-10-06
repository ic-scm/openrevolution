/**************************************/
#pragma once
/**************************************/
#include <stdint.h>
#include <stddef.h>
/**************************************/

//! Configuration
typedef uint32_t ADPCM_Frame_t;
#define ADPCM_RES_BITS_LOG2 1 //! log2(ResidualBits)

//! Helper defines
#define ADPCM_RES_BITS   (1<<ADPCM_RES_BITS_LOG2)
#define ADPCM_RES_MIN    (-(1<<ADPCM_RES_BITS)/2)
#define ADPCM_RES_MAX    ( (1<<ADPCM_RES_BITS)/2-1)
#define ADPCM_FRAME_SIZE ((sizeof(ADPCM_Frame_t)*8 - 4 - 4) / ADPCM_RES_BITS) //! LPC selection (4bit), shift selection (4bit)

/**************************************/

//! Encoding state
struct ADPCM_State_t {
	int16_t yz1, yz2;
	int16_t Dec[ADPCM_FRAME_SIZE];
	int16_t Res[ADPCM_FRAME_SIZE];
};

/**************************************/

//! Reset ADPCM state
static inline void ADPCM_Reset(struct ADPCM_State_t *State) {
	State->yz1 = State->yz2 = 0;
}

//! Encode ADPCM frame
ADPCM_Frame_t ADPCM_Encode(struct ADPCM_State_t *State, const int16_t *Frame);

#include "adpcm.c"

/**************************************/
//! EOF
/**************************************/
