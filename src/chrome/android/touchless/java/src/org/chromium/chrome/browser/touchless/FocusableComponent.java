// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

/**
 * Mediators for focusable UI components can implement this to participate in focus save/restore for
 * a page.
 */
public interface FocusableComponent {
    /** Signals that the component should try to focus itself as soon as possible. */
    void requestFocus();

    /**
     * Sets a single callback to be invoked when the component is focused. Calling this multiple
     * times will override the previous listeners.
     */
    void setOnFocusListener(Runnable onFocus);
}