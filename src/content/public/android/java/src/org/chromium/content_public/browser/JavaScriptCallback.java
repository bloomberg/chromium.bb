// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/** Callback interface for WebContents evaluateJavaScript(). */
public interface JavaScriptCallback {
    /**
     * Called from native in response to evaluateJavaScript().
     * @param jsonResult json result curresponds to JS execution
     */
    void handleJavaScriptResult(String jsonResult);
}
