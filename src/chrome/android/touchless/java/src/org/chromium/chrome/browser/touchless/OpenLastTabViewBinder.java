// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binder between the OpenLastTabModel and OpenLastTabButton.
 */
class OpenLastTabViewBinder {
    public static void bind(PropertyModel model, OpenLastTabView view, PropertyKey propertyKey) {
        if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_ON_CLICK_LISTENER) {
            view.setOpenLastTabOnClickListener(
                    model.get(OpenLastTabProperties.OPEN_LAST_TAB_ON_CLICK_LISTENER));
        } else if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_FAVICON) {
            view.setFavicon(model.get(OpenLastTabProperties.OPEN_LAST_TAB_FAVICON));
        } else if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_TITLE) {
            view.setTitle(model.get(OpenLastTabProperties.OPEN_LAST_TAB_TITLE));
        } else if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_TIMESTAMP) {
            view.setTimestamp(model.get(OpenLastTabProperties.OPEN_LAST_TAB_TIMESTAMP));
        } else if (propertyKey == OpenLastTabProperties.OPEN_LAST_TAB_LOAD_SUCCESS) {
            view.setLoadSuccess(model.get(OpenLastTabProperties.OPEN_LAST_TAB_LOAD_SUCCESS));
        } else {
            assert false : "Unhandled property detected.";
        }
    }
}
