// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the header of the Autofill Assistant.
 */
public class AssistantHeaderCoordinator {
    public AssistantHeaderCoordinator(
            Context context, ViewGroup bottomBarView, AssistantHeaderModel model) {
        // Create the poodle and insert it before the status message. We have to create a view
        // bigger than the desired poodle size (24dp) because the actual downstream implementation
        // needs extra space for the animation.
        AnimatedPoodle poodle = new AnimatedPoodle(context,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_poodle_view_size),
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_poodle_size));
        addPoodle(bottomBarView, poodle.getView());

        setProfileImage(bottomBarView, context);

        // Bind view and mediator through the model.
        AssistantHeaderViewBinder.ViewHolder viewHolder =
                new AssistantHeaderViewBinder.ViewHolder(bottomBarView, poodle);
        AssistantHeaderViewBinder viewBinder = new AssistantHeaderViewBinder();
        PropertyModelChangeProcessor.create(model, viewHolder, viewBinder);

        model.set(AssistantHeaderModel.VISIBLE, true);
        model.set(AssistantHeaderModel.PROGRESS_VISIBLE, true);
    }

    private void addPoodle(ViewGroup root, View poodleView) {
        View statusMessage = root.findViewById(R.id.status_message);
        ViewGroup parent = (ViewGroup) statusMessage.getParent();
        parent.addView(poodleView, parent.indexOfChild(statusMessage));
    }

    // TODO(b/130415092): Use image from AGSA if chrome is not signed in.
    private void setProfileImage(ViewGroup root, Context context) {
        String signedInAccountName = ChromeSigninController.get().getSignedInAccountName();
        if (signedInAccountName != null) {
            ProfileDataCache profileCache =
                    new ProfileDataCache(context, R.dimen.autofill_assistant_profile_size);
            DisplayableProfileData profileData =
                    profileCache.getProfileDataOrDefault(signedInAccountName);
            ImageView profileView = root.findViewById(R.id.profile_image);
            profileView.setImageDrawable(profileData.getImage());
        }
    }
}
