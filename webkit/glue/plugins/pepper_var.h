// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_VAR_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_VAR_H_

#include <string>

struct PP_Var;
struct PPB_Var;
typedef struct NPObject NPObject;
typedef struct _NPVariant NPVariant;
typedef void* NPIdentifier;

namespace pepper {

class String;

// There's no class implementing Var since it could represent a number of
// objects. Instead, we just expose a getter for the interface implemented in
// the .cc file here.
const PPB_Var* GetVarInterface();

// Returns a PP_Var of type object that wraps the given NPObject.  Calling this
// function multiple times given the same NPObject results in the same PP_Var.
PP_Var NPObjectToPPVar(NPObject* object);

// Returns a PP_Var that corresponds to the given NPVariant.  The contents of
// the NPVariant will be copied unless the NPVariant corresponds to an object.
PP_Var NPVariantToPPVar(const NPVariant* variant);

// Returns the NPObject corresponding to the PP_Var.  This pointer has not been
// retained, so you should not call WebBindings::releaseObject unless you first
// call WebBindings::retainObject.  Returns NULL if the PP_Var is not an object
// type.
NPObject* GetNPObject(PP_Var var);

// Returns a PP_Var of type string that contains a copy of the given string.
// The input data must be valid UTF-8 encoded text. The return value will
// have a reference count of 1.
PP_Var StringToPPVar(const std::string& str);

// Returns the String corresponding to the PP_Var.  This pointer has not been
// AddRef'd, so you should not call Release!  Returns NULL if the PP_Var is not
// a string type.
String* GetString(PP_Var var);

// Instantiate this object on the stack to catch V8 exceptions and pass them
// to an optional out parameter supplied by the plugin.
class TryCatch {
 public:
  // The given exception may be NULL if the consumer isn't interested in
  // catching exceptions. If non-NULL, the given var will be updated if any
  // exception is thrown (so it must outlive the TryCatch object).
  TryCatch(PP_Var* exception);
  ~TryCatch();

  // Returns true is an exception has been thrown. This can be true immediately
  // after construction if the var passed to the constructor is non-void.
  bool HasException() const;

  // Sets the given exception.
  void SetException(const char* message);

 private:
  static void Catch(void* self, const char* message);

  // May be null if the consumer isn't interesting in catching exceptions.
  PP_Var* exception_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_VAR_H_
