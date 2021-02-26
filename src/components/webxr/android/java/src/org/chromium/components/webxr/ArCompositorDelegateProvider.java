// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.content_public.browser.WebContents;

/**
 * Interface used to create instances of ArCompositorDelegates, needed by
 * ArImmersiveOverlay to work correctly for WebXR's DOM Overlay.
 */
public interface ArCompositorDelegateProvider {
    ArCompositorDelegate create(WebContents webContents);
}