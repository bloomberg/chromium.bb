// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * The main coordinator for the Autofill Assistant, responsible for instantiating all other
 * sub-components and shutting down the Autofill Assistant.
 */
class AssistantCoordinator {
    interface Delegate {
        /** Completely stop the Autofill Assistant. */
        void stop(@DropOutReason int reason);

        // TODO(crbug.com/806868): Move onboarding and snackbar out of this class and remove the
        // delegate.
    }

    private static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";

    private final ChromeActivity mActivity;
    private final Delegate mDelegate;

    private final AssistantModel mModel;
    private final View mAssistantView;

    private final AssistantBottomBarCoordinator mBottomBarCoordinator;
    private final AssistantKeyboardCoordinator mKeyboardCoordinator;
    private final AssistantOverlayCoordinator mOverlayCoordinator;

    /**
     * Returns {@code true} if an AA UI is active on the given activity.
     *
     * <p>Used to avoid creating duplicate coordinators views.
     *
     * <p>TODO(crbug.com/806868): Refactor to have AssistantCoordinator owned by the activity, so
     * it's easy to guarantee that there will be at most one per activity.
     */
    static boolean isActive(ChromeActivity activity) {
        View found = activity.findViewById(R.id.autofill_assistant);
        return found != null && found.getParent() != null;
    }

    AssistantCoordinator(ChromeActivity activity, Delegate delegate) {
        mActivity = activity;
        mDelegate = delegate;
        mModel = new AssistantModel();

        // Inflate autofill_assistant_sheet layout and add it to the main coordinator view.
        ViewGroup coordinator = activity.findViewById(R.id.coordinator);
        mAssistantView = activity.getLayoutInflater()
                                 .inflate(R.layout.autofill_assistant_sheet, coordinator)
                                 .findViewById(R.id.autofill_assistant);

        // Instantiate child components.
        mBottomBarCoordinator = new AssistantBottomBarCoordinator(activity, mAssistantView, mModel);
        mKeyboardCoordinator = new AssistantKeyboardCoordinator(activity);
        mOverlayCoordinator =
                new AssistantOverlayCoordinator(activity, mAssistantView, mModel.getOverlayModel());

        // Listen when we should (dis)allow the soft keyboard or swiping the bottom sheet.
        mModel.addObserver((source, propertyKey) -> {
            if (AssistantModel.ALLOW_SOFT_KEYBOARD == propertyKey) {
                mKeyboardCoordinator.allowShowingSoftKeyboard(
                        mModel.get(AssistantModel.ALLOW_SOFT_KEYBOARD));
            } else if (AssistantModel.ALLOW_SWIPING_SHEET == propertyKey) {
                mBottomBarCoordinator.allowSwipingBottomSheet(
                        mModel.get(AssistantModel.ALLOW_SWIPING_SHEET));
            } else if (AssistantModel.VISIBLE == propertyKey) {
                setVisible(mModel.get(AssistantModel.VISIBLE));
            }
        });
        mModel.setVisible(true);
    }

    /** Detaches and destroys the view. */
    public void destroy() {
        setVisible(false);
        detachAssistantView();
        mOverlayCoordinator.destroy();
    }

    /**
     * Show the onboarding screen and call {@code onAccept} if the user agreed to proceed, shutdown
     * otherwise.
     */
    public void showOnboarding(Runnable onAccept) {
        mModel.getHeaderModel().set(AssistantHeaderModel.FEEDBACK_VISIBLE, false);

        // Show overlay to prevent user from interacting with the page during onboarding.
        mModel.getOverlayModel().set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL);

        // Disable swiping for the onboarding because it interferes with letting the user scroll
        // the onboarding contents.
        mBottomBarCoordinator.allowSwipingBottomSheet(false);
        AssistantOnboardingCoordinator.show(mActivity, mBottomBarCoordinator.getContainerView())
                .then(accepted -> {
                    mBottomBarCoordinator.allowSwipingBottomSheet(true);
                    if (!accepted) {
                        mDelegate.stop(DropOutReason.DECLINED);
                        return;
                    }

                    mModel.getHeaderModel().set(AssistantHeaderModel.FEEDBACK_VISIBLE, true);

                    // Hide overlay.
                    mModel.getOverlayModel().set(
                            AssistantOverlayModel.STATE, AssistantOverlayState.HIDDEN);

                    onAccept.run();
                });
    }

    /**
     * Get the model representing the current state of the UI.
     */

    public AssistantModel getModel() {
        return mModel;
    }

    // Getters to retrieve the sub coordinators.

    public AssistantBottomBarCoordinator getBottomBarCoordinator() {
        return mBottomBarCoordinator;
    }

    /**
     * Show the Chrome feedback form.
     */
    public void showFeedback(String debugContext) {
        HelpAndFeedback.getInstance(mActivity).showFeedback(mActivity, Profile.getLastUsedProfile(),
                mActivity.getActivityTab().getUrl(), FEEDBACK_CATEGORY_TAG,
                FeedbackContext.buildContextString(mActivity, debugContext, 4));
    }

    // Private methods.

    private void setVisible(boolean visible) {
        if (visible) {
            mAssistantView.setVisibility(View.VISIBLE);
            mKeyboardCoordinator.enableListenForKeyboardVisibility(true);

            mBottomBarCoordinator.expand();
            mBottomBarCoordinator.getView().announceForAccessibility(
                    mActivity.getString(R.string.autofill_assistant_available_accessibility));
        } else {
            mAssistantView.setVisibility(View.GONE);
            mKeyboardCoordinator.enableListenForKeyboardVisibility(false);
        }
        // TODO(crbug.com/806868): Control visibility of bottom bar and overlay separately.
    }

    private void detachAssistantView() {
        mActivity.<ViewGroup>findViewById(R.id.coordinator).removeView(mAssistantView);
    }
}
