// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_VAR_H_
#define PPAPI_SHARED_IMPL_VAR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_module.h"

struct PP_Var;

namespace ppapi {

class NPObjectVar;
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
  virtual NPObjectVar* AsNPObjectVar();

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

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_VAR_H_
