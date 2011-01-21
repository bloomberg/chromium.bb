/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_TESTING_DEV_H_
#define PPAPI_C_DEV_PPB_TESTING_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

struct PP_Point;

#define PPB_TESTING_DEV_INTERFACE "PPB_Testing(Dev);0.5"

// This interface contains functions used for unit testing. Do not use in
// production code. They are not guaranteed to be available in normal plugin
// environments so you should not depend on them.
struct PPB_Testing_Dev {
  // Reads the bitmap data out of the backing store for the given
  // DeviceContext2D and into the given image. If the data was successfully
  // read, it will return PP_TRUE.
  //
  // This function should not generally be necessary for normal plugin
  // operation. If you want to update portions of a device, the expectation is
  // that you will either regenerate the data, or maintain a backing store
  // pushing updates to the device from your backing store via PaintImageData.
  // Using this function will introduce an extra copy which will make your
  // plugin slower. In some cases, this may be a very expensive operation (it
  // may require slow cross-process transitions or graphics card readbacks).
  //
  // Data will be read into the image starting at |top_left| in the device
  // context, and proceeding down and to the right for as many pixels as the
  // image is large. If any part of the image bound would fall outside of the
  // backing store of the device if positioned at |top_left|, this function
  // will fail and return PP_FALSE.
  //
  // The image format must be of the format
  // PPB_ImageData.GetNativeImageDataFormat() or this function will fail and
  // return PP_FALSE.
  //
  // The returned image data will represent the current status of the backing
  // store. This will not include any paint, scroll, or replace operations
  // that have not yet been flushed; these operations are only reflected in
  // the backing store (and hence ReadImageData) until after a Flush()
  // operation has completed.
  PP_Bool (*ReadImageData)(PP_Resource device_context_2d,
                           PP_Resource image,
                           const struct PP_Point* top_left);

  // Runs a nested message loop. The plugin will be reentered from this call.
  // This function is used for unit testing the API. The normal pattern is to
  // issue some asynchronous call that has a callback. Then you call
  // RunMessageLoop which will suspend the plugin and go back to processing
  // messages, giving the asynchronous operation time to complete. In your
  // callback, you save the data and call QuitMessageLoop, which will then
  // pop back up and continue with the test. This avoids having to write a
  // complicated state machine for simple tests for asynchronous APIs.
  void (*RunMessageLoop)();

  // Posts a quit message for the outermost nested message loop. Use this to
  // exit and return back to the caller after you call RunMessageLoop.
  void (*QuitMessageLoop)();

  // Returns the number of live objects (resources + strings + objects)
  // associated with this plugin instance. Used for detecting leaks. Returns
  // (uint32_t)-1 on failure.
  uint32_t (*GetLiveObjectsForInstance)(PP_Instance instance);
};

#endif  /* PPAPI_C_DEV_PPB_TESTING_DEV_H_ */

