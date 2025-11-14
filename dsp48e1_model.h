#ifndef DSP48E1_MODEL_H
#define DSP48E1_MODEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file dsp48e1_model.h
 *
 * Cycle-aware behavioural model for a tensor unit composed of DSP48E1 slices.
 * The model is format-aware but currently implements execution for IEEE-754
 * single precision (FP32) only.  Hooks are provided so that additional numeric
 * formats can be layered on top without altering the core datapath contract.
 */

typedef enum {
    DSP48E1_FORMAT_FP32 = 0,
    DSP48E1_FORMAT_BFLOAT16,
    DSP48E1_FORMAT_FP16,
    DSP48E1_FORMAT_INT8,
    DSP48E1_FORMAT_CUSTOM
} dsp48e1_format_kind_t;

typedef struct {
    dsp48e1_format_kind_t kind;
    uint8_t total_bits;
    uint8_t exponent_bits;
    uint8_t mantissa_bits;
    uint8_t fractional_bits; /* For fixed-point / integer formats. */
    int32_t exponent_bias;
} dsp48e1_format_desc_t;

typedef struct {
    dsp48e1_format_desc_t format;
    uint8_t multiplier_latency;
    uint8_t adder_latency;
    uint8_t accumulator_latency;
    uint8_t rounding_latency;
    uint8_t saturation_latency;
    int enable_rounding;
    int enable_saturation;
} dsp48e1_config_t;

typedef struct {
    dsp48e1_config_t config;
    size_t rows;
    size_t cols;
    size_t depth;
    size_t mul_span;
    size_t add_span;
    size_t accum_span;
    size_t round_span;
    size_t out_span;
    float *accumulators;
    float *pipeline_mul;
    float *pipeline_add;
    float *pipeline_accum;
    float *pipeline_round;
    float *pipeline_out;
    uint8_t *pipeline_mul_valid;
    uint8_t *pipeline_add_valid;
    uint8_t *pipeline_accum_valid;
    uint8_t *pipeline_round_valid;
    uint8_t *pipeline_out_valid;
    size_t *contrib_counts;
    uint64_t cycle;
} dsp48e1_model_t;

/**
 * Populate a format descriptor for IEEE-754 single precision.
 */
void dsp48e1_format_fp32(dsp48e1_format_desc_t *desc);

/**
 * Populate a configuration that mirrors the default DSP48E1 pipeline for FP32.
 */
void dsp48e1_default_fp32_config(dsp48e1_config_t *cfg);

/**
 * Initialise the tensor-unit model for a tile of dimension rows x cols with an
 * inner-product depth of depth (i.e. GEMM tile of size rows x cols x depth).
 * Memory is allocated internally; call dsp48e1_model_free() to release it.
 *
 * Returns 0 on success, non-zero on allocation or configuration failure.
 */
int dsp48e1_model_init(dsp48e1_model_t *model,
                       const dsp48e1_config_t *config,
                       size_t rows,
                       size_t cols,
                       size_t depth);

/**
 * Release internal buffers owned by the model.
 */
void dsp48e1_model_free(dsp48e1_model_t *model);

/**
 * Reset the cycle counter, accumulators, and pipeline registers to zero.
 */
void dsp48e1_model_reset(dsp48e1_model_t *model);

/**
 * Simulate a multiply-accumulate operation feeding one tensor output.
 *
 * The API expects coordinates (row, col) within the configured tile.  The
 * product a * b is accumulated into the running partial sum for that element.
 * When the pipeline produces a valid output, *out_valid is set to 1 and the
 * value is written to *out_value; otherwise *out_valid is set to 0.
 *
 * Pass input_valid = 0 to inject a pipeline bubble (useful for flushing).
 *
 * A return value of 0 indicates success; non-zero indicates parameter error.
 */
int dsp48e1_model_step_fp32(dsp48e1_model_t *model,
                            size_t row,
                            size_t col,
                            int input_valid,
                            float a,
                            float b,
                            float addend,
                            int *out_valid,
                            float *out_value);

/**
 * Convenience routine: execute a full GEMM tile (rows x cols x depth) in FP32.
 * Bias may be NULL; when present it is assumed to have length cols.
 *
 * The destination buffer must have at least rows * dst_stride entries.
 * Returns 0 on success.
 */
int dsp48e1_model_gemm_fp32(dsp48e1_model_t *model,
                            const float *lhs,
                            size_t lhs_stride,
                            const float *rhs,
                            size_t rhs_stride,
                            const float *bias,
                            float *dst,
                            size_t dst_stride);

/**
 * Lightweight self-check to validate the FP32 datapath against a scalar GEMM.
 * Returns 0 when all checks pass.
 */
int dsp48e1_model_self_test_fp32(void);

#ifdef __cplusplus
}
#endif

#endif /* DSP48E1_MODEL_H */
