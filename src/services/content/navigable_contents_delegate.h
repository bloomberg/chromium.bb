// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_NAVIGABLE_CONTENTS_DELEGATE_H_
#define SERVICES_CONTENT_NAVIGABLE_CONTENTS_DELEGATE_H_

#include "services/content/public/mojom/navigable_contents.mojom.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace content {

// A virtual interface which must be implemented as a backing for
// NavigableContentsImpl instances.
//
// This is the primary interface by which the Content Service delegates
// NavigableContentsImpl behavior out to WebContentsImpl in src/content. As such
// it is a transitional API which will be removed as soon as WebContentsImpl
// itself can be fully migrated into Content Service.
//
// Each instance of this interface is constructed by the ContentServiceDelegate
// implementation and owned by a NavigableContentsImpl.
class NavigableContentsDelegate {
 public:
  virtual ~NavigableContentsDelegate() {}

  // Returns a NativeView that can be embedded into a client application's
  // window tree to display the web contents navigated by the delegate's
  // NavigableContents.
  virtual gfx::NativeView GetNativeView() = 0;

  // Navigates the content object to a new URL.
  virtual void Navigate(const GURL& url, mojom::NavigateParamsPtr params) = 0;

  // Attempts to navigate the web contents back in its history stack. The
  // supplied |callback| is run to indicate success/failure of the attempt. The
  // navigation attempt will fail if the history stack is empty.
  virtual void GoBack(
      content::mojom::NavigableContents::GoBackCallback callback) = 0;

  // Attempts to transfer global input focus to the navigated contents if they
  // have an active visual representation.
  virtual void Focus() = 0;

  // Similar to above but for use specifically when UI element traversal is
  // being done via Tab-key cycling or a similar mechanism.
  virtual void FocusThroughTabTraversal(bool reverse) = 0;
};

}  // namespace content

#endif  // SERVICES_CONTENT_NAVIGABLE_CONTENTS_DELEGATE_H_
