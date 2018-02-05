// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_CORE_DOM_ABORTCONTROLLER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_CORE_DOM_ABORTCONTROLLER_H_

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Member.h"

namespace blink {

class AbortSignal;
class ExecutionContext;

// Implementation of https://dom.spec.whatwg.org/#interface-abortcontroller
// See also design doc at
// https://docs.google.com/document/d/1OuoCG2uiijbAwbCw9jaS7tHEO0LBO_4gMNio1ox0qlY/edit
class AbortController final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AbortController* Create(ExecutionContext*);
  virtual ~AbortController();

  // AbortController.idl

  // https://dom.spec.whatwg.org/#dom-abortcontroller-signal
  AbortSignal* signal() const { return signal_.Get(); }

  // https://dom.spec.whatwg.org/#dom-abortcontroller-abort
  void abort();

  virtual void Trace(Visitor*);

 private:
  explicit AbortController(ExecutionContext*);

  Member<AbortSignal> signal_;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_CORE_DOM_ABORTCONTROLLER_H_
