/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#ifndef AOM_AV1_ENCODER_PICKCDEF_H_
#define AOM_AV1_ENCODER_PICKCDEF_H_

#include "av1/common/cdef.h"
#include "av1/encoder/speed_features.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!\brief AV1 CDEF parameter search
 *
 * \ingroup in_loop_cdef
 *
 * Searches for optimal CDEF parameters for frame
 *
 * \param[in]      frame        Compressed frame buffer
 * \param[in]      ref          Source frame buffer
 * \param[in,out]  cm           Pointer to top level common structure
 * \param[in]      xd           Pointer to common current coding block structure
 * \param[in]      pick_method  The method used to select params
 * \param[in]      rdmult       rd multiplier to use in making param choices
 *
 * \return Nothing is returned. Instead, optimal CDEF parameters are stored
 * in the \c cdef_info structure of type \ref CdefInfo inside \c cm:
 * \arg \c cdef_bits: Bits of strength parameters
 * \arg \c nb_cdef_strengths: Number of strength parameters
 * \arg \c cdef_strengths: list of \c nb_cdef_strengths strength parameters
 * for the luma plane.
 * \arg \c uv_cdef_strengths: list of \c nb_cdef_strengths strength parameters
 * for the chroma planes.
 * \arg \c damping_factor: CDEF damping factor.
 *
 */
void av1_cdef_search(const YV12_BUFFER_CONFIG *frame,
                     const YV12_BUFFER_CONFIG *ref, AV1_COMMON *cm,
                     MACROBLOCKD *xd, CDEF_PICK_METHOD pick_method, int rdmult);

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AOM_AV1_ENCODER_PICKCDEF_H_
