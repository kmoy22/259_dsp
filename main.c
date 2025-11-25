#include <stdio.h>
#include <stdint.h>
#include <string.h>


uint32_t dsp48e1_combined(uint32_t a, uint32_t b);


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

int main() {
    float test_a[] = {
        1.0f, 1.5f, 2.5f, 3.75f,
        0.5f, 10.0f, -1.5f, -2.25f,
        100.0f, 0.00123f, 
        100.0f
    };

    float test_b[] = {
        2.0f, 2.5f, -4.0f, 0.75f,
        -0.5f, -3.0f,  5.25f, 8.0f,
        0.0001f, -200.0f, 
        0.0001f
    };

    int N = sizeof(test_a) / sizeof(test_a[0]);

    printf("=== DSP48E1 Float Multiply Test ===\n\n");

    for (int i = 0; i < N; i++) {
        float a = test_a[i];
        float b = test_b[i];

        uint32_t a_bits = f2u(a);
        uint32_t b_bits = f2u(b);
        printf("a_bits = %u\n", a_bits);
        printf("b_bits = %u\n", b_bits);

        uint32_t c_bits = dsp48e1_combined(a_bits, b_bits);
        
        float c = u2f(c_bits);

        float ref = a * b;

        printf("Test %d:\n", i);
        printf("   a = %-12.7f   (0x%08X)\n", a, a_bits);
        printf("   b = %-12.7f   (0x%08X)\n", b, b_bits);
        printf("ref=a*b = %-12.7f   (0x%08X)\n", ref, f2u(ref));
        printf("C-model = %-12.7f   (0x%08X)\n", c, c_bits);
        printf("----------------------------------------------------\n");
    }

    return 0;
}
