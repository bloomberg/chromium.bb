// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_CONTROLS_NAVIGATION_STATE_HANDLER_DELEGATE_H_
#define WEBLAYER_BROWSER_BROWSER_CONTROLS_NAVIGATION_STATE_HANDLER_DELEGATE_H_

#include "content/public/common/browser_controls_state.h"

namespace weblayer {

// Called to propagate changse to BrowserControlsState.
class BrowserControlsNavigationStateHandlerDelegate {
 public:
  // Called when the state changes.
  virtual void OnBrowserControlsStateStateChanged(
      content::BrowserControlsState state) = 0;

  // Called when UpdateBrowserControlsState() should be called because a new
  // navigation started. This is necessary as the browser-controls state is
  // specific to a renderer, and each navigation may trigger a new renderer.
  virtual void OnUpdateBrowserControlsStateBecauseOfProcessSwitch(
      bool did_commit) = 0;

  // Called if the browser controls need to be shown and the renderer is in a
  // hung/crashed state. This needs to be handled specially as the renderer
  // normally drives the offsets, but in this situation it won't.
  virtual void OnForceBrowserControlsShown() = 0;

 protected:
  virtual ~BrowserControlsNavigationStateHandlerDelegate() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_CONTROLS_NAVIGATION_STATE_HANDLER_DELEGATE_H_
