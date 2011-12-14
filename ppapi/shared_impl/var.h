// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_VAR_H_
#define PPAPI_SHARED_IMPL_VAR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class NPObjectVar;
class ProxyObjectVar;
class StringVar;
class VarTracker;

// Var -------------------------------------------------------------------------

// Represents a non-POD var.
class PPAPI_SHARED_EXPORT Var : public base::RefCounted<Var> {
 public:
  virtual ~Var();

  // Returns a string representing the given var for logging purposes.
  static std::string PPVarToLogString(PP_Var var);

  virtual StringVar* AsStringVar();
  virtual NPObjectVar* AsNPObjectVar();
  virtual ProxyObjectVar* AsProxyObjectVar();

  // Creates a PP_Var corresponding to this object. The return value will have
  // one reference addrefed on behalf of the caller.
  virtual PP_Var GetPPVar() = 0;

  // Returns the type of this var.
  virtual PP_VarType GetType() const = 0;

  // Returns the ID corresponing to the string or object if it exists already,
  // or 0 if an ID hasn't been generated for this object (the plugin is holding
  // no refs).
  //
  // Contrast to GetOrCreateVarID which creates the ID and a ref on behalf of
  // the plugin.
  int32 GetExistingVarID() const;

 protected:
  friend class VarTracker;

  Var();

  // Returns the unique ID associated with this string or object, creating it
  // if necessary. The return value will be 0 if the string or object is
  // invalid.
  //
  // This function will take a reference to the var that will be passed to the
  // caller.
  int32 GetOrCreateVarID();

  // Sets the internal object ID. This assumes that the ID hasn't been set
  // before. This is used in cases where the ID is generated externally.
  void AssignVarID(int32 id);

  // Reset the assigned object ID.
  void ResetVarID() { AssignVarID(0); };

 private:
  // This will be 0 if no ID has been assigned (this happens lazily).
  int32 var_id_;

  DISALLOW_COPY_AND_ASSIGN(Var);
};

// StringVar -------------------------------------------------------------------

// Represents a string-based Var.
//
// Returning a given string as a PP_Var:
//   return StringVar::StringToPPVar(my_string);
//
// Converting a PP_Var to a string:
//   StringVar* string = StringVar::FromPPVar(var);
//   if (!string)
//     return false;  // Not a string or an invalid var.
//   DoSomethingWithTheString(string->value());
class PPAPI_SHARED_EXPORT StringVar : public Var {
 public:
  StringVar(const std::string& str);
  StringVar(const char* str, uint32 len);
  virtual ~StringVar();

  const std::string& value() const { return value_; }

  // Var override.
  virtual StringVar* AsStringVar() OVERRIDE;
  virtual PP_Var GetPPVar() OVERRIDE;
  virtual PP_VarType GetType() const OVERRIDE;

  // Helper function to create a PP_Var of type string that contains a copy of
  // the given string. The input data must be valid UTF-8 encoded text, if it
  // is not valid UTF-8, a NULL var will be returned.
  //
  // The return value will have a reference count of 1. Internally, this will
  // create a StringVar and return the reference to it in the var.
  static PP_Var StringToPPVar(const std::string& str);
  static PP_Var StringToPPVar(const char* str, uint32 len);

  // Helper function that converts a PP_Var to a string. This will return NULL
  // if the PP_Var is not of string type or the string is invalid.
  static StringVar* FromPPVar(PP_Var var);

 private:
  std::string value_;

  DISALLOW_COPY_AND_ASSIGN(StringVar);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_VAR_H_
