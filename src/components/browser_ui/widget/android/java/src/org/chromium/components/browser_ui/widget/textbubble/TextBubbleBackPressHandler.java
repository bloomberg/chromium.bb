// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.textbubble;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * Back gesture handler for {@link TextBubble}.
 */
public class TextBubbleBackPressHandler implements BackPressHandler {
    private final ObservableSupplierImpl<Boolean> mSupplier = new ObservableSupplierImpl<>();
    private final Callback<Integer> mCallback = (count) -> mSupplier.set(count != 0);

    public TextBubbleBackPressHandler() {
        TextBubble.getCountSupplier().addObserver(mCallback);
    }

    @Override
    public void handleBackPress() {
        TextBubble.dismissBubbles();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mSupplier;
    }

    public void destroy() {
        TextBubble.getCountSupplier().removeObserver(mCallback);
    }
}
