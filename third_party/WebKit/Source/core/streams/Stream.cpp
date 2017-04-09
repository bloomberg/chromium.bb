/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/streams/Stream.h"

#include "platform/blob/BlobData.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/blob/BlobURL.h"

namespace blink {

Stream::Stream(ExecutionContext* context, const String& media_type)
    : SuspendableObject(context), media_type_(media_type), is_neutered_(false) {
  // Create a new internal URL for a stream and register it with the provided
  // media type.
  internal_url_ = BlobURL::CreateInternalStreamURL();
  BlobRegistry::RegisterStreamURL(internal_url_, media_type_);
}

void Stream::AddData(const char* data, size_t len) {
  RefPtr<RawData> buffer(RawData::Create());
  buffer->MutableData()->Resize(len);
  memcpy(buffer->MutableData()->Data(), data, len);
  BlobRegistry::AddDataToStream(internal_url_, buffer);
}

void Stream::Flush() {
  BlobRegistry::FlushStream(internal_url_);
}

void Stream::Finalize() {
  BlobRegistry::FinalizeStream(internal_url_);
}

void Stream::Abort() {
  BlobRegistry::AbortStream(internal_url_);
}

Stream::~Stream() {
  BlobRegistry::UnregisterStreamURL(internal_url_);
}

void Stream::Suspend() {}

void Stream::Resume() {}

void Stream::ContextDestroyed(ExecutionContext*) {
  Neuter();
  Abort();
}

DEFINE_TRACE(Stream) {
  SuspendableObject::Trace(visitor);
}

}  // namespace blink
