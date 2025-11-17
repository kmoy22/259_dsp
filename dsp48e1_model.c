#include "dsp48e1_model.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static size_t total_pipeline_latency(const dsp48e1_config_t *cfg) {
    return (size_t)cfg->multiplier_latency +
           (size_t)cfg->adder_latency +
           (size_t)cfg->accumulator_latency +
           (size_t)cfg->rounding_latency +
           (size_t)cfg->saturation_latency;
}

static float round_to_nearest_even(float value) {
    return nearbyintf(value);
}

static float saturate_fp32(float value) {
    if (!isfinite(value)) {
        return value > 0.0f ? FLT_MAX : -FLT_MAX;
    }
    if (value > FLT_MAX) {
        return FLT_MAX;
    }
    if (value < -FLT_MAX) {
        return -FLT_MAX;
    }
    return value;
}

static float stage_shift_float(float *stage,
                               size_t span,
                               size_t idx,
                               float incoming) {
    if (span == 0 || stage == NULL) {
        return incoming;
    }

    float *base = stage + idx * span;
    float outgoing = base[span - 1];
    for (size_t i = span - 1; i > 0; --i) {
        base[i] = base[i - 1];
    }
    base[0] = incoming;
    return outgoing;
}

static uint8_t stage_shift_u8(uint8_t *stage,
                              size_t span,
                              size_t idx,
                              uint8_t incoming) {
    if (span == 0 || stage == NULL) {
        return incoming;
    }

    uint8_t *base = stage + idx * span;
    uint8_t outgoing = base[span - 1];
    for (size_t i = span - 1; i > 0; --i) {
        base[i] = base[i - 1];
    }
    base[0] = incoming;
    return outgoing;
}

static void free_stage_buffers(size_t span,
                               float **value_buf,
                               uint8_t **valid_buf) {
    if (span > 0) {
        free(*value_buf);
        free(*valid_buf);
    }
    *value_buf = NULL;
    *valid_buf = NULL;
}

void dsp48e1_format_fp32(dsp48e1_format_desc_t *desc) {
    if (!desc) {
        return;
    }
    desc->kind = DSP48E1_FORMAT_FP32;
    desc->total_bits = 32;
    desc->exponent_bits = 8;
    desc->mantissa_bits = 23;
    desc->fractional_bits = 0;
    desc->exponent_bias = 127;
}

void dsp48e1_default_fp32_config(dsp48e1_config_t *cfg) {
    if (!cfg) {
        return;
    }
    dsp48e1_format_fp32(&cfg->format);
    cfg->multiplier_latency = 2;
    cfg->adder_latency = 1;
    cfg->accumulator_latency = 1;
    cfg->rounding_latency = 1;
    cfg->saturation_latency = 0;
    cfg->enable_rounding = 1;
    cfg->enable_saturation = 0;
}

int dsp48e1_model_init(dsp48e1_model_t *model,
                       const dsp48e1_config_t *config,
                       size_t rows,
                       size_t cols,
                       size_t depth) {
    if (!model || !config || rows == 0 || cols == 0 || depth == 0) {
        return -1;
    }

    memset(model, 0, sizeof(*model));
    model->config = *config;
    model->rows = rows;
    model->cols = cols;
    model->depth = depth;
    model->mul_span = config->multiplier_latency;
    model->add_span = config->adder_latency;
    model->accum_span = config->accumulator_latency;
    model->round_span = config->rounding_latency;
    model->out_span = config->saturation_latency;

    const size_t elements = rows * cols;

    model->accumulators = (float *)calloc(elements, sizeof(float));
    model->contrib_counts = (size_t *)calloc(elements, sizeof(size_t));
    if (!model->accumulators || !model->contrib_counts) {
        dsp48e1_model_free(model);
        return -1;
    }

    if (model->mul_span) {
        model->pipeline_mul = (float *)calloc(elements * model->mul_span, sizeof(float));
        model->pipeline_mul_valid = (uint8_t *)calloc(elements * model->mul_span, sizeof(uint8_t));
        if (!model->pipeline_mul || !model->pipeline_mul_valid) {
            dsp48e1_model_free(model);
            return -1;
        }
    }

    if (model->add_span) {
        model->pipeline_add = (float *)calloc(elements * model->add_span, sizeof(float));
        model->pipeline_add_valid = (uint8_t *)calloc(elements * model->add_span, sizeof(uint8_t));
        if (!model->pipeline_add || !model->pipeline_add_valid) {
            dsp48e1_model_free(model);
            return -1;
        }
    }

    if (model->accum_span) {
        model->pipeline_accum = (float *)calloc(elements * model->accum_span, sizeof(float));
        model->pipeline_accum_valid = (uint8_t *)calloc(elements * model->accum_span, sizeof(uint8_t));
        if (!model->pipeline_accum || !model->pipeline_accum_valid) {
            dsp48e1_model_free(model);
            return -1;
        }
    }

    if (model->round_span) {
        model->pipeline_round = (float *)calloc(elements * model->round_span, sizeof(float));
        model->pipeline_round_valid = (uint8_t *)calloc(elements * model->round_span, sizeof(uint8_t));
        if (!model->pipeline_round || !model->pipeline_round_valid) {
            dsp48e1_model_free(model);
            return -1;
        }
    }

    if (model->out_span) {
        model->pipeline_out = (float *)calloc(elements * model->out_span, sizeof(float));
        model->pipeline_out_valid = (uint8_t *)calloc(elements * model->out_span, sizeof(uint8_t));
        if (!model->pipeline_out || !model->pipeline_out_valid) {
            dsp48e1_model_free(model);
            return -1;
        }
    }

    return 0;
}

void dsp48e1_model_free(dsp48e1_model_t *model) {
    if (!model) {
        return;
    }

    free(model->accumulators);
    free(model->contrib_counts);
    model->accumulators = NULL;
    model->contrib_counts = NULL;

    free_stage_buffers(model->mul_span, &model->pipeline_mul, &model->pipeline_mul_valid);
    free_stage_buffers(model->add_span, &model->pipeline_add, &model->pipeline_add_valid);
    free_stage_buffers(model->accum_span, &model->pipeline_accum, &model->pipeline_accum_valid);
    free_stage_buffers(model->round_span, &model->pipeline_round, &model->pipeline_round_valid);
    free_stage_buffers(model->out_span, &model->pipeline_out, &model->pipeline_out_valid);

    model->mul_span = model->add_span = model->accum_span = 0;
    model->round_span = model->out_span = 0;
    model->rows = model->cols = model->depth = 0;
    model->cycle = 0;
    model->config = (dsp48e1_config_t){0};
}

void dsp48e1_model_reset(dsp48e1_model_t *model) {
    if (!model) {
        return;
    }

    const size_t elements = model->rows * model->cols;
    if (model->accumulators) {
        memset(model->accumulators, 0, sizeof(float) * elements);
    }
    if (model->contrib_counts) {
        memset(model->contrib_counts, 0, sizeof(size_t) * elements);
    }

    if (model->mul_span && model->pipeline_mul) {
        memset(model->pipeline_mul, 0, sizeof(float) * elements * model->mul_span);
        memset(model->pipeline_mul_valid, 0, sizeof(uint8_t) * elements * model->mul_span);
    }
    if (model->add_span && model->pipeline_add) {
        memset(model->pipeline_add, 0, sizeof(float) * elements * model->add_span);
        memset(model->pipeline_add_valid, 0, sizeof(uint8_t) * elements * model->add_span);
    }
    if (model->accum_span && model->pipeline_accum) {
        memset(model->pipeline_accum, 0, sizeof(float) * elements * model->accum_span);
        memset(model->pipeline_accum_valid, 0, sizeof(uint8_t) * elements * model->accum_span);
    }
    if (model->round_span && model->pipeline_round) {
        memset(model->pipeline_round, 0, sizeof(float) * elements * model->round_span);
        memset(model->pipeline_round_valid, 0, sizeof(uint8_t) * elements * model->round_span);
    }
    if (model->out_span && model->pipeline_out) {
        memset(model->pipeline_out, 0, sizeof(float) * elements * model->out_span);
        memset(model->pipeline_out_valid, 0, sizeof(uint8_t) * elements * model->out_span);
    }

    model->cycle = 0;
}

int dsp48e1_model_step_fp32(dsp48e1_model_t *model,
                            size_t row,
                            size_t col,
                            int input_valid,
                            float a,
                            float b,
                            float addend,
                            int *out_valid,
                            float *out_value) {
    if (!model || row >= model->rows || col >= model->cols) {
        return -1;
    }

    const size_t idx = row * model->cols + col;
    const uint8_t valid_in = input_valid ? 1U : 0U;

    const float product = a * b;
    const float mul_ready = stage_shift_float(model->pipeline_mul,
                                              model->mul_span,
                                              idx,
                                              product);
    const uint8_t mul_valid = stage_shift_u8(model->pipeline_mul_valid,
                                             model->mul_span,
                                             idx,
                                             valid_in);

    const float add_input = mul_ready + addend;
    const float add_ready = stage_shift_float(model->pipeline_add,
                                              model->add_span,
                                              idx,
                                              add_input);
    const uint8_t add_valid = stage_shift_u8(model->pipeline_add_valid,
                                             model->add_span,
                                             idx,
                                             mul_valid);

    const float prev_accum = model->accumulators[idx];
    float accum_input = prev_accum;
    if (add_valid) {
        accum_input = prev_accum + add_ready;
    }

    const float accum_ready = stage_shift_float(model->pipeline_accum,
                                                model->accum_span,
                                                idx,
                                                accum_input);
    const uint8_t accum_valid = stage_shift_u8(model->pipeline_accum_valid,
                                               model->accum_span,
                                               idx,
                                               add_valid);

    if (add_valid) {
        model->accumulators[idx] = accum_input;
        if (model->contrib_counts[idx] < SIZE_MAX) {
            model->contrib_counts[idx] += 1;
        }
    }

    float round_input = accum_ready;
    if (accum_valid && model->config.enable_rounding) {
        round_input = round_to_nearest_even(accum_ready);
    }

    const float round_ready = stage_shift_float(model->pipeline_round,
                                                model->round_span,
                                                idx,
                                                round_input);
    const uint8_t round_valid = stage_shift_u8(model->pipeline_round_valid,
                                               model->round_span,
                                               idx,
                                               accum_valid);

    float sat_input = round_ready;
    if (round_valid && model->config.enable_saturation) {
        sat_input = saturate_fp32(round_ready);
    }

    const float out_ready = stage_shift_float(model->pipeline_out,
                                              model->out_span,
                                              idx,
                                              sat_input);
    const uint8_t out_ready_valid = stage_shift_u8(model->pipeline_out_valid,
                                                   model->out_span,
                                                   idx,
                                                   round_valid);

    model->cycle++;

    if (out_valid) {
        *out_valid = (int)out_ready_valid;
    }
    if (out_value) {
        if (out_ready_valid) {
            *out_value = out_ready;
        } else {
            *out_value = 0.0f;
        }
    }

    return 0;
}

int dsp48e1_model_gemm_fp32(dsp48e1_model_t *model,
                            const float *lhs,
                            size_t lhs_stride,
                            const float *rhs,
                            size_t rhs_stride,
                            const float *bias,
                            float *dst,
                            size_t dst_stride) {
    if (!model || !lhs || !rhs || !dst ||
        lhs_stride < model->depth || rhs_stride < model->cols ||
        dst_stride < model->cols) {
        return -1;
    }

    dsp48e1_model_reset(model);

    const size_t elements = model->rows * model->cols;
    float *last_values = (float *)calloc(elements, sizeof(float));
    if (!last_values) {
        return -1;
    }

    for (size_t k = 0; k < model->depth; ++k) {
        for (size_t row = 0; row < model->rows; ++row) {
            const float *lhs_row = lhs + row * lhs_stride;
            for (size_t col = 0; col < model->cols; ++col) {
                float addend = 0.0f;
                if (bias && k == 0) {
                    addend = bias[col];
                }

                int valid = 0;
                float value = 0.0f;
                const int status = dsp48e1_model_step_fp32(model,
                                                           row,
                                                           col,
                                                           1,
                                                           lhs_row[k],
                                                           rhs[k * rhs_stride + col],
                                                           addend,
                                                           &valid,
                                                           &value);
                if (status != 0) {
                    free(last_values);
                    return status;
                }
                if (valid) {
                    last_values[row * model->cols + col] = value;
                }
            }
        }
    }

    const size_t flush_cycles = total_pipeline_latency(&model->config);
    for (size_t f = 0; f <= flush_cycles; ++f) {
        for (size_t row = 0; row < model->rows; ++row) {
            for (size_t col = 0; col < model->cols; ++col) {
                int valid = 0;
                float value = 0.0f;
                dsp48e1_model_step_fp32(model,
                                        row,
                                        col,
                                        0,
                                        0.0f,
                                        0.0f,
                                        0.0f,
                                        &valid,
                                        &value);
                if (valid) {
                    last_values[row * model->cols + col] = value;
                }
            }
        }
    }

    for (size_t row = 0; row < model->rows; ++row) {
        float *dst_row = dst + row * dst_stride;
        for (size_t col = 0; col < model->cols; ++col) {
            dst_row[col] = last_values[row * model->cols + col];
        }
    }

    free(last_values);
    return 0;
}

int dsp48e1_model_self_test_fp32(void) {
    dsp48e1_config_t cfg;
    dsp48e1_default_fp32_config(&cfg);

    dsp48e1_model_t model;
    if (dsp48e1_model_init(&model, &cfg, 2, 2, 3) != 0) {
        return -1;
    }

    const float lhs[2][3] = {
        {1.0f, 2.0f, 3.0f},
        {-1.0f, -2.0f, -3.0f}
    };
    const float rhs[3][2] = {
        {4.0f, 5.0f},
        {6.0f, 7.0f},
        {8.0f, 9.0f}
    };
    const float bias[2] = {0.5f, -0.5f};
    float dst[2][2] = {{0.0f, 0.0f}, {0.0f, 0.0f}};

    if (dsp48e1_model_gemm_fp32(&model,
                                &lhs[0][0],
                                3,
                                &rhs[0][0],
                                2,
                                bias,
                                &dst[0][0],
                                2) != 0) {
        dsp48e1_model_free(&model);
        return -1;
    }

    float golden[2][2] = {{0.0f, 0.0f}, {0.0f, 0.0f}};
    for (size_t row = 0; row < 2; ++row) {
        for (size_t col = 0; col < 2; ++col) {
            float sum = bias[col];
            for (size_t k = 0; k < 3; ++k) {
                sum += lhs[row][k] * rhs[k][col];
            }
            golden[row][col] = sum;
        }
    }

    int status = 0;
    for (size_t row = 0; row < 2; ++row) {
        for (size_t col = 0; col < 2; ++col) {
            const float diff = fabsf(dst[row][col] - golden[row][col]);
            if (diff > 1e-4f) {
                status = -1;
                break;
            }
        }
    }

    dsp48e1_model_free(&model);
    return status;
}

int main(void) {
    printf("%f\n", round_to_nearest_even(2.9));
    return 1;
}