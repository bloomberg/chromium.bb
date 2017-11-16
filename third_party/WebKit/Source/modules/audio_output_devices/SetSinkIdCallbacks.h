// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SetSinkIdCallbacks_h
#define SetSinkIdCallbacks_h

#include "base/memory/scoped_refptr.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebSetSinkIdCallbacks.h"

namespace blink {

class HTMLMediaElement;
class ScriptPromiseResolver;

class SetSinkIdCallbacks final : public WebSetSinkIdCallbacks {
  // FIXME(tasak): When making public/platform classes to use PartitionAlloc,
  // the following macro should be moved to WebCallbacks defined in
  // public/platform/WebCallbacks.h.
  USING_FAST_MALLOC(SetSinkIdCallbacks);
  WTF_MAKE_NONCOPYABLE(SetSinkIdCallbacks);

 public:
  SetSinkIdCallbacks(ScriptPromiseResolver*,
                     HTMLMediaElement&,
                     const String& sink_id);
  ~SetSinkIdCallbacks() override;

  void OnSuccess() override;
  void OnError(WebSetSinkIdError) override;

 private:
  Persistent<ScriptPromiseResolver> resolver_;
  Persistent<HTMLMediaElement> element_;
  String sink_id_;
};

}  // namespace blink

#endif  // SetSinkIdCallbacks_h
