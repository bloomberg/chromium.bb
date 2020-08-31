// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import android.app.Activity;
import android.os.Handler;
import android.view.View;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Class that shows and hides in-product help message bubbles.
 * Recipes for use:
 * 1. Create an IPH bubble anchored to a view:
 * mUserEducationHelper.requestShowIPH(new IPHCommandBuilder(myContext.getResources(),
 *                 FeatureConstants.MY_FEATURE_NAME, R.string.my_feature_iph_text,
 *                 R.string.my_feature_iph_accessibility_text)
 *                                                     .setAnchorView(myAnchorView)
 *                                                     .setCircleHighlight(true)
 *                                                     .build());
 * 2. Create an IPH bubble that does custom logic when shown and hidden
 * mUserEducationHelper.requestShowIPH(new IPHCommandBuilder(myContext.getResources(),
 *                 FeatureConstants.MY_FEATURE_NAME, R.string.my_feature_iph_text,
 *                 R.string.my_feature_iph_accessibility_text)
 *                                                     .setAnchorView(myAnchorView)
 *                                                     .setCircleHighlight(true)
 *                                                     .setOnShowCallback( ()-> doCustomShowLogic())
 *                                                     .setOnDismissCallback(() ->
 *                                                         doCustomDismissLogic())
 *                                                     .build());
 */
public class UserEducationHelper {
    private final Activity mActivity;
    private final Handler mHandler;

    public UserEducationHelper(Activity activity, Handler handler) {
        mActivity = activity;
        mHandler = handler;
    }

    /**
     * Requests display of the in-product help (IPH) data in @param iphCommand.
     * @see IPHCommand for a breakdown of this data.
     * Display will only occur if the feature engagement tracker for the current profile says it
     * should.
     */
    public void requestShowIPH(IPHCommand iphCommand) {
        // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile. Currently always original
        // profile is used not to start popping IPH messages as soon as opening an incognito tab.
        Profile profile = Profile.getLastUsedRegularProfile();
        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.addOnInitializedCallback(success -> showIPH(tracker, iphCommand));
    }

    private void showIPH(Tracker tracker, IPHCommand iphCommand) {
        // Activity was destroyed; don't show IPH.
        View anchorView = iphCommand.anchorView;
        if (mActivity.isFinishing() || mActivity.isDestroyed() || anchorView == null) return;

        if (mActivity.isFinishing() || mActivity.isDestroyed()) return;

        String featureName = iphCommand.featureName;
        if (!tracker.shouldTriggerHelpUI(featureName)) return;
        String contentString = iphCommand.contentString;
        String accessibilityString = iphCommand.accessibilityText;
        assert (!contentString.isEmpty());
        assert (!accessibilityString.isEmpty());
        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);

        TextBubble textBubble =
                new TextBubble(mActivity, anchorView, contentString, accessibilityString, true,
                        rectProvider, AccessibilityUtil.isAccessibilityEnabled());
        textBubble.setDismissOnTouchInteraction(iphCommand.dismissOnTouch);
        textBubble.addOnDismissListener(() -> mHandler.postDelayed(() -> {
            tracker.dismissed(featureName);
            iphCommand.onDismissCallback.run();
            if (iphCommand.shouldHighlight) {
                ViewHighlighter.turnOffHighlight(anchorView);
            }
        }, ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS));
        textBubble.setAutoDismissTimeout(iphCommand.autoDismissTimeout);

        if (iphCommand.shouldHighlight) {
            ViewHighlighter.turnOnHighlight(anchorView, iphCommand.circleHighlight);
        }

        rectProvider.setInsetPx(iphCommand.insetRect);
        textBubble.show();
        iphCommand.onShowCallback.run();
    }
}