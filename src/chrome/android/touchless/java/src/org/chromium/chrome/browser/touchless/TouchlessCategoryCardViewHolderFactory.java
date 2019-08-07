// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.chrome.browser.explore_sites.CategoryCardViewHolderFactory;
import org.chromium.chrome.touchless.R;

/**
 * Factory to provide resources for touchless devices.
 */
class TouchlessCategoryCardViewHolderFactory extends CategoryCardViewHolderFactory {
    @Override
    protected int getCategoryCardViewResource() {
        return R.layout.touchless_explore_sites_category_card_view;
    }

    @Override
    protected int getTileViewResource() {
        return R.layout.touchless_explore_sites_tile_view_condensed;
    }
}
