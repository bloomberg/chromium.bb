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

import java.util.Collections;

/**
 * Coordinator for the header of the Autofill Assistant.
 */
public class AssistantHeaderCoordinator implements ProfileDataCache.Observer {
    private final ProfileDataCache mProfileCache;
    private final ImageView mProfileView;
    private final String mSignedInAccountName;

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

        int imageSize = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_profile_size);
        mProfileCache = new ProfileDataCache(context, imageSize);
        mProfileView = bottomBarView.findViewById(R.id.profile_image);
        mSignedInAccountName = ChromeSigninController.get().getSignedInAccountName();
        setupProfileImage(context, bottomBarView);

        // Bind view and mediator through the model.
        AssistantHeaderViewBinder.ViewHolder viewHolder =
                new AssistantHeaderViewBinder.ViewHolder(context, bottomBarView, poodle);
        AssistantHeaderViewBinder viewBinder = new AssistantHeaderViewBinder();
        PropertyModelChangeProcessor.create(model, viewHolder, viewBinder);

        model.set(AssistantHeaderModel.VISIBLE, true);
        model.set(AssistantHeaderModel.PROGRESS_VISIBLE, true);
    }

    @Override
    public void onProfileDataUpdated(String account) {
        if (!mSignedInAccountName.equals(account)) {
            return;
        }
        setProfileImageFor(mSignedInAccountName);
    }

    /**
     * Cleanup resources when this goes out of scope.
     */
    public void destroy() {
        if (mSignedInAccountName != null) {
            mProfileCache.removeObserver(this);
        }
    }

    private void addPoodle(ViewGroup root, View poodleView) {
        View statusMessage = root.findViewById(R.id.status_message);
        ViewGroup parent = (ViewGroup) statusMessage.getParent();
        parent.addView(poodleView, parent.indexOfChild(statusMessage));
    }

    // TODO(b/130415092): Use image from AGSA if chrome is not signed in.
    private void setupProfileImage(Context context, ViewGroup root) {
        if (mSignedInAccountName != null) {
            mProfileCache.addObserver(this);
            mProfileCache.update(Collections.singletonList(mSignedInAccountName));
        }
    }

    private void setProfileImageFor(String signedInAccountName) {
        DisplayableProfileData profileData =
                mProfileCache.getProfileDataOrDefault(signedInAccountName);
        mProfileView.setImageDrawable(profileData.getImage());
    }
}
