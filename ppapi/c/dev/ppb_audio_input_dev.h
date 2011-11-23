/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_audio_input_dev.idl modified Wed Nov 23 09:26:09 2011. */

#ifndef PPAPI_C_DEV_PPB_AUDIO_INPUT_DEV_H_
#define PPAPI_C_DEV_PPB_AUDIO_INPUT_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_AUDIO_INPUT_DEV_INTERFACE_0_1 "PPB_AudioInput(Dev);0.1"
#define PPB_AUDIO_INPUT_DEV_INTERFACE PPB_AUDIO_INPUT_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_AudioInput_Dev</code> interface, which
 * provides realtime audio input capture.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * <code>PPB_AudioInput_Callback</code> defines the type of an audio callback
 * function used to provide the audio buffer with data. This callback will be
 * called on a separate thread from the creation thread.
 */
typedef void (*PPB_AudioInput_Callback)(const void* sample_buffer,
                                        uint32_t buffer_size_in_bytes,
                                        void* user_data);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_AudioInput_Dev</code> interface contains pointers to several
 * functions for handling audio input resources.
 */
struct PPB_AudioInput_Dev {
  /**
   * Create is a pointer to a function that creates an audio input resource.
   * No sound will be captured until StartCapture() is called.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_Resource config,
                        PPB_AudioInput_Callback audio_input_callback,
                        void* user_data);
  /**
   * IsAudioInput is a pointer to a function that determines if the given
   * resource is an audio input resource.
   *
   * @param[in] resource A PP_Resource containing a resource.
   *
   * @return A PP_BOOL containing containing PP_TRUE if the given resource is
   * an audio input resource, otherwise PP_FALSE.
   */
  PP_Bool (*IsAudioInput)(PP_Resource audio_input);
  /**
   * GetCurrrentConfig() returns an audio config resource for the given audio
   * resource.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * resource.
   *
   * @return A <code>PP_Resource</code> containing the audio config resource if
   * successful.
   */
  PP_Resource (*GetCurrentConfig)(PP_Resource audio_input);
  /**
   * StartCapture() starts the capture of the audio input resource and begins
   * periodically calling the callback.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * input resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if
   * successful, otherwise <code>PP_FALSE</code>. Also returns
   * <code>PP_TRUE</code> (and be a no-op) if called while callback is already
   * in progress.
   */
  PP_Bool (*StartCapture)(PP_Resource audio_input);
  /**
   * StopCapture is a pointer to a function that stops the capture of
   * the audio input resource.
   *
   * @param[in] config A PP_Resource containing the audio input resource.
   *
   * @return A PP_BOOL containing PP_TRUE if successful, otherwise PP_FALSE.
   * Also returns PP_TRUE (and is a no-op) if called while capture is already
   * stopped. If a buffer is being captured, StopCapture will block until the
   * call completes.
   */
  PP_Bool (*StopCapture)(PP_Resource audio_input);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_AUDIO_INPUT_DEV_H_ */

