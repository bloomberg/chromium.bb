/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#define PPB_VAR_INTERFACE "PPB_Var;0.4"

/**
 * @file
 * Defines the API ...
 */

/**
 *
 * @addtogroup Enums
 * @{
 */

/**
 * Defines the PPB_Var struct.
 * See http://code.google.com/p/ppapi/wiki/InterfacingWithJavaScript
 * for general information on using this interface.
 * {PENDING: Should the generated doc really be pointing to methods?}
 */
enum PP_ObjectProperty_Modifier {
  PP_OBJECTPROPERTY_MODIFIER_NONE       = 0,
  PP_OBJECTPROPERTY_MODIFIER_READONLY   = 1 << 0,
  PP_OBJECTPROPERTY_MODIFIER_DONTENUM   = 1 << 1,
  PP_OBJECTPROPERTY_MODIFIER_DONTDELETE = 1 << 2,
  PP_OBJECTPROPERTY_MODIFIER_HASVALUE   = 1 << 3
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_ObjectProperty_Modifier, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_ObjectProperty {
  struct PP_Var name;
  struct PP_Var value;
  struct PP_Var getter;
  struct PP_Var setter;
  uint32_t modifiers;

  /** Ensure that this struct is 72 bytes wide by padding the end.  In some
   *  compilers, PP_Var is 8-byte aligned, so those compilers align this struct
   *  on 8-byte boundaries as well and pad it to 72 bytes even without this
   *  padding attribute.  This padding makes its size consistent across
   *  compilers.
   */
  int32_t padding;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_ObjectProperty, 72);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */

/**
 * PPB_Var API
 *
 * JavaScript specification:
 *
 * When referencing JS specification, we will refer to ECMAScript, 5th edition,
 * and we will put section numbers in square brackets.
 *
 * Exception handling:
 *
 * If an exception parameter is NULL, then any exceptions that happen during the
 * execution of the function will be ignored. If it is non-NULL, and has a type
 * of PP_VARTYPE_UNDEFINED, then if an exception is thrown it will be stored in
 * the exception variable. It it is non-NULL and not PP_VARTYPE_UNDEFINED, then
 * the function is a no-op, and, if it returns a value, it will return
 * PP_VARTYPE_UNDEFINED. This can be used to chain together multiple calls and
 * only check the exception at the end.
 *
 * Make sure not to intermix non-JS with JS calls when relying on this behavior
 * to catch JS exceptions, as non-JS functions will still execute!

 * JS engine's exceptions will always be of type PP_VARTYPE_OBJECT. However,
 * PP_Var interface can also throw PP_VARTYPE_STRING exceptions, in situations
 * where there's no JS execution context defined. These are usually invalid
 * parameter errors - passing an invalid PP_Var value, for example, will always
 * result in an PP_VARTYPE_STRING exception. Exceptions will not be of any other
 * type.
 * TODO(neb): Specify the exception for ill-formed PP_Vars, invalid module,
 * instance, resource, string and object ids.
 *
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

  /**
   *  Convert a variable to a different type using rules from ECMAScript
   *  specification, section [9].
   *
   * For conversions from/to PP_VARTYPE_OBJECT, the instance must be specified,
   * or an exception of type PP_VARTYPE_STRING will be thrown.
   */
  struct PP_Var (*ConvertType)(PP_Instance instance,
                               struct PP_Var var,
                               PP_VarType new_type,
                               struct PP_Var* exception);

  /**
   * Sets a property on the object, similar to Object.prototype.defineProperty.
   *
   * First, if object is not PP_VARTYPE_OBJECT, throw an exception.
   * TODO(neb): Specify the exception. Ideally, it would be a TypeError, but
   * don't have the JS context to create new objects, we might throw a string.
   * Then, the property's 'name' field is converted to string using
   * ConvertType (ToString [9.8]).
   * After that, defineOwnProperty [8.12.9, 15.4.5.1] is called with the
   * property.
   * To set a simple property, set the value and set modifiers to default
   * (Writable|Enumerable|Configurable|HasValue), see [8.12.15] and
   * function PPB_MakeSimpleProperty.
   */
  void (*DefineProperty)(struct PP_Var object,
                         struct PP_ObjectProperty property,
                         struct PP_Var* exception);

  /**
   * Tests whether an object has a property with a given name.
   *
   * First, if object is not PP_VARTYPE_OBJECT, throw an exception.
   * TODO(neb): Specify the exception. Ideally, it would be a TypeError, but
   * don't have the JS context to create new objects, we might throw a string.
   * Then, convert 'property' to string using ConvertType (ToString [9.8]).
   * Then return true if the given property exists on the object [8.12.6].
   */
  PP_Bool (*HasProperty)(struct PP_Var object,
                         struct PP_Var property,
                         struct PP_Var* exception);

  /**
   * Returns a given property of the object.
   *
   * First, if object is not PP_VARTYPE_OBJECT, throw an exception.
   * TODO(neb): Specify the exception. Ideally, it would be a TypeError, but
   * don't have the JS context to create new objects, we might throw a string.
   * Then, convert 'property' to string using ConvertType (ToString [9.8]).
   * Then return the given property of the object [8.12.2].
   */
  struct PP_Var (*GetProperty)(struct PP_Var object,
                               struct PP_Var property,
                               struct PP_Var* exception);

  /**
   * Delete a property from the object, return true if succeeded.
   *
   * True is returned if the property didn't exist in the first place.
   *
   * First, if object is not PP_VARTYPE_OBJECT, throw an exception.
   * TODO(neb): Specify the exception. Ideally, it would be a TypeError, but
   * don't have the JS context to create new objects, we might throw a string.
   * Then, convert 'property' to string using ConvertType (ToString [9.8]).
   * Then delete the given property of the object [8.12.7].
   */
  PP_Bool (*DeleteProperty)(struct PP_Var object,
                            struct PP_Var property,
                            struct PP_Var* exception);

  /**
   * Retrieves all property names on the given object. Property names include
   * methods.
   *
   * If object is not PP_VARTYPE_OBJECT, throw an exception.
   * TODO(neb): Specify the exception. Ideally, it would be a TypeError, but
   * don't have the JS context to create new objects, we might throw a string.
   *
   * If there is a failure, the given exception will be set (if it is non-NULL).
   * On failure, |*properties| will be set to NULL and |*property_count| will be
   * set to 0.
   *
   * A pointer to the array of property names will be placed in |*properties|.
   * The caller is responsible for calling Release() on each of these properties
   * (as per normal refcounted memory management) as well as freeing the array
   * pointer with PPB_Core.MemFree().
   *
   * This function returns all "enumerable" properties. Some JavaScript
   * properties are "hidden" and these properties won't be retrieved by this
   * function, yet you can still set and get them. You can use JS
   * Object.getOwnPropertyNames() to access these properties.
   *
   * Example:
   * <pre>  uint32_t count;
   *   PP_Var* properties;
   *   ppb_var.EnumerateProperties(object, &count, &properties);
   *
   *   ...use the properties here...
   *
   *   for (uint32_t i = 0; i < count; i++)
   *     ppb_var.Release(properties[i]);
   *   ppb_core.MemFree(properties); </pre>
   */
  void (*EnumerateProperties)(struct PP_Var object,
                              uint32_t* property_count,
                              struct PP_Var** properties,
                              struct PP_Var* exception);

  /**
   * Check if an object is a JS Function [9.11].
   */
  PP_Bool (*IsCallable)(struct PP_Var object);

  /**
   * Call the functions.
   *
   * Similar to Function.prototype.call [15.3.4.4]. It will throw a TypeError
   * and return undefined if object is not PP_VARTYPE_OBJECT, or is not
   * callable.
   *
   * Pass the arguments to the function in order in the |argv| array, and the
   * number of arguments in the |argc| parameter. |argv| can be NULL if |argc|
   * is zero.
   *
   * Example:
   *   Call(obj.GetProperty("DoIt"), obj, 0, NULL, NULL)
   *   Equivalent to obj.DoIt() in JavaScript.
   *
   *   Call(obj, PP_MakeUndefined(), 0, NULL, NULL)
   *   Equivalent to obj() in JavaScript.
   */
  struct PP_Var (*Call)(struct PP_Var object,
                        struct PP_Var this_object,
                        uint32_t argc,
                        struct PP_Var* argv,
                        struct PP_Var* exception);

  /**
   * Invoke the object as a constructor. It will throw a |TypeError| and return
   * |undefined| if |object| is not PP_VARTYPE_OBJECT, or cannot be used as a
   * constructor.
   *
   * Pass the arguments to the function in order in the |argv| array, and the
   * number of arguments in the |argc| parameter. |argv| can be NULL if |argc|
   * is zero.
   *
   * For example, if |object| is |String|, this is like saying |new String| in
   * JavaScript. Similar to the [[Construct]] internal method [13.2.2].
   *
   * For examples, to construct an empty object, do:
   * GetWindow().GetProperty("Object").Construct(0, NULL);
   */
  struct PP_Var (*Construct)(struct PP_Var object,
                             uint32_t argc,
                             struct PP_Var* argv,
                             struct PP_Var* exception);
};
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */
PP_INLINE struct PP_ObjectProperty PP_MakeSimpleProperty(struct PP_Var name,
                                                         struct PP_Var value) {
  struct PP_ObjectProperty result;
  result.name = name;
  result.value = value;
  result.getter = PP_MakeUndefined();
  result.setter = PP_MakeUndefined();
  result.modifiers = PP_OBJECTPROPERTY_MODIFIER_HASVALUE;
  return result;
}
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_VAR_H_ */

