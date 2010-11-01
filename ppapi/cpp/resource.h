// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_RESOURCE_H_
#define PPAPI_CPP_RESOURCE_H_

#include "ppapi/c/pp_resource.h"

namespace pp {

// Base class for refcounted plugin resources.
class Resource {
 public:
  Resource();
  Resource(const Resource& other);

  virtual ~Resource();

  Resource& operator=(const Resource& other);
  void swap(Resource& other);

  // Returns true if the given resource is invalid or uninitialized.
  bool is_null() const { return !pp_resource_; }

  PP_Resource pp_resource() const { return pp_resource_; }

  // Releases ownership of the PP_Resource and returns it to the caller.
  // Note the the reference count on the resource is unchanged and the caller
  // needs to release the resource.
  PP_Resource detach();

 protected:
  // This constructor is used when we've gotten a PP_Resource as a return value
  // that we need to addref.
  explicit Resource(PP_Resource resource);

  // Called by derived class' constructors to initialize this Resource with
  // a PP_Resource that has already been AddRef'ed. It also assumes this object
  // has no current resource.
  //
  // The intended usage is that the derived class constructor will call the
  // default Resource constructor, then make a call to create a resource. It
  // then wants to assign the new resource (which, since it was returned by the
  // browser, is already AddRef'ed).
  void PassRefFromConstructor(PP_Resource resource);

 private:
  PP_Resource pp_resource_;
};

}  // namespace pp

inline bool operator==(const pp::Resource& lhs, const pp::Resource& rhs) {
  return lhs.pp_resource() == rhs.pp_resource();
}

#endif // PPAPI_CPP_RESOURCE_H_
