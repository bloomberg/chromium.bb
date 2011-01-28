/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_AUDIO_CONFIG_H_
#define PPAPI_C_PPB_AUDIO_CONFIG_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_AUDIO_CONFIG_INTERFACE "PPB_AudioConfig;0.5"

/**
 *
 * @addtogroup Enums
 * @{
 */
enum {
  PP_AUDIOMINSAMPLEFRAMECOUNT = 64,
  PP_AUDIOMAXSAMPLEFRAMECOUNT = 32768
};
/**
 * @}
 */

/**
 *
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_AUDIOSAMPLERATE_NONE = 0,
  PP_AUDIOSAMPLERATE_44100 = 44100,
  PP_AUDIOSAMPLERATE_48000 = 48000
} PP_AudioSampleRate;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_AudioSampleRate, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */

/**
 * Audio configuration. This base configuration interface supports only stereo
 * 16bit output. This class is not mutable, therefore it is okay to access
 * instances from different threads.
 */
struct PPB_AudioConfig {
  /**
   * Create a 16 bit stereo config with the given sample rate. We guarantee
   * that PP_AUDIOSAMPLERATE_44100 and PP_AUDIOSAMPLERATE_48000 sample rates
   * are supported. The |sample_frame_count| should be the result of calling
   * RecommendSampleFrameCount. If the sample frame count or bit rate aren't
   * supported, this function will fail and return a null resource.
   *
   * A single sample frame on a stereo device means one value for the left
   * channel and one value for the right channel.
   *
   * Buffer layout for a stereo int16 configuration:
   * int16_t *buffer16;
   * buffer16[0] is the first left channel sample
   * buffer16[1] is the first right channel sample
   * buffer16[2] is the second left channel sample
   * buffer16[3] is the second right channel sample
   * ...
   * buffer16[2 * (sample_frame_count - 1)] is the last left channel sample
   * buffer16[2 * (sample_frame_count - 1) + 1] is the last right channel sample
   * Data will always be in the native endian format of the platform.
   */
  PP_Resource (*CreateStereo16Bit)(PP_Instance instance,
                                   PP_AudioSampleRate sample_rate,
                                   uint32_t sample_frame_count);

  /*
   * Returns a supported sample frame count closest to the given requested
   * count. The sample frame count determines the overall latency of audio.
   * Since one "frame" is always buffered in advance, smaller frame counts
   * will yield lower latency, but higher CPU utilization.
   *
   * Supported sample frame counts will vary by hardware and system (consider
   * that the local system might be anywhere from a cell phone or a high-end
   * audio workstation). Sample counts less than PP_AUDIOMINSAMPLEFRAMECOUNT
   * and greater than PP_AUDIOMAXSAMPLEFRAMECOUNT are never supported on any
   * system, but values in between aren't necessarily valid. This function
   * will return a supported count closest to the requested value.
   *
   * If you pass 0 as the requested sample count, the recommended sample for
   * the local system is returned.
   */
  uint32_t (*RecommendSampleFrameCount)(PP_AudioSampleRate sample_rate,
                                        uint32_t requested_sample_frame_count);

  /**
   * Returns true if the given resource is an AudioConfig object.
   */
  PP_Bool (*IsAudioConfig)(PP_Resource resource);

  /**
   * Returns the sample rate for the given AudioConfig resource. If the
   * resource is invalid, this will return PP_AUDIOSAMPLERATE_NONE.
   */
  PP_AudioSampleRate (*GetSampleRate)(PP_Resource config);

  /**
   * Returns the sample frame count for the given AudioConfig resource. If the
   * resource is invalid, this will return 0. See RecommendSampleFrameCount for
   * more on sample frame counts.
   */
  uint32_t (*GetSampleFrameCount)(PP_Resource config);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_AUDIO_CONFIG_H_ */

