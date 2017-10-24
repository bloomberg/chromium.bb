// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef External_h
#define External_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class External : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  External() = default;

  void AddSearchProvider() {}
  void IsSearchProviderInstalled() {}
};

}  // namespace blink

#endif  // External_h
