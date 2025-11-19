#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int64_t dsp48e1(int a, int b, long c, int d){
    // Module bit widths
    const int A_WIDTH = 30;
    const int A_WIDTH_PREADDER = 25;
    const int B_WIDTH = 18;
    const int C_WIDTH = 48;
    const int D_WIDTH = 25;

    // Correct bit widths
    int a_mask  = 0x3FFFFFFF; // Lower 30 bits`
    int a_mask_preadder  = 0x01FFFFFF; // Lower 25 bits
    int b_mask  = 0x003FFFFF; // Lower 18 bits
    long long c_mask = 0x0000FFFFFFFFFFFF; // Lower 48 bits
    int d_mask  = 0x01FFFFFF; // Lower 25 bits

    unsigned int a_val  = a & a_mask;
    unsigned int a_val_preadder  = a & a_mask_preadder;
    unsigned int b_val  = b & b_mask;
    unsigned long c_val = c & c_mask;
    unsigned int d_val  = d & d_mask;

    // Sign extend to fit the C types
    if (a_val & (1 << (A_WIDTH - 1))) {
        a_val |= ~a_mask;
    }
    if (a_val_preadder & (1 << (A_WIDTH_PREADDER - 1))) {
        a_val_preadder |= ~a_mask_preadder;
    }
    if (b_val & (1 << (B_WIDTH - 1))) {
        b_val |= ~b_mask;
    }
    if (c_val & (1L << (C_WIDTH - 1))) {
        c_val |= ~c_mask;
    }
    if (d_val & (1 << (D_WIDTH - 1))) {
        d_val |= ~d_mask;
    }

    // Pre adder
    int preadder_result = a_val_preadder + d_val;

    // Multiplier
    long multiplier_result = (long)preadder_result * (long)b_val;

    // Accumulator
    long accumulator_result = multiplier_result + c_val;

    int64_t p = 0;

    p = accumulator_result & c_mask;
    
    return p;
}
/*
int main() {
    int a = 0x000ABCDE; // 30 bits
    int b =   0x00012345;    // 18 bits
    long c = 0x00001111; // 48 bits
    int d = 0x00000010; // 25 bits

    long result = dsp48e1(a, b, c, d);
    printf("Result: 0x%lX\n", result);
    return 0;
}*/