// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.content_public.browser.WebContents;

/**
 * Factory implementation to create AutofillAssistantClient as
 * as AutofillAssistantModuleEntry to serve as interface between
 * base module and assistant DFM.
 */
@UsedByReflection("AutofillAssistantModuleEntryProvider.java")
public class AutofillAssistantModuleEntryFactoryImpl
        implements AutofillAssistantModuleEntryFactory {
    @Override
    public AutofillAssistantModuleEntry createEntry(WebContents webContents) {
        return AutofillAssistantClient.fromWebContents(webContents);
    }
}