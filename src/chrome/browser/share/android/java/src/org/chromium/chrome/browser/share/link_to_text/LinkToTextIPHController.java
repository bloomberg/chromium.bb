// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import android.net.Uri;

import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.blink.mojom.TextFragmentReceiver;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * This class is responsible for rendering an IPH, when receiving a link-to-text.
 */
public class LinkToTextIPHController {
    private static final String FEATURE_NAME =
            FeatureConstants.SHARED_HIGHLIGHTING_RECEIVER_FEATURE;

    private final TabModelSelector mTabModelSelector;
    private CurrentTabObserver mCurrentTabObserver;
    private Tracker mTracker;

    /**
     * Creates an {@link LinkToTextIPHController}.
     * @param ObservableSupplier<Tab> An {@link ObservableSupplier} for {@link Tab} where the IPH
     *         will be rendered.
     * @param TabModelSelector The {@link TabModelSelector} to open a new tab.
     */
    public LinkToTextIPHController(
            ObservableSupplier<Tab> tabSupplier, TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        mTracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                if (!ChromeFeatureList.isEnabled(
                            ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE)) {
                    return;
                }

                if (!hasTextFragment(tab, url)) return;

                if (!mTracker.wouldTriggerHelpUI(FEATURE_NAME)) {
                    return;
                }

                getExistingSelectors(tab);
            }
        }, null);
    }

    // Request text fragment selectors for existing highlights
    private void getExistingSelectors(Tab tab) {
        TextFragmentReceiver producer =
                tab.getWebContents().getMainFrame().getInterfaceToRendererFrame(
                        TextFragmentReceiver.MANAGER);
        LinkToTextCoordinator.getExistingSelectors(producer, (text) -> {
            if (text.length > 0) {
                if (mTracker.shouldTriggerHelpUI(FEATURE_NAME)) showMessageIPH(tab);
            }
            producer.close();
        });
    }

    private void showMessageIPH(Tab tab) {
        MessageDispatcher mMessageDispatcher =
                MessageDispatcherProvider.from(tab.getWindowAndroid());
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.SHARED_HIGHLIGHTING)
                        .with(MessageBannerProperties.ICON,
                                VectorDrawableCompat.create(tab.getContext().getResources(),
                                        R.drawable.ink_highlighter, tab.getContext().getTheme()))
                        .with(MessageBannerProperties.TITLE,
                                tab.getContext().getResources().getString(
                                        R.string.iph_message_shared_highlighting_title))
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                tab.getContext().getResources().getString(
                                        R.string.iph_message_shared_highlighting_button))
                        .with(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed)
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                                this::onMessageButtonClicked)
                        .build();
        mMessageDispatcher.enqueueMessage(
                model, tab.getWebContents(), MessageScopeType.NAVIGATION, false);
    }

    private void onMessageButtonClicked() {
        onOpenInChrome(LinkToTextCoordinator.SHARED_HIGHLIGHTING_SUPPORT_URL);
        mTracker.dismissed(FEATURE_NAME);
    }

    private void onMessageDismissed(Integer dismissReason) {
        mTracker.dismissed(FEATURE_NAME);
    }

    private void onOpenInChrome(String linkUrl) {
        mTabModelSelector.openNewTab(new LoadUrlParams(linkUrl), TabLaunchType.FROM_LINK,
                mTabModelSelector.getCurrentTab(), mTabModelSelector.isIncognitoSelected());
    }

    private boolean hasTextFragment(Tab tab, GURL url) {
        Uri uri = Uri.parse(url.getSpec());
        String fragment = uri.getEncodedFragment();
        return fragment != null ? fragment.contains(LinkToTextCoordinator.TEXT_FRAGMENT_PREFIX)
                                : false;
    }
}
