/* Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_PRIVATE_H_

#include "ppapi/c/private/ppb_image_capture_private.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// Defines the <code>ImageCapture_Private</code> interface. Used for
/// acquiring a single still image from a camera source.
namespace pp {

class CameraCapabilities_Private;
class CompletionCallback;
class InstanceHandle;

template <typename T>
class CompletionCallbackWithOutput;

/// To query camera capabilities:
/// 1. Create an ImageCapture_Private object.
/// 2. Open() camera device with track id of MediaStream video track.
/// 3. Call GetCameraCapabilities() to get a
///    <code>CameraCapabilities_Private</code> object, which can be used to
///    query camera capabilities.
class ImageCapture_Private : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>ImageCapture_Private</code> object.
  ImageCapture_Private();

  /// The copy constructor for <code>ImageCapture_Private</code>.
  ///
  /// @param[in] other A reference to a <code>ImageCapture_Private</code>.
  ImageCapture_Private(const ImageCapture_Private& other);

  /// Constructs an <code>ImageCapture_Private</code> from
  /// a <code>Resource</code>.
  ///
  /// @param[in] resource A <code>PPB_ImageCapture_Private</code> resource.
  explicit ImageCapture_Private(const Resource& resource);

  /// Constructs an ImageCapture_Private resource.
  ///
  /// @param[in] instance A <code>PP_Instance</code> identifying one instance
  /// of a module.
  explicit ImageCapture_Private(const InstanceHandle& instance);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_ImageCapture_Private</code> resource.
  ImageCapture_Private(PassRef, PP_Resource resource);

  // Destructor.
  ~ImageCapture_Private();

  /// Opens a video capture device.
  ///
  /// @param[in] device_id A <code>Var</code> identifying a camera
  /// device. The type is string. The ID can be obtained from
  /// MediaStreamTrack.getSources() or MediaStreamVideoTrack.id.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion of <code>Open()</code>.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t Open(const Var& device_id, const CompletionCallback& callback);

  /// Disconnects from the camera and cancels all pending capture requests.
  /// After this returns, no callbacks will be called. If <code>
  /// ImageCapture_Private</code> is destroyed and is not closed yet, this
  /// function will be automatically called. Calling this more than once has no
  /// effect.
  void Close();

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
