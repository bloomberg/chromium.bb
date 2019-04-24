// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.infobox;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxViewBinder.ViewHolder;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator responsible for showing an InfoBox.
 */
public class AssistantInfoBoxCoordinator {
    private final View mView;

    public AssistantInfoBoxCoordinator(Context context, AssistantInfoBoxModel model) {
        mView = LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_info_box, /* root= */ null);
        ViewHolder viewHolder = new ViewHolder(context, mView);
        AssistantInfoBoxViewBinder viewBinder = new AssistantInfoBoxViewBinder(context);
        PropertyModelChangeProcessor.create(model, viewHolder, viewBinder);

        // InfoBox view is initially hidden.
        setVisible(false);

        // Observe InfoBox in model to hide or show this coordinator view.
        model.addObserver((source, propertyKey) -> {
            if (AssistantInfoBoxModel.INFO_BOX == propertyKey) {
                AssistantInfoBox infoBox = model.get(AssistantInfoBoxModel.INFO_BOX);
                if (infoBox != null) {
                    setVisible(true);
                } else {
                    setVisible(false);
                }
            }
        });
    }

    /**
     * Return the view associated to the info box.
     */
    public View getView() {
        return mView;
    }

    /**
     * Show or hide the info box within its parent and call the {@code mOnVisibilityChanged}
     * listener.
     */
    private void setVisible(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        if (mView.getVisibility() != visibility) {
            mView.setVisibility(visibility);
        }
    }
}
