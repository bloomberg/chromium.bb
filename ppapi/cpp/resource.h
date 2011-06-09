// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_RESOURCE_H_
#define PPAPI_CPP_RESOURCE_H_

#include "ppapi/c/pp_resource.h"

/// @file
/// This file defines a Resource type representing data associated
/// with the module.
namespace pp {

/// A reference counted module resource.
class Resource {
 public:

  /// The default constructor.
  Resource();

  /// A constructor for copying a resource.
  ///
  /// @param[in] other A Resource.
  Resource(const Resource& other);

  /// Destructor.
  virtual ~Resource();

  /// This function assigns one Resource to another Resource.
  ///
  /// @param[in] other A Resource.
  /// @return A Resource containing the assigned Resource.
  Resource& operator=(const Resource& other);

  /// This functions determines if this resource is invalid or
  /// uninitialized.
  ///
  /// @return true if this resource is invalid or uninitialized.
  bool is_null() const { return !pp_resource_; }

  PP_Resource pp_resource() const { return pp_resource_; }

  /// This function releases ownership of this resource and returns it to the
  /// caller.
  ///
  /// Note that the reference count on the resource is unchanged and the caller
  /// needs to release the resource.
  ///
  /// @return The detached PP_Resource.
  PP_Resource detach();

 protected:
  /// A constructor used when a PP_Resource is provided as a return value
  /// whose reference count we need to increment.
  ///
  /// @param[in] resource A PP_Resource.
  explicit Resource(PP_Resource resource);

  /// This function is called by derived class' constructors to initialize this
  /// Resource with a PP_Resource that has already had its reference count
  /// incremented by Core::AddRefResource. It also assumes this object has no
  /// current resource.
  ///
  /// The intended usage is that the derived class constructor will call the
  /// default Resource constructor, then make a call to create a resource. It
  /// then wants to assign the new resource (which, since it was returned by the
  /// browser, is already AddRef'ed).
  ///
  /// @param[in] resource A PP_Resource.
  void PassRefFromConstructor(PP_Resource resource);

 private:
  PP_Resource pp_resource_;
};

}  // namespace pp

inline bool operator==(const pp::Resource& lhs, const pp::Resource& rhs) {
  return lhs.pp_resource() == rhs.pp_resource();
}

#endif // PPAPI_CPP_RESOURCE_H_
