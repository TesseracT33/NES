#pragma once

#include "Types.h"

namespace BitUtils
{
	inline bool CheckBit(u8& byte, u8 pos) { return byte & (1u << pos); }
	inline bool CheckBit(u16& word, u8 pos) { return word & (1u << pos); }
	inline void SetBit(u8& byte, u8 pos) { byte |= (1u << pos); }
	inline void SetBit(u16& word, u8 pos) { word |= (1u << pos); }
	inline void ClearBit(u8& byte, u8 pos) { byte &= ~(1u << pos); }
	inline void ClearBit(u16& word, u8 pos) { word &= ~(1u << pos); }
	inline void ToggleBit(u8& byte, u8 pos) { byte ^= ~(1u << pos); }
	inline void ToggleBit(u16& word, u8 pos) { word ^= ~(1u << pos); }

	inline bool CheckBit(u8* byte, u8 pos) { return *byte & (1u << pos); }
	inline bool CheckBit(u16* word, u8 pos) { return *word & (1u << pos); }
	inline void SetBit(u8* byte, u8 pos) { *byte |= (1u << pos); }
	inline void SetBit(u16* word, u8 pos) { *word |= (1u << pos); }
	inline void ClearBit(u8* byte, u8 pos) { *byte &= ~(1u << pos); }
	inline void ClearBit(u16* word, u8 pos) { *word &= ~(1u << pos); }
	inline void ToggleBit(u8* byte, u8 pos) { *byte ^= ~(1u << pos); }
	inline void ToggleBit(u16* word, u8 pos) { *word ^= ~(1u << pos); }
}