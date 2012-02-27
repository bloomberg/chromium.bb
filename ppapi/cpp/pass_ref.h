// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PASS_REF_H_
#define PPAPI_CPP_PASS_REF_H_

namespace pp {

/// An annotation for constructors and other functions that take ownership of
/// a pointer. For example, a resource constructor that takes ownership of a
/// passed in PP_Resource ref count would take this to differentiate from
/// the more typical use case of taking its own ref.
enum PassRef { PASS_REF };

}  // namespace pp

#endif  // PPAPI_CPP_PASS_REF_H_
