/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_var_dictionary_dev.idl modified Wed Mar 13 21:47:05 2013. */

#ifndef PPAPI_C_DEV_PPB_VAR_DICTIONARY_DEV_H_
#define PPAPI_C_DEV_PPB_VAR_DICTIONARY_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_VAR_DICTIONARY_DEV_INTERFACE_0_1 "PPB_VarDictionary(Dev);0.1"
#define PPB_VAR_DICTIONARY_DEV_INTERFACE PPB_VAR_DICTIONARY_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_VarDictionary_Dev</code> struct providing
 * a way to interact with dictionary vars.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * A dictionary var contains key-value pairs with unique keys. The keys are
 * strings while the values can be arbitrary vars. Key comparison is always
 * done by value instead of by reference.
 */
struct PPB_VarDictionary_Dev_0_1 {
  /**
   * Creates a dictionary var, i.e., a <code>PP_Var</code> with type set to
   * <code>PP_VARTYPE_DICTIONARY</code>.
   *
   * @return An empty dictionary var, whose reference count is set to 1 on
   * behalf of the caller.
   */
  struct PP_Var (*Create)(void);
  /**
   * Gets the value associated with the specified key.
   *
   * @param[in] dict A dictionary var.
   * @param[in] key A string var.
   *
   * @return The value that is associated with <code>key</code>. The reference
   * count is incremented on behalf of the caller. If <code>key</code> is not a
   * string var, or it doesn't exist in <code>dict</code>, an undefined var is
   * returned.
   */
  struct PP_Var (*Get)(struct PP_Var dict, struct PP_Var key);
  /**
   * Sets the value associated with the specified key. The dictionary is
   * responsible for holding references to its children to keep them alive.
   *
   * @param[in] dict A dictionary var.
   * @param[in] key A string var. If this key hasn't existed in
   * <code>dict</code>, it is added and associated with <code>value</code>;
   * otherwise, the previous value is replaced with <code>value</code>.
   * @param[in] value The value to set.
   *
   * @return A <code>PP_Bool</code> indicating whether the operation succeeds.
   */
  PP_Bool (*Set)(struct PP_Var dict, struct PP_Var key, struct PP_Var value);
  /**
   * Deletes the specified key and its associated value, if the key exists.
   *
   * @param[in] dict A dictionary var.
   * @param[in] key A string var.
   */
  void (*Delete)(struct PP_Var dict, struct PP_Var key);
  /**
   * Checks whether a key exists.
   *
   * @param[in] dict A dictionary var.
   * @param[in] key A string var.
   *
   * @return A <code>PP_Bool</code> indicating whether the key exists.
   */
  PP_Bool (*HasKey)(struct PP_Var dict, struct PP_Var key);
  /**
   * Gets all the keys in a dictionary. Please note that for each key that you
   * set into the dictionary, a string var with the same contents is returned;
   * but it may not be the same string var (i.e., <code>value.as_id</code> may
   * be different).
   *
   * @param[in] dict A dictionary var.
   *
   * @return An array var which contains all the keys of <code>dict</code>. Its
   * reference count is incremented on behalf of the caller. The elements are
   * string vars. Returns a null var if failed.
   */
  struct PP_Var (*GetKeys)(struct PP_Var dict);
};

typedef struct PPB_VarDictionary_Dev_0_1 PPB_VarDictionary_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_VAR_DICTIONARY_DEV_H_ */

