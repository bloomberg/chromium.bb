// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

namespace views {

class NativeViewHost;

class VIEWS_EXPORT WebView : public View {
 public:
  explicit WebView(content::BrowserContext* browser_context);
  virtual ~WebView();

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  void Init();

  // Overridden from View:
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;

  NativeViewHost* wcv_holder_;
  scoped_ptr<content::WebContents> web_contents_;
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(WebView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_