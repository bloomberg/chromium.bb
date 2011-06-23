// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_IMAGE_DATA_H_
#define PPAPI_CPP_IMAGE_DATA_H_

#include "ppapi/c/ppb_image_data.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/resource.h"


/// @file
/// This file defines the APIs for determining how a browser
/// handles image data.
namespace pp {

class Instance;
class Plugin;

class ImageData : public Resource {
 public:
  /// Default constructor for creating an is_null() ImageData object.
  ImageData();

  /// A special structure used by the constructor that does not increment the
  /// reference count of the underlying Image resource.
  struct PassRef {};

  /// A constructor used when you have received a PP_Resource as a return
  /// value that has already been reference counted.
  ///
  /// @param[in] resource A PP_Resource corresponding to image data.
  ImageData(PassRef, PP_Resource resource);

  /// A constructor accepting a reference to an ImageData to increment the
  /// reference count of the underlying Image resource. This constructor
  /// produces an ImageData object that shares the underlying Image resource
  /// with <code>other</code>.
  ///
  /// @param[in] other A pointer to an image data.
  ImageData(const ImageData& other);

  /// A constructor that allocates a new ImageData in the browser with the
  /// provided parameters.
  /// The resulting object will be is_null() if the allocation failed.
  ///
  /// @param[in] instance A PP_Instance indentifying one instance of a module.
  /// @param[in] format The desired image format.
  /// PP_ImageDataFormat is an enumeration of the different types of
  /// image data formats.
  ///
  /// The third part of each enumeration value describes the memory layout from
  /// the lowest address to the highest. For example, BGRA means the B component
  /// is stored in the lowest address, no matter what endianness the platform is
  /// using.
  ///
  /// The PREMUL suffix implies pre-multiplied alpha is used. In this mode, the
  /// red, green and blue color components of the pixel data supplied to an
  /// image data should be pre-multiplied by their alpha value. For example:
  /// starting with floating point color components, here is how to convert
  /// them to 8-bit premultiplied components for image data:
  ///    ...components of a pixel, floats ranging from 0 to 1...
  ///    float red = 1.0f;
  ///    float green = 0.50f;
  ///    float blue = 0.0f;
  ///    float alpha = 0.75f;
  ///    ...components for image data are 8-bit values ranging from 0 to 255...
  ///    uint8_t image_data_red_premul = (uint8_t)(red * alpha * 255.0f);
  ///    uint8_t image_data_green_premul = (uint8_t)(green * alpha * 255.0f);
  ///    uint8_t image_data_blue_premul = (uint8_t)(blue * alpha * 255.0f);
  ///    uint8_t image_data_alpha_premul = (uint8_t)(alpha * 255.0f);
  /// Note the resulting pre-multiplied red, green and blue components should
  /// not be greater than the alpha value.
  /// @param[in] size A pointer to a PP_Size containing the image size.
  /// @param[in] init_to_zero A PP_Bool to determine transparency at creation.
  /// Set the init_to_zero flag if you want the bitmap initialized to
  /// transparent during the creation process. If this flag is not set, the
  /// current contents of the bitmap will be undefined, and the plugin should
  /// be sure to set all the pixels.
  ImageData(Instance* instance,
            PP_ImageDataFormat format,
            const Size& size,
            bool init_to_zero);

  /// This function decrements the reference count of this ImageData and
  /// increments the reference count of the <code>other</code> ImageData.
  /// This ImageData shares the underlying image resource with
  /// <code>other</code>.
  ///
  /// @param[in] other An other image data.
  /// @return A new image data context.
  ImageData& operator=(const ImageData& other);

  /// This function determines the browser's preferred format for images.
  /// Using this format guarantees no extra conversions will occur when
  /// painting.
  ///
  /// @return PP_ImageDataFormat containing the preferred format.
  static PP_ImageDataFormat GetNativeImageDataFormat();

  /// A getter function for returning the current format for images.
  ///
  /// @return PP_ImageDataFormat containing the preferred format.
  PP_ImageDataFormat format() const { return desc_.format; }

  /// A getter function for returning the image size.
  ///
  /// @return The image size in pixels.
  pp::Size size() const { return desc_.size; }

  /// A getter function for returning the row width in bytes.
  ///
  /// @return The row width in bytes.
  int32_t stride() const { return desc_.stride; }

  /// A getter function for returning a raw pointer to the image pixels.
  ///
  /// @return A raw pointer to the image pixels.
  void* data() const { return data_; }

  /// This function is used retrieve the address of the given pixel for 32-bit
  /// pixel formats.
  ///
  /// @param[in] coord A Point representing the x and y coordinates for a
  /// specific pixel.
  /// @return The address for the pixel.
  const uint32_t* GetAddr32(const Point& coord) const;

  /// This function is used retrieve the address of the given pixel for 32-bit
  /// pixel formats.
  ///
  /// @param[in] coord A Point representing the x and y coordinates for a
  /// specific pixel.
  /// @return The address for the pixel.
  uint32_t* GetAddr32(const Point& coord);

 private:
  void PassRefAndInitData(PP_Resource resource);

  PP_ImageDataDesc desc_;
  void* data_;
};

}  // namespace pp

#endif  // PPAPI_CPP_IMAGE_DATA_H_
