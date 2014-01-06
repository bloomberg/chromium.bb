/* Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_media_stream_video_track.idl modified Fri Dec 27 17:28:11 2013. */

#ifndef PPAPI_C_PPB_MEDIA_STREAM_VIDEO_TRACK_H_
#define PPAPI_C_PPB_MEDIA_STREAM_VIDEO_TRACK_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_MEDIASTREAMVIDEOTRACK_INTERFACE_0_1 \
    "PPB_MediaStreamVideoTrack;0.1" /* dev */
/**
 * @file
 * Defines the <code>PPB_MediaStreamVideoTrack</code> interface. Used for
 * receiving video frames from a MediaStream video track in the browser.
 * This interface is still in development (Dev API status) and may change.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 */
struct PPB_MediaStreamVideoTrack_0_1 { /* dev */
  /**
   * Determines if a resource is a MediaStream video track resource.
   *
   * @param[in] resource The <code>PP_Resource</code> to test.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * resource is a Mediastream video track resource or <code>PP_FALSE</code>
   * otherwise.
   */
  PP_Bool (*IsMediaStreamVideoTrack)(PP_Resource resource);
  /**
   * Configures underlying frame buffers for incoming frames.
   * If the application doesn't want to drop frames, then the
   * <code>max_buffered_frames</code> should be chosen such that inter-frame
   * processing time variability won't overrun the input buffer. If the buffer
   * is overfilled, then frames will be dropped. The application can detect
   * this by examining the timestamp on returned frames.
   * If <code>Configure()</code> is not used, default settings will be used.
   *
   * @param[in] video_track A <code>PP_Resource</code> corresponding to a video
   * resource.
   * @param[in] max_buffered_frames The maximum number of video frames to
   * hold in the input buffer.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*Configure)(PP_Resource video_track, uint32_t max_buffered_frames);
  /**
   * Returns the track ID of the underlying MediaStream video track.
   *
   * @param[in] video_track The <code>PP_Resource</code> to check.
   *
   * @return A <code>PP_Var</code> containing the MediaStream track ID as
   * a string.
   */
  struct PP_Var (*GetId)(PP_Resource video_track);
  /**
   * Checks whether the underlying MediaStream track has ended.
   * Calls to GetFrame while the track has ended are safe to make and will
   * complete, but will fail.
   *
   * @param[in] video_track The <code>PP_Resource</code> to check.
   *
   * @return A <code>PP_Bool</code> with <code>PP_TRUE</code> if the given
   * MediaStream track has ended or <code>PP_FALSE</code> otherwise.
   */
  PP_Bool (*HasEnded)(PP_Resource video_track);
  /**
   * Gets the next video frame from the MediaStream track.
   * If internal processing is slower than the incoming frame rate, new frames
   * will be dropped from the incoming stream. Once the input buffer is full,
   * frames will be dropped until <code>RecycleFrame()</code> is called to free
   * a spot for another frame to be buffered.
   * If there are no frames in the input buffer,
   * <code>PP_OK_COMPLETIONPENDING</code> will be returned immediately and the
   * <code>callback</code> will be called, when a new frame is received or an
   * error happens.
   * If the caller holds a frame returned by the previous call of
   * <code>GetFrame()</code>, <code>PP_ERROR_INPROGRESS</code> will be returned.
   * The caller should recycle the previous frame before getting the next frame.
   *
   * @param[in] video_track A <code>PP_Resource</code> corresponding to a video
   * resource.
   * @param[out] frame A <code>PP_Resource</code> corresponding to a VideoFrame
   * resource.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of GetFrame().
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   * Returns PP_ERROR_NOMEMORY if <code>max_buffered_frames</code> frames buffer
   * was not allocated successfully.
   */
  int32_t (*GetFrame)(PP_Resource video_track,
                      PP_Resource* frame,
                      struct PP_CompletionCallback callback);
  /**
   * Recycles a frame returned by <code>GetFrame()</code>, so the track can
   * reuse the underlying buffer of this frame. And the frame will become
   * invalid. The caller should release all references it holds to
   * <code>frame</code> and not use it anymore.
   *
   * @param[in] video_track A <code>PP_Resource</code> corresponding to a video
   * resource.
   * @param[in] frame A <code>PP_Resource</code> corresponding to a VideoFrame
   * resource returned by <code>GetFrame()</code>.
   *
   * @return An int32_t containing a result code from <code>pp_errors.h</code>.
   */
  int32_t (*RecycleFrame)(PP_Resource video_track, PP_Resource frame);
  /**
   * Closes the MediaStream video track and disconnects it from video source.
   * After calling <code>Close()</code>, no new frames will be received.
   *
   * @param[in] video_track A <code>PP_Resource</code> corresponding to a
   * MediaStream video track resource.
   */
  void (*Close)(PP_Resource video_track);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_MEDIA_STREAM_VIDEO_TRACK_H_ */

