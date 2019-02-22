// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.ColorStateList;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.toolbar.TabCountProvider.TabCountObserver;
import org.chromium.chrome.browser.toolbar.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.util.AccessibilityUtil;

/**
 * The controller for the tab switcher button. This class handles all interactions that the tab
 * switcher button has with the outside world.
 */
public class TabSwitcherButtonCoordinator {
    /**
     *  The model that handles events from outside the tab switcher button. Normally the coordinator
     *  should acces the mediator which then updates the model. Since this component is very simple
     *  the mediator is omitted.
     */
    private final PropertyModel mTabSwitcherButtonModel =
            new PropertyModel(TabSwitcherButtonProperties.ALL_KEYS);

    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;

    private ThemeColorProvider mThemeColorProvider;
    private ThemeColorObserver mThemeColorObserver;

    private TabCountProvider mTabCountProvider;
    private TabCountObserver mTabCountObserver;

    /**
     * Build the controller that manages the tab switcher button.
     * @param root The root {@link ViewGroup} for locating the view to inflate.
     */
    public TabSwitcherButtonCoordinator(ViewGroup root) {
        final TabSwitcherButtonView view = root.findViewById(R.id.tab_switcher_button);
        PropertyModelChangeProcessor.create(
                mTabSwitcherButtonModel, view, new TabSwitcherButtonViewBinder());

        CharSequence description = root.getResources().getString(R.string.open_tabs);
        mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.ON_LONG_CLICK_LISTENER,
                v -> AccessibilityUtil.showAccessibilityToast(root.getContext(), v, description));
    }

    /**
     * @param onClickListener An {@link OnClickListener} that is triggered when the tab switcher
     *                        button is clicked.
     */
    public void setTabSwitcherListener(OnClickListener onClickListener) {
        mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.ON_CLICK_LISTENER, onClickListener);
    }

    public void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorObserver = new ThemeColorObserver() {
            @Override
            public void onThemeColorChanged(ColorStateList tint, int primaryColor) {
                mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.TINT, tint);
            }
        };
        mThemeColorProvider.addObserver(mThemeColorObserver);
    }

    public void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        mTabCountObserver = new TabCountObserver() {
            @Override
            public void onTabCountChanged(int tabCount, boolean isIncognito) {
                mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.NUMBER_OF_TABS, tabCount);
            }
        };
        mTabCountProvider.addObserver(mTabCountObserver);
    }

    public void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeObserver(mThemeColorObserver);
            mThemeColorProvider = null;
        }
        if (mTabCountProvider != null) {
            mTabCountProvider.removeObserver(mTabCountObserver);
            mTabCountProvider = null;
        }
    }
}
