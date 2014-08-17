/* Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_CONFIG_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_CONFIG_PRIVATE_H_

#include "ppapi/c/private/ppb_image_capture_config_private.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"

/// @file
/// This file defines the ImageCaptureConfig_Private interface for
/// establishing an image capture configuration resource within the browser.
namespace pp {

/// The <code>ImageCaptureConfig_Private</code> interface contains methods for
/// establishing image capture configuration within the browser. The new
/// configuration will take effect after <code>
/// ImageCaptureConfig_Private.SetConfig</code> is called.
class ImageCaptureConfig_Private {
 public:
  /// Default constructor for creating an is_null()
  /// <code>ImageCaptureConfig_Private</code> object.
  ImageCaptureConfig_Private();

  /// The copy constructor for <code>ImageCaptureConfig_Private</code>.
  ///
  /// @param[in] other A reference to a <code>ImageCaptureConfig_Private
  /// </code>.
  ImageCaptureConfig_Private(const ImageCaptureConfig_Private& other);

  /// Constructs a <code>ImageCaptureConfig_Private</code> from a <code>
  /// Resource</code>.
  ///
  /// @param[in] resource A <code>PPB_ImageCaptureConfig_Private</code>
  /// resource.
  explicit ImageCaptureConfig_Private(const Resource& resource);

  /// Constructs a <code>ImageCaptureConfig_Private</code> object.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit ImageCaptureConfig_Private(const InstanceHandle& instance);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_ImageCaptureConfig_Private</code>
  /// resource.
  ImageCaptureConfig_Private(PassRef, PP_Resource resource);

  // Destructor.
  ~ImageCaptureConfig_Private();

  /// GetPreviewSize() returns the preview image size in pixels for the given
  /// <code>ImageCaptureConfig_Private</code>.
  ///
  /// @param[out] preview_size A <code>Size</code> that indicates the
  /// requested preview image size.
  void GetPreviewSize(Size* preview_size);

  /// SetPreviewSize() sets the preview image size for the given <code>
  /// ImageCaptureConfig_Private</code>.
  ///
  /// @param[in] preview_size A <code>Size</code> that indicates the
  /// requested preview image size.
  void SetPreviewSize(const Size& preview_size);

  /// GetJpegSize() returns the JPEG image size in pixels for the given
  /// <code>ImageCaptureConfig_Private</code>.
  ///
  /// @param[out] jpeg_size A <code>Size</code> that indicates the current
  /// JPEG image size.
  void GetJpegSize(Size* jpeg_size);

  /// SetJpegSize() sets the JPEG image size for the given <code>
  /// ImageCaptureConfig_Private</code>.
  ///
  /// @param[in] jpeg_size A <code>Size</code> that indicates the requested
  /// JPEG image size.
  void SetJpegSize(const Size& jpeg_size);

  /// IsImageCaptureConfig() determines if the given resource is a
  /// <code>ImageCaptureConfig_Private</code>.
  ///
  /// @param[in] resource A <code>Resource</code> corresponding to an image
  /// capture config resource.
  ///
  /// @return true if the given resource is an <code>
  /// ImageCaptureConfig_Private</code> resource, otherwise false.
  static bool IsImageCaptureConfig(const Resource& resource);
};

} // namespace pp

#endif  /* PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_CONFIG_PRIVATE_H_ */

