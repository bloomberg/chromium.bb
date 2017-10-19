// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/html/media/HTMLMediaElement.h"
#include "modules/remoteplayback/RemotePlayback.h"

namespace blink {

// static
bool HTMLMediaElementRemotePlayback::FastHasAttribute(
    const QualifiedName& name,
    const HTMLMediaElement& element) {
  DCHECK(name == HTMLNames::disableremoteplaybackAttr);
  return element.FastHasAttribute(name);
}

// static
void HTMLMediaElementRemotePlayback::SetBooleanAttribute(
    const QualifiedName& name,
    HTMLMediaElement& element,
    bool value) {
  DCHECK(name == HTMLNames::disableremoteplaybackAttr);
  element.SetBooleanAttribute(name, value);

  HTMLMediaElementRemotePlayback& self =
      HTMLMediaElementRemotePlayback::From(element);
  if (self.remote_ && value)
    self.remote_->RemotePlaybackDisabled();
}

// static
HTMLMediaElementRemotePlayback& HTMLMediaElementRemotePlayback::From(
    HTMLMediaElement& element) {
  HTMLMediaElementRemotePlayback* supplement =
      static_cast<HTMLMediaElementRemotePlayback*>(
          Supplement<HTMLMediaElement>::From(element, SupplementName()));
  if (!supplement) {
    supplement = new HTMLMediaElementRemotePlayback();
    ProvideTo(element, SupplementName(), supplement);
  }
  return *supplement;
}

// static
RemotePlayback* HTMLMediaElementRemotePlayback::remote(
    HTMLMediaElement& element) {
  HTMLMediaElementRemotePlayback& self =
      HTMLMediaElementRemotePlayback::From(element);
  Document& document = element.GetDocument();
  if (!document.GetFrame())
    return nullptr;

  if (!self.remote_)
    self.remote_ = RemotePlayback::Create(element);

  return self.remote_;
}

// static
const char* HTMLMediaElementRemotePlayback::SupplementName() {
  return "HTMLMediaElementRemotePlayback";
}

void HTMLMediaElementRemotePlayback::Trace(blink::Visitor* visitor) {
  visitor->Trace(remote_);
  Supplement<HTMLMediaElement>::Trace(visitor);
}

}  // namespace blink
