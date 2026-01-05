/**
 * I am not the author of this code.
 * This code originates from https://github.com/dalathegreat/Battery-Emulator
 * and distributed under GPL-3.0 license
 *
 * TODO MISRAfy this
 **/

#include <stdbool.h>

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
