// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyModel;

import java.util.Locale;

/**
 * This is a util class for creating the property model of the TabSuggestionMessageCard.
 */
public class TabSuggestionMessageCardViewModel {
    /**
     * Create a {@link PropertyModel} for TabSuggestionMessageCardView.
     * @param context The {@link Context} to use.
     * @param uiDismissActionProvider The {@link MessageCardView.DismissActionProvider} to set.
     * @param data The {@link TabSuggestionMessageService.TabSuggestionMessageData} to use.
     * @return A {@link PropertyModel} for the given {@code data}.
     */
    public static PropertyModel create(Context context,
            MessageCardView.DismissActionProvider uiDismissActionProvider,
            TabSuggestionMessageService.TabSuggestionMessageData data) {
        String descriptionTextTemplate = context.getString(
                org.chromium.chrome.tab_ui.R.string.tab_suggestion_close_stale_message);
        String descriptionText = String.format(Locale.getDefault(), "%d", data.getSize());
        String actionText =
                context.getString(org.chromium.chrome.tab_ui.R.string.tab_suggestion_review_button);
        String dismissButtonContextDescription = context.getString(
                org.chromium.chrome.tab_ui.R.string.accessibility_tab_suggestion_dismiss_button);

        return new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                .with(MessageCardViewProperties.MESSAGE_TYPE,
                        MessageService.MessageType.TAB_SUGGESTION)
                .with(MessageCardViewProperties.ICON_PROVIDER,
                        TabSuggestionMessageCardViewModel::getIconDrawable)
                .with(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, uiDismissActionProvider)
                .with(MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                        data.getDismissActionProvider())
                .with(MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                        data.getReviewActionProvider())
                .with(MessageCardViewProperties.DESCRIPTION_TEXT_TEMPLATE, descriptionTextTemplate)
                .with(MessageCardViewProperties.DESCRIPTION_TEXT, descriptionText)
                .with(MessageCardViewProperties.ACTION_TEXT, actionText)
                .with(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION,
                        dismissButtonContextDescription)
                .with(MessageCardViewProperties.IS_ICON_VISIBLE, true)
                .with(CARD_TYPE, MESSAGE)
                .with(CARD_ALPHA, 1f)
                .build();
    }

    private static Drawable getIconDrawable() {
        // TODO(meiliang): returns a drawable with first tab suggested tab's favicon.
        return null;
    }
}