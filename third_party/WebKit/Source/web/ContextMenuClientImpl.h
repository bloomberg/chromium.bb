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

#ifndef ContextMenuClientImpl_h
#define ContextMenuClientImpl_h

#include "core/page/ContextMenuClient.h"

namespace blink {

class Document;
class Editor;
class WebViewBase;
struct WebContextMenuData;

class ContextMenuClientImpl final : public ContextMenuClient {
 public:
  explicit ContextMenuClientImpl(WebViewBase* web_view) : web_view_(web_view) {}
  ~ContextMenuClientImpl() override {}
  bool ShowContextMenu(const ContextMenu*, bool from_touch) override;
  void ClearContextMenu() override;

 private:
  void PopulateCustomMenuItems(const ContextMenu*, WebContextMenuData*);
  static int ComputeEditFlags(Document&, Editor&);
  bool ShouldShowContextMenuFromTouch(const blink::WebContextMenuData&);
  WebViewBase* web_view_;
};

}  // namespace blink

#endif  // ContextMenuClientImpl_h
