/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LocalWindowProxy_h
#define LocalWindowProxy_h

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/frame/LocalFrame.h"
#include "v8/include/v8.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class HTMLDocument;
class SecurityOrigin;

// Subclass of WindowProxy that only handles LocalFrame.
class LocalWindowProxy final : public WindowProxy {
 public:
  static LocalWindowProxy* create(v8::Isolate* isolate,
                                  LocalFrame& frame,
                                  RefPtr<DOMWrapperWorld> world) {
    return new LocalWindowProxy(isolate, frame, std::move(world));
  }

  // Update document object of the frame.
  void updateDocument();

  void namedItemAdded(HTMLDocument*, const AtomicString&);
  void namedItemRemoved(HTMLDocument*, const AtomicString&);

  // Update the security origin of a document
  // (e.g., after setting docoument.domain).
  void updateSecurityOrigin(SecurityOrigin*);

 private:
  LocalWindowProxy(v8::Isolate*, LocalFrame&, RefPtr<DOMWrapperWorld>);

  void initialize() override;
  void disposeContext(GlobalDetachmentBehavior) override;

  // Creates a new v8::Context with the window wrapper object as the global
  // object (aka the inner global).  Note that the window wrapper and its
  // prototype chain do not get fully initialized yet, e.g. the window
  // wrapper is not yet associated with the native DOMWindow object.
  void createContext();

  void setSecurityToken(SecurityOrigin*);

  // Triggers updates of objects that are associated with a Document:
  // - the activity logger
  // - the document DOM wrapper
  // - the security origin
  void updateDocumentInternal();

  // The JavaScript wrapper for the document object is cached on the global
  // object for fast access. UpdateDocumentProperty sets the wrapper
  // for the current document on the global object.
  void updateDocumentProperty();

  // Updates Activity Logger for the current context.
  void updateActivityLogger();

  LocalFrame* frame() const { return toLocalFrame(WindowProxy::frame()); }
};

}  // namespace blink

#endif  // LocalWindowProxy_h
