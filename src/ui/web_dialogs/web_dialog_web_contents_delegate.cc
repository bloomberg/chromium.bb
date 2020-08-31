// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

#include <utility>

#include "base/check.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"

using content::BrowserContext;
using content::OpenURLParams;
using content::WebContents;

namespace ui {

// Incognito profiles are not long-lived, so we always want to store a
// non-incognito profile.
//
// TODO(akalin): Should we make it so that we have a default incognito
// profile that's long-lived?  Of course, we'd still have to clear it out
// when all incognito browsers close.
WebDialogWebContentsDelegate::WebDialogWebContentsDelegate(
    content::BrowserContext* browser_context,
    std::unique_ptr<WebContentsHandler> handler)
    : browser_context_(browser_context), handler_(std::move(handler)) {
  DCHECK(handler_);
}

WebDialogWebContentsDelegate::~WebDialogWebContentsDelegate() {
}

void WebDialogWebContentsDelegate::Detach() {
  browser_context_ = nullptr;
}

WebContents* WebDialogWebContentsDelegate::OpenURLFromTab(
    WebContents* source, const OpenURLParams& params) {
  return handler_->OpenURLFromTab(browser_context_, source, params);
}

void WebDialogWebContentsDelegate::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  // TODO(erikchen): Refactor AddNewContents to take strong ownership semantics.
  // https://crbug.com/832879.
  handler_->AddNewContents(browser_context_, source, std::move(new_contents),
                           target_url, disposition, initial_rect, user_gesture);
}

bool WebDialogWebContentsDelegate::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

}  // namespace ui
