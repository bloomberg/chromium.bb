// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.ChromeImageView;

/**
 * The binder to bind the app menu  {@link PropertyModel} with the view.
 */
class AppMenuItemViewBinder {
    /** IDs of all of the buttons in icon_row_menu_item.xml. */
    private static final int[] BUTTON_IDS = {R.id.button_one, R.id.button_two, R.id.button_three,
            R.id.button_four, R.id.button_five};

    public static void bindStandardItem(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            view.setId(id);
        } else if (key == AppMenuItemProperties.TITLE) {
            ((TextView) view.findViewById(R.id.menu_item_text))
                    .setText(model.get(AppMenuItemProperties.TITLE));
        } else if (key == AppMenuItemProperties.TITLE_CONDENSED) {
            setContentDescription(view.findViewById(R.id.menu_item_text), model);
        } else if (key == AppMenuItemProperties.ENABLED) {
            boolean enabled = model.get(AppMenuItemProperties.ENABLED);
            ((TextView) view.findViewById(R.id.menu_item_text)).setEnabled(enabled);
            view.setEnabled(enabled);
        } else if (key == AppMenuItemProperties.HIGHLIGHTED) {
            if (model.get(AppMenuItemProperties.HIGHLIGHTED)) {
                ViewHighlighter.turnOnHighlight(
                        view, new HighlightParams(HighlightShape.RECTANGLE));
            } else {
                ViewHighlighter.turnOffHighlight(view);
            }
        } else if (key == AppMenuItemProperties.ICON) {
            Drawable icon = model.get(AppMenuItemProperties.ICON);
            ChromeImageView imageView = (ChromeImageView) view.findViewById(R.id.menu_item_icon);
            imageView.setImageDrawable(icon);
            imageView.setVisibility(icon == null ? View.GONE : View.VISIBLE);

            // tint the icon
            @ColorRes
            int colorResId = model.get(AppMenuItemProperties.ICON_COLOR_RES);
            if (colorResId == 0) {
                // If there is no color assigned to the icon, use the default color.
                colorResId = R.color.default_icon_color_secondary_tint_list;
            }
            ApiCompatibilityUtils.setImageTintList(imageView,
                    AppCompatResources.getColorStateList(imageView.getContext(), colorResId));
        } else if (key == AppMenuItemProperties.CLICK_HANDLER) {
            view.setOnClickListener(
                    v -> model.get(AppMenuItemProperties.CLICK_HANDLER).onItemClick(model));
        }
    }

    public static void bindTitleButtonItem(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        if (key == AppMenuItemProperties.SUBMENU) {
            ModelList subList = model.get(AppMenuItemProperties.SUBMENU);
            PropertyModel titleModel = subList.get(0).model;

            view.setId(titleModel.get(AppMenuItemProperties.MENU_ITEM_ID));

            TextViewWithCompoundDrawables title =
                    (TextViewWithCompoundDrawables) view.findViewById(R.id.title);
            title.setText(titleModel.get(AppMenuItemProperties.TITLE));
            title.setEnabled(titleModel.get(AppMenuItemProperties.ENABLED));
            title.setFocusable(titleModel.get(AppMenuItemProperties.ENABLED));
            title.setCompoundDrawablesRelative(
                    titleModel.get(AppMenuItemProperties.ICON), null, null, null);
            setContentDescription(title, titleModel);

            AppMenuClickHandler appMenuClickHandler =
                    model.get(AppMenuItemProperties.CLICK_HANDLER);
            title.setOnClickListener(v -> appMenuClickHandler.onItemClick(titleModel));
            if (titleModel.get(AppMenuItemProperties.HIGHLIGHTED)) {
                ViewHighlighter.turnOnHighlight(
                        view, new HighlightParams(HighlightShape.RECTANGLE));
            } else {
                ViewHighlighter.turnOffHighlight(view);
            }

            AppMenuItemIcon checkbox = (AppMenuItemIcon) view.findViewById(R.id.checkbox);
            ChromeImageButton button = (ChromeImageButton) view.findViewById(R.id.button);
            PropertyModel buttonModel = null;
            boolean checkable = false;
            boolean checked = false;
            Drawable subIcon = null;

            if (subList.size() == 2) {
                buttonModel = subList.get(1).model;
                checkable = buttonModel.get(AppMenuItemProperties.CHECKABLE);
                checked = buttonModel.get(AppMenuItemProperties.CHECKED);
                subIcon = buttonModel.get(AppMenuItemProperties.ICON);
            }

            if (checkable) {
                // Display a checkbox for the MenuItem.
                button.setVisibility(View.GONE);
                checkbox.setVisibility(View.VISIBLE);
                checkbox.setChecked(checked);
                ApiCompatibilityUtils.setImageTintList(checkbox,
                        AppCompatResources.getColorStateList(
                                checkbox.getContext(), R.color.checkbox_tint));
                setupMenuButton(checkbox, buttonModel, appMenuClickHandler);
            } else if (subIcon != null) {
                // Display an icon alongside the MenuItem.
                checkbox.setVisibility(View.GONE);
                button.setVisibility(View.VISIBLE);
                setupImageButton(button, buttonModel, appMenuClickHandler);
            } else {
                // Display just the label of the MenuItem.
                checkbox.setVisibility(View.GONE);
                button.setVisibility(View.GONE);
            }
        } else if (key == AppMenuItemProperties.HIGHLIGHTED) {
            if (model.get(AppMenuItemProperties.HIGHLIGHTED)) {
                ViewHighlighter.turnOnHighlight(
                        view, new HighlightParams(HighlightShape.RECTANGLE));
            } else {
                ViewHighlighter.turnOffHighlight(view);
            }
        }
    }

    public static void bindIconRowItem(PropertyModel model, View view, PropertyKey key) {
        if (key == AppMenuItemProperties.SUBMENU) {
            ModelList iconList = model.get(AppMenuItemProperties.SUBMENU);

            int numItems = iconList.size();
            ImageButton[] buttons = new ImageButton[numItems];
            // Save references to all the buttons.
            for (int i = 0; i < numItems; i++) {
                buttons[i] = (ImageButton) view.findViewById(BUTTON_IDS[i]);
            }

            // Remove unused menu items.
            for (int i = numItems; i < 5; i++) {
                ((ViewGroup) view).removeView(view.findViewById(BUTTON_IDS[i]));
            }

            AppMenuClickHandler appMenuClickHandler =
                    model.get(AppMenuItemProperties.CLICK_HANDLER);
            for (int i = 0; i < numItems; i++) {
                setupImageButton(buttons[i], iconList.get(i).model, appMenuClickHandler);
            }

            view.setTag(
                    R.id.menu_item_enter_anim_id, AppMenuUtil.buildIconItemEnterAnimator(buttons));

            // Tint action bar's background.
            view.setBackgroundDrawable(ApiCompatibilityUtils.getDrawable(
                    view.getContext().getResources(), R.drawable.menu_action_bar_bg));

            view.setEnabled(false);
        }
    }

    public static void setContentDescription(View view, final PropertyModel model) {
        CharSequence titleCondensed = model.get(AppMenuItemProperties.TITLE_CONDENSED);
        if (TextUtils.isEmpty(titleCondensed)) {
            view.setContentDescription(null);
        } else {
            view.setContentDescription(titleCondensed);
        }
    }

    private static void setupImageButton(ImageButton button, final PropertyModel model,
            AppMenuClickHandler appMenuClickHandler) {
        // Store and recover the level of image as button.setimageDrawable
        // resets drawable to default level.
        Drawable icon = model.get(AppMenuItemProperties.ICON);
        int currentLevel = icon.getLevel();
        button.setImageDrawable(icon);
        icon.setLevel(currentLevel);

        // TODO(gangwu): Resetting this tint if we go from checked -> not checked while the menu is
        // visible.
        if (model.get(AppMenuItemProperties.CHECKED)) {
            ApiCompatibilityUtils.setImageTintList(button,
                    AppCompatResources.getColorStateList(
                            button.getContext(), R.color.blue_mode_tint));
        }

        setupMenuButton(button, model, appMenuClickHandler);
    }

    private static void setupMenuButton(
            View button, final PropertyModel model, AppMenuClickHandler appMenuClickHandler) {
        // On Android M, even setEnabled(false), the button still focusable.
        button.setEnabled(model.get(AppMenuItemProperties.ENABLED));
        button.setFocusable(model.get(AppMenuItemProperties.ENABLED));

        CharSequence titleCondensed = model.get(AppMenuItemProperties.TITLE_CONDENSED);
        if (TextUtils.isEmpty(titleCondensed)) {
            button.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        } else {
            button.setContentDescription(titleCondensed);
            button.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        }

        button.setOnClickListener(v -> appMenuClickHandler.onItemClick(model));
        button.setOnLongClickListener(v -> appMenuClickHandler.onItemLongClick(model, button));

        if (model.get(AppMenuItemProperties.HIGHLIGHTED)) {
            ViewHighlighter.turnOnHighlight(button, new HighlightParams(HighlightShape.CIRCLE));
        } else {
            ViewHighlighter.turnOffHighlight(button);
        }

        // Menu items may be hidden by command line flags before they get to this point.
        button.setVisibility(View.VISIBLE);
    }
}