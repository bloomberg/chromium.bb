// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_DELEGATE_H_

namespace content {

class PageImpl;

// Interface implemented by an object (in practice, WebContentsImpl) which
// owns (possibly indirectly) and is interested in knowing about the state of
// one or more Pages. It must outlive the Page.
class PageDelegate {
 public:
  // Called when a paint happens after the first non empty layout. In other
  // words, after the page has painted something.
  virtual void OnFirstVisuallyNonEmptyPaint(PageImpl& page) {}

  // Called when the theme color (from the theme-color meta tag) has changed.
  virtual void OnThemeColorChanged(PageImpl& page) {}

  // Called when the main document background color has changed.
  virtual void OnBackgroundColorChanged(PageImpl& page) {}

  // Called when the main document color scheme was inferred.
  virtual void DidInferColorScheme(PageImpl& page) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_DELEGATE_H_
