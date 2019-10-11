if(LIBGAV1_SRC_DSP_LIBGAV1_DSP_CMAKE_)
  return()
endif() # LIBGAV1_SRC_DSP_LIBGAV1_DSP_CMAKE_
set(LIBGAV1_SRC_DSP_LIBGAV1_DSP_CMAKE_ 1)

include("${libgav1_root}/cmake/libgav1_targets.cmake")

list(APPEND libgav1_dsp_sources
            "${libgav1_source}/dsp/average_blend.cc"
            "${libgav1_source}/dsp/average_blend.h"
            "${libgav1_source}/dsp/cdef.cc"
            "${libgav1_source}/dsp/cdef.h"
            "${libgav1_source}/dsp/common.h"
            "${libgav1_source}/dsp/constants.cc"
            "${libgav1_source}/dsp/constants.h"
            "${libgav1_source}/dsp/convolve.cc"
            "${libgav1_source}/dsp/convolve.h"
            "${libgav1_source}/dsp/distance_weighted_blend.cc"
            "${libgav1_source}/dsp/distance_weighted_blend.h"
            "${libgav1_source}/dsp/dsp.cc"
            "${libgav1_source}/dsp/dsp.h"
            "${libgav1_source}/dsp/film_grain.cc"
            "${libgav1_source}/dsp/film_grain.h"
            "${libgav1_source}/dsp/intra_edge.cc"
            "${libgav1_source}/dsp/intra_edge.h"
            "${libgav1_source}/dsp/intrapred.cc"
            "${libgav1_source}/dsp/intrapred.h"
            "${libgav1_source}/dsp/inverse_transform.cc"
            "${libgav1_source}/dsp/inverse_transform.h"
            "${libgav1_source}/dsp/loop_filter.cc"
            "${libgav1_source}/dsp/loop_filter.h"
            "${libgav1_source}/dsp/loop_restoration.cc"
            "${libgav1_source}/dsp/loop_restoration.h"
            "${libgav1_source}/dsp/mask_blend.cc"
            "${libgav1_source}/dsp/mask_blend.h"
            "${libgav1_source}/dsp/obmc.cc"
            "${libgav1_source}/dsp/obmc.h"
            "${libgav1_source}/dsp/warp.cc"
            "${libgav1_source}/dsp/warp.h")

list(APPEND libgav1_dsp_sources_neon
            ${libgav1_dsp_sources_neon}
            "${libgav1_source}/dsp/arm/average_blend_neon.cc"
            "${libgav1_source}/dsp/arm/average_blend_neon.h"
            "${libgav1_source}/dsp/arm/cdef_neon.cc"
            "${libgav1_source}/dsp/arm/cdef_neon.h"
            "${libgav1_source}/dsp/arm/common_neon.h"
            "${libgav1_source}/dsp/arm/convolve_neon.cc"
            "${libgav1_source}/dsp/arm/convolve_neon.h"
            "${libgav1_source}/dsp/arm/distance_weighted_blend_neon.cc"
            "${libgav1_source}/dsp/arm/distance_weighted_blend_neon.h"
            "${libgav1_source}/dsp/arm/film_grain_neon.cc"
            "${libgav1_source}/dsp/arm/film_grain_neon.h"
            "${libgav1_source}/dsp/arm/intra_edge_neon.cc"
            "${libgav1_source}/dsp/arm/intra_edge_neon.h"
            "${libgav1_source}/dsp/arm/intrapred_cfl_neon.cc"
            "${libgav1_source}/dsp/arm/intrapred_directional_neon.cc"
            "${libgav1_source}/dsp/arm/intrapred_filter_intra_neon.cc"
            "${libgav1_source}/dsp/arm/intrapred_neon.cc"
            "${libgav1_source}/dsp/arm/intrapred_neon.h"
            "${libgav1_source}/dsp/arm/intrapred_smooth_neon.cc"
            "${libgav1_source}/dsp/arm/inverse_transform_neon.cc"
            "${libgav1_source}/dsp/arm/inverse_transform_neon.h"
            "${libgav1_source}/dsp/arm/loop_filter_neon.cc"
            "${libgav1_source}/dsp/arm/loop_filter_neon.h"
            "${libgav1_source}/dsp/arm/loop_restoration_neon.cc"
            "${libgav1_source}/dsp/arm/loop_restoration_neon.h"
            "${libgav1_source}/dsp/arm/mask_blend_neon.cc"
            "${libgav1_source}/dsp/arm/mask_blend_neon.h"
            "${libgav1_source}/dsp/arm/obmc_neon.cc"
            "${libgav1_source}/dsp/arm/obmc_neon.h"
            "${libgav1_source}/dsp/arm/warp_neon.cc"
            "${libgav1_source}/dsp/arm/warp_neon.h")

list(APPEND libgav1_dsp_sources_sse4
            ${libgav1_dsp_sources_sse4}
            "${libgav1_source}/dsp/x86/average_blend_sse4.cc"
            "${libgav1_source}/dsp/x86/average_blend_sse4.h"
            "${libgav1_source}/dsp/x86/common_sse4.h"
            "${libgav1_source}/dsp/x86/convolve_sse4.cc"
            "${libgav1_source}/dsp/x86/convolve_sse4.h"
            "${libgav1_source}/dsp/x86/distance_weighted_blend_sse4.cc"
            "${libgav1_source}/dsp/x86/distance_weighted_blend_sse4.h"
            "${libgav1_source}/dsp/x86/intra_edge_sse4.cc"
            "${libgav1_source}/dsp/x86/intra_edge_sse4.h"
            "${libgav1_source}/dsp/x86/intrapred_sse4.cc"
            "${libgav1_source}/dsp/x86/intrapred_sse4.h"
            "${libgav1_source}/dsp/x86/intrapred_cfl_sse4.cc"
            "${libgav1_source}/dsp/x86/intrapred_smooth_sse4.cc"
            "${libgav1_source}/dsp/x86/inverse_transform_sse4.cc"
            "${libgav1_source}/dsp/x86/inverse_transform_sse4.h"
            "${libgav1_source}/dsp/x86/loop_filter_sse4.cc"
            "${libgav1_source}/dsp/x86/loop_filter_sse4.h"
            "${libgav1_source}/dsp/x86/loop_restoration_sse4.cc"
            "${libgav1_source}/dsp/x86/loop_restoration_sse4.h"
            "${libgav1_source}/dsp/x86/obmc_sse4.cc"
            "${libgav1_source}/dsp/x86/obmc_sse4.h"
            "${libgav1_source}/dsp/x86/transpose_sse4.h")

macro(libgav1_add_dsp_targets)
  unset(dsp_sources)
  list(APPEND dsp_sources ${libgav1_dsp_sources} ${libgav1_dsp_sources_neon}
              ${libgav1_dsp_sources_sse4})

  libgav1_add_library(NAME
                      libgav1_dsp
                      TYPE
                      OBJECT
                      SOURCES
                      ${dsp_sources}
                      DEFINES
                      ${libgav1_defines}
                      $<$<CONFIG:Debug>:LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS>
                      INCLUDES
                      ${libgav1_include_paths})
endmacro()
