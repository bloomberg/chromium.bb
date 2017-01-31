// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URLFileAPI_h
#define URLFileAPI_h

#include "wtf/Allocator.h"
#include "wtf/Forward.h"

namespace blink {

class ExceptionState;
class ScriptState;
class Blob;

class URLFileAPI {
  STATIC_ONLY(URLFileAPI);

 public:
  static String createObjectURL(ScriptState*, Blob*, ExceptionState&);
  static void revokeObjectURL(ScriptState*, const String&);
};

}  // namespace blink

#endif
