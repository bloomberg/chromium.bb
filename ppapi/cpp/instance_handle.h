// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_INSTANCE_HANDLE_H_
#define PPAPI_CPP_INSTANCE_HANDLE_H_

#include "ppapi/c/pp_instance.h"

namespace pp {

class Instance;

/// An instance handle identifies an instance in a constructor for a resource.
/// Its existence solves two different problems:
///
/// A pp::Instance objects' lifetime is managed by the system on the main thread
/// of the plugin. This means that it may get destroyed at any time based on
/// something that happens on the web page. This means that it's never OK to
/// refer to a pp::Instance object on a background thread. So we need to pass
/// some kind of identifier instead to resource constructors so that they may
/// safely be used on background threads. If the instance becomes invalid, the
/// resource creation will fail on the background thread, but it won't crash.
///
/// Background: PP_Instance would be a good identifier to use for this case.
/// However, using it in the constructor to resources is problematic.
/// PP_Instance is just a typedef for an integer, as is a PP_Resource. Many
/// resources have alternate constructors that just take an existing
/// PP_Resource, so the constructors would be ambiguous. Having this wrapper
/// around a PP_Instance prevents this ambiguity, and also gives us a nice
/// place to consolidate an implicit conversion from pp::Instance* for prettier
/// code on the main thread (you can just pass "this" to resource constructors
/// in your instance objects).
///
/// So you should always pass InstanceHandles to background threads instead of
/// a pp::Instance, and use them in resource constructors and code that may be
/// used from background threads.
class InstanceHandle {
 public:
  /// Implicit constructor for converting a pp::Instance to an instance handle.
  InstanceHandle(Instance* instance);

  /// Explicitly convert a PP_Instance to an instance handle. This should not
  /// be implicit because it can make some resource constructors ambiguous.
  /// PP_Instance is just a typedef for an integer, as is PP_Resource, so the
  /// compiler can get confused between the two.
  explicit InstanceHandle(PP_Instance pp_instance)
      : pp_instance_(pp_instance) {}

  PP_Instance pp_instance() const { return pp_instance_; }

 private:
  PP_Instance pp_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_INSTANCE_HANDLE_H_
