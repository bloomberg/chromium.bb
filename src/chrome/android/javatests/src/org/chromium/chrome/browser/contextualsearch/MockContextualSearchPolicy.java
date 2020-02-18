// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

/**
 * A mock ContextualSearchPolicy class that excludes any business logic.
 * TODO(mdjones): Allow the return values of these function to be set.
 */
public class MockContextualSearchPolicy extends ContextualSearchPolicy {
    public MockContextualSearchPolicy() {
        super(null, null);
    }

    @Override
    public boolean shouldPrefetchSearchResult() {
        return false;
    }

    @Override
    public boolean maySendBasePageUrl() {
        return false;
    }

    @Override
    public boolean shouldAnimateSearchProviderIcon() {
        return false;
    }

    @Override
    public boolean isPromoAvailable() {
        return false;
    }
}
