/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_IMAGE_DATA_H_
#define PPAPI_C_PPB_IMAGE_DATA_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * Defines the API ...
 */

/**
 * @addtogroup Enums
 * @{
 */

typedef enum {
  PP_IMAGEDATAFORMAT_BGRA_PREMUL,
  PP_IMAGEDATAFORMAT_RGBA_PREMUL
} PP_ImageDataFormat;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_ImageDataFormat, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_ImageDataDesc {
  PP_ImageDataFormat format;

  /** Size of the bitmap in pixels. */
  struct PP_Size size;

  /** The row width in bytes. This may be different than width * 4 since there
   * may be padding at the end of the lines.
   */
  int32_t stride;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_ImageDataDesc, 16);
/**
 * @}
 */

#define PPB_IMAGEDATA_INTERFACE "PPB_ImageData;0.3"

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_ImageData {
  /**
   * Returns the browser's preferred format for image data. This format will be
   * the format is uses internally for painting. Other formats may require
   * internal conversions to paint or may have additional restrictions depending
   * on the function.
   */
  PP_ImageDataFormat (*GetNativeImageDataFormat)();

  /**
   * Returns PP_TRUE if the given image data format is supported by the browser.
   */
  PP_Bool (*IsImageDataFormatSupported)(PP_ImageDataFormat format);

  /**
   * Allocates an image data resource with the given format and size. The
   * return value will have a nonzero ID on success, or zero on failure.
   * Failure means the instance, image size, or format was invalid.
   *
   * Set the init_to_zero flag if you want the bitmap initialized to
   * transparent during the creation process. If this flag is not set, the
   * current contents of the bitmap will be undefined, and the plugin should
   * be sure to set all the pixels.
   *
   * For security reasons, if uninitialized, the bitmap will not contain random
   * memory, but may contain data from a previous image produced by the same
   * plugin if the bitmap was cached and re-used.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_ImageDataFormat format,
                        const struct PP_Size* size,
                        PP_Bool init_to_zero);

  /**
   * Returns PP_TRUE if the given resource is an image data. Returns PP_FALSE if
   * the resource is invalid or some type other than an image data.
   */
  PP_Bool (*IsImageData)(PP_Resource image_data);

  /**
   * Computes the description of the image data. Returns PP_TRUE on success,
   * PP_FALSE if the resource is not an image data. On PP_FALSE, the |desc|
   * structure will be filled with 0.
   */
  PP_Bool (*Describe)(PP_Resource image_data,
                      struct PP_ImageDataDesc* desc);

  /**
   * Maps this bitmap into the plugin address space and returns a pointer to the
   * beginning of the data.
   */
  void* (*Map)(PP_Resource image_data);

  void (*Unmap)(PP_Resource image_data);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_IMAGE_DATA_H_ */

