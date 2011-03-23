/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_VAR_H_
#define PPAPI_C_PPB_VAR_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_VAR_INTERFACE "PPB_Var;0.5"

/**
 * @file
 * This file defines the PPB_Var struct.
 */

/**
 * @addtogroup Interfaces
 * @{
 */

/**
 * PPB_Var API
 */
struct PPB_Var {
  /**
   * Adds a reference to the given var. If this is not a refcounted object,
   * this function will do nothing so you can always call it no matter what the
   * type.
   */
  void (*AddRef)(struct PP_Var var);

  /**
   * Removes a reference to given var, deleting it if the internal refcount
   * becomes 0. If the given var is not a refcounted object, this function will
   * do nothing so you can always call it no matter what the type.
   */
  void (*Release)(struct PP_Var var);

  /**
   * Creates a string var from a string. The string must be encoded in valid
   * UTF-8 and is NOT NULL-terminated, the length must be specified in |len|.
   * It is an error if the string is not valid UTF-8.
   *
   * If the length is 0, the |data| pointer will not be dereferenced and may
   * be NULL. Note, however, that if you do this, the "NULL-ness" will not be
   * preserved, as VarToUtf8 will never return NULL on success, even for empty
   * strings.
   *
   * The resulting object will be a refcounted string object. It will be
   * AddRef()ed for the caller. When the caller is done with it, it should be
   * Release()d.
   *
   * On error (basically out of memory to allocate the string, or input that
   * is not valid UTF-8), this function will return a Null var.
   */
  struct PP_Var (*VarFromUtf8)(PP_Module module,
                               const char* data, uint32_t len);

  /**
   * Converts a string-type var to a char* encoded in UTF-8. This string is NOT
   * NULL-terminated. The length will be placed in |*len|. If the string is
   * valid but empty the return value will be non-NULL, but |*len| will still
   * be 0.
   *
   * If the var is not a string, this function will return NULL and |*len| will
   * be 0.
   *
   * The returned buffer will be valid as long as the underlying var is alive.
   * If the plugin frees its reference, the string will be freed and the pointer
   * will be to random memory.
   */
  const char* (*VarToUtf8)(struct PP_Var var, uint32_t* len);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VAR_H_ */

