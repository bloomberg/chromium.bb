// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_CARD_VISIBILITY;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_ICON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_MANAGER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_TOGGLE_ENFORCEMENT;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.INCOGNITO_LEARN_MORE_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_SURFACE_BODY_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.IncognitoCookieControlsManager;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for handling {@link TasksSurface}-related logic.
 */
class TasksSurfaceMediator {
    @Nullable
    private FakeboxDelegate mFakeboxDelegate;
    private final IncognitoCookieControlsManager mIncognitoCookieControlsManager;
    private IncognitoCookieControlsManager.Observer mIncognitoCookieControlsObserver;
    private final PropertyModel mModel;

    TasksSurfaceMediator(PropertyModel model, View.OnClickListener incognitoLearnMoreClickListener,
            IncognitoCookieControlsManager incognitoCookieControlsManager, boolean isTabCarousel) {
        mModel = model;
        mModel.set(IS_TAB_CAROUSEL_VISIBLE, isTabCarousel);

        model.set(INCOGNITO_LEARN_MORE_CLICK_LISTENER, incognitoLearnMoreClickListener);

        // Set Incognito Cookie Controls functionality
        mIncognitoCookieControlsManager = incognitoCookieControlsManager;
        mModel.set(INCOGNITO_COOKIE_CONTROLS_MANAGER, mIncognitoCookieControlsManager);

        // Set the initial state.
        mModel.set(IS_SURFACE_BODY_VISIBLE, true);
    }

    public void initWithNative(FakeboxDelegate fakeboxDelegate) {
        mFakeboxDelegate = fakeboxDelegate;
        assert mFakeboxDelegate != null;

        mModel.set(FAKE_SEARCH_BOX_CLICK_LISTENER, new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mFakeboxDelegate.setUrlBarFocus(
                        true, null, LocationBar.OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP);
                RecordUserAction.record("TasksSurface.FakeBox.Tapped");
            }
        });
        mModel.set(FAKE_SEARCH_BOX_TEXT_WATCHER, new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                if (s.length() == 0) return;
                mFakeboxDelegate.setUrlBarFocus(true, s.toString(),
                        LocationBar.OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS);
                RecordUserAction.record("TasksSurface.FakeBox.LongPressed");

                // This won't cause infinite loop since we checked s.length() == 0 above.
                s.clear();
            }
        });
        mModel.set(VOICE_SEARCH_BUTTON_CLICK_LISTENER, new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mFakeboxDelegate.getVoiceRecognitionHandler().startVoiceRecognition(
                        VoiceRecognitionHandler.VoiceInteractionSource.TASKS_SURFACE);
                RecordUserAction.record("TasksSurface.FakeBox.VoiceSearch");
            }
        });

        mIncognitoCookieControlsObserver = new IncognitoCookieControlsManager.Observer() {
            @Override
            public void onUpdate(boolean checked, @CookieControlsEnforcement int enforcement) {
                mModel.set(INCOGNITO_COOKIE_CONTROLS_TOGGLE_ENFORCEMENT, enforcement);
                mModel.set(INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED, checked);
            }
        };
        mIncognitoCookieControlsManager.addObserver(mIncognitoCookieControlsObserver);
        mModel.set(
                INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED_LISTENER, mIncognitoCookieControlsManager);
        mModel.set(INCOGNITO_COOKIE_CONTROLS_ICON_CLICK_LISTENER, mIncognitoCookieControlsManager);

        // Set the initial state.
        mModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, true);
        mModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false);
    }

    /**
     * Called to initialize this Mediator.
     */
    void initialize() {
        mIncognitoCookieControlsManager.initialize();
        mModel.set(INCOGNITO_COOKIE_CONTROLS_CARD_VISIBILITY,
                mIncognitoCookieControlsManager.shouldShowCookieControlsCard());
    }
}
