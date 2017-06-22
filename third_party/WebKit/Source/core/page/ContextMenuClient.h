/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef ContextMenuClient_h
#define ContextMenuClient_h

#include "core/CoreExport.h"
#include "public/platform/WebMenuSourceType.h"

namespace blink {

class ContextMenu;
class Document;
class Editor;
class WebViewBase;
struct WebContextMenuData;

class CORE_EXPORT ContextMenuClient {
 public:
  explicit ContextMenuClient(WebViewBase& web_view) : web_view_(&web_view) {}
  virtual ~ContextMenuClient() {}

  // Returns whether a Context Menu was actually shown.
  virtual bool ShowContextMenu(const ContextMenu*, WebMenuSourceType);
  virtual void ClearContextMenu();

 protected:
  ContextMenuClient() : web_view_(nullptr) {}

 private:
  void PopulateCustomMenuItems(const ContextMenu*, WebContextMenuData*);
  static int ComputeEditFlags(Document&, Editor&);
  bool ShouldShowContextMenuFromTouch(const blink::WebContextMenuData&);
  WebViewBase* web_view_;
};

}  // namespace blink

#endif
