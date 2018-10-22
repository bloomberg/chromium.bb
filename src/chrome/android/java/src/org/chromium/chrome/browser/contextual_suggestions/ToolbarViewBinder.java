// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsModel.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/**
 * A view binder for use with a {@link ToolbarView}.
 */
class ToolbarViewBinder
        implements PropertyModelChangeProcessor
                           .ViewBinder<ContextualSuggestionsModel, ToolbarView, PropertyKey> {
    @Override
    public void bind(ContextualSuggestionsModel model, ToolbarView view, PropertyKey propertyKey) {
        if (propertyKey == PropertyKey.CLOSE_BUTTON_ON_CLICK_LISTENER) {
            view.setCloseButtonOnClickListener(model.getCloseButtonOnClickListener());
        } else if (propertyKey == PropertyKey.MENU_BUTTON_VISIBILITY) {
            view.setMenuButtonVisibility(model.getMenuButtonVisibility());
        } else if (propertyKey == PropertyKey.MENU_BUTTON_ALPHA) {
            view.setMenuButtonAlpha(model.getMenuButtonAlpha());
        } else if (propertyKey == PropertyKey.MENU_BUTTON_DELEGATE) {
            view.setMenuButtonDelegate(model.getMenuButtonDelegate());
        } else if (propertyKey == PropertyKey.TITLE) {
            view.setTitle(model.getTitle());
        } else if (propertyKey == PropertyKey.TOOLBAR_SHADOW_VISIBILITY) {
            view.setShadowVisibility(model.getToolbarShadowVisibility());
        } else if (propertyKey == PropertyKey.DEFAULT_TOOLBAR_ON_CLICK_LISTENER) {
            view.setOnClickListener(model.getDefaultToolbarClickListener());
        } else if (propertyKey == PropertyKey.SLIM_PEEK_ENABLED) {
            view.setSlimPeekEnabled(model.isSlimPeekEnabled());
        } else if (propertyKey == PropertyKey.TOOLBAR_TRANSLATION_PERCENT) {
            view.setTranslationPercent(model.getToolbarTranslationPercent());
        } else if (propertyKey == PropertyKey.TOOLBAR_ARROW_TINT_RESOURCE_ID) {
            view.setArrowTintResourceId(model.getToolbarArrowTintResourceId());
        } else {
            assert false : "Unhandled property detected.";
        }
    }
}
