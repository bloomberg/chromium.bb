// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_ARRAY_VAR_H_
#define PPAPI_SHARED_IMPL_ARRAY_VAR_H_

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT ArrayVar : public Var {
 public:
  typedef std::vector<ScopedPPVar> ElementVector;

  ArrayVar();

  // Helper function that converts a PP_Var to an ArrayVar. This will return
  // NULL if the PP_Var is not of type PP_VARTYPE_ARRAY or the array cannot be
  // found from the var tracker.
  static ArrayVar* FromPPVar(const PP_Var& var);

  // Var overrides.
  ArrayVar* AsArrayVar() override;
  PP_VarType GetType() const override;

  // The returned PP_Var has had a ref added on behalf of the caller.
  PP_Var Get(uint32_t index) const;
  PP_Bool Set(uint32_t index, const PP_Var& value);
  uint32_t GetLength() const;
  PP_Bool SetLength(uint32_t length);

  const ElementVector& elements() const { return elements_; }

  ElementVector& elements() { return elements_; }

 protected:
  ~ArrayVar() override;

 private:
  ElementVector elements_;

  DISALLOW_COPY_AND_ASSIGN(ArrayVar);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_ARRAY_VAR_H_
