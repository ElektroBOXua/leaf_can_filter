/**
 * I am not the author of this code.
 * This code originates from https://github.com/dalathegreat/Battery-Emulator
 * and distributed under GPL-3.0 license
 *
 * TODO MISRAfy this
 **/

#include <stdbool.h>
#include <stdint.h>

/******************************************************************************
 * ORIGINAL
 *****************************************************************************/
unsigned int CyclicXorHash16Bit(unsigned int param_1, unsigned int param_2) {
  bool bVar1;
  unsigned int uVar2, uVar3, uVar4, uVar5, uVar6, uVar7, uVar8, uVar9, uVar10, uVar11, iVar12;

  param_1 = param_1 & 0xffff;
  param_2 = param_2 & 0xffff;
  uVar10 = 0xffff;
  iVar12 = 2;
  do {
    uVar2 = param_2;
    if ((param_1 & 1) == 1) {
      uVar2 = param_1 >> 1;
    }
    uVar3 = param_2;
    if ((param_1 >> 1 & 1) == 1) {
      uVar3 = param_1 >> 2;
    }
    uVar4 = param_2;
    if ((param_1 >> 2 & 1) == 1) {
      uVar4 = param_1 >> 3;
    }
    uVar5 = param_2;
    if ((param_1 >> 3 & 1) == 1) {
      uVar5 = param_1 >> 4;
    }
    uVar6 = param_2;
    if ((param_1 >> 4 & 1) == 1) {
      uVar6 = param_1 >> 5;
    }
    uVar7 = param_2;
    if ((param_1 >> 5 & 1) == 1) {
      uVar7 = param_1 >> 6;
    }
    uVar11 = param_1 >> 7;
    uVar8 = param_2;
    if ((param_1 >> 6 & 1) == 1) {
      uVar8 = uVar11;
    }
    param_1 = param_1 >> 8;
    uVar9 = param_2;
    if ((uVar11 & 1) == 1) {
      uVar9 = param_1;
    }
    uVar10 =
        (((((((((((((((uVar10 & 0x7fff) << 1 ^ uVar2) & 0x7fff) << 1 ^ uVar3) & 0x7fff) << 1 ^ uVar4) & 0x7fff) << 1 ^
                uVar5) &
               0x7fff)
                  << 1 ^
              uVar6) &
             0x7fff)
                << 1 ^
            uVar7) &
           0x7fff)
              << 1 ^
          uVar8) &
         0x7fff)
            << 1 ^
        uVar9;
    bVar1 = iVar12 != 1;
    iVar12 = iVar12 + -1;
  } while (bVar1);
  return uVar10;
}

unsigned int ComputeMaskedXorProduct(unsigned int param_1, unsigned int param_2, unsigned int param_3) {
  return ((param_3 ^ 0x7F88) | (param_2 ^ 0x8FE7)) * ((((param_1 & 0xffff) >> 8) ^ (param_1 & 0xff))) & 0xffff;
}

short ShortMaskedSumAndProduct(short param_1, short param_2) {
  unsigned short uVar1;

  uVar1 = (param_2 + (param_1 * 0x0006)) & 0xff;
  return (uVar1 + param_1) * (uVar1 + param_2);
}

unsigned int MaskedBitwiseRotateMultiply(unsigned int param_1, unsigned int param_2) {
  unsigned int uVar1;

  param_1 = param_1 & 0xffff;
  param_2 = param_2 & 0xffff;
  uVar1 = param_2 & (param_1 | 0x0006) & 0xf;
  return ((unsigned int)param_1 >> uVar1 | param_1 << (0x10 - (uVar1 & 0x1f))) *
             (param_2 << uVar1 | (unsigned int)param_2 >> (0x10 - (uVar1 & 0x1f))) &
         0xffff;
}

unsigned int CryptAlgo(unsigned int param_1, unsigned int param_2, unsigned int param_3) {
  unsigned int uVar1, uVar2, iVar3, iVar4;

  uVar1 = MaskedBitwiseRotateMultiply(param_2, param_3);
  uVar2 = ShortMaskedSumAndProduct(param_2, param_3);
  uVar1 = ComputeMaskedXorProduct(param_1, uVar1, uVar2);
  uVar2 = ComputeMaskedXorProduct(param_1, uVar2, uVar1);
  iVar3 = CyclicXorHash16Bit(uVar1, 0x8421);
  iVar4 = CyclicXorHash16Bit(uVar2, 0x8421);
  return iVar4 + iVar3 * 0x10000;
}

void decodeChallengeData(unsigned int incomingChallenge, unsigned char* solvedChallenge) {
  unsigned int uVar1, uVar2;

  uVar1 = CryptAlgo(0x54e9, 0x3afd, incomingChallenge >> 0x10);
  uVar2 = CryptAlgo(incomingChallenge & 0xffff, incomingChallenge >> 0x10, 0x54e9);
  *solvedChallenge = (unsigned char)uVar1;
  solvedChallenge[1] = (unsigned char)uVar2;
  solvedChallenge[2] = (unsigned char)((unsigned int)uVar2 >> 8);
  solvedChallenge[3] = (unsigned char)((unsigned int)uVar1 >> 8);
  solvedChallenge[4] = (unsigned char)((unsigned int)uVar2 >> 0x10);
  solvedChallenge[5] = (unsigned char)((unsigned int)uVar1 >> 0x10);
  solvedChallenge[6] = (unsigned char)((unsigned int)uVar2 >> 0x18);
  solvedChallenge[7] = (unsigned char)((unsigned int)uVar1 >> 0x18);
  return;
}

/******************************************************************************
 * REFACTORED
 *****************************************************************************/
/** Generates 16 bit cyclic XOR hash */
uint16_t _lcf_keygen_hash_u16(uint16_t val, uint16_t seed)
{
	uint8_t i;

	uint16_t hash = 0u;

	for (i = 0u; i < 16u; i++) {
		/* If the i-th bit is set, XOR with the shifted value, 
		   otherwise XOR with the seed. */
		uint16_t mask = ((val >> i) & 1u) ? (val >> (i + 1u)) : seed;
        
		hash = (hash << 1u) ^ mask;
	}

	return hash;
}

/**
 * Mixes three 16-bit values using XOR constants and bitwise-or.
 * The constants 0x7F88 and 0x8FE7 act as a static "key" layer.
 */
uint16_t _lcf_keygen_mix_xor_product(uint16_t seed, uint16_t val_a,
				     uint16_t val_b)
{
	uint16_t key_mix = (val_b ^ 0x7F88u) | (val_a ^ 0x8FE7u);
	uint16_t seed_diff = (seed >> 8u) ^ (seed & 0xFFu);
    
	return (key_mix * seed_diff) & 0xFFFFu;
}

/**
 * Generates a non-linear 8-bit intermediate value used for further mixing.
 */
uint16_t _lcf_keygen_mix_sum_and_product(uint16_t a, uint16_t b)
{
	uint16_t intermediate = (b + (a * 6u)) & 0xFFu;
	return (intermediate + a) * (intermediate + b);
}

/**
 * Performs a "Barrel Shift" (Rotate) on two values.
 * The shift amount is derived dynamically from the input bits.
 */
uint16_t _lcf_keygen_rotate_and_multiply(uint16_t a, uint16_t b)
{
	/* Determine shift amount (0-15) based on low nibble of inputs */
	uint32_t shift = b & (a | 0x0006u) & 0xFu;

	/* Circular shift (Rotate) */
	uint16_t rot_a = (a >> shift) | (a << (16u - shift));
	uint16_t rot_b = (b << shift) | (b >> (16u - shift));
    
	return (rot_a * rot_b) & 0xFFFFu;
}

/**
 * The core transformation logic. 
 * Combines rotation, non-linear mixing, and the cyclic XOR hash.
 */
uint32_t _lcf_keygen_run_auth_transform(uint16_t seed, uint16_t part_a,
				 uint16_t part_b)
{
	uint16_t mixed_rot = _lcf_keygen_rotate_and_multiply(part_a, part_b);
	uint16_t mixed_sum = _lcf_keygen_mix_sum_and_product(part_a, part_b);
    
	uint16_t step1 = _lcf_keygen_mix_xor_product(seed, mixed_rot,
						     mixed_sum);
	uint16_t step2 = _lcf_keygen_mix_xor_product(seed, mixed_sum, step1);
    
	uint16_t hash_lo = _lcf_keygen_hash_u16(step1, 0x8421u);
	uint16_t hash_hi = _lcf_keygen_hash_u16(step2, 0x8421u);
    
	return ((uint32_t)hash_lo << 16u) | hash_hi;
}

/**
 * Main entry point: Takes a 32-bit challenge and fills an
 * 8-byte response buffer.
 */
void _lcf_keygen_solve_battery_challenge(uint32_t challenge,
					 uint8_t* response)
{
	uint16_t chal_hi = (uint16_t)(challenge >> 16u);
	uint16_t chal_lo = (uint16_t)(challenge & 0xFFFFu);

	/* Static magic number 0x54e9 and 0x3afd likely act as
	* device-specific keys */
	uint32_t res_a = _lcf_keygen_run_auth_transform(0x54E9u, 0x3AFDu,
							chal_hi);
	uint32_t res_b = _lcf_keygen_run_auth_transform(chal_lo, chal_hi,
							0x54E9u);

	/* Byte-packing the result into the 8-byte response array */
	response[0] = (uint8_t)(res_a);
	response[3] = (uint8_t)(res_a >> 8u);
	response[5] = (uint8_t)(res_a >> 16u);
	response[7] = (uint8_t)(res_a >> 24u);

	response[1] = (uint8_t)(res_b);
	response[2] = (uint8_t)(res_b >> 8u);
	response[4] = (uint8_t)(res_b >> 16u);
	response[6] = (uint8_t)(res_b >> 24u);
}
/* TODO keep refactoring */
