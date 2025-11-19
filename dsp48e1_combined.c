#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int64_t dsp48e1(int a, int b, int64_t c, int d);
/*
dsp48e1_2_combined.c:
1. 32bit a, 32bit b, return 32 bit c
2. IEEE-754 single precision format: 1 sign bit, 8 exponent bits, 23 mantissa bits
3. split sign bit from A and B, then send to xor gate, as sign bit of c
4. compute Mc = (1 + Ma) * (1 + Mb)
5. case1: 1 <= Mc < 2, then Mc = Mc - 1, and shift exponent bits by -127
6. case2: 2 <= Mc < 4, then Mc = Mc/2 -1, and shift exponent bits by -126
7. return Mc, exponent bits, and sign bit
8. concatenate Mc, exponent bits, and sign bit, return 32 bit c

when calculating the Mc = (1 + Ma) * (1 + Mb)
use the function long dsp48e1(int a, int b, long c, int d), a and b need to refilled by 0 as alignment, long c
is the preadder, d is the acculator. (A+C)*B + D
*/

uint32_t dsp48e1_combined(uint32_t a, uint32_t b){
    // masks
    const uint32_t signbit_mask = 0x80000000u;
    const uint32_t exponent_mask = 0x7F800000u;
    const uint32_t mantissa_mask = 0x007FFFFFu;
    

    //extracted bits
    uint32_t signbit_a = a & signbit_mask;
    uint32_t signbit_b = b & signbit_mask;
    
    uint32_t exponent_a = (a & exponent_mask) >> 23;
    uint32_t exponent_b = (b & exponent_mask) >> 23;
    int32_t Ea = (int32_t)exponent_a - 127;
    int32_t Eb = (int32_t)exponent_b - 127;
    int32_t Ec = Ea + Eb;
    
    uint32_t mantissa_a = a & mantissa_mask;
    uint32_t mantissa_b = b & mantissa_mask;

    uint32_t signbit_c = (a^b) & signbit_mask;

    //send to long dsp48e1(int a, int b, long c, int d) to calculate Mc = (1 + Ma) * (1 + Mb)
    //align mantissa_a and mantissa_b to 30 bits by adding 0 to the left
    // add a hidden bit 1 to the left of the mantissa
    uint32_t mantissa_a_24 = (1u << 23) | mantissa_a;  // [23:0]
    uint32_t mantissa_b_24 = (1u << 23) | mantissa_b;  // [23:0]
    //fill 0 to the left of the mantissa to 30 bits
    uint32_t mantissa_a_30 = (mantissa_a_24 << 6) | 0x00000000;
    uint32_t mantissa_b_30 = (mantissa_b_24 << 6) | 0x00000000;

    //split mantissa_b_30 into 18 bits and 12 bits
    uint32_t mantissa_b_18 = mantissa_b_30 & 0x0003FFFFu;
    uint32_t mantissa_b_12 = mantissa_b_30 >> 18;




//--------------------------question part--------------------------------------------------------------------------------------
    //send 30 bit a and 12 bit b to dsp48e1(int a, int b, long c, int d)
    //int64_t Mc_1 = dsp48e1(mantissa_a_30, mantissa_b_12, 0, 0);
    //int64_t Mc_2 = dsp48e1(mantissa_a_30, mantissa_b_18, 0 ,0);

    //benchmark skip the function
    int64_t Mc_1 = (int64_t)mantissa_a_30 * (int64_t)mantissa_b_12;
    int64_t Mc_2 = (int64_t)mantissa_a_30 * (int64_t)mantissa_b_18;
//--------------------------------------------------------------------------------------



    int64_t Mc = (Mc_1 << 18) + Mc_2;
    uint64_t Mc_u = (uint64_t)Mc; 


    //compare Mc and 2.0f, if Mc < 2.0f, then Mc = Mc - 1, and shift exponent bits by -127
    const uint64_t ONE = 1ull << 58;
    const uint64_t TWO = 1ull << 59;
    if (Mc_u >= TWO){
        Mc_u = Mc_u >> 1;
        Ec += 1;
    }

    uint64_t frac = Mc_u - ONE;
    uint32_t exponent_c = (uint32_t)(Ec + 127);
    uint32_t mantissa_c = (uint32_t)(frac>>(58-23));

    //concatenate signbit_c, exponent_c, and mantissa_c
    uint32_t result_c = (signbit_c) | (exponent_c << 23) | mantissa_c & mantissa_mask;


    return result_c;
}
/*
int main(){
    uint32_t a = 0x3F800000;
    uint32_t b = 0x3F800000;
    uint32_t c = dsp48e1_combined(a, b);
    printf("c = %f\n", c);
    return 0;
}
    */