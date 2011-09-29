/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_text_input_dev.idl modified Tue Sep 27 14:34:33 2011. */

#ifndef PPAPI_C_DEV_PPB_TEXT_INPUT_DEV_H_
#define PPAPI_C_DEV_PPB_TEXT_INPUT_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_TEXTINPUT_DEV_INTERFACE_0_1 "PPB_TextInput(Dev);0.1"
#define PPB_TEXTINPUT_DEV_INTERFACE PPB_TEXTINPUT_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_TextInput_Dev</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * PP_TextInput_Type is used to indicate the status of a plugin in regard to
 * text input.
 */
typedef enum {
  /**
   * Input caret is not in an editable mode, no input method shall be used.
   */
  PP_TEXTINPUT_TYPE_NONE = 0,
  /**
   * Input caret is in a normal editable mode, any input method can be used.
   */
  PP_TEXTINPUT_TYPE_TEXT = 1,
  /**
   * Input caret is in a password box, an input method may be used only if
   * it's suitable for password input.
   */
  PP_TEXTINPUT_TYPE_PASSWORD = 2,
  PP_TEXTINPUT_TYPE_SEARCH = 3,
  PP_TEXTINPUT_TYPE_EMAIL = 4,
  PP_TEXTINPUT_TYPE_NUMBER = 5,
  PP_TEXTINPUT_TYPE_TELEPHONE = 6,
  PP_TEXTINPUT_TYPE_URL = 7
} PP_TextInput_Type;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_TextInput_Type, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * <code>PPB_TextInput_Dev</code> provides a set of functions for giving hints
 * to the browser about the text input status of plugins, and functions for
 * controlling input method editors (IMEs).
 */
struct PPB_TextInput_Dev {
  /**
   * Informs the browser about the current text input mode of the plugin.
   * Typical use of this information in the browser is to properly
   * display/suppress tools for supporting text inputs (such as virtual
   * keyboards in touch screen based devices, or input method editors often
   * used for composing East Asian characters).
   */
  void (*SetTextInputType)(PP_Instance instance, PP_TextInput_Type type);
  /**
   * Informs the browser about the coordinates of the text input caret and the
   * bounding box of the text input area. Typical use of this information in
   * the browser is to layout IME windows etc.
   */
  void (*UpdateCaretPosition)(PP_Instance instance,
                              const struct PP_Rect* caret,
                              const struct PP_Rect* bounding_box);
  /**
   * Cancels the current composition in IME.
   */
  void (*CancelCompositionText)(PP_Instance instance);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_TEXT_INPUT_DEV_H_ */

