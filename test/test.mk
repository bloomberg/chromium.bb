LIBAOM_TEST_SRCS-yes += acm_random.h
LIBAOM_TEST_SRCS-yes += clear_system_state.h
LIBAOM_TEST_SRCS-yes += codec_factory.h
LIBAOM_TEST_SRCS-yes += md5_helper.h
LIBAOM_TEST_SRCS-yes += register_state_check.h
LIBAOM_TEST_SRCS-yes += test.mk
LIBAOM_TEST_SRCS-yes += test_libaom.cc
LIBAOM_TEST_SRCS-yes += util.h
LIBAOM_TEST_SRCS-yes += video_source.h

##
## BLACK BOX TESTS
##
## Black box tests only use the public API.
##
LIBAOM_TEST_SRCS-yes                   += ../md5_utils.h ../md5_utils.c
LIBAOM_TEST_SRCS-$(CONFIG_DECODERS)    += ivf_video_source.h
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += ../y4minput.h ../y4minput.c
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += aq_segment_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += datarate_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += encode_api_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += error_resilience_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += i420_video_source.h
##TODO(jimbankoski): Figure out why resize is failing.
##LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += resize_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += y4m_video_source.h
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += yuv_video_source.h

LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += active_map_refresh_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += active_map_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += borders_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += cpu_speed_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += frame_size_tests.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += lossless_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += end_to_end_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += ethread_test.cc

LIBAOM_TEST_SRCS-yes                   += decode_test_driver.cc
LIBAOM_TEST_SRCS-yes                   += decode_test_driver.h
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += encode_test_driver.cc
LIBAOM_TEST_SRCS-yes                   += encode_test_driver.h

## IVF writing.
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += ../ivfenc.c ../ivfenc.h

## Y4m parsing.
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS)    += y4m_test.cc ../y4menc.c ../y4menc.h

## WebM Parsing
ifeq ($(CONFIG_WEBM_IO), yes)
LIBWEBM_PARSER_SRCS                    += ../third_party/libwebm/mkvparser.cpp
LIBWEBM_PARSER_SRCS                    += ../third_party/libwebm/mkvreader.cpp
LIBWEBM_PARSER_SRCS                    += ../third_party/libwebm/mkvparser.hpp
LIBWEBM_PARSER_SRCS                    += ../third_party/libwebm/mkvreader.hpp
LIBAOM_TEST_SRCS-$(CONFIG_DECODERS)    += $(LIBWEBM_PARSER_SRCS)
LIBAOM_TEST_SRCS-$(CONFIG_DECODERS)    += ../tools_common.h
LIBAOM_TEST_SRCS-$(CONFIG_DECODERS)    += ../webmdec.cc
LIBAOM_TEST_SRCS-$(CONFIG_DECODERS)    += ../webmdec.h
LIBAOM_TEST_SRCS-$(CONFIG_DECODERS)    += webm_video_source.h
endif

LIBAOM_TEST_SRCS-$(CONFIG_DECODERS)    += decode_api_test.cc

# Currently we only support decoder perf tests for av1. Also they read from WebM
# files, so WebM IO is required.
ifeq ($(CONFIG_DECODE_PERF_TESTS)$(CONFIG_AV1_DECODER)$(CONFIG_WEBM_IO), \
      yesyesyes)
LIBAOM_TEST_SRCS-yes                   += decode_perf_test.cc
endif

# encode perf tests are av1 only
ifeq ($(CONFIG_ENCODE_PERF_TESTS)$(CONFIG_AV1_ENCODER), yesyes)
LIBAOM_TEST_SRCS-yes += encode_perf_test.cc
endif

##
## WHITE BOX TESTS
##
## Whitebox tests invoke functions not exposed via the public API. Certain
## shared library builds don't make these functions accessible.
##
ifeq ($(CONFIG_SHARED),)

## AV1
ifeq ($(CONFIG_AV1),yes)

# These tests require both the encoder and decoder to be built.
ifeq ($(CONFIG_AV1_ENCODER)$(CONFIG_AV1_DECODER),yesyes)
# IDCT test currently depends on FDCT function
LIBAOM_TEST_SRCS-yes                   += idct8x8_test.cc
LIBAOM_TEST_SRCS-yes                   += partial_idct_test.cc
LIBAOM_TEST_SRCS-yes                   += superframe_test.cc
LIBAOM_TEST_SRCS-yes                   += tile_independence_test.cc
LIBAOM_TEST_SRCS-yes                   += boolcoder_test.cc
LIBAOM_TEST_SRCS-yes                   += divu_small_test.cc
LIBAOM_TEST_SRCS-yes                   += encoder_parms_get_to_decoder.cc
endif

LIBAOM_TEST_SRCS-yes                   += convolve_test.cc
LIBAOM_TEST_SRCS-yes                   += lpf_8_test.cc
LIBAOM_TEST_SRCS-yes                   += intrapred_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += dct16x16_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += dct32x32_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += fdct4x4_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += fdct8x8_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += variance_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += quantize_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += subtract_test.cc

ifeq ($(CONFIG_AV1_ENCODER)$(CONFIG_AV1_TEMPORAL_DENOISING),yesyes)
LIBAOM_TEST_SRCS-$(HAVE_SSE2) += denoiser_sse2_test.cc
endif
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += arf_freq_test.cc

LIBAOM_TEST_SRCS-yes                    += av1_inv_txfm_test.cc
LIBAOM_TEST_SRCS-$(CONFIG_AV1_ENCODER) += av1_dct_test.cc

endif # AV1

## Multi-codec / unconditional whitebox tests.

ifeq ($(findstring yes,$(CONFIG_AV1_ENCODER)$(CONFIG_AV1_ENCODER)),yes)
LIBAOM_TEST_SRCS-yes += avg_test.cc
endif
ifeq ($(CONFIG_INTERNAL_STATS),yes)
LIBAOM_TEST_SRCS-$(CONFIG_AOM_HIGHBITDEPTH) += hbd_metrics_test.cc
endif
LIBAOM_TEST_SRCS-$(CONFIG_ENCODERS) += sad_test.cc

TEST_INTRA_PRED_SPEED_SRCS-yes := test_intra_pred_speed.cc
TEST_INTRA_PRED_SPEED_SRCS-yes += ../md5_utils.h ../md5_utils.c

endif # CONFIG_SHARED

include $(SRC_PATH_BARE)/test/test-data.mk
