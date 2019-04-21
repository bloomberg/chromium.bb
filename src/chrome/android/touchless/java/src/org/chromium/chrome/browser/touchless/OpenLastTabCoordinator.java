// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;

import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Provides data for the view and coordinates interactivity.
 */
class OpenLastTabCoordinator {
    private final OpenLastTabMediator mMediator;

    OpenLastTabCoordinator(
            Context context, Profile profile, NativePageHost nativePageHost, OpenLastTabView view) {
        PropertyModel model = new PropertyModel.Builder(OpenLastTabProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, view, OpenLastTabViewBinder::bind);

        mMediator = new OpenLastTabMediator(context, profile, nativePageHost, model, view);
    }

    void destroy() {
        mMediator.destroy();
    }
}
