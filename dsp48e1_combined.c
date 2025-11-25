#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int64_t dsp48e1(int32_t a1, int32_t a2, int32_t b1, int32_t b2, int64_t c, int32_t d, int8_t opmode, int8_t alumode, int8_t inmode, int8_t carryinsel, bool carryin, bool carrycascin);


static inline uint32_t f2u(float x) {
    uint32_t r;
    memcpy(&r, &x, sizeof(r));
    return r;
}


static inline float u2f(uint32_t x) {
    float r;
    memcpy(&r, &x, sizeof(r));
    return r;
}

void print_binary(unsigned int x) {
    for (int i = 31; i >= 0; i--) {
        printf("%d", (x >> i) & 1);
        if (i % 4 == 0) printf(" ");  // optional: group into nibbles
    }
}

void print_float_ieee754(uint32_t bits) {
    //uint32_t bits;
    //memcpy(&bits, &x, sizeof(float));  // copy raw bytes

    // Print as hex
    printf("Hex: 0x%08X\n", bits);

    // Decode sign / exponent / mantissa
    uint32_t sign     = (bits >> 31) & 0x1;
    uint32_t exponent = ((bits >> 23) & 0xFF);
    uint32_t mantissa = bits & 0x7FFFFF;

    printf("Sign:      %u\n", sign);
    printf("Exponent:  0x%02X (%u)\n", exponent, exponent);
    printf("Mantissa:  0x%06X\n", mantissa);

    // Optional: print full 32-bit binary
    printf("Binary:    ");
    for (int i = 31; i >= 0; --i) {
        printf("%u", (bits >> i) & 1);
        if (i == 31 || i == 23) printf(" "); // S | E | F
    }
    printf("\n");
}

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
    printf("Input_a = %u\n",a);
    printf("Input_b = %u\n", b);
    print_float_ieee754(a);
    print_float_ieee754(b);
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
    printf("Ec = %d\n", Ec);
    printf("Ea = %d\n", Ea);
    printf("Eb = %d\n", Eb);
    
    uint32_t mantissa_a = a & mantissa_mask;
    uint32_t mantissa_b = b & mantissa_mask;
    printf("mantissa_a = %u\n", mantissa_a);
    printf("mantissa_b = %u\n", mantissa_b);
    //print mantissa_a and mantissa_b


    uint32_t signbit_c = (a^b) & signbit_mask;


    //send to long dsp48e1(int a, int b, long c, int d) to calculate Mc = (1 + Ma) * (1 + Mb)
    //align mantissa_a and mantissa_b to 30 bits by adding 0 to the left
    // add a hidden bit 1 to the left of the mantissa
    uint32_t mantissa_a_24 = (1u << 23) | mantissa_a;  // [23:0]
    uint32_t mantissa_b_24 = (1u << 23) | mantissa_b;  // [23:0]
    printf("mantissa_a_24 = %u\n", mantissa_a_24);
    printf("mantissa_b_24 = %u\n", mantissa_b_24);
    //fill 0 to the left of the mantissa to 30 bits
    //uint32_t mantissa_a_30 = (mantissa_a_24 << 6) | 0x000000;
    //uint32_t mantissa_b_30 = (mantissa_b_24 << 6) | 0x000000;
    uint32_t mantissa_a_30 = mantissa_a_24 ;
    uint32_t mantissa_b_30 = mantissa_b_24 ;
    printf("mantissa_a_30 = %u\n", mantissa_a_30);
    printf("mantissa_b_30 = %u\n", mantissa_b_30);

    //split mantissa_b_30 into 18 bits and 12 bits
    uint32_t mantissa_b_18 = mantissa_b_30 & 0x0003FFFFu;
    uint32_t mantissa_b_12 = mantissa_b_30 >> 18;
    printf("mantissa_b_18 = %u\n", mantissa_b_18);
    printf("mantissa_b_12 = %u\n", mantissa_b_12);




//--------------------------question part--------------------------------------------------------------------------------------
    //send 30 bit a and 12 bit b to dsp48e1(int a, int b, long c, int d)
    //int64_t Mc_1 = dsp48e1(mantissa_a_30, mantissa_b_12, 0, 0);
    //int64_t Mc_2 = dsp48e1(mantissa_a_30, mantissa_b_18, 0 ,0);
    //float input_mantissa_a = u2f(mantissa_a_30);
    //float input_mantissa_b = u2f(mantissa_b_30);

    //benchmark skip the function
    //int64_t Mc_1 = (int64_t)mantissa_a_30 * (int64_t)mantissa_b_12;
    //int64_t Mc_2 = (int64_t)mantissa_a_30 * (int64_t)mantissa_b_18;
    //int64_t dsp48e1(int32_t a1, int32_t a2, int32_t b1, int32_t b2, int64_t c, int32_t d, int8_t opmode, int8_t alumode, int8_t inmode, int8_t carryinsel, bool carryin, bool carrycascin)
    int64_t Mc_1 = dsp48e1(mantissa_a_30, mantissa_a_30, mantissa_b_12, mantissa_b_12, 0, 0, 0b0000101, 0b0000, 0b00000, 0b000, false, false);
    int64_t Mc_2 = dsp48e1(mantissa_a_30, mantissa_a_30, mantissa_b_18, mantissa_b_18, 0, 0, 0b0000101, 0b0000, 0b00000, 0b000, false, false);
    //int64_t Mc_1 = dsp48e1(12582912, 12582912, 40, 40, 0, 0, 0b0000101, 0b0000, 0b00000, 0b000, false, false);
    //int64_t Mc_2 = dsp48e1(1000000, 1000000, 1000, 1000, 0, 0, 0b0000101, 0b0000, 0b00000, 0b000, false, false);
    
    printf("Mc_1 =%u\n", Mc_1);
    printf("Mc_2 = %d\n", Mc_2);
    //printf("Mantissa_a = %u\n", f2u(mantissa_a_30));
    //printf("Mantissa_b = %u\n", f2u(mantissa_b_30));

//--------------------------------------------------------------------------------------



    int64_t Mc = (Mc_1 << 18) + Mc_2;
    printf("Mc = %lld\n", Mc);
    uint64_t Mc_u = (uint64_t)Mc; 
    printf("Mc_u = %llu\n", Mc_u);
    printf("builtin_clzll(Mc_u) = %d\n", __builtin_clzll(Mc_u));
    int shift = __builtin_clzll(Mc_u)-5;
    //int shift = leading - 23;
    printf("shift = %d\n", shift);
    Mc_u = Mc_u << 12;
    printf("Mc_u = %llu\n", Mc_u);


    //compare Mc and 2.0f, if Mc < 2.0f, then Mc = Mc - 1, and shift exponent bits by -127
    const uint64_t ONE = 1ull << 58;
    printf("ONE = %llu\n", ONE);
    const uint64_t TWO = 1ull << 59;
    printf("TWO = %llu\n", TWO);
    if (Mc_u >= TWO){
        Mc_u = Mc_u >> 1;
        Ec += 1;
    }
    printf("Ec_new = %d\n", Ec);

    uint64_t frac = Mc_u - ONE;
    uint32_t exponent_c = (uint32_t)(Ec + 127);
    uint32_t mantissa_c = (uint32_t)(frac>>(58-23));

    //concatenate signbit_c, exponent_c, and mantissa_c
    uint32_t result_c = (signbit_c) | (exponent_c << 23) | mantissa_c & mantissa_mask;
    printf("result_c = %u\n", result_c);


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