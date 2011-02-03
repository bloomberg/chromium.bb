/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_VAR_H_
#define PPAPI_C_PP_VAR_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines the API for handling the passing of data types between
 * your module and the page.
 */

/**
 *
 * @addtogroup Enums
 * @{
 */

/**
 * PP_VarType is an enumeration of the different types that can be contained
 * within a PP_VAR structure.
 */
typedef enum {
  PP_VARTYPE_UNDEFINED,
  PP_VARTYPE_NULL,
  PP_VARTYPE_BOOL,
  PP_VARTYPE_INT32,
  PP_VARTYPE_DOUBLE,
  PP_VARTYPE_STRING,
  PP_VARTYPE_OBJECT
} PP_VarType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_VarType, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */

/**
 * The PP_VAR struct is a variant data type and can contain any
 * value of one of the types named in the PP_VarType enum. This structure is
 * for passing data between native code which can be strongly typed and the
 * browser (JavaScript) which isn't strongly typed.
 *
 * JavaScript has a "number" type for holding a number, and does not
 * differentiate between floating point and integer numbers. The
 * JavaScript operations will try to optimize operations by using
 * integers when possible, but could end up with doubles. Therefore,
 * you can't assume a numeric PP_Var will be the type you expect.
 * Your code should be capable of handling either int32_t or double for numeric
 * PP_Vars sent from JavaScript.
 */
struct PP_Var {
  PP_VarType type;

  /** Ensures @a value is aligned on an 8-byte boundary relative to the
   *  start of the struct. Some compilers align doubles on 8-byte boundaries
   *  for 32-bit x86, and some align on 4-byte boundaries.
   */
  int32_t padding;

  union {
    PP_Bool as_bool;
    int32_t as_int;
    double as_double;

    /**
     * Internal ID for strings and objects. The identifier is an opaque handle
     * assigned by the browser to the plugin. It is guaranteed never to be 0,
     * so a plugin can initialize this ID to 0 to indicate a "NULL handle."
     */
    int64_t as_id;
  } value;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_Var, 16);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PP_MakeUndefined() is a utility function used to wrap an undefined value
 * into a PP_VAR struct for passing to the browser.
 * @return A PP_Var structure
 */
PP_INLINE struct PP_Var PP_MakeUndefined() {
  struct PP_Var result = { PP_VARTYPE_UNDEFINED, 0, {PP_FALSE} };
  return result;
}

/**
 * PP_MakeNull() is a utility function used to wrap a null value into a
 * PP_VAR struct for passing to the browser.
 * @return A PP_Var structure
 */
PP_INLINE struct PP_Var PP_MakeNull() {
  struct PP_Var result = { PP_VARTYPE_NULL, 0, {PP_FALSE} };
  return result;
}

/**
 * PP_MakeBool() is a utility function used to wrap a boolean value into a
 * PP_VAR struct for passing to the browser.
 * @param[in] value A PP_Bool enumeration
 * @return A PP_Var structure
 */
PP_INLINE struct PP_Var PP_MakeBool(PP_Bool value) {
  struct PP_Var result = { PP_VARTYPE_BOOL, 0, {PP_FALSE} };
  result.value.as_bool = value;
  return result;
}

/**
 * PP_MakeInt32() is a utility function used to wrap a 32 bit integer value
 * into a PP_VAR struct for passing to the browser.
 * @param[in] value An int32
 * @return A PP_Var structure
 */
PP_INLINE struct PP_Var PP_MakeInt32(int32_t value) {
  struct PP_Var result = { PP_VARTYPE_INT32, 0, {PP_FALSE} };
  result.value.as_int = value;
  return result;
}

/**
 * PP_MakeDouble() is a utility function used to wrap a double value into a
 * PP_VAR struct for passing to the browser.
 * @param[in] value A double
 * @return A PP_Var structure
 */
PP_INLINE struct PP_Var PP_MakeDouble(double value) {
  struct PP_Var result = { PP_VARTYPE_DOUBLE, 0, {PP_FALSE} };
  result.value.as_double = value;
  return result;
}
/**
 * @}
 */

#endif  /* PPAPI_C_PP_VAR_H_ */

