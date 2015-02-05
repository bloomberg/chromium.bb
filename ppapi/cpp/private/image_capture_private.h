/* Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_PRIVATE_H_

#include "ppapi/c/private/ppb_image_capture_private.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/private/camera_capabilities_private.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// Defines the <code>ImageCapture_Private</code> interface. Used for
/// acquiring a single still image from a camera source.
namespace pp {

/// To query camera capabilities:
/// 1. Get a PPB_ImageCapture_Private object by Create().
/// 2. Open() camera device with track id of MediaStream video track.
/// 3. Call GetCameraCapabilities() to get a
///    <code>PPB_CameraCapabilities_Private</code> object, which can be used to
///    query camera capabilities.
class ImageCapture_Private {
 public:
  /// Default constructor for creating an is_null()
  /// <code>ImageCapture_Private</code> object.
  ImageCapture_Private();

  /// Creates an ImageCapture_Private resource.
  ///
  /// @param[in] instance A <code>PP_Instance</code> identifying one instance
  /// of a module.
  /// @param[in] camera_source_id A <code>Var</code> identifying a camera
  /// source. The type is string. The ID can be obtained from
  /// MediaStreamTrack.getSources() or MediaStreamVideoTrack.id. If a
  /// MediaStreamVideoTrack is associated with the same source and the track
  /// is closed, this ImageCapture_Private object can still do image capture.
  /// @param[in] error_callback A <code>ImageCapture_Private_ErrorCallback
  /// </code> callback to indicate the image capture has failed.
  /// @param[inout] user_data An opaque pointer that will be passed to the
  /// callbacks of ImageCapture_Private.
  ImageCapture_Private(const InstanceHandle& instance,
                       const Var& camera_source_id,
                       void* user_data);

  /// Constructs a <code>ImageCapture_Private</code> from a <code>
  /// Resource</code>.
  ///
  /// @param[in] resource A <code>ImageCapture_Private</code>
  /// resource.
  explicit ImageCapture_Private(const Resource& resource);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>ImageCapture_Private</code>
  /// resource.
  ImageCapture_Private(PassRef, PP_Resource resource);

  // Destructor.
  ~ImageCapture_Private();

  /// Disconnects from the camera and cancels all pending capture requests.
  /// After this returns, no callbacks will be called. If <code>
  /// ImageCapture_Private</code> is destroyed and is not closed yet, this
  /// function will be automatically called. Calling this more than once has no
  /// effect.
  ///
  /// @param[in] callback <code>CompletionCallback</code> to be called upon
  /// completion of <code>Close()</code>.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t Close(const CompletionCallback& callback);

  /// Gets the camera capabilities.
  ///
  /// The camera capabilities do not change for a given camera source.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code>
  /// to be called upon completion.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t GetCameraCapabilities(
      const CompletionCallbackWithOutput<CameraCapabilities_Private>& callback);

  /// Determines if a resource is an image capture resource.
  ///
  /// @param[in] resource The <code>Resource</code> to test.
  ///
  /// @return true if the given resource is an image capture resource or false
  /// otherwise.
  static bool IsImageCapture(const Resource& resource);
};

} // namespace pp

#endif  /* PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_PRIVATE_H_ */

