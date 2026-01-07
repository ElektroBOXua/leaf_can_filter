#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "../../lcf_keygen.h"

/* Data extracted from real CAN bus */
uint32_t incoming_challenge_example   = 0x0926BABA;
uint8_t  solved_challenge_example[8u] = { 0x77u, 0x1Fu, 0x7Cu, 0x9Bu,
					  0x1Fu, 0x82u, 0x7Cu, 0x3Eu };

uint8_t  solved_challenge[8u] = { 0 };

int main()
{
	decodeChallengeData(incoming_challenge_example, solved_challenge);
	_lcf_keygen_solve_battery_challenge(incoming_challenge_example,
					    solved_challenge);

	/* solved_challenge must match exactly solved_challenge_example */
	assert(!memcmp(solved_challenge_example, solved_challenge, 8u));

	
	assert(_lcf_keygen_hash_u16(0x1234, 0x8421) ==
		CyclicXorHash16Bit(0x1234, 0x8421));

	return 0;
}
