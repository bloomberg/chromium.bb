// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VAR_RESOURCE_DEV_H_
#define PPAPI_CPP_VAR_RESOURCE_DEV_H_

#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the API for interacting with resource vars.

namespace pp {

class VarResource_Dev : public Var {
 public:
  /// Constructs a <code>VarResource_Dev</code> given a resource.
  explicit VarResource_Dev(const pp::Resource& resource);

  /// Constructs a <code>VarResource_Dev</code> given a var for which
  /// is_resource() is true. This will refer to the same resource var, but allow
  /// you to access methods specific to resources.
  ///
  /// @param[in] var A resource var.
  explicit VarResource_Dev(const Var& var);

  /// Copy constructor.
  VarResource_Dev(const VarResource_Dev& other);

  virtual ~VarResource_Dev();

  /// Assignment operator.
  VarResource_Dev& operator=(const VarResource_Dev& other);

  /// The <code>Var</code> assignment operator is overridden here so that we can
  /// check for assigning a non-resource var to a <code>VarResource_Dev</code>.
  ///
  /// @param[in] other The resource var to be assigned.
  ///
  /// @return The resulting <code>VarResource_Dev</code> (as a
  /// <code>Var</code>&).
  virtual Var& operator=(const Var& other);

  /// Gets the resource contained in the var.
  ///
  /// @return The <code>pp::Resource</code> that is contained in the var.
  pp::Resource AsResource();
};

}  // namespace pp

#endif  // PPAPI_CPP_VAR_RESOURCE_DEV_H_
