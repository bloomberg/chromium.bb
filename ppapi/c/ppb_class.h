/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_CLASS_H_
#define PPAPI_C_PPB_CLASS_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"

#define PPB_CLASS_INTERFACE "PPB_Class;0.4"

/**
 * @file
 * Defines the PPB_Class struct.
 *
 */

/**
 * @addtogroup Typedefs
 * @{
 */

/**
 * Function callback.
 *
 * native_ptr will always be the native_ptr used to create this_object. If
 * this object was not created in this module, native_ptr will be NULL. There
 * is no other type protection - if your module contains two objects with
 * different native_ptr information, make sure you can handle the case of
 * JS calling one object's function with another object set as this.
 *
 */
typedef struct PP_Var (*PP_ClassFunction)(void* native_ptr,
                                          struct PP_Var this_object, /*NOLINT*/
                                          struct PP_Var* args,
                                          uint32_t argc,
                                          struct PP_Var* exception);
/**
 * @}
 */

/**
 * @addtogroup Typedefs
 * @{
 */
typedef void (*PP_ClassDestructor)(void* native_ptr);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */

/**
 * One property of a class.
 *
 * It can be either a value property, in which case it need to have getter
 * and/or setter fields set, and method NULL, or a function, in which case it
 * needs to have method set, and getter/setter set to NULL. It is an error to
 * have method and either getter or setter set, as it is an error to not provide
 * any of them.
 *
 * Not providing a getter will be equivalent to having a getter which returns
 * undefined. Not providing a setter will be equivalent to providing a setter
 * which doesn't do anything.
 *
 */
struct PP_ClassProperty {
  const char* name;
  PP_ClassFunction method;
  PP_ClassFunction getter;
  PP_ClassFunction setter;
  uint32_t modifiers;
};
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */

/** Interface for implementing JavaScript-accessible objects.
 *
 *
 * Example usage:
 *
 * struct PP_ClassProperty properties[] = {
 *   { "method", methodFunc },
 *   { "hiddenMethod", hiddenMethodFunc, NULL, NULL,
 *       PP_OBJECTPROPERTY_MODIFIER_DONTENUM },
 *   { "property", NULL, propertyGetter, propertySetter },
 *   { "readonlyProperty", NULL, propertyGetter, NULL,
 *       PP_OBJECTPROPERTY_MODIFIER_READONLY },
 *   { NULL }
 * };
 *
 * PP_Resource object_template =
 *   Create(module, &operator delete, NULL, properties);
 *
 * ...
 *
 * struct NativeData { int blah; ... }; // Can be anything.
 * NativeData* native_data = new NativeData;
 * native_data->blah = 123; // Initialize native data.
 *
 * PP_Var object = Instantiate(object_template, native_data);
 *
 * Please also see:
 *   http://code.google.com/p/ppapi/wiki/InterfacingWithJavaScript
 * for general information on using and implementing vars.
 */
struct PPB_Class {
  /**
   * Creates a class containing given methods and properties.
   *
   * Properties list is terminated with a NULL-named property. New instances are
   * created using Instantiate(). Each instance carries one void* of native
   * state, which is passed to Instantiate(). When the instance is finalized,
   * the destructor function is called to destruct the native state.
   *
   * If invoke handler is specified, then the instances can be used as
   * functions.
   */
  PP_Resource (*Create)(PP_Module module,
                        PP_ClassDestructor destruct,
                        PP_ClassFunction invoke,
                        struct PP_ClassProperty* properties);

  /**
   * Creates an instance of the given class, and attaches given native pointer
   * to it.
   *
   * If the class_object is invalid, throws an exception.
   */
  struct PP_Var (*Instantiate)(PP_Resource class_object,
                               void* native_ptr,
                               struct PP_Var* exception);
};
/**
 * @}
 */


#endif  /* PPAPI_C_PPP_CLASS_H_ */

