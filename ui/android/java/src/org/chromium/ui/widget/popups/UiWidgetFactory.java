// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget.popups;

import android.content.Context;
import android.widget.PopupWindow;

/**
 * The factory that creates UI widgets. Instead of direct creation of Android popups
 * we should ask this factory to create the object for us.
 */
public class UiWidgetFactory {
    private static UiWidgetFactory sFactory;

    private UiWidgetFactory() {}

    /**
     * returns a UiWidgetFactory.
     */
    public static UiWidgetFactory getInstance() {
        if (sFactory == null) sFactory = new UiWidgetFactory();
        return sFactory;
    }

    /**
     * Sets a new instance for UiWidgetFactory. This can be used to replace the
     * factory with a new one that supports different set of widgets.
     *
     * @param widgetFactory The new UiWidgetFactory that can be accessed via {@link getInstance}
     */
    public static void setInstance(UiWidgetFactory widgetFactory) {
        sFactory = widgetFactory;
    }

    /**
     * Returns a new PopupWindow using the given context.
     *
     * @param context The Context that is used to create a new PopupWindow.
     * @return a new PopupWindow.
     */
    public PopupWindow createPopupWindow(Context context) {
        return new PopupWindow(context);
    }
}