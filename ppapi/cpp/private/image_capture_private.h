/* Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_IMAGE_CAPTURE_PRIVATE_H_

#include "ppapi/c/private/ppb_image_capture_private.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/private/camera_capabilities_private.h"
#include "ppapi/cpp/private/image_capture_config_private.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// Defines the <code>ImageCapture_Private</code> interface. Used for
/// acquiring a single still image from a camera source.
namespace pp {

/// To capture a still image with this class, use the following steps.
/// 1. Create an ImageCapture_Private object by the constructor.
/// 2. Call GetCameraCapabilities to get the supported preview sizes.
/// 3. For optimal performance, set one of the supported preview size as the
///    constraints of getUserMedia. Use the created MediaStreamVideoTrack for
///    camera previews.
/// 4. Set the same preview size and other settings by SetConfig.
/// 5. Call CaptureStillImage to capture a still image. Play the shutter sound
///    in the shutter callback. The image from the preview callback can be used
///    for display. JPEG image will be returned to the JPEG callback.
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
                       PPB_ImageCapture_Private_ErrorCallback error_callback,
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

  /// Sets the configuration of the image capture.
  /// If <code>SetConfig()</code> is not called, default settings will be used.
  ///
  /// @param[in] config A <code>ImageCaptureConfig_Private</code> object.
  /// @param[in] callback <code>CompletionCallback</code> to be called upon
  /// completion of <code>SetConfig()</code>.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_INPROGRESS</code> if there is a pending call of
  /// <code>SetConfig()</code> or <code>CaptureStillImage()</code>.
  /// If an error is returned, the configuration will not be changed.
  int32_t SetConfig(const ImageCaptureConfig_Private& config,
                    const CompletionCallback& callback);

  /// Gets the configuration of the image capture.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code>
  /// to be called upon completion.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t GetConfig(
      const CompletionCallbackWithOutput<ImageCaptureConfig_Private>& callback);

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

  /// Captures a still JPEG image from the camera.
  ///
  /// Triggers an asynchronous image capture. The camera will initiate a series
  /// of callbacks to the application as the image capture progresses. The
  /// callbacks will be invoked in the order of shutter callback, preview
  /// callback, and JPEG callback. The shutter callback occurs after the image
  /// is captured. This can be used to trigger a sound to let the user know that
  /// image has been captured. The preview callback occurs when a scaled, fully
  /// processed preview image is available. The JPEG callback occurs when the
  /// compressed image is available. If there is an error after the capture is
  /// in progress, the error callback passed to <code>
  /// ImageCapture_Private.Create()</code> will be invoked. All the callbacks
  /// are invoked by the thread that calls this function.
  ///
  /// The size of the preview image in preview callback is determined by
  /// <code>ImageCaptureConfig_Private.SetPreviewSize</code>. The format is
  /// decided by the camera and can be got from <code>VideoFrame.GetFormat
  /// </code>. The size of the JPEG image is determined by <code>
  /// ImageCaptureConfig_Private.SetJpegSize</code>.
  ///
  /// The camera may need to stop and re-start streaming during image capture.
  /// If some MediaStreamVideoTrack are associated with the camera source, they
  /// will receive mute and unmute events. The mute event will be received
  /// before all the callbacks. The unmute event will be received after all the
  /// callbacks. The preview image will not be sent to the video tracks
  /// associated with the camera.
  ///
  /// @param[in] shutter_callback A <code>
  /// ImageCapture_Private_ShutterCallback</code> callback to indicate the
  /// image has been taken.
  /// @param[in] preview_callback A <code>
  /// ImageCapture_Private_PreviewCallback</code> callback to return a
  /// preview of the captured image.
  /// @param[in] jpeg_callback A <code>
  /// ImageCapture_Private_JpegCallback</code> callback to return captured
  /// JPEG image.
  /// @param[out] sequence_id The sequence ID is a unique monotonically
  /// increasing value starting from 0, incremented every time a new request
  /// like image capture is submitted.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  /// PP_OK means the callbacks will be triggered. Other values mean the
  /// callbacks will not be triggered.
  int32_t CaptureStillImage(
      PPB_ImageCapture_Private_ShutterCallback shutter_callback,
      PPB_ImageCapture_Private_PreviewCallback preview_callback,
      PPB_ImageCapture_Private_JpegCallback jpeg_callback,
      int64_t* sequence_id);

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

