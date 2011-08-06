/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PP_VIDEO_CAPTURE_DEV_H_
#define PPAPI_C_DEV_PP_VIDEO_CAPTURE_DEV_H_

#include "ppapi/c/pp_stdint.h"

/**
 * PP_VideoCaptureDeviceInfo_Dev is a structure that represent a video capture
 * configuration, such as resolution and frame rate.
 */
struct PP_VideoCaptureDeviceInfo_Dev {
  uint32_t width;
  uint32_t height;
  uint32_t frames_per_second;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_VideoCaptureDeviceInfo_Dev, 12);

/**
 * PP_VideoCaptureStatus_Dev is an enumeration that defines the various possible
 * states of a VideoCapture.
 */
typedef enum {
  /**
   * Initial state, capture is stopped.
   */
  PP_VIDEO_CAPTURE_STATUS_STOPPED,
  /**
   * StartCapture has been called, but capture hasn't started yet.
   */
  PP_VIDEO_CAPTURE_STATUS_STARTING,
  /**
   * Capture is started.
   */
  PP_VIDEO_CAPTURE_STATUS_STARTED,
  /**
   * Capture has been started, but is paused because no buffer is available.
   */
  PP_VIDEO_CAPTURE_STATUS_PAUSED,
  /**
   * StopCapture has been called, but capture hasn't stopped yet.
   */
  PP_VIDEO_CAPTURE_STATUS_STOPPING
} PP_VideoCaptureStatus_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_VideoCaptureStatus_Dev, 4);

#endif  /* PPAPI_C_DEV_PP_VIDEO_CAPTURE_DEV_H_ */
