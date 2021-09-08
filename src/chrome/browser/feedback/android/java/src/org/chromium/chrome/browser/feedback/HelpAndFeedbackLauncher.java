// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;

import java.util.Map;

/**
 * Interface for launching a help and feedback page.
 */
public interface HelpAndFeedbackLauncher {
    /**
     * Starts an activity showing a help page for the specified context ID.
     *
     * @param activity The activity to use for starting the help activity and to take a
     *                 screenshot of.
     * @param helpContext One of the CONTEXT_* constants. This should describe the user's current
     *                    context and will be used to show a more relevant help page.
     * @param profile the current profile.
     * @param url the current URL. May be null.
     */
    void show(final Activity activity, String helpContext, Profile profile, @Nullable String url);

    /**
     * Starts an activity prompting the user to enter feedback.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *                 screenshot of.
     * @param profile the current profile.
     * @param url the current URL. May be null.
     * @param categoryTag The category that this feedback report falls under.
     * @param screenshotMode The kind of screenshot to include with the feedback.
     * @param feedbackContext The context that describes the current feature being used.
     */
    void showFeedback(final Activity activity, Profile profile, @Nullable String url,
            @Nullable final String categoryTag, int screenshotMode,
            @Nullable final String feedbackContext);

    /**
     * Starts an activity prompting the user to enter feedback.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *                 screenshot of.
     * @param profile the current profile.
     * @param url the current URL. May be null.
     * @param categoryTag The category that this feedback report falls under.
     */
    void showFeedback(final Activity activity, Profile profile, @Nullable String url,
            @Nullable final String categoryTag);

    /**
     * Starts an activity prompting the user to enter feedback for the interest feed.
     *
     * @param activity The activity to use for starting the feedback activity and to take a
     *                 screenshot of.
     * @param profile the current profile.
     * @param categoryTag The category that this feedback report falls under.
     * @param feedContext Feed specific parameters (url, title, etc) to include with feedback.
     */
    void showFeedback(final Activity activity, Profile profile, @Nullable String url,
            @Nullable final String categoryTag, @Nullable final Map<String, String> feedContext);
}
