// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes;

import org.chromium.base.Callback;
import org.chromium.components.content_creation.notes.models.NoteTemplate;

import java.util.List;

/**
 * Interface for interacting with the native note service. The service is
 * responsible for fetching templates.
 */
public interface NoteService {
    /**
     * Called to get the list of available Note templates.
     */
    void getTemplates(Callback<List<NoteTemplate>> callback);

    /**
     * Whether the Publish feature is available.
     */
    boolean isPublishAvailable();

    /**
     * Called to publish the note to the server.
     */
    void publishNote(String selectedText, String shareUrl, Callback<String> callback);
}
