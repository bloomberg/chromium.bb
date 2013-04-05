/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_video_writer.idl modified Thu Apr  4 13:47:32 2013. */

#ifndef PPAPI_C_PPB_VIDEO_WRITER_H_
#define PPAPI_C_PPB_VIDEO_WRITER_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/pp_video_frame.h"

#define PPB_VIDEOWRITER_INTERFACE_0_1 "PPB_VideoWriter;0.1"
#define PPB_VIDEOWRITER_INTERFACE PPB_VIDEOWRITER_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_VideoWriter</code> struct for a video writer
 * resource.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_VideoWriter</code> interface contains pointers to several
 * functions for creating video writer resources and using them to generate
 * streams of video frames.
 */
struct PPB_VideoWriter_0_1 {
  /**
   * Creates a video writer resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   *
   * @return A <code>PP_Resource</code> with a nonzero ID on success or zero on
   * failure. Failure means the instance was invalid.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a given resource is a video writer.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a resource.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is a video writer or <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*IsVideoWriter)(PP_Resource resource);
  /**
   * Opens a video stream with the given id for writing.
   *
   * @param[in] writer A <code>PP_Resource</code> corresponding to a video
   * writer resource.
   * @param[in] stream_id A <code>PP_Var</code> holding a string uniquely
   * identifying the stream. This string is application defined.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Open().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_BADRESOURCE if writer isn't a valid video writer.
   * Returns PP_ERROR_INPROGRESS if the writer has already opened a stream.
   */
  int32_t (*Open)(PP_Resource writer,
                  struct PP_Var stream_id,
                  struct PP_CompletionCallback callback);
  /**
   * Puts a frame of video to the writer's open stream.
   *
   * After this call, you should take care to release your references to the
   * image embedded in the video frame. If you paint to the image after
   * PutFrame(), there is the possibility of artifacts because the browser may
   * still be copying the frame to the stream.
   *
   * @param[in] writer A <code>PP_Resource</code> corresponding to a video
   * writer resource.
   * @param[in] frame A <code>PP_VideoFrame</code> holding a video frame to
   * write to the open stream.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_BADRESOURCE if writer isn't a valid video writer.
   * Returns PP_ERROR_FAILED if the writer has not opened a stream, if the video
   * frame has an invalid image data resource, or if some other error occurs.
   */
  int32_t (*PutFrame)(PP_Resource writer, const struct PP_VideoFrame* frame);
  /**
   * Closes the writer's video stream.
   *
   * @param[in] writer A <code>PP_Resource</code> corresponding to a video
   * writer resource.
   */
  void (*Close)(PP_Resource writer);
};

typedef struct PPB_VideoWriter_0_1 PPB_VideoWriter;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VIDEO_WRITER_H_ */

