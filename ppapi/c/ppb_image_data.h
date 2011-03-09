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
 * This file defines the PPB_ImageData struct for determining how a browser
 * handles image data.
 */

/**
 * @addtogroup Enums
 * @{
 */

/**
 * PP_ImageDataFormat is an enumeration of the different types of
 * image data formats.
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

/**
 * The PP_ImageDataDesc structure represents a description of image data.
 */
struct PP_ImageDataDesc {

  /**
   * This value represents one of the image data types in the
   * PP_ImageDataFormat enum.
   */
  PP_ImageDataFormat format;

  /** This value represents the size of the bitmap in pixels. */
  struct PP_Size size;

  /**
   * This value represents the row width in bytes. This may be different than
   * width * 4 since there may be padding at the end of the lines.
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

/**
 * The PPB_ImageData interface contains pointers to several functions for
 * determining the browser's treatment of image data.
 */
struct PPB_ImageData {
  /**
   * GetNativeImageDataFormat is a pointer to a function that returns the
   * browser's preferred format for image data. The browser uses this format
   * internally for painting. Other formats may require internal conversions
   * to paint or may have additional restrictions depending on the function.
   *
   * @return PP_ImageDataFormat containing the preferred format.
   */
  PP_ImageDataFormat (*GetNativeImageDataFormat)();

  /**
   * IsImageDataFormatSupported is a pointer to a function that determines if
   * the given image data format is supported by the browser.
   *
   * @param[in] format The image data format.
   * @return PP_Bool with PP_TRUE if the given image data format is supported
   * by the browser.
   */
  PP_Bool (*IsImageDataFormatSupported)(PP_ImageDataFormat format);

  /**
   * Create is a pointer to a function that allocates an image data resource
   * with the given format and size.
   *
   * For security reasons, if uninitialized, the bitmap will not contain random
   * memory, but may contain data from a previous image produced by the same
   * plugin if the bitmap was cached and re-used.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   * @param[in] format The desired image data format.
   * @param[in] size A pointer to a PP_Size containing the image size.
   * @param[in] init_to_zero A PP_Bool to determine transparency at creation.
   * Set the init_to_zero flag if you want the bitmap initialized to
   * transparent during the creation process. If this flag is not set, the
   * current contents of the bitmap will be undefined, and the plugin should
   * be sure to set all the pixels.
   *
   * @return A PP_Resource with a nonzero ID on succes or zero on failure.
   * Failure means the instance, image size, or format was invalid.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_ImageDataFormat format,
                        const struct PP_Size* size,
                        PP_Bool init_to_zero);

  /**
   * IsImageData is a pointer to a function that determiens if a given resource
   * is image data.
   *
   * @param[in] image_data A PP_Resource corresponding to image data.
   * @return A PP_Bool with PP_TRUE if the given resrouce is an image data
   * or PP_FALSE if the resource is invalid or some type other than image data.
   */
  PP_Bool (*IsImageData)(PP_Resource image_data);

  /**
   * Describe is a pointer to a function that computes the description of the
   * image data.
   *
   * @param[in] image_data A PP_Resource corresponding to image data.
   * @param[in/out] desc A pointer to a PP_ImageDataDesc containing the
   * description.
   * @return A PP_Bool with PP_TRUE on success or PP_FALSE
   * if the resource is not an image data. On PP_FALSE, the |desc|
   * structure will be filled with 0.
   */
  PP_Bool (*Describe)(PP_Resource image_data,
                      struct PP_ImageDataDesc* desc);

  /**
   * Map is a pointer to a function that maps an image data into the plugin
   * address space.
   *
   * @param[in] image_data A PP_Resource corresponding to image data.
   * @return A pointer to the beginning of the data.
   */
  void* (*Map)(PP_Resource image_data);

  /**
   * Unmap is a pointer to a function that unmaps an image data from the plugin
   * address space.
   *
   * @param[in] image_data A PP_Resource corresponding to image data.
   */
  void (*Unmap)(PP_Resource image_data);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_IMAGE_DATA_H_ */

