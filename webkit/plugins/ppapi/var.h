// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_VAR_H_
#define WEBKIT_PLUGINS_PPAPI_VAR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"

struct PP_Var;
struct PPB_Var;
struct PPB_Var_Deprecated;
typedef struct NPObject NPObject;
typedef struct _NPVariant NPVariant;
typedef void* NPIdentifier;

namespace webkit {
namespace ppapi {

class ObjectVar;
class StringVar;

// Var -------------------------------------------------------------------------

// Represents a non-POD var. This is derived from a resource even though it
// isn't a resource from the plugin's perspective. This allows us to re-use
// the refcounting and the association with the module from the resource code.
class Var : public base::RefCounted<Var> {
 public:
  virtual ~Var();

  // Returns a string representing the given var for logging purposes.
  static std::string PPVarToLogString(PP_Var var);

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

  virtual StringVar* AsStringVar();
  virtual ObjectVar* AsObjectVar();

  // Creates a PP_Var corresponding to this object. The return value will have
  // one reference addrefed on behalf of the caller.
  virtual PP_Var GetPPVar() = 0;

  // Returns the ID corresponing to the string or object if it exists already,
  // or 0 if an ID hasn't been generated for this object (the plugin is holding
  // no refs).
  //
  // Contrast to GetOrCreateVarID which creates the ID and a ref on behalf of
  // the plugin.
  int32 GetExistingVarID() const;

  PP_Module pp_module() const { return pp_module_; }

 protected:
  // This can only be constructed as a StringVar or an ObjectVar.
  explicit Var(PP_Module module);

  // Returns the unique ID associated with this string or object, creating it
  // if necessary. The return value will be 0 if the string or object is
  // invalid.
  //
  // This function will take a reference to the var that will be passed to the
  // caller.
  int32 GetOrCreateVarID();

 private:
  PP_Module pp_module_;

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
  StringVar(PP_Module module, const char* str, uint32 len);
  virtual ~StringVar();

  const std::string& value() const { return value_; }

  // Var override.
  virtual StringVar* AsStringVar() OVERRIDE;
  virtual PP_Var GetPPVar() OVERRIDE;

  // Helper function to create a PP_Var of type string that contains a copy of
  // the given string. The input data must be valid UTF-8 encoded text, if it
  // is not valid UTF-8, a NULL var will be returned.
  //
  // The return value will have a reference count of 1. Internally, this will
  // create a StringVar, associate it with a module, and return the reference
  // to it in the var.
  static PP_Var StringToPPVar(PP_Module module, const std::string& str);
  static PP_Var StringToPPVar(PP_Module module, const char* str, uint32 len);

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
  // You should always use FromNPObject to create an ObjectVar. This function
  // guarantees that we maintain the 1:1 mapping between NPObject and
  // ObjectVar.
  ObjectVar(PP_Module module, PP_Instance instance, NPObject* np_object);

  virtual ~ObjectVar();

  // Var overrides.
  virtual ObjectVar* AsObjectVar() OVERRIDE;
  virtual PP_Var GetPPVar() OVERRIDE;

  // Returns the underlying NPObject corresponding to this ObjectVar.
  // Guaranteed non-NULL.
  NPObject* np_object() const { return np_object_; }

  // Notification that the instance was deleted, the internal reference will be
  // zeroed out.
  void InstanceDeleted();

  // Possibly 0 if the object has outlived its instance.
  PP_Instance pp_instance() const { return pp_instance_; }

  // Helper function that converts a PP_Var to an object. This will return NULL
  // if the PP_Var is not of object type or the object is invalid.
  static scoped_refptr<ObjectVar> FromPPVar(PP_Var var);

 private:
  // Possibly 0 if the object has outlived its instance.
  PP_Instance pp_instance_;

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
  TryCatch(PP_Module module, PP_Var* exception);
  ~TryCatch();

  // Get and set the module. This may be NULL (see the constructor).
  PP_Module pp_module() { return pp_module_; }
  void set_pp_module(PP_Module module) { pp_module_ = module; }

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

  PP_Module pp_module_;

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
