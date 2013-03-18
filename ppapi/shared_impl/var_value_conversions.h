// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_VAR_VALUE_CONVERSIONS_H_
#define PPAPI_SHARED_IMPL_VAR_VALUE_CONVERSIONS_H_

#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace base {
class Value;
}

namespace ppapi {

// Converts a PP_Var to a base::Value object. The caller takes ownership of the
// returned object.
//
// Both PP_VARTYPE_UNDEFINED and PP_VARTYPE_NULL are converted to
// base::Value::TYPE_NULL. In dictionary vars, key-value pairs whose value is
// undefined (PP_VARTYPE_UNDEFINED) are ignored. If a node in |var| appears more
// than once, it is duplicated in the result. For example, if |var| is an array
// and it has two elements pointing to the same dictionary, the resulting list
// value will have two copies of the dictionary.
//
// The conversion fails and returns NULL if
// - |var| is object (PP_VARTYPE_OBJECT); or
// - |var| is an array or dictionary, and calling CreateValueFromVar() on any of
//   the array elements or dictionary values fails; or
// - there exist circular references, i.e., an array or dictionary is its own
//   ancestor/descendant.
PPAPI_SHARED_EXPORT base::Value* CreateValueFromVar(const PP_Var& var);

// The returned var has been added ref on behalf of the caller.
// Returns an undefined var if the conversion fails.
PPAPI_SHARED_EXPORT PP_Var CreateVarFromValue(const base::Value& value);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_VAR_VALUE_CONVERSIONS_H_
