//  qcms
//  Copyright (C) 2009 Mozilla Foundation
//  Copyright (C) 2015 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <emmintrin.h>

#include "qcmsint.h"

/* pre-shuffled: just load these into XMM reg instead of load-scalar/shufps sequence */
#define FLOATSCALE  (float)(PRECACHE_OUTPUT_SIZE)
#define CLAMPMAXVAL ( ((float) (PRECACHE_OUTPUT_SIZE - 1)) / PRECACHE_OUTPUT_SIZE )
static const ALIGN float floatScaleX4[4] =
    { FLOATSCALE, FLOATSCALE, FLOATSCALE, FLOATSCALE};
static const ALIGN float clampMaxValueX4[4] =
    { CLAMPMAXVAL, CLAMPMAXVAL, CLAMPMAXVAL, CLAMPMAXVAL};

void qcms_transform_data_rgb_out_lut_sse2(qcms_transform *transform,
                                          unsigned char *src,
                                          unsigned char *dest,
                                          size_t length,
                                          qcms_format_type output_format)
{
    unsigned int i;
    float (*mat)[4] = transform->matrix;
    char input_back[32];
    /* Ensure we have a buffer that's 16 byte aligned regardless of the original
     * stack alignment. We can't use __attribute__((aligned(16))) or __declspec(align(32))
     * because they don't work on stack variables. gcc 4.4 does do the right thing
     * on x86 but that's too new for us right now. For more info: gcc bug #16660 */
    float const * input = (float*)(((uintptr_t)&input_back[16]) & ~0xf);
    /* share input and output locations to save having to keep the
     * locations in separate registers */
    uint32_t const * output = (uint32_t*)input;

    /* deref *transform now to avoid it in loop */
    const float *igtbl_r = transform->input_gamma_table_r;
    const float *igtbl_g = transform->input_gamma_table_g;
    const float *igtbl_b = transform->input_gamma_table_b;

    /* deref *transform now to avoid it in loop */
    const uint8_t *otdata_r = &transform->output_table_r->data[0];
    const uint8_t *otdata_g = &transform->output_table_g->data[0];
    const uint8_t *otdata_b = &transform->output_table_b->data[0];

    /* input matrix values never change */
    const __m128 mat0  = _mm_load_ps(mat[0]);
    const __m128 mat1  = _mm_load_ps(mat[1]);
    const __m128 mat2  = _mm_load_ps(mat[2]);

    /* these values don't change, either */
    const __m128 max   = _mm_load_ps(clampMaxValueX4);
    const __m128 min   = _mm_setzero_ps();
    const __m128 scale = _mm_load_ps(floatScaleX4);

    /* working variables */
    __m128 vec_r, vec_g, vec_b, result;
    const int r_out = output_format.r;
    const int b_out = output_format.b;

    /* CYA */
    if (!length)
        return;

    /* one pixel is handled outside of the loop */
    length--;

    /* setup for transforming 1st pixel */
    vec_r = _mm_load_ss(&igtbl_r[src[0]]);
    vec_g = _mm_load_ss(&igtbl_g[src[1]]);
    vec_b = _mm_load_ss(&igtbl_b[src[2]]);
    src += 3;

    /* transform all but final pixel */

    for (i=0; i<length; i++)
    {
        /* position values from gamma tables */
        vec_r = _mm_shuffle_ps(vec_r, vec_r, 0);
        vec_g = _mm_shuffle_ps(vec_g, vec_g, 0);
        vec_b = _mm_shuffle_ps(vec_b, vec_b, 0);

        /* gamma * matrix */
        vec_r = _mm_mul_ps(vec_r, mat0);
        vec_g = _mm_mul_ps(vec_g, mat1);
        vec_b = _mm_mul_ps(vec_b, mat2);

        /* crunch, crunch, crunch */
        vec_r  = _mm_add_ps(vec_r, _mm_add_ps(vec_g, vec_b));
        vec_r  = _mm_max_ps(min, vec_r);
        vec_r  = _mm_min_ps(max, vec_r);
        result = _mm_mul_ps(vec_r, scale);

        /* store calc'd output tables indices */
        _mm_store_si128((__m128i*)output, _mm_cvtps_epi32(result));

        /* load for next loop while store completes */
        vec_r = _mm_load_ss(&igtbl_r[src[0]]);
        vec_g = _mm_load_ss(&igtbl_g[src[1]]);
        vec_b = _mm_load_ss(&igtbl_b[src[2]]);
        src += 3;

        /* use calc'd indices to output RGB values */
        dest[r_out] = otdata_r[output[0]];
        dest[1]     = otdata_g[output[1]];
        dest[b_out] = otdata_b[output[2]];
        dest += 3;
    }

    /* handle final (maybe only) pixel */

    vec_r = _mm_shuffle_ps(vec_r, vec_r, 0);
    vec_g = _mm_shuffle_ps(vec_g, vec_g, 0);
    vec_b = _mm_shuffle_ps(vec_b, vec_b, 0);

    vec_r = _mm_mul_ps(vec_r, mat0);
    vec_g = _mm_mul_ps(vec_g, mat1);
    vec_b = _mm_mul_ps(vec_b, mat2);

    vec_r  = _mm_add_ps(vec_r, _mm_add_ps(vec_g, vec_b));
    vec_r  = _mm_max_ps(min, vec_r);
    vec_r  = _mm_min_ps(max, vec_r);
    result = _mm_mul_ps(vec_r, scale);

    _mm_store_si128((__m128i*)output, _mm_cvtps_epi32(result));

    dest[r_out] = otdata_r[output[0]];
    dest[1]     = otdata_g[output[1]];
    dest[b_out] = otdata_b[output[2]];
}

void qcms_transform_data_rgba_out_lut_sse2(qcms_transform *transform,
                                           unsigned char *src,
                                           unsigned char *dest,
                                           size_t length,
                                           qcms_format_type output_format)
{
    unsigned int i;
    float (*mat)[4] = transform->matrix;
    char input_back[32];
    /* Ensure we have a buffer that's 16 byte aligned regardless of the original
     * stack alignment. We can't use __attribute__((aligned(16))) or __declspec(align(32))
     * because they don't work on stack variables. gcc 4.4 does do the right thing
     * on x86 but that's too new for us right now. For more info: gcc bug #16660 */
    float const * input = (float*)(((uintptr_t)&input_back[16]) & ~0xf);
    /* share input and output locations to save having to keep the
     * locations in separate registers */
    uint32_t const * output = (uint32_t*)input;

    /* deref *transform now to avoid it in loop */
    const float *igtbl_r = transform->input_gamma_table_r;
    const float *igtbl_g = transform->input_gamma_table_g;
    const float *igtbl_b = transform->input_gamma_table_b;

    /* deref *transform now to avoid it in loop */
    const uint8_t *otdata_r = &transform->output_table_r->data[0];
    const uint8_t *otdata_g = &transform->output_table_g->data[0];
    const uint8_t *otdata_b = &transform->output_table_b->data[0];

    /* input matrix values never change */
    const __m128 mat0  = _mm_load_ps(mat[0]);
    const __m128 mat1  = _mm_load_ps(mat[1]);
    const __m128 mat2  = _mm_load_ps(mat[2]);

    /* these values don't change, either */
    const __m128 max   = _mm_load_ps(clampMaxValueX4);
    const __m128 min   = _mm_setzero_ps();
    const __m128 scale = _mm_load_ps(floatScaleX4);

    /* working variables */
    __m128 vec_r, vec_g, vec_b, result;
    const int r_out = output_format.r;
    const int b_out = output_format.b;
    unsigned char alpha;

    /* CYA */
    if (!length)
        return;

    /* one pixel is handled outside of the loop */
    length--;

    /* setup for transforming 1st pixel */
    vec_r = _mm_load_ss(&igtbl_r[src[0]]);
    vec_g = _mm_load_ss(&igtbl_g[src[1]]);
    vec_b = _mm_load_ss(&igtbl_b[src[2]]);
    alpha = src[3];
    src += 4;

    /* transform all but final pixel */

    for (i=0; i<length; i++)
    {
        /* position values from gamma tables */
        vec_r = _mm_shuffle_ps(vec_r, vec_r, 0);
        vec_g = _mm_shuffle_ps(vec_g, vec_g, 0);
        vec_b = _mm_shuffle_ps(vec_b, vec_b, 0);

        /* gamma * matrix */
        vec_r = _mm_mul_ps(vec_r, mat0);
        vec_g = _mm_mul_ps(vec_g, mat1);
        vec_b = _mm_mul_ps(vec_b, mat2);

        /* store alpha for this pixel; load alpha for next */
        dest[3] = alpha;
        alpha   = src[3];

        /* crunch, crunch, crunch */
        vec_r  = _mm_add_ps(vec_r, _mm_add_ps(vec_g, vec_b));
        vec_r  = _mm_max_ps(min, vec_r);
        vec_r  = _mm_min_ps(max, vec_r);
        result = _mm_mul_ps(vec_r, scale);

        /* store calc'd output tables indices */
        _mm_store_si128((__m128i*)output, _mm_cvtps_epi32(result));

        /* load gamma values for next loop while store completes */
        vec_r = _mm_load_ss(&igtbl_r[src[0]]);
        vec_g = _mm_load_ss(&igtbl_g[src[1]]);
        vec_b = _mm_load_ss(&igtbl_b[src[2]]);
        src += 4;

        /* use calc'd indices to output RGB values */
        dest[r_out] = otdata_r[output[0]];
        dest[1]     = otdata_g[output[1]];
        dest[b_out] = otdata_b[output[2]];
        dest += 4;
    }

    /* handle final (maybe only) pixel */

    vec_r = _mm_shuffle_ps(vec_r, vec_r, 0);
    vec_g = _mm_shuffle_ps(vec_g, vec_g, 0);
    vec_b = _mm_shuffle_ps(vec_b, vec_b, 0);

    vec_r = _mm_mul_ps(vec_r, mat0);
    vec_g = _mm_mul_ps(vec_g, mat1);
    vec_b = _mm_mul_ps(vec_b, mat2);

    dest[3] = alpha;

    vec_r  = _mm_add_ps(vec_r, _mm_add_ps(vec_g, vec_b));
    vec_r  = _mm_max_ps(min, vec_r);
    vec_r  = _mm_min_ps(max, vec_r);
    result = _mm_mul_ps(vec_r, scale);

    _mm_store_si128((__m128i*)output, _mm_cvtps_epi32(result));

    dest[r_out] = otdata_r[output[0]];
    dest[1]     = otdata_g[output[1]];
    dest[b_out] = otdata_b[output[2]];
}

void qcms_transform_data_tetra_clut_rgba_sse2(qcms_transform *transform,
        unsigned char *src,
        unsigned char *dest,
        size_t length,
        qcms_format_type output_format)
{
    const int r_out = output_format.r;
    const int b_out = output_format.b;

    size_t i;
    int xy_len = 1, mask;
    int x_len = transform->grid_size;
    int len = x_len * x_len;
    float* r_table = transform->r_clut;
    float* g_table = transform->g_clut;
    float* b_table = transform->b_clut;

    const __m128 __factor = _mm_set1_ps(255.0f);
    const __m128 __zero = _mm_set1_ps(0);
    const __m128 __one = _mm_set1_ps(1);
    const __m128 __stride = _mm_set_ps((float)(3 * xy_len), (float)(3 * x_len), (float)(3 * len), (float)(3 * len));
    const __m128 __div_grid = _mm_set1_ps((float)(transform->grid_size - 1) / 255.0f);

    __m128 lookups_r, lookups_g, lookups_b;

    ALIGN int ixyz_0[4];
    ALIGN int ixyz_n[4];

    __m128 xyz, xyz_n, r_xyz, input;
    __m128i result;

    for (i = 0; i < length; i++) {
        input = _mm_set_ps((float)src[2], (float)src[1], (float)src[0], (float)src[0]);

        input = _mm_mul_ps(input, __div_grid);

        // Floor
        xyz = _mm_cvtepi32_ps(_mm_cvttps_epi32(input));
        // Ceil
        xyz_n = _mm_add_ps(xyz, _mm_and_ps(_mm_cmpgt_ps(input, xyz), __one));

        r_xyz = _mm_sub_ps(input, xyz);

        xyz = _mm_mul_ps(xyz, __stride);
        _mm_store_si128((__m128i*) ixyz_0, _mm_cvtps_epi32(xyz));
        int x = ixyz_0[1];
        int y = ixyz_0[2];
        int z = ixyz_0[3];

        xyz_n = _mm_mul_ps(xyz_n, __stride);
        _mm_store_si128((__m128i*) ixyz_n, _mm_cvtps_epi32(xyz_n));
        int x_n = ixyz_n[1];
        int y_n = ixyz_n[2];
        int z_n = ixyz_n[3];

        dest[3] = src[3];
        src += 4;

        mask = _mm_movemask_ps(_mm_cmplt_ps(r_xyz, _mm_shuffle_ps(r_xyz, r_xyz, 0xFE)));

        switch(mask) {
        case 0x0: //rx >= ry && ry >= rz
        {
            const int i3 = x_n + y_n + z_n;
            const int i2 = x_n + y_n + z;
            const int i1 = x_n + y + z;
            const int i0 = x + y + z;

            lookups_r = _mm_set_ps(
                    r_table[i3],
                    r_table[i2],
                    r_table[i1],
                    r_table[i0]);
            lookups_r = _mm_move_ss(_mm_sub_ps(lookups_r, _mm_shuffle_ps(lookups_r, lookups_r, 0x90)),
                    lookups_r);

            lookups_g = _mm_set_ps(
                    g_table[i3],
                    g_table[i2],
                    g_table[i1],
                    g_table[i0]);
            lookups_g = _mm_move_ss(_mm_sub_ps(lookups_g, _mm_shuffle_ps(lookups_g, lookups_g, 0x90)),
                    lookups_g);

            lookups_b = _mm_set_ps(
                    b_table[i3],
                    b_table[i2],
                    b_table[i1],
                    b_table[i0]);
            lookups_b = _mm_move_ss(_mm_sub_ps(lookups_b, _mm_shuffle_ps(lookups_b, lookups_b, 0x90)),
                    lookups_b);

            break;
        }
        case 0x4: //rx >= rz && rz >= ry
        {
            const int i3 = x_n + y + z_n;
            const int i2 = x_n + y_n + z_n;
            const int i1 = x_n + y + z;
            const int i0 = x + y + z;

            lookups_r = _mm_set_ps(
                    r_table[i3],
                    r_table[i2],
                    r_table[i1],
                    r_table[i0]);
            lookups_r = _mm_move_ss(_mm_sub_ps(lookups_r, _mm_shuffle_ps(lookups_r, lookups_r, 0x70)),
                    lookups_r);

            lookups_g = _mm_set_ps(
                    g_table[i3],
                    g_table[i2],
                    g_table[i1],
                    g_table[i0]);
            lookups_g = _mm_move_ss(_mm_sub_ps(lookups_g, _mm_shuffle_ps(lookups_g, lookups_g, 0x70)),
                    lookups_g);

            lookups_b = _mm_set_ps(
                    b_table[i3],
                    b_table[i2],
                    b_table[i1],
                    b_table[i0]);
            lookups_b = _mm_move_ss(_mm_sub_ps(lookups_b, _mm_shuffle_ps(lookups_b, lookups_b, 0x70)),
                    lookups_b);
            break;
        }
        case 0x6:  //rz > rx && rx >= ry
        {
            const int i3 = x + y + z_n;
            const int i2 = x_n + y_n + z_n;
            const int i1 = x_n + y + z_n;
            const int i0 = x + y + z;

            lookups_r = _mm_set_ps(
                    r_table[i3],
                    r_table[i2],
                    r_table[i1],
                    r_table[i0]);
            lookups_r = _mm_move_ss(_mm_sub_ps(lookups_r, _mm_shuffle_ps(lookups_r, lookups_r, 0x1C)),
                    lookups_r);

            lookups_g = _mm_set_ps(
                    g_table[i3],
                    g_table[i2],
                    g_table[i1],
                    g_table[i0]);
            lookups_g = _mm_move_ss(_mm_sub_ps(lookups_g, _mm_shuffle_ps(lookups_g, lookups_g, 0x1C)),
                    lookups_g);

            lookups_b = _mm_set_ps(
                    b_table[i3],
                    b_table[i2],
                    b_table[i1],
                    b_table[i0]);
            lookups_b = _mm_move_ss(_mm_sub_ps(lookups_b, _mm_shuffle_ps(lookups_b, lookups_b, 0x1C)),
                    lookups_b);
            break;
        }
        case 0x1: //ry > rx && rx >= rz
        {
            const int i3 = x_n + y_n + z_n;
            const int i2 = x + y_n + z;
            const int i1 = x_n + y_n + z;
            const int i0 = x + y + z;

            lookups_r = _mm_set_ps(
                    r_table[i3],
                    r_table[i2],
                    r_table[i1],
                    r_table[i0]);
            lookups_r = _mm_move_ss(_mm_sub_ps(lookups_r, _mm_shuffle_ps(lookups_r, lookups_r, 0x48)),
                    lookups_r);

            lookups_g = _mm_set_ps(
                    g_table[i3],
                    g_table[i2],
                    g_table[i1],
                    g_table[i0]);
            lookups_g = _mm_move_ss(_mm_sub_ps(lookups_g, _mm_shuffle_ps(lookups_g, lookups_g, 0x48)),
                    lookups_g);

            lookups_b = _mm_set_ps(
                    b_table[i3],
                    b_table[i2],
                    b_table[i1],
                    b_table[i0]);
            lookups_b = _mm_move_ss(_mm_sub_ps(lookups_b, _mm_shuffle_ps(lookups_b, lookups_b, 0x48)),
                    lookups_b);
            break;
        }
        case 0x3: //ry >= rz && rz > rx
        {
            const int i3 = x + y_n + z_n;
            const int i2 = x + y_n + z;
            const int i1 = x_n + y_n + z_n;
            const int i0 = x + y + z;

            lookups_r = _mm_set_ps(
                    r_table[i3],
                    r_table[i2],
                    r_table[i1],
                    r_table[i0]);
            lookups_r = _mm_move_ss(_mm_sub_ps(lookups_r, _mm_shuffle_ps(lookups_r, lookups_r, 0x8C)),
                    lookups_r);

            lookups_g = _mm_set_ps(
                    g_table[i3],
                    g_table[i2],
                    g_table[i1],
                    g_table[i0]);
            lookups_g = _mm_move_ss(_mm_sub_ps(lookups_g, _mm_shuffle_ps(lookups_g, lookups_g, 0x8C)),
                    lookups_g);

            lookups_b = _mm_set_ps(
                    b_table[i3],
                    b_table[i2],
                    b_table[i1],
                    b_table[i0]);
            lookups_b = _mm_move_ss(_mm_sub_ps(lookups_b, _mm_shuffle_ps(lookups_b, lookups_b, 0x8C)),
                    lookups_b);
            break;
        }
        case 0x7: //rz > ry && ry > rx
        {
            const int i3 = x + y + z_n;
            const int i2 = x + y_n + z_n;
            const int i1 = x_n + y_n + z_n;
            const int i0 = x + y + z;

            lookups_r = _mm_set_ps(
                    r_table[i3],
                    r_table[i2],
                    r_table[i1],
                    r_table[i0]);
            lookups_r = _mm_move_ss(_mm_sub_ps(lookups_r, _mm_shuffle_ps(lookups_r, lookups_r, 0x38)),
                    lookups_r);

            lookups_g = _mm_set_ps(
                    g_table[i3],
                    g_table[i2],
                    g_table[i1],
                    g_table[i0]);
            lookups_g = _mm_move_ss(_mm_sub_ps(lookups_g, _mm_shuffle_ps(lookups_g, lookups_g, 0x38)),
                    lookups_g);

            lookups_b = _mm_set_ps(
                    b_table[i3],
                    b_table[i2],
                    b_table[i1],
                    b_table[i0]);
            lookups_b = _mm_move_ss(_mm_sub_ps(lookups_b, _mm_shuffle_ps(lookups_b, lookups_b, 0x38)),
                    lookups_b);
            break;
        }
        }

        r_xyz = _mm_mul_ps(_mm_move_ss(r_xyz, __one), __factor);

        lookups_r = _mm_mul_ps(lookups_r, r_xyz);
        lookups_g = _mm_mul_ps(lookups_g, r_xyz);
        lookups_b = _mm_mul_ps(lookups_b, r_xyz);

        lookups_r = _mm_add_ps(lookups_r, _mm_shuffle_ps(lookups_r, lookups_r, 0x4E));
        lookups_r = _mm_add_ps(lookups_r, _mm_shuffle_ps(lookups_r, lookups_r, 0xB1));

        lookups_g = _mm_add_ps(lookups_g, _mm_shuffle_ps(lookups_g, lookups_g, 0x4E));
        lookups_g = _mm_add_ps(lookups_g, _mm_shuffle_ps(lookups_g, lookups_g, 0xB1));

        lookups_b = _mm_add_ps(lookups_b, _mm_shuffle_ps(lookups_b, lookups_b, 0x4E));
        lookups_b = _mm_add_ps(lookups_b, _mm_shuffle_ps(lookups_b, lookups_b, 0xB1));

        // Clamp values to 0..255.
        input = _mm_set_ps(_mm_cvtss_f32(lookups_b), _mm_cvtss_f32(lookups_g),
                _mm_cvtss_f32(lookups_r), 0);
        input = _mm_min_ps(_mm_max_ps(input, __zero), __factor);

        // Conversion to int performs rounding operation.
        result = _mm_cvtps_epi32(input);
        dest[r_out] = (unsigned char)_mm_extract_epi16(result, 2);
        dest[1] = (unsigned char)_mm_extract_epi16(result, 4);
        dest[b_out] = (unsigned char)_mm_extract_epi16(result, 6);

        dest += 4;
    }
}
