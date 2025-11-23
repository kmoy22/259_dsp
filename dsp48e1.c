#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define USE_DPORT = 1;

typedef struct dsp48e1_output_t
{
    int64_t p; // 48-bits
    int64_t pcout; // 48-bits

    int32_t acout; // 30-bits
    int32_t bcout; // 18-bits

    int8_t carryout; // 4-bit
    bool carrycascout; // 1-bit
    bool multsignout; // 1-bit

    bool patterndetect; // 1-bit
    bool patternbdetect; // 1-bit

    bool overflow; // 1-bit
    bool underflow; // 1-bit
} dsp48e1_output_t;

typedef struct dsp48e1_input_t
{
    int32_t a; // 30-bits
    int32_t b; // 18-bits
    int64_t c; // 48-bits
    int32_t d; // 25-bits

    int8_t opmode; // 7-bits
    int8_t alumode; // 4-bits
    bool carryin; // 1-bit
    int8_t carryinsel; // 3-bits
    int8_t inmode; // 5-bits

    bool cea_1; // 1-bit
    bool cea_2; // 1-bit
    bool ceb_1; // 1-bit
    bool ceb_2; // 1-bit
    bool cec; // 1-bit
    bool ced; // 1-bit
    bool cem; // 1-bit
    bool cep; // 1-bit
    bool cead; // 1-bit

    bool cealumode; // 1-bit
    bool cectrl; // 1-bit
    bool cecarryin; // 1-bit
    bool ceinmode; // 1-bit

    bool rsta; // 1-bit
    bool rstb; // 1-bit
    bool rstc; // 1-bit
    bool rstd; // 1-bit
    bool rstm; // 1-bit
    bool rstp; // 1-bit
    bool rstctrl; // 1-bit
    bool rstallcarryin; // 1-bit
    bool rstaluinmode; // 1-bit
    bool rstinmode; // 1-bit

    bool clk; // 1-bit

    int32_t acin; // 30-bits
    int32_t bcin; // 18-bits
    int64_t pcin; // 48-bits
    bool carrycascin; // 1-bit
    bool multsignin; // 1-bit
} dsp48e1_input_t;

int64_t dsp48e1(int32_t a1, int32_t a2, int32_t b1, int32_t b2, int64_t c, int32_t d, int8_t opmode, int8_t alumode, int8_t inmode, int8_t carryinsel, bool carryin, bool carrycascin) {
    // Core functionality of the DSP48E1 module
    // Inputs:
    // A[29:0] - Input to the multiplier or pre-adder
    // A1 vs A2 are the pipeline stages for the A input, with A2 being the registered version of A1
    // B[17:0] - Input to the multiplier
    // B1 vs B2 are the pipeline stages for the B input, with B2 being the registered version of B1
    // C[47:0] - Input to the second stage adder/subtractor, pattern detector, or logic function
    // D[24:0] - Input to the pre-adder or alternative input to the multiplier
    // INMODE[4:0] - Selects the funtionality of the pre-adder, A, B, D inputs, and input registers
    // OPMODE[6:0] - Controls the input to the X, Y, and Z MUXes
    // ALUMODE[3:0] - Controls the selection of the logic function 
    // Outputs:
    // P[47:0] - Primary output of the DSP48E1 slice

    // Input bit widths
    const int A_WIDTH = 30;
    const int A_WIDTH_PREADDER = 25;
    const int B_WIDTH = 18;
    const int C_WIDTH = 48;
    const int D_WIDTH = 25;

    // Mask inputs to correct bit widths
    int32_t a_mask          = 0x3FFFFFFF; // Lower 30 bits
    int32_t a_mask_preadder = 0x01FFFFFF; // Lower 25 bits
    int32_t b_mask          = 0x003FFFFF; // Lower 18 bits
    int64_t c_mask          = 0x0000FFFFFFFFFFFF; // Lower 48 bits
    int32_t d_mask          = 0x01FFFFFF; // Lower 25 bits

    int32_t a1_val          = a1 & a_mask;
    int32_t a1_val_preadder = a1 & a_mask_preadder;
    int32_t a2_val          = a2 & a_mask;
    int32_t a2_val_preadder = a2 & a_mask_preadder;
    int32_t b1_val          = b1 & b_mask;
    int32_t b2_val          = b2 & b_mask;
    int64_t c_val           = c & c_mask;
    int32_t d_val           = d & d_mask;

    // Sign extend to fit the C types
    // If the sign bit is set, extend the sign via the inverted mask
    if (a1_val & (1 << (A_WIDTH - 1))) {
        a1_val |= ~a_mask;
    }
    if (a1_val_preadder & (1 << (A_WIDTH_PREADDER - 1))) {
        a1_val_preadder |= ~a_mask_preadder;
    }
    if (a2_val & (1 << (A_WIDTH - 1))) {
        a2_val |= ~a_mask;
    }
    if (a2_val_preadder & (1 << (A_WIDTH_PREADDER - 1))) {
        a2_val_preadder |= ~a_mask_preadder;
    }
    if (b1_val & (1 << (B_WIDTH - 1))) {
        b1_val |= ~b_mask;
    }
    if (b2_val & (1 << (B_WIDTH - 1))) {
        b2_val |= ~b_mask;
    }
    if (c_val & (0x1ULL << (C_WIDTH - 1))) {
        c_val |= ~c_mask;
    }
    if (d_val & (1 << (D_WIDTH - 1))) {
        d_val |= ~d_mask;
    }

    int32_t a_val_preadder = a_select(a1_val_preadder, a2_val_preadder, inmode);
    int32_t a_val = a_select(a1_val, a2_val, inmode);
    int32_t b_val = b_select(b1_val, b2_val, inmode);

    // Pre adder  
    int32_t preadder_result = pre_adder(a1_val_preadder, a2_val_preadder, d_val, inmode);

    // 25 x 18 Multiplier
    int64_t multiplier_result = multiplier(preadder_result, b_val);

    int64_t mux_x_output = x_mux(multiplier_result, 0, a_val, b_val, opmode);
    int64_t mux_y_output = y_mux(multiplier_result, c_val, opmode);
    int64_t mux_z_output = z_mux(0, 0, c_val, opmode);

    int64_t cin = carry_select(carryinsel, carryin, carrycascin, 0, a_val_preadder, b_val, 0, 0);

    // Second stage: Adder/Subtractor/Logic
    int64_t p = stage2(mux_x_output, mux_y_output, mux_z_output, cin, alumode, opmode);
    
    return p;
}

int32_t a_select(int32_t a1, int32_t a2, int8_t inmode) {
    // A input selection based on INMODE
    // INMODE[0] - Selects between A1 and A2 inputs
    // INMODE[1] - Zero A input
    // INMODE[2] - Enable D input
    // INMODE[3] - Add/Subtract control for pre-adder
    // INMODE[4] - Selects between B1 and B2 inputs

    // Get individual bits from inmode
    int8_t mask0 = 0x1;
    int8_t mask1 = 0x2;

    bool inmode0 = (inmode & mask0) != 0;
    bool inmode1 = (inmode & mask1) != 0; 

    int32_t a_output = inmode1 ? 0 : inmode0 ? a1 : a2;
    return a_output;
}

int32_t b_select(int32_t b1, int32_t b2, int8_t inmode) {
    // B input selection based on INMODE
    // INMODE[0] - Selects between A1 and A2 inputs
    // INMODE[1] - Zero A input
    // INMODE[2] - Enable D input
    // INMODE[3] - Add/Subtract control for pre-adder
    // INMODE[4] - Selects between B1 and B2 inputs

    // Get individual bits from inmode
    int8_t mask4 = 0x10;
    bool inmode4 = (inmode & mask4) != 0;

    int32_t b_output = inmode4 ? b1 : b2;
    return b_output;
}

int32_t pre_adder(int32_t a1, int32_t a2, int32_t d, int8_t inmode) {
    // Pre-adder functionality based on INMODE
    // Assume that the inputs are already masked and sign-extended appropriately
    // INMODE[0] - Selects between A1 and A2 inputs (used in a_select)
    // INMODE[1] - Zero A input (used in a_select)
    // INMODE[2] - Enable D input
    // INMODE[3] - Add/Subtract control for pre-adder
    // INMODE[4] - B input selection (not used here)

    // Get individual bits from inmode
    int8_t mask2 = 0x4;
    int8_t mask3 = 0x8;

    bool inmode2 = (inmode & mask2) != 0;
    bool inmode3 = (inmode & mask3) != 0;

    int32_t a_input;
    int32_t d_input;
    int32_t preadder_output;   

    #ifndef USE_DPORT
        preadder_output = a_select(a1, a2, inmode);
    #else
        a_input = a_select(a1, a2, inmode);
        d_input = inmode2 ? d : 0;
        preadder_output = inmode3 ? d_input - a_input : d_input + a_input;
    #endif

    return preadder_output;
}

int64_t multiplier(int32_t a, int32_t b) {
    // Multiplier functionality
    // Assume that the inputs are already masked and sign-extended appropriately

    int64_t m = (int64_t)a * (int64_t)b;

    return m;
}

int64_t x_mux(int64_t m, int64_t p, int32_t a, int32_t b, int8_t opmode) {
    // X MUX functionality based on OPMODE
    int8_t x_mask = 0x3; // Bits 0 and 1
    int8_t y_mask = 0xC; // Bits 2 and 3

    int8_t x_control = opmode & x_mask;
    int8_t y_control = (opmode & y_mask) >> 2;

    int64_t x_output;

    uint64_t mask43 = 0x7FFFFFFFFFFULL; // Lower 43 bits
    int64_t a_b_concat = ((int64_t)((((((uint64_t)a) << 18) | (uint64_t)b) & mask43) << 20)) >> 20; // Concatenate A and B inputs 

    switch (x_control) {
        case 0: // 00
            x_output = 0;
            break;
        case 1: // 01
            if (y_control == 1) { // 01
                x_output = m;
            } else {
                x_output = 0; // Illegal case, default to 0
            }
            break;
        case 2: // 10
            x_output = p;
            break;
        case 3: // 11
            x_output = a_b_concat;
            break;
    }

    return x_output;
}

int64_t y_mux(int64_t m, int64_t c, int8_t opmode) {
    // Y MUX functionality based on OPMODE
    int8_t x_mask = 0x3; // Bits 0 and 1
    int8_t y_mask = 0xC; // Bits 2 and 3

    int8_t x_control = opmode & x_mask;
    int8_t y_control = (opmode & y_mask) >> 2;

    int64_t y_output;

    switch (y_control) {
        case 0: // 00
            y_output = 0;
            break;
        case 1: // 01
            if (x_control == 1) { // 01
                y_output = m;
            } else {
                y_output = 0; // Illegal case, default to 0
            }
            break;
        case 2: // 10 - Illegal case, default to 0
            y_output = 0xFFFFFFFFFFFFULL;
            break;
        case 3: // 11 - Illegal case, default to 0
            y_output = c;
            break;
    }

    return y_output;
}

int64_t z_mux(int64_t pcin, int64_t p, int64_t c, int8_t opmode) {
    // Z MUX functionality based on OPMODE
    int8_t x_mask = 0x3; // Bits 0 and 1
    int8_t y_mask = 0xC; // Bits 2 and 3
    int8_t z_mask = 0x70; // Bits 4, 5, and 6

    int8_t x_control = opmode & x_mask;
    int8_t y_control = (opmode & y_mask) >> 2;
    int8_t z_control = (opmode & z_mask) >> 4;

    int64_t z_output;

    switch (z_control) {
        case 0: 
            z_output = 0;
            break;
        case 1: 
            z_output = pcin;
            break;
        case 2: // 010
            z_output = p;
            break;
        case 3: // 011
            z_output = c;
            break;
        case 4: // 100
            z_output = ((y_control == 2) & (x_control == 0)) ? p : 0;
            break;
        case 5: // 101
            z_output = pcin << 17;
            break;
        case 6: // 110
            z_output = p << 17;
            break;
        case 7: // 111 
            z_output = 0; // Illegal case, default to 0
            break;
    }

    return z_output;
}

int64_t stage2(int64_t x, int64_t y, int64_t z, int64_t cin, int8_t alumode, int8_t opmode) {
    // Stage 2 functionality: Adder/Subtractor/Logic based on ALUMODE
    // Assume that the inputs are already masked and sign-extended appropriately

    int8_t opmode_y_mask = 0xC; // Bits 2 and 3
    int8_t alumode4_mask = 0xF; // Bits 0 to 3
    int8_t alumode2_mask = 0x3; // Bits 0 and 1

    int8_t opmode_y_control = (opmode & opmode_y_mask) >> 2;
    int8_t alumode4_control = alumode & alumode4_mask;
    int8_t alumode2_control = alumode & alumode2_mask;

    int64_t result;

    if (opmode_y_control == 1 || opmode_y_control == 3) { // Three input operation
        if (alumode4_control < 4) { // Three input operation
            // Arithmetic operations
            switch (alumode2_control) {
                case 0: // 00
                    result = x + y + z + cin;
                    break;
                case 1: // 01
                    result = ~z + x + y + cin;
                    break;
                case 2: // 10
                    result = ~(x + y + z + cin);
                    break;
                case 3: // 11
                    result = z - (x + y + cin);  
                    break; 
            }
        } else {
            result = 0; // Illegal case
        }
    } else { // Two input operation
        if (opmode_y_control == 0) { // 00
            switch(alumode4_control) {
                case 4: // 0100
                    result = x ^ z; // X XOR Z
                    break;
                case 5: // 0101
                    result = ~(x ^ z); // X XNOR Z
                    break;
                case 6: // 0110
                    result = ~(x ^ z); // X XNOR Z
                    break;
                case 7: // 0111
                    result = x ^ z; // X XOR Z
                    break;
                case 12: // 1100
                    result = x & z; // X AND Z
                    break;
                case 13: // 1101
                    result = x & ~z; // X AND (NOT Z)
                    break;
                case 14: // 1110
                    result = ~(x & z); // X NAND Z
                    break;
                case 15: // 1111
                    result = ~x | z; // (NOT X) OR Z
                    break;
                default:
                    result = 0; // Illegal case
                    break;
            }
        } else if (opmode_y_control == 2) { // 10
            switch(alumode4_control) {
                case 4: // 0100
                    result = ~(x ^ z); // X XNOR Z
                    break;
                case 5: // 0101
                    result = x ^ z; // X XOR Z
                    break;
                case 6: // 0110
                    result = x ^ z; // X XOR Z
                    break;
                case 7: // 0111
                    result = ~(x ^ z); // X XNOR Z
                    break;
                case 12: // 1100
                    result = x | z; // X OR Z
                    break;
                case 13: // 1101
                    result = x | ~z; // X OR (NOT Z)
                    break;
                case 14: // 1110
                    result = ~(x | z); // X NOR Z
                    break;
                case 15: // 1111
                    result = ~x & z; // (NOT X) AND Z
                    break;
                default:
                    result = 0; // Illegal case
                    break;
            }
        }
    }

    return result;
}

int64_t carry_select(int8_t carryinsel, bool carryin, bool carrycascin, bool carrycascout, int32_t a, int32_t b, int64_t p, int64_t pcin) {
    // Carry input selection based on CARRYINSEL
    // CARRYINSEL[2:0] - Selects the source for the carry-in to the adder/subtractor

    int8_t mask_carryinsel = 0x7; // Bits 0, 1, and 2
    int8_t carryinsel_control = carryinsel & mask_carryinsel;

    int64_t carryin_output; // 64 bit for compatibility

    switch(carryinsel_control){
        case 0: // 000
            carryin_output = (int64_t) carryin;
            break;
        case 1: // 001
            carryin_output = (int64_t) (~pcin & (0x1 << 47));
            break;
        case 2: // 010
            carryin_output = (int64_t) carrycascin;
            break;
        case 3: // 011
            carryin_output = (int64_t) ~(~pcin & (0x1 << 47));
            break;
        case 4: // 100
            carryin_output = (int64_t) carrycascout;
            break;
        case 5: // 101
            carryin_output = (int64_t) (~p & (0x1 << 47));
            break;
        case 6: // 110
            bool a_24 = (a & (0x1ULL << 24)) != 0;
            bool b_17 = (b & (0x1ULL << 17)) != 0;
            carryin_output = (int64_t) (a_24 ^ b_17);
            break;
        case 7: // 111
            carryin_output = (int64_t) ~(~p & (0x1 << 47));
            break;
    }

    return carryin_output;
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