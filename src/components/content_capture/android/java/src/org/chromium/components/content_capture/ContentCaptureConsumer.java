// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.content_public.browser.WebContents;

/**
 * This abstract class is for consumer to consume the captured content. This object is used by
 * multiple objects and typically released by the last RFH in the WebContents.
 */
public abstract class ContentCaptureConsumer {
    private ContentCaptureReceiverManager mManager;
    public ContentCaptureConsumer(WebContents webContents) {
        onWebContentsChanged(webContents);
    }

    /**
     * Invoked when the content is captured from a frame.
     * @param parentFrame is the parent of the frame from that the content captured.
     * @param contentCaptureFrame is the captured content tree, its root is the frame.
     */
    public abstract void onContentCaptured(
            FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame);

    /**
     * Invoked when the content is updated in a frame.
     * @param parentFrame is the parent of the frame from that the content captured.
     * @param contentCaptureFrame is the captured content tree, its root is the frame.
     */
    public abstract void onContentUpdated(
            FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame);

    /**
     * Invoked when the session is removed
     * @param session is the removed frame.
     */
    public abstract void onSessionRemoved(FrameSession session);

    /**
     * Invoked when the content is removed from a frame
     * @param session defines the frame from that the content removed
     * @param removedIds are array of removed content id.
     */
    public abstract void onContentRemoved(FrameSession session, long[] removedIds);

    /**
     * Invoked when the title is updated.
     * @param mainFrame the frame whose title is updated.
     */
    public abstract void onTitleUpdated(ContentCaptureFrame mainFrame);

    public void onWebContentsChanged(WebContents current) {
        if (!ContentCaptureFeatures.isEnabled()) return;
        // Not reset previous mManager's ContentCaptureConsumer, because it will be used to notify
        // of the removal of session.
        if (current != null) {
            mManager = ContentCaptureReceiverManager.createOrGet(current);
            mManager.addContentCaptureConsumer(this);
        } else {
            mManager = null;
        }
    }

    protected boolean shouldCapture(String[] urls) {
        return true;
    }
}
