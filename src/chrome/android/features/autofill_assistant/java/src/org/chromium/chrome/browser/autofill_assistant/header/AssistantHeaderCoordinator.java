// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the header of the Autofill Assistant.
 */
public class AssistantHeaderCoordinator {
    public AssistantHeaderCoordinator(Context context, ViewGroup root, AssistantHeaderModel model) {
        // Create the poodle and insert it before the status message. We have to create a view
        // bigger than the desired poodle size (24dp) because the actual downstream implementation
        // needs extra space for the animation.
        AnimatedPoodle poodle = new AnimatedPoodle(context,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_poodle_view_size),
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_poodle_size));
        addPoodle(root, poodle.getView());

        // Bind view and mediator through the model.
        AssistantHeaderViewBinder.ViewHolder viewHolder =
                new AssistantHeaderViewBinder.ViewHolder(root, poodle);
        AssistantHeaderViewBinder viewBinder = new AssistantHeaderViewBinder();
        PropertyModelChangeProcessor.create(model, viewHolder, viewBinder);
    }

    private void addPoodle(ViewGroup root, View poodleView) {
        View statusMessage = root.findViewById(R.id.status_message);
        ViewGroup parent = (ViewGroup) statusMessage.getParent();
        parent.addView(poodleView, parent.indexOfChild(statusMessage));
    }
}
