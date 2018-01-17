/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/IdentifiersFactory.h"

#include "core/dom/WeakIdentifierMap.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/inspector/InspectedFrames.h"
#include "core/loader/DocumentLoader.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

volatile int g_last_used_identifier = 0;

}  // namespace

// static
String IdentifiersFactory::CreateIdentifier() {
  int identifier = AtomicIncrement(&g_last_used_identifier);
  return AddProcessIdPrefixTo(identifier);
}

// static
String IdentifiersFactory::RequestId(DocumentLoader* loader,
                                     unsigned long identifier) {
  if (!identifier)
    return String();
  if (loader && loader->MainResourceIdentifier() == identifier)
    return LoaderId(loader);
  return AddProcessIdPrefixTo(identifier);
}

// static
String IdentifiersFactory::SubresourceRequestId(unsigned long identifier) {
  return RequestId(nullptr, identifier);
}

// static
String IdentifiersFactory::FrameId(Frame* frame) {
  if (!frame)
    return g_empty_string;
  return frame->GetDevToolsFrameToken();
}

// static
LocalFrame* IdentifiersFactory::FrameById(InspectedFrames* inspected_frames,
                                          const String& frame_id) {
  for (auto* frame : *inspected_frames) {
    if (frame->Client() && frame->GetDevToolsFrameToken() == frame_id)
      return frame;
  }
  return nullptr;
}

// static
String IdentifiersFactory::LoaderId(DocumentLoader* loader) {
  if (!loader)
    return g_empty_string;
  const base::UnguessableToken& token = loader->GetDevToolsNavigationToken();
  // token.ToString() is latin1.
  return String(token.ToString().c_str());
}

// static
String IdentifiersFactory::AddProcessIdPrefixTo(int id) {
  static uint32_t process_id = Platform::Current()->GetUniqueIdForProcess();

  StringBuilder builder;

  builder.AppendNumber(process_id);
  builder.Append('.');
  builder.AppendNumber(id);

  return builder.ToString();
}

// static
int IdentifiersFactory::RemoveProcessIdPrefixFrom(const String& id, bool* ok) {
  size_t dot_index = id.find('.');

  if (dot_index == kNotFound) {
    *ok = false;
    return 0;
  }
  return id.Substring(dot_index + 1).ToInt(ok);
}

}  // namespace blink
