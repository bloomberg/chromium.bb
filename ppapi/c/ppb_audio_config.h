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
 * @file
 * This file defines the PPB_AudioConfig interface for establishing an
 * audio configuration resource within the browser. Refer to the
 * <a href="/chrome/nativeclient/docs/audio.html">Pepper Audio API Code
 * Walkthrough</a> for information on using this interface.
 */

/**
 *
 * @addtogroup Enums
 * @{
 */

/**
 * This enumeration contains audio frame count constants.
 * PP_AUDIOMINSAMPLEFRAMECOUNT is the minimum possible frame count.
 * PP_AUDIOMAXSAMPLEFRAMECOUNT is the maximum possible frame count.
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

/**
 * PP_AudioSampleRate is an enumeration of the different audio sampling rates.
 * PP_AUDIOSAMPLERATE_44100 is the sample rate used on CDs and
 * PP_AUDIOSAMPLERATE_48000 is the sample rate used on DVDs and Digital Audio
 * Tapes.
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
 * The PPB_AudioConfig interface contains pointers to several functions for
 * establishing your audio configuration within the browser. This interface
 * only supports stereo * 16bit output. This class is not mutable, therefore
 * it is okay to access instances from different threads.
 */
struct PPB_AudioConfig {
  /**
   * CreateStereo16bit is a pointer to a function that creates a 16 bit audio
   * configuration resource. The |sample_frame_count| should be the result of
   * calling RecommendSampleFrameCount. If the sample frame count or bit rate
   * isn't supported, this function will fail and return a null resource.
   *
   * A single sample frame on a stereo device means one value for the left
   * channel and one value for the right channel.
   *
   * Buffer layout for a stereo int16 configuration:
   * int16_t *buffer16;
   * buffer16[0] is the first left channel sample.
   * buffer16[1] is the first right channel sample.
   * buffer16[2] is the second left channel sample.
   * buffer16[3] is the second right channel sample.
   * ...
   * buffer16[2 * (sample_frame_count - 1)] is the last left channel sample.
   * buffer16[2 * (sample_frame_count - 1) + 1] is the last right channel
   * sample.
   * Data will always be in the native endian format of the platform.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   * @param[in] sample_rate A PP_AudioSampleRate which is either
   * PP_AUDIOSAMPLERATE_44100 or PP_AUDIOSAMPLERATE_48000.
   * @param[in] sample_frame_count A uint32_t frame count returned from the
   * RecommendSampleFrameCount function.
   * @return A PP_Resource containing the PPB_Audio_Config if successful or
   * a null resource if the sample frame count or bit rate are not supported.
   */
  PP_Resource (*CreateStereo16Bit)(PP_Instance instance,
                                   PP_AudioSampleRate sample_rate,
                                   uint32_t sample_frame_count);

  /**
   * RecommendSampleFrameCount is a pointer to a function that returns the
   * supported sample frame count closest to the requested count. The sample
   * frame count determines the overall latency of audio. Since one "frame" is
   * always buffered in advance, smaller frame counts will yield lower latency,
   * but higher CPU utilization.
   *
   * Supported sample frame counts will vary by hardware and system (consider
   * that the local system might be anywhere from a cell phone or a high-end
   * audio workstation). Sample counts less than PP_AUDIOMINSAMPLEFRAMECOUNT
   * and greater than PP_AUDIOMAXSAMPLEFRAMECOUNT are never supported on any
   * system, but values in between aren't necessarily valid. This function
   * will return a supported count closest to the requested value.
   *
   * @param[in] sample_rate A PP_AudioSampleRate which is either
   * PP_AUDIOSAMPLERATE_44100 or PP_AUDIOSAMPLERATE_48000.
   * @param[in] requested_sample_frame_count A uint_32t requested frame count.
   * If you pass 0 as the requested sample count, the recommended sample for
   * the local system is returned.
   * @return A uint32_t containing the recommended sample frame count if
   * successful.
   */
  uint32_t (*RecommendSampleFrameCount)(PP_AudioSampleRate sample_rate,
                                        uint32_t requested_sample_frame_count);

  /**
   * IsAudioConfig is a pointer to a function that determines if the given
   * resource is a PPB_Audio_Config.
   *
   * @param[in] resource A PP_Resource containing the audio config resource.
   * @return A PP_BOOL containing PP_TRUE if the given resource is
   * an AudioConfig resource, otherwise PP_FALSE.
   */
  PP_Bool (*IsAudioConfig)(PP_Resource resource);

  /**
   * GetSampleRate is a pointer to a function that returns the sample
   * rate for the given PPB_Audio_Config.
   *
   * @param[in] config A PP_Resource containing the PPB_Audio_Config.
   * @return A PP_AudioSampleRate containing sample rate or
   * PP_AUDIOSAMPLERATE_NONE if the resource is invalid.
   */
  PP_AudioSampleRate (*GetSampleRate)(PP_Resource config);

  /**
   * GetSampleFrameCount is a pointer to a function that returns the sample
   * frame count for the given PPB_Audio_Config.
   *
   * @param[in] config A PP_Resource containing the audio config resource.
   * @return A uint32_t containing sample frame count or
   * 0 if the resource is invalid. See RecommendSampleFrameCount for
   * more on sample frame counts.
   */
  uint32_t (*GetSampleFrameCount)(PP_Resource config);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_AUDIO_CONFIG_H_ */

