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

    const int xy_len = 1;
    const int x_len = transform->grid_size;
    const int len = x_len * x_len;

    const __m128 __clut_stride = _mm_set_ps((float)(3 * xy_len), (float)(3 * x_len), (float)(3 * len), 0);
    const __m128 __grid_scaled = _mm_set1_ps((1.0f / 255.0f) * (transform->grid_size - 1));

    const __m128 __255 = _mm_set1_ps(255.0f);
    const __m128 __one = _mm_set1_ps(1.0f);
    const __m128 __000 = _mm_setzero_ps();

    const float* r_table = transform->r_clut;
    const float* g_table = transform->g_clut;
    const float* b_table = transform->b_clut;

    int i3, i2, i1, i0;

    __m128 c3;
    __m128 c2;
    __m128 c1;
    __m128 c0;

    __m128 in;

    __m128 xyz_r;
    __m128 xyz_0;
    __m128 xyz_n;

    ALIGN float xyz_r_f[4];
    ALIGN int   xyz_0_i[4];
    ALIGN int   xyz_n_i[4];

    __m128i result;

#define TETRA_SRC_RGB(r, g, b) _mm_set_ps((float)b, (float)g, (float)r, 0.f)

    for (i = 0; i < length; ++i) {
        // compute input point in cube lattice (grid) co-ordinates
        in = _mm_mul_ps(TETRA_SRC_RGB(src[0], src[1], src[2]), __grid_scaled);

        // floor: convert to int (truncate), convert back to float
        xyz_0 = _mm_cvtepi32_ps(_mm_cvttps_epi32(in));

        // ceil: where in is greater than xyz_0 = floor(in), add 1
        xyz_n = _mm_add_ps(xyz_0, _mm_and_ps(_mm_cmpgt_ps(in, xyz_0), __one));

        // compute the input point relative to the sub-cube origin
        xyz_r = _mm_sub_ps(in, xyz_0);

#define rx (xyz_r_f[1])
#define ry (xyz_r_f[2])
#define rz (xyz_r_f[3])

        _mm_store_ps(xyz_r_f, xyz_r);

#define x0 (xyz_0_i[1])
#define y0 (xyz_0_i[2])
#define z0 (xyz_0_i[3])

        xyz_0 = _mm_mul_ps(xyz_0, __clut_stride);
        _mm_store_si128((__m128i*) xyz_0_i, _mm_cvtps_epi32(xyz_0));

#define xn (xyz_n_i[1])
#define yn (xyz_n_i[2])
#define zn (xyz_n_i[3])

        xyz_n = _mm_mul_ps(xyz_n, __clut_stride);
        _mm_store_si128((__m128i*) xyz_n_i, _mm_cvtps_epi32(xyz_n));

        dest[3] = src[3];
        src += 4;

#define SET_I0_AND_PREFETCH_CLUT() \
        _mm_prefetch((char*)&(r_table[i0 = x0 + y0 + z0]), _MM_HINT_T0)

#if !defined(_MSC_VER)
        SET_I0_AND_PREFETCH_CLUT();
#endif

#define TETRA_LOOKUP_CLUT(i3, i2, i1, i0) \
        c0 = _mm_set_ps(b_table[i0], g_table[i0], r_table[i0], 0.f), \
        c1 = _mm_set_ps(b_table[i1], g_table[i1], r_table[i1], 0.f), \
        c2 = _mm_set_ps(b_table[i2], g_table[i2], r_table[i2], 0.f), \
        c3 = _mm_set_ps(b_table[i3], g_table[i3], r_table[i3], 0.f)

        if (rx >= ry) {

#if defined(_MSC_VER)
            SET_I0_AND_PREFETCH_CLUT();
#endif
            if (ry >= rz) {         // rx >= ry && ry >= rz

                i3 = yn + (i1 = xn);
                i1 += i0 - x0;
                i2 = i3 + z0;
                i3 += zn;

                TETRA_LOOKUP_CLUT(i3, i2, i1, i0);

                c3 = _mm_sub_ps(c3, c2);
                c2 = _mm_sub_ps(c2, c1);
                c1 = _mm_sub_ps(c1, c0);

            } else if (rx >= rz) {  // rx >= rz && rz >= ry

                i3 = zn + (i1 = xn);
                i1 += i0 - x0;
                i2 = i3 + yn;
                i3 += y0;

                TETRA_LOOKUP_CLUT(i3, i2, i1, i0);

                c2 = _mm_sub_ps(c2, c3);
                c3 = _mm_sub_ps(c3, c1);
                c1 = _mm_sub_ps(c1, c0);

            } else {                // rz > rx && rx >= ry

                i2 = xn + (i3 = zn);
                i3 += i0 - z0;
                i1 = i2 + y0;
                i2 += yn;

                TETRA_LOOKUP_CLUT(i3, i2, i1, i0);

                c2 = _mm_sub_ps(c2, c1);
                c1 = _mm_sub_ps(c1, c3);
                c3 = _mm_sub_ps(c3, c0);
            }
        } else {

#if defined(_MSC_VER)
            SET_I0_AND_PREFETCH_CLUT();
#endif
            if (rx >= rz) {         // ry > rx && rx >= rz

                i3 = xn + (i2 = yn);
                i2 += i0 - y0;
                i1 = i3 + z0;
                i3 += zn;

                TETRA_LOOKUP_CLUT(i3, i2, i1, i0);

                c3 = _mm_sub_ps(c3, c1);
                c1 = _mm_sub_ps(c1, c2);
                c2 = _mm_sub_ps(c2, c0);

            } else if (ry >= rz) {  // ry >= rz && rz > rx

                i3 = zn + (i2 = yn);
                i2 += i0 - y0;
                i1 = i3 + xn;
                i3 += x0;

                TETRA_LOOKUP_CLUT(i3, i2, i1, i0);

                c1 = _mm_sub_ps(c1, c3);
                c3 = _mm_sub_ps(c3, c2);
                c2 = _mm_sub_ps(c2, c0);

            } else {                // rz > ry && ry > rx

                i2 = yn + (i3 = zn);
                i3 += i0 - z0;
                i1 = i2 + xn;
                i2 += x0;

                TETRA_LOOKUP_CLUT(i3, i2, i1, i0);

                c1 = _mm_sub_ps(c1, c2);
                c2 = _mm_sub_ps(c2, c3);
                c3 = _mm_sub_ps(c3, c0);
            }
        }

        // output.xyz = column_matrix(c1, c2, c3) x r.xyz + c0.xyz

        in = _mm_shuffle_ps(xyz_r, xyz_r, _MM_SHUFFLE(1, 1, 1, 1));
        c1 = _mm_mul_ps(c1, in);
        in = _mm_shuffle_ps(xyz_r, xyz_r, _MM_SHUFFLE(2, 2, 2, 2));
        c2 = _mm_mul_ps(c2, in);
        in = _mm_shuffle_ps(xyz_r, xyz_r, _MM_SHUFFLE(3, 3, 3, 3));
        c3 = _mm_mul_ps(c3, in);

        in = _mm_add_ps(c3, c2);
        in = _mm_add_ps(in, c1);
        in = _mm_add_ps(in, c0);

        // clamp to [0.0..1.0] and scale by 255

        in = _mm_max_ps(in, __000);
        in = _mm_min_ps(in, __one);
        in = _mm_mul_ps(in, __255);

        result = _mm_cvtps_epi32(in); // convert to int (rounding)

        dest[r_out] = (unsigned char) _mm_extract_epi16(result, 2);
        dest[1]     = (unsigned char) _mm_extract_epi16(result, 4);
        dest[b_out] = (unsigned char) _mm_extract_epi16(result, 6);

        dest += 4;
    }
}
