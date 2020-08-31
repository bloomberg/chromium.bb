// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.ThreadUtils;

import java.lang.ref.WeakReference;

/**
 * Used for Js Java interaction, to delete the document start JavaScript snippet.
 */
public class ScriptReference {
    private WeakReference<AwContents> mAwContentsRef;
    private int mScriptId;

    public ScriptReference(AwContents awContents, int scriptId) {
        assert scriptId >= 0;
        mAwContentsRef = new WeakReference(awContents);
        mScriptId = scriptId;
    }

    // Must be called on UI thread.
    public void remove() {
        ThreadUtils.checkUiThread();

        AwContents awContents = mAwContentsRef.get();
        if (awContents == null) return;
        awContents.removeDocumentStartJavascript(mScriptId);
    }
}
