/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_VIDEO_CAPTURE_DEV_H_
#define PPAPI_C_DEV_PPB_VIDEO_CAPTURE_DEV_H_

#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_VIDEO_CAPTURE_DEV_INTERFACE_0_1 "PPB_VideoCapture(Dev);0.1"
#define PPB_VIDEO_CAPTURE_DEV_INTERFACE PPB_VIDEO_CAPTURE_DEV_INTERFACE_0_1

/**
 * Video capture interface. It goes hand-in-hand with PPP_VideoCapture_Dev.
 *
 * Theory of operation:
 * 1- Create a VideoCapture resource using Create.
 * 2- Start the capture using StartCapture. You pass in the requested info
 * (resolution, frame rate), as well as suggest a number of buffers you will
 * need.
 * 3- Receive the OnDeviceInfo callback, in PPP_VideoCapture_Dev, which will
 * give you the actual capture info (the requested one is not guaranteed), as
 * well as an array of buffers allocated by the browser.
 * 4- On every frame captured by the browser, OnBufferReady (in
 * PPP_VideoCapture_Dev) is called with the index of the buffer from the array
 * containing the new frame. The buffer is now "owned" by the plugin, and the
 * browser won't reuse it until ReuseBuffer is called.
 * 5- When the plugin is done with the buffer, call ReuseBuffer
 * 6- Stop the capture using StopCapture.
 *
 * The browser may change the resolution based on the constraints of the system,
 * in which case OnDeviceInfo will be called again, with new buffers.
 *
 * The buffers contain the pixel data for a frame. The format is planar YUV
 * 4:2:0, one byte per pixel, tightly packed (width x height Y values, then
 * width/2 x height/2 U values, then width/2 x height/2 V values).
 */
struct PPB_VideoCapture_Dev {
  /**
   * Creates a new VideoCapture.
   */
  PP_Resource (*Create)(PP_Instance instance);

  /**
   * Returns PP_TRUE if the given resource is a VideoCapture.
   */
  PP_Bool (*IsVideoCapture)(PP_Resource video_capture);

  /**
   * Starts the capture. |requested_info| is a pointer to a structure containing
   * the requested resolution and frame rate. |buffer_count| is the number of
   * buffers requested by the plugin. Note: it is only used as advisory, the
   * browser may allocate more of fewer based on available resources.
   * How many buffers depends on usage. At least 2 to make sure latency doesn't
   * cause lost frames. If the plugin expects to hold on to more than one buffer
   * at a time (e.g. to do multi-frame processing, like video encoding), it
   * should request that many more.
   *
   * Returns PP_ERROR_FAILED if called when the capture was already started, or
   * PP_OK on success.
   */
  int32_t (*StartCapture)(
      PP_Resource video_capture,
      const struct PP_VideoCaptureDeviceInfo_Dev* requested_info,
      uint32_t buffer_count);

  /**
   * Allows the browser to reuse a buffer that was previously sent by
   * PPP_VideoCapture_Dev.OnBufferReady. |buffer| is the index of the buffer in
   * the array returned by PPP_VideoCapture_Dev.OnDeviceInfo.
   *
   * Returns PP_ERROR_BADARGUMENT if buffer is out of range (greater than the
   * number of buffers returned by PPP_VideoCapture_Dev.OnDeviceInfo), or if it
   * is not currently owned by the plugin. Returns PP_OK otherwise.
   */
  int32_t (*ReuseBuffer)(PP_Resource video_capture, uint32_t buffer);

  /**
   * Stops the capture.
   *
   * Returns PP_ERROR_FAILED if the capture wasn't already started, or PP_OK on
   * success.
   */
  int32_t (*StopCapture)(PP_Resource video_capture);
};

#endif  /* PPAPI_C_DEV_PPB_VIDEO_CAPTURE_DEV_H_ */
