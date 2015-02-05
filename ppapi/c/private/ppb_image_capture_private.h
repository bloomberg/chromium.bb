/* Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_image_capture_private.idl,
 *   modified Thu Feb  5 22:47:43 2015.
 */

#ifndef PPAPI_C_PRIVATE_PPB_IMAGE_CAPTURE_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_IMAGE_CAPTURE_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_IMAGECAPTURE_PRIVATE_INTERFACE_0_1 "PPB_ImageCapture_Private;0.1"
#define PPB_IMAGECAPTURE_PRIVATE_INTERFACE \
    PPB_IMAGECAPTURE_PRIVATE_INTERFACE_0_1

/**
 * @file
 * Defines the <code>PPB_ImageCapture_Private</code> interface. Used for
 * acquiring a single still image from a camera source.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * To query camera capabilities:
 * 1. Get a PPB_ImageCapture_Private object by Create().
 * 2. Open() camera device with track id of MediaStream video track.
 * 3. Call GetCameraCapabilities() to get a
 *    <code>PPB_CameraCapabilities_Private</code> object, which can be used to
 *    query camera capabilities.
 */
struct PPB_ImageCapture_Private_0_1 {
  /**
   * Creates a PPB_ImageCapture_Private resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] camera_source_id A <code>PP_Var</code> identifying a camera
   * source. The type is string. The ID can be obtained from
   * MediaStreamTrack.getSources() or MediaStreamVideoTrack.id. If a
   * MediaStreamVideoTrack is associated with the same source and the track
   * is closed, this PPB_ImageCapture_Private object can still do image capture.
   * @param[in] error_callback A <code>PPB_ImageCapture_Private_ErrorCallback
   * </code> callback to indicate the image capture has failed.
   * @param[inout] user_data An opaque pointer that will be passed to the
   * callbacks of PPB_ImageCapture_Private.
   *
   * @return A <code>PP_Resource</code> corresponding to a
   * PPB_ImageCapture_Private resource if successful, 0 if failed.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        struct PP_Var camera_source_id,
                        void* user_data);
  /**
   * Determines if a resource is an image capture resource.
   *
   * @param[in] resource The <code>PP_Resource</code> to test.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is an image capture resource or <code>PP_FALSE</code>
   * otherwise.
   */
  PP_Bool (*IsImageCapture)(PP_Resource resource);
  /**
   * Disconnects from the camera and cancels all pending capture requests.
   * After this returns, no callbacks will be called. If <code>
   * PPB_ImageCapture_Private</code> is destroyed and is not closed yet, this
   * function will be automatically called. Calling this more than once has no
   * effect.
   *
   * @param[in] image_capture A <code>PP_Resource</code> corresponding to an
   * image capture resource.
   * @param[in] callback <code>PP_CompletionCallback</code> to be called upon
   * completion of <code>Close()</code>.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*Close)(PP_Resource resource, struct PP_CompletionCallback callback);
  /**
   * Gets the camera capabilities.
   *
   * The camera capabilities do not change for a given camera source.
   *
   * @param[in] image_capture A <code>PP_Resource</code> corresponding to an
   * image capture resource.
   * @param[out] capabilities A <code>PPB_CameraCapabilities_Private</code> for
   * storing the image capture capabilities on success. Otherwise, the value
   * will not be changed.
   * @param[in] callback <code>PP_CompletionCallback</code> to be called upon
   * completion of <code>GetCameraCapabilities()</code>.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*GetCameraCapabilities)(PP_Resource image_capture,
                                   PP_Resource* capabilities,
                                   struct PP_CompletionCallback callback);
};

typedef struct PPB_ImageCapture_Private_0_1 PPB_ImageCapture_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_IMAGE_CAPTURE_PRIVATE_H_ */

