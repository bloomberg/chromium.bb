// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.view.GestureDetector;
import android.view.MotionEvent;

import org.chromium.chrome.browser.gesturenav.NavigationGlowFactory;
import org.chromium.chrome.browser.gesturenav.NavigationHandler;
import org.chromium.chrome.browser.gesturenav.TabbedActionDelegate;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;

/**
 * {@link ContentView.Delegate} implementation for adding history navigation logic
 * to Tab's container view of rendered pages.
 */
public class TabContentViewDelegate
        extends TabWebContentsUserData implements ContentView.TouchEventDelegate {
    private static final Class<TabContentViewDelegate> USER_DATA_KEY = TabContentViewDelegate.class;

    private final Tab mTab;
    private GestureDetector mGestureDetector;
    private NavigationHandler mNavigationHandler;

    public static TabContentViewDelegate from(Tab tab) {
        TabContentViewDelegate handler = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (handler == null) {
            handler = tab.getUserDataHost().setUserData(
                    USER_DATA_KEY, new TabContentViewDelegate(tab));
        }
        return handler;
    }

    private TabContentViewDelegate(Tab tab) {
        super(tab);
        mTab = tab;
    }

    private class SideNavGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent event) {
            return mNavigationHandler.onDown();
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            return mNavigationHandler.onScroll(
                    e1.getX(), distanceX, distanceY, e2.getX(), e2.getY());
        }
    }

    @Override
    public void initWebContents(WebContents webContents) {
        ContentView parent = (ContentView) mTab.getContentView();
        parent.setTouchEventDelegate(this);
        mGestureDetector = new GestureDetector(parent.getContext(), new SideNavGestureListener());
        mNavigationHandler = new NavigationHandler(parent, new TabbedActionDelegate(mTab),
                NavigationGlowFactory.forRenderedPage(parent, mTab.getWebContents()));
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        if (mNavigationHandler != null) {
            mNavigationHandler.destroy();
            mNavigationHandler = null;
        }
        mGestureDetector = null;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        if (mNavigationHandler != null) {
            mGestureDetector.onTouchEvent(e);
            mNavigationHandler.onTouchEvent(e.getAction());
        }
        return false;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // Do not propagate touch events down to children if navigation UI was triggered.
        return mNavigationHandler != null && mNavigationHandler.isActive();
    }
}
