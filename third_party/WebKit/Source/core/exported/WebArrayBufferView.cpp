/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebArrayBufferView.h"

#include "bindings/core/v8/V8ArrayBufferView.h"

namespace blink {

void WebArrayBufferView::Assign(const WebArrayBufferView& other) {
  private_ = other.private_;
}

void WebArrayBufferView::Reset() {
  private_.Reset();
}

void* WebArrayBufferView::BaseAddress() const {
  return private_->BaseAddress();
}

unsigned WebArrayBufferView::ByteOffset() const {
  return private_->byteOffset();
}

unsigned WebArrayBufferView::ByteLength() const {
  return private_->byteLength();
}

WebArrayBufferView* WebArrayBufferView::CreateFromV8Value(
    v8::Local<v8::Value> value) {
  if (!value->IsArrayBufferView())
    return nullptr;
  DOMArrayBufferView* view = V8ArrayBufferView::ToImpl(value.As<v8::Object>());
  return new WebArrayBufferView(view);
}

WebArrayBufferView::WebArrayBufferView(DOMArrayBufferView* value)
    : private_(value) {}

WebArrayBufferView& WebArrayBufferView::operator=(DOMArrayBufferView* value) {
  private_ = value;
  return *this;
}

WebArrayBufferView::operator DOMArrayBufferView*() const {
  return private_.Get();
}

}  // namespace blink
