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

#include "public/platform/modules/indexeddb/WebIDBKeyRange.h"

#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "public/platform/modules/indexeddb/WebIDBKey.h"

namespace blink {

void WebIDBKeyRange::Assign(const WebIDBKeyRange& other) {
  private_ = other.private_;
}

void WebIDBKeyRange::Assign(const WebIDBKey& lower,
                            const WebIDBKey& upper,
                            bool lower_open,
                            bool upper_open) {
  if (!lower.IsValid() && !upper.IsValid()) {
    private_.Reset();
  } else {
    private_ = IDBKeyRange::Create(lower, upper,
                                   lower_open ? IDBKeyRange::kLowerBoundOpen
                                              : IDBKeyRange::kLowerBoundClosed,
                                   upper_open ? IDBKeyRange::kUpperBoundOpen
                                              : IDBKeyRange::kUpperBoundClosed);
  }
}

void WebIDBKeyRange::Reset() {
  private_.Reset();
}

WebIDBKey WebIDBKeyRange::Lower() const {
  if (!private_.Get())
    return WebIDBKey::CreateInvalid();
  return WebIDBKey(private_->Lower());
}

WebIDBKey WebIDBKeyRange::Upper() const {
  if (!private_.Get())
    return WebIDBKey::CreateInvalid();
  return WebIDBKey(private_->Upper());
}

bool WebIDBKeyRange::LowerOpen() const {
  return private_.Get() && private_->lowerOpen();
}

bool WebIDBKeyRange::UpperOpen() const {
  return private_.Get() && private_->upperOpen();
}

}  // namespace blink
