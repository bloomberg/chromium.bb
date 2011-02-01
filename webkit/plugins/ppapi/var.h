// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_VAR_H_
#define WEBKIT_PLUGINS_PPAPI_VAR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/ref_counted.h"

struct PP_Var;
struct PPB_Var;
struct PPB_Var_Deprecated;
typedef struct NPObject NPObject;
typedef struct _NPVariant NPVariant;
typedef void* NPIdentifier;

namespace webkit {
namespace ppapi {

class ObjectVar;
class PluginInstance;
class PluginModule;
class StringVar;

// Var -------------------------------------------------------------------------

// Represents a non-POD var. This is derived from a resource even though it
// isn't a resource from the plugin's perspective. This allows us to re-use
// the refcounting and the association with the module from the resource code.
class Var : public base::RefCounted<Var> {
 public:
  virtual ~Var();

  // Returns a PP_Var that corresponds to the given NPVariant. The contents of
  // the NPVariant will be copied unless the NPVariant corresponds to an
  // object. This will handle all Variant types including POD, strings, and
  // objects.
  //
  // The returned PP_Var will have a refcount of 1, this passing ownership of
  // the reference to the caller. This is suitable for returning to a plugin.
  static PP_Var NPVariantToPPVar(PluginInstance* instance,
                                 const NPVariant* variant);

  // Returns a NPIdentifier that corresponds to the given PP_Var. The contents
  // of the PP_Var will be copied. Returns 0 if the given PP_Var is not a a
  // string or integer type.
  static NPIdentifier PPVarToNPIdentifier(PP_Var var);

  // Returns a PP_Var corresponding to the given identifier. In the case of
  // a string identifier, the string will be allocated associated with the
  // given module. A returned string will have a reference count of 1.
  static PP_Var NPIdentifierToPPVar(PluginModule* module, NPIdentifier id);

  // Provides access to the manual refcounting of a PP_Var from the plugin's
  // perspective. This is different than the AddRef/Release on this scoped
  // object. This uses the ResourceTracker, which keeps a separate "plugin
  // refcount" that prevents the plugin from messing up our refcounting or
  // freeing something out from under us.
  //
  // You should not generally need to use these functions. However, if you
  // call a plugin function that returns a var, it will transfer a ref to us
  // (the caller) which in the case of a string or object var will need to
  // be released.
  //
  // Example, assuming we're expecting the plugin to return a string:
  //   PP_Var rv = some_ppp_interface->DoSomething(a, b, c);
  //
  //   // Get the string value. This will take a reference to the object which
  //   // will prevent it from being deleted out from under us when we call
  //   // PluginReleasePPVar().
  //   scoped_refptr<StringVar> string(StringVar::FromPPVar(rv));
  //
  //   // Release the reference the plugin gave us when returning the value.
  //   // This is legal to do for all types of vars.
  //   Var::PluginReleasePPVar(rv);
  //
  //   // Use the string.
  //   if (!string)
  //     return false;  // It didn't return a proper string.
  //   UseTheString(string->value());
  static void PluginAddRefPPVar(PP_Var var);
  static void PluginReleasePPVar(PP_Var var);

  // Returns the PPB_Var_Deprecated interface for the plugin to use.
  static const PPB_Var_Deprecated* GetDeprecatedInterface();

  // Returns the PPB_Var interface for the plugin to use.
  static const PPB_Var* GetInterface();

  virtual StringVar* AsStringVar();
  virtual ObjectVar* AsObjectVar();

  PluginModule* module() const { return module_; }

  // Returns the unique ID associated with this string or object. The object
  // must be a string or an object var, and the return value is guaranteed
  // nonzero.
  int32 GetID();

 protected:
  // This can only be constructed as a StringVar or an ObjectVar.
  explicit Var(PluginModule* module);

 private:
  PluginModule* module_;

  // This will be 0 if no ID has been assigned (this happens lazily).
  int32 var_id_;

  DISALLOW_COPY_AND_ASSIGN(Var);
};

// StringVar -------------------------------------------------------------------

// Represents a string-based Var.
//
// Returning a given string as a PP_Var:
//   return StringVar::StringToPPVar(module, my_string);
//
// Converting a PP_Var to a string:
//   scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
//   if (!string)
//     return false;  // Not a string or an invalid var.
//   DoSomethingWithTheString(string->value());
class StringVar : public Var {
 public:
  StringVar(PluginModule* module, const char* str, uint32 len);
  virtual ~StringVar();

  const std::string& value() const { return value_; }

  // Var override.
  virtual StringVar* AsStringVar() OVERRIDE;

  // Helper function to create a PP_Var of type string that contains a copy of
  // the given string. The input data must be valid UTF-8 encoded text, if it
  // is not valid UTF-8, a NULL var will be returned.
  //
  // The return value will have a reference count of 1. Internally, this will
  // create a StringVar, associate it with a module, and return the reference
  // to it in the var.
  static PP_Var StringToPPVar(PluginModule* module, const std::string& str);
  static PP_Var StringToPPVar(PluginModule* module,
                              const char* str, uint32 len);

  // Helper function that converts a PP_Var to a string. This will return NULL
  // if the PP_Var is not of string type or the string is invalid.
  static scoped_refptr<StringVar> FromPPVar(PP_Var var);

 private:
  std::string value_;

  DISALLOW_COPY_AND_ASSIGN(StringVar);
};

// ObjectVar -------------------------------------------------------------------

// Represents a JavaScript object Var. By itself, this represents random
// NPObjects that a given plugin (identified by the resource's module) wants to
// reference. If two different modules reference the same NPObject (like the
// "window" object), then there will be different ObjectVar's (and hence PP_Var
// IDs) for each module. This allows us to track all references owned by a
// given module and free them when the plugin exits independently of other
// plugins that may be running at the same time.
//
// See StringVar for examples, except obviously using NPObjects instead of
// strings.
class ObjectVar : public Var {
 public:
  virtual ~ObjectVar();

  // Var overrides.
  virtual ObjectVar* AsObjectVar() OVERRIDE;

  // Returns the underlying NPObject corresponding to this ObjectVar.
  // Guaranteed non-NULL.
  NPObject* np_object() const { return np_object_; }

  // Notification that the instance was deleted, the internal pointer will be
  // NULLed out.
  void InstanceDeleted();

  // Possibly NULL if the object has outlived its instance.
  PluginInstance* instance() const { return instance_; }

  // Helper function to create a PP_Var of type object that contains the given
  // NPObject for use byt he given module. Calling this function multiple times
  // given the same module + NPObject results in the same PP_Var, assuming that
  // there is still a PP_Var with a reference open to it from the previous
  // call.
  //
  // The module is necessary because we can have different modules pointing to
  // the same NPObject, and we want to keep their refs separate.
  //
  // If no ObjectVar currently exists corresponding to the NPObject, one is
  // created associated with the given module.
  static PP_Var NPObjectToPPVar(PluginInstance* instance, NPObject* object);

  // Helper function that converts a PP_Var to an object. This will return NULL
  // if the PP_Var is not of object type or the object is invalid.
  static scoped_refptr<ObjectVar> FromPPVar(PP_Var var);

 protected:
  // You should always use FromNPObject to create an ObjectVar. This function
  // guarantees that we maintain the 1:1 mapping between NPObject and
  // ObjectVar.
  ObjectVar(PluginInstance* instance, NPObject* np_object);

 private:
  // Possibly NULL if the object has outlived its instance.
  PluginInstance* instance_;

  // Guaranteed non-NULL, this is the underlying object used by WebKit. We
  // hold a reference to this object.
  NPObject* np_object_;

  DISALLOW_COPY_AND_ASSIGN(ObjectVar);
};

// TryCatch --------------------------------------------------------------------

// Instantiate this object on the stack to catch V8 exceptions and pass them
// to an optional out parameter supplied by the plugin.
class TryCatch {
 public:
  // The given exception may be NULL if the consumer isn't interested in
  // catching exceptions. If non-NULL, the given var will be updated if any
  // exception is thrown (so it must outlive the TryCatch object).
  //
  // The module associated with the exception is passed so we know which module
  // to associate any exception string with. It may be NULL if you don't know
  // the module at construction time, in which case you should set it later
  // by calling set_module().
  //
  // If an exception is thrown when the module is NULL, setting *any* exception
  // will result in using the InvalidObjectException.
  TryCatch(PluginModule* module, PP_Var* exception);
  ~TryCatch();

  // Get and set the module. This may be NULL (see the constructor).
  PluginModule* module() { return module_; }
  void set_module(PluginModule* module) { module_ = module; }

  // Returns true is an exception has been thrown. This can be true immediately
  // after construction if the var passed to the constructor is non-void.
  bool has_exception() const { return has_exception_; }

  // Sets the given exception. If no module has been set yet, the message will
  // be ignored (since we have no module to associate the string with) and the
  // SetInvalidObjectException() will be used instead.
  //
  // If an exception has been previously set, this function will do nothing
  // (normally you want only the first exception).
  void SetException(const char* message);

  // Sets the exception to be a generic message contained in a magic string
  // not associated with any module.
  void SetInvalidObjectException();

 private:
  static void Catch(void* self, const char* message);

  PluginModule* module_;

  // True if an exception has been thrown. Since the exception itself may be
  // NULL if the plugin isn't interested in getting the exception, this will
  // always indicate if SetException has been called, regardless of whether
  // the exception itself has been stored.
  bool has_exception_;

  // May be null if the consumer isn't interesting in catching exceptions.
  PP_Var* exception_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_VAR_H_
