// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.listmenu;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * A menu button meant to be used with modern lists throughout Chrome.  Will automatically show and
 * anchor a popup on press and will rely on a delegate for positioning and content of the popup.
 * You can define your own content description for accessibility through the
 * android:contentDescription parameter in the XML layout of the ListMenuButton. The default content
 * description that corresponds to
 * context.getString(R.string.accessibility_list_menu_button, "") is used otherwise.
 */
public class ListMenuButton
        extends ChromeImageButton implements AnchoredPopupWindow.LayoutObserver {
    /**
     * A listener that is notified when the popup menu is shown.
     */
    @FunctionalInterface
    public interface PopupMenuShownListener {
        void onPopupMenuShown();
    }

    private final int mMenuMaxWidth;
    private final boolean mMenuVerticalOverlapAnchor;
    private final boolean mMenuHorizontalOverlapAnchor;

    private Drawable mMenuBackground;
    private AnchoredPopupWindow mPopupMenu;
    private ListMenuButtonDelegate mDelegate;
    private ObserverList<PopupMenuShownListener> mPopupListeners = new ObserverList<>();
    private boolean mTryToFitLargestItem = false;

    /**
     * Creates a new {@link ListMenuButton}.
     *
     * @param context The {@link Context} used to build the visuals from.
     * @param attrs The specific {@link AttributeSet} used to build the button.
     */
    public ListMenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ListMenuButton);
        mMenuMaxWidth = a.getDimensionPixelSize(R.styleable.ListMenuButton_menuMaxWidth,
                getResources().getDimensionPixelSize(R.dimen.list_menu_width));
        mMenuBackground = a.getDrawable(R.styleable.ListMenuButton_menuBackground);
        if (mMenuBackground == null) {
            mMenuBackground =
                    ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.popup_bg_tinted);
        }
        mMenuHorizontalOverlapAnchor =
                a.getBoolean(R.styleable.ListMenuButton_menuHorizontalOverlapAnchor, true);
        mMenuVerticalOverlapAnchor =
                a.getBoolean(R.styleable.ListMenuButton_menuVerticalOverlapAnchor, true);

        a.recycle();
    }

    /**
     * Text that represents the item this menu button is related to.  This will affect the content
     * description of the view {@see #setContentDescription(CharSequence)}.
     *
     * @param context The string representation of the list item this button represents.
     */
    public void setContentDescriptionContext(String context) {
        if (context == null) {
            context = "";
        }
        setContentDescription(getContext().getResources().getString(
                R.string.accessibility_list_menu_button, context));
    }

    /**
     * Sets the delegate this menu will rely on for populating the popup menu and handling selection
     * responses.  The menu will not show or work without it.
     *
     * @param delegate The {@link ListMenuButtonDelegate} to use for menu creation and selection
     *         handling.
     */
    public void setDelegate(ListMenuButtonDelegate delegate) {
        dismiss();
        mDelegate = delegate;
    }

    /**
     * Called to dismiss any popup menu that might be showing for this button.
     */
    public void dismiss() {
        if (mPopupMenu != null) {
            mPopupMenu.dismiss();
        }
    }

    /**
     * Shows a popupWindow built by ListMenuButton
     */
    public void showMenu() {
        initPopupWindow();
        mPopupMenu.show();
        notifyPopupListeners();
    }

    /**
     * Init the popup window with provided attributes, called before {@link #showMenu()}
     */
    private void initPopupWindow() {
        if (mDelegate == null) throw new IllegalStateException("Delegate was not set.");

        ListMenu menu = mDelegate.getListMenu();
        menu.addContentViewClickRunnable(this::dismiss);

        mPopupMenu = new AnchoredPopupWindow(getContext(), this, mMenuBackground,
                menu.getContentView(), mDelegate.getRectProvider(this));
        mPopupMenu.setVerticalOverlapAnchor(mMenuVerticalOverlapAnchor);
        mPopupMenu.setHorizontalOverlapAnchor(mMenuHorizontalOverlapAnchor);
        mPopupMenu.setMaxWidth(mMenuMaxWidth);
        if (mTryToFitLargestItem) {
            mPopupMenu.setDesiredContentWidth(menu.getMaxItemWidth());
        }
        mPopupMenu.setFocusable(true);
        mPopupMenu.setLayoutObserver(this);
        mPopupMenu.addOnDismissListener(() -> { mPopupMenu = null; });
    }

    /**
     * Adds a listener which will be notified when the popup menu is shown.
     *
     * @param l The listener of interest.
     */
    public void addPopupListener(PopupMenuShownListener l) {
        mPopupListeners.addObserver(l);
    }

    /**
     * Removes a popup menu listener.
     *
     * @param l The listener of interest.
     */
    public void removePopupListener(PopupMenuShownListener l) {
        mPopupListeners.removeObserver(l);
    }

    // AnchoredPopupWindow.LayoutObserver implementation.
    @Override
    public void onPreLayoutChange(
            boolean positionBelow, int x, int y, int width, int height, Rect anchorRect) {
        mPopupMenu.setAnimationStyle(
                positionBelow ? R.style.OverflowMenuAnim : R.style.OverflowMenuAnimBottom);
    }

    /**
     * Determines whether to try to fit the largest menu item without overflowing by measuring the
     * exact width of each item.
     *
     * WARNING: do not call when the menu list has more than a handful of items, the performance
     * will be terrible since it measures every single item.
     *
     * @param value Determines whether to try to exactly fit the width of the largest item in the
     *              list.
     */
    public void tryToFitLargestItem(boolean value) {
        mTryToFitLargestItem = value;
    }

    // View implementation.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        if (TextUtils.isEmpty(getContentDescription())) setContentDescriptionContext("");
        setOnClickListener((view) -> showMenu());
    }

    @Override
    protected void onDetachedFromWindow() {
        dismiss();
        super.onDetachedFromWindow();
    }

    /**
     * Notify all of the PopupMenuShownListeners of a popup menu action.
     */
    private void notifyPopupListeners() {
        for (PopupMenuShownListener l : mPopupListeners) {
            l.onPopupMenuShown();
        }
    }
}