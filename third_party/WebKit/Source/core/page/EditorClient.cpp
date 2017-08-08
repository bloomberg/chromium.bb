/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
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

#include "core/page/EditorClient.h"

#include "core/editing/SelectionType.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebViewClient.h"

namespace blink {

EditorClient::EditorClient(WebViewImpl& webview) : web_view_(&webview) {}

EditorClient::~EditorClient() {}

void EditorClient::RespondToChangedSelection(LocalFrame* frame,
                                             SelectionType selection_type) {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  if (web_frame->Client())
    web_frame->Client()->DidChangeSelection(selection_type != kRangeSelection);
}

void EditorClient::RespondToChangedContents() {
  if (web_view_->Client())
    web_view_->Client()->DidChangeContents();
}

bool EditorClient::CanCopyCut(LocalFrame* frame, bool default_value) const {
  if (!frame->GetContentSettingsClient())
    return default_value;
  return frame->GetContentSettingsClient()->AllowWriteToClipboard(
      default_value);
}

bool EditorClient::CanPaste(LocalFrame* frame, bool default_value) const {
  if (!frame->GetContentSettingsClient())
    return default_value;
  return frame->GetContentSettingsClient()->AllowReadFromClipboard(
      default_value);
}

bool EditorClient::HandleKeyboardEvent(LocalFrame* frame) {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  return web_frame->Client()->HandleCurrentKeyboardEvent();
}

}  // namespace blink
