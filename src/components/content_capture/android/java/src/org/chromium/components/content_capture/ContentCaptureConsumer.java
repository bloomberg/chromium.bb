// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

/**
 * This interface is for consumer to consume the captured content.
 */
public interface ContentCaptureConsumer {
    /**
     * Invoked when the content is captured from a frame.
     * @param parentFrame is the parent of the frame from that the content captured.
     * @param contentCaptureData is the captured content tree, its root is the frame.
     */
    void onContentCaptured(FrameSession parentFrame, ContentCaptureData contentCaptureData);

    /**
     * Invoked when the session is removed
     * @param session is the removed frame.
     */
    void onSessionRemoved(FrameSession session);

    /**
     * Invoked when the content is removed from a frame
     * @param session defines the frame from that the content removed
     * @param removedIds are array of removed content id.
     */
    void onContentRemoved(FrameSession session, long[] removedIds);
}
