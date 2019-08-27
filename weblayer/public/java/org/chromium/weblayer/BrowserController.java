// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Activity;

import org.chromium.weblayer_private.BrowserControllerImpl;

public final class BrowserController {
    private BrowserControllerImpl mBrowserController;
    private NavigationController mNavigationController;

    public BrowserController(Activity activity, Profile profile) {
        mBrowserController = new BrowserControllerImpl(activity, profile.getProfileImpl());
        mNavigationController = new NavigationController(mBrowserController);
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mProfile is non-null.
    }

    public NavigationController getNavigationController() {
        return mNavigationController;
    }
}
