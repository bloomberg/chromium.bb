/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_video_reader.idl modified Thu Apr  4 13:47:30 2013. */

#ifndef PPAPI_C_PPB_VIDEO_READER_H_
#define PPAPI_C_PPB_VIDEO_READER_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/pp_video_frame.h"

#define PPB_VIDEOREADER_INTERFACE_0_1 "PPB_VideoReader;0.1"
#define PPB_VIDEOREADER_INTERFACE PPB_VIDEOREADER_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_VideoReader</code> struct for a video reader
 * resource.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_VideoReader</code> interface contains pointers to several
 * functions for creating video reader resources and using them to consume
 * streams of video frames.
 */
struct PPB_VideoReader_0_1 {
  /**
   * Creates a video reader resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   *
   * @return A <code>PP_Resource</code> with a nonzero ID on success or zero on
   * failure. Failure means the instance was invalid.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a given resource is a video reader.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a resource.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is a video reader or <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*IsVideoReader)(PP_Resource resource);
  /**
   * Opens a video stream with the given id for reading.
   *
   * @param[in] reader A <code>PP_Resource</code> corresponding to a video
   * reader resource.
   * @param[in] stream_id A <code>PP_Var</code> holding a string uniquely
   * identifying the stream. This string is application defined.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Open().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_BADRESOURCE if reader isn't a valid video reader.
   * Returns PP_ERROR_INPROGRESS if the reader has already opened a stream.
   */
  int32_t (*Open)(PP_Resource reader,
                  struct PP_Var stream_id,
                  struct PP_CompletionCallback callback);
  /**
   * Gets the next frame of video from the reader's open stream. The image data
   * resource returned in the frame will have its reference count incremented by
   * one and must be managed by the plugin.
   *
   * @param[in] reader A <code>PP_Resource</code> corresponding to a video
   * reader resource.
   * @param[out] frame A <code>PP_VideoFrame</code> to hold a video frame to
   * read from the open stream.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of GetNextFrame().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_BADRESOURCE if reader isn't a valid video reader.
   * Returns PP_ERROR_FAILED if the reader has not opened a stream, if the video
   * frame has an invalid image data resource, or if some other error occurs.
   */
  int32_t (*GetFrame)(PP_Resource reader,
                      struct PP_VideoFrame* frame,
                      struct PP_CompletionCallback callback);
  /**
   * Closes the reader's video stream.
   *
   * @param[in] reader A <code>PP_Resource</code> corresponding to a video
   * reader resource.
   */
  void (*Close)(PP_Resource reader);
};

typedef struct PPB_VideoReader_0_1 PPB_VideoReader;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VIDEO_READER_H_ */

