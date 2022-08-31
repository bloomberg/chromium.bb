// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;

import org.chromium.base.TraceEvent;
import org.chromium.ui.base.ViewUtils;

/**
 * Container holding messages.
 */
public class MessageContainer extends FrameLayout {
    interface MessageContainerA11yDelegate {
        void onA11yFocused();
        void onA11yFocusCleared();
        void onA11yDismiss();
    }

    private View mMessageBannerView;
    private MessageContainerA11yDelegate mA11yDelegate;

    public MessageContainer(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setAccessibilityDelegate(new AccessibilityDelegate() {
            @Override
            public void onInitializeAccessibilityEvent(
                    @NonNull View host, @NonNull AccessibilityEvent event) {
                if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED) {
                    if (mA11yDelegate != null) mA11yDelegate.onA11yFocused();
                } else if (event.getEventType()
                        == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED) {
                    if (mA11yDelegate != null) mA11yDelegate.onA11yFocusCleared();
                }
                super.onInitializeAccessibilityEvent(host, event);
            }
        });
        ViewCompat.replaceAccessibilityAction(
                this, AccessibilityActionCompat.ACTION_DISMISS, null, (v, c) -> {
                    if (mA11yDelegate != null) {
                        mA11yDelegate.onA11yDismiss();
                        return true;
                    }
                    return false;
                });
    }

    /**
     * Show a given message view on the screen. There should be no view inside the container
     * before adding a message.
     * @param view The message view to display on the screen.
     */
    void addMessage(View view) {
        if (mMessageBannerView != null) {
            throw new IllegalStateException(
                    "Should not contain any view when adding a new message.");
        }
        mMessageBannerView = view;
        addView(view);
        // TODO(crbug.com/1178965): clipChildren should be set to false only when the message is in
        // motion.
        ViewUtils.setAncestorsShouldClipChildren(this, false);
    }

    /**
     * Remove the given message view, which is being shown inside the container.
     * @param view The message which should be removed.
     */
    void removeMessage(View view) {
        if (mMessageBannerView != view) {
            throw new IllegalStateException("The given view is not being shown.");
        }
        ViewUtils.setAncestorsShouldClipChildren(this, true);
        removeAllViews();
        mMessageBannerView = null;
        mA11yDelegate = null;
    }

    public int getMessageBannerHeight() {
        assert mMessageBannerView != null;
        return mMessageBannerView.getHeight();
    }

    public int getMessageShadowTopMargin() {
        return getResources().getDimensionPixelOffset(R.dimen.message_shadow_top_margin);
    }

    @Override
    public void setLayoutParams(ViewGroup.LayoutParams params) {
        try (TraceEvent e = TraceEvent.scoped("MessageContainer.setLayoutParams")) {
            super.setLayoutParams(params);
        }
    }

    void setA11yDelegate(MessageContainerA11yDelegate a11yDelegate) {
        mA11yDelegate = a11yDelegate;
    }

    /**
     * Runs a {@link Runnable} after the message's initial layout. If the view is already laid out,
     * the {@link Runnable} will be called immediately.
     * @param runnable The {@link Runnable}.
     */
    void runAfterInitialMessageLayout(Runnable runnable) {
        assert mMessageBannerView != null;
        if (mMessageBannerView.getHeight() > 0) {
            runnable.run();
            return;
        }

        mMessageBannerView.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (v.getHeight() == 0) return;

                runnable.run();
                v.removeOnLayoutChangeListener(this);
            }
        });
    }
}
