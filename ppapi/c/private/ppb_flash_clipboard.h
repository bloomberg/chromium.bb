/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_flash_clipboard.idl modified Wed Nov 23 12:39:42 2011. */

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_CLIPBOARD_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_CLIPBOARD_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_FLASH_CLIPBOARD_INTERFACE_3_0 "PPB_Flash_Clipboard;3.0"
#define PPB_FLASH_CLIPBOARD_INTERFACE PPB_FLASH_CLIPBOARD_INTERFACE_3_0

/**
 * @file
 * This file defines the private <code>PPB_Flash_Clipboard</code> API used by
 * Pepper Flash for reading and writing to the clipboard.
 */


/**
 * The old version string for this interface, equivalent to version 3.0.
 * TODO(viettrungluu): Remove this when enough time has passed. crbug.com/104184
 */
#define PPB_FLASH_CLIPBOARD_INTERFACE_3_LEGACY "PPB_Flash_Clipboard;3"

/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains the types of clipboards that can be accessed.
 * These types correspond to clipboard types in WebKit.
 */
typedef enum {
  /** The standard clipboard. */
  PP_FLASH_CLIPBOARD_TYPE_STANDARD = 0,
  /** The selection clipboard (e.g., on Linux). */
  PP_FLASH_CLIPBOARD_TYPE_SELECTION = 1
} PP_Flash_Clipboard_Type;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Flash_Clipboard_Type, 4);

/**
 * This enumeration contains the supported clipboard data formats.
 */
typedef enum {
  /** Indicates an invalid or unsupported clipboard data format. */
  PP_FLASH_CLIPBOARD_FORMAT_INVALID = 0,
  /** Indicates plain text clipboard data. */
  PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT = 1,
  /** Indicates HTML clipboard data. */
  PP_FLASH_CLIPBOARD_FORMAT_HTML = 2
} PP_Flash_Clipboard_Format;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Flash_Clipboard_Format, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_Flash_Clipboard</code> interface contains pointers to functions
 * used by Pepper Flash to access the clipboard.
 *
 * TODO(viettrungluu): Support more formats (e.g., HTML)....
 */
struct PPB_Flash_Clipboard {
  /**
   * Checks whether a given data format is available from the given clipboard.
   * Returns true if the given format is available from the given clipboard.
   */
  PP_Bool (*IsFormatAvailable)(PP_Instance instance_id,
                               PP_Flash_Clipboard_Type clipboard_type,
                               PP_Flash_Clipboard_Format format);
  /**
   * Reads plain text data from the clipboard.
   */
  struct PP_Var (*ReadPlainText)(PP_Instance instance_id,
                                 PP_Flash_Clipboard_Type clipboard_type);
  /**
   * Writes plain text data to the clipboard. If <code>text</code> is too large,
   * it will return <code>PP_ERROR_NOSPACE</code> and not write to the
   * clipboard.
   */
  int32_t (*WritePlainText)(PP_Instance instance_id,
                            PP_Flash_Clipboard_Type clipboard_type,
                            struct PP_Var text);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_FLASH_CLIPBOARD_H_ */

