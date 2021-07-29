// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.base.FeatureList;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Handles displaying share button on toolbar depending on several conditions (e.g.,device width,
 * whether NTP is shown).
 */
public class ShareButtonController implements ButtonDataProvider, ConfigurationChangedObserver {
    /**
     * Default minimum width to show the share button.
     */
    public static final int MIN_WIDTH_DP = 360;

    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;

    private final ShareUtils mShareUtils;

    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    private final Supplier<Tracker> mTrackerSupplier;

    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    // The activity tab provider.
    private ActivityTabProvider mTabProvider;

    private ButtonDataImpl mButtonData;
    private ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private OnClickListener mOnClickListener;

    private ModalDialogManager mModalDialogManager;
    private ModalDialogManagerObserver mModalDialogManagerObserver;

    private Integer mMinimumWidthDp;
    private int mScreenWidthDp;

    private int mCurrentOrientation;

    /**
     * Creates ShareButtonController object.
     * @param context The Context for retrieving resources, etc.
     * @param buttonDrawable Drawable for the new tab button.
     * @param tabProvider The {@link ActivityTabProvider} used for accessing the tab.
     * @param shareDelegateSupplier The supplier to get a handle on the share delegate.
     * @param trackerSupplier  Supplier for the current profile tracker.
     * @param shareUtils The share utility functions used by this class.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events, e.g.
     * configuration changes.
     * @param modalDialogManager dispatcher for modal lifecycles events
     * @param onShareRunnable A {@link Runnable} to execute when a share event occurs. This object
     *                        does not actually handle sharing, but can provide supplemental
     *                        functionality when the share button is pressed.
     */
    public ShareButtonController(Context context, Drawable buttonDrawable,
            ActivityTabProvider tabProvider,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            Supplier<Tracker> trackerSupplier, ShareUtils shareUtils,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ModalDialogManager modalDialogManager, Runnable onShareRunnable) {
        mContext = context;

        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mTabProvider = tabProvider;
        mShareUtils = shareUtils;

        mShareDelegateSupplier = shareDelegateSupplier;
        mTrackerSupplier = trackerSupplier;
        mOnClickListener = ((view) -> {
            ShareDelegate shareDelegate = mShareDelegateSupplier.get();
            assert shareDelegate
                    != null : "Share delegate became null after share button was displayed";
            if (shareDelegate == null) return;
            Tab tab = mTabProvider.get();
            assert tab != null : "Tab became null after share button was displayed";
            if (tab == null) return;
            if (onShareRunnable != null) onShareRunnable.run();
            RecordUserAction.record("MobileTopToolbarShareButton");
            shareDelegate.share(tab, /*shareDirectly=*/false, ShareOrigin.TOP_TOOLBAR);

            if (mTrackerSupplier.hasValue()) {
                mTrackerSupplier.get().notifyEvent(
                        EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SHARE_OPENED);
            }
        });

        mModalDialogManagerObserver = new ModalDialogManagerObserver() {
            @Override
            public void onDialogAdded(PropertyModel model) {
                mButtonData.setEnabled(false);
                notifyObservers(mButtonData.canShow());
            }

            @Override
            public void onLastDialogDismissed() {
                mButtonData.setEnabled(true);
                notifyObservers(mButtonData.canShow());
            }
        };
        mModalDialogManager = modalDialogManager;
        mModalDialogManager.addObserver(mModalDialogManagerObserver);

        mButtonData = new ButtonDataImpl(/*canShow=*/false, buttonDrawable, mOnClickListener,
                R.string.share, /*supportsTinting=*/true,
                /*iphCommandBuilder=*/null, /*isEnabled=*/true, AdaptiveToolbarButtonVariant.SHARE);

        mScreenWidthDp = mContext.getResources().getConfiguration().screenWidthDp;
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        if (mScreenWidthDp == configuration.screenWidthDp) {
            return;
        }
        mScreenWidthDp = configuration.screenWidthDp;
        updateButtonVisibility(mTabProvider.get());
        notifyObservers(mButtonData.canShow());
    }

    @Override
    public void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }
        if (mModalDialogManagerObserver != null && mModalDialogManager != null) {
            mModalDialogManager.removeObserver(mModalDialogManagerObserver);
            mModalDialogManagerObserver = null;
            mModalDialogManager = null;
        }
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(Tab tab) {
        updateButtonVisibility(tab);
        maybeSetIphCommandBuilder(tab);
        return mButtonData;
    }

    private void updateButtonVisibility(Tab tab) {
        if (tab == null || tab.getWebContents() == null || mTabProvider == null
                || mTabProvider.get() == null || !isFeatureEnabled()) {
            mButtonData.setCanShow(false);
            return;
        }

        if (mMinimumWidthDp == null) {
            mMinimumWidthDp = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR, "minimum_width", MIN_WIDTH_DP);
        }

        final boolean isDeviceWideEnough = mScreenWidthDp >= mMinimumWidthDp;
        if (mShareDelegateSupplier.get() == null || !isDeviceWideEnough) {
            mButtonData.setCanShow(false);
            return;
        }

        mButtonData.setCanShow(mShareUtils.shouldEnableShare(tab));
    }

    private static boolean isFeatureEnabled() {
        return (AdaptiveToolbarFeatures.isSingleVariantModeEnabled()
                       && AdaptiveToolbarFeatures.getSingleVariantMode()
                               == AdaptiveToolbarButtonVariant.SHARE)
                || AdaptiveToolbarFeatures.isCustomizationEnabled();
    }

    private void notifyObservers(boolean hint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(hint);
        }
    }

    /**
     * Since Features are not yet initialized when ButtonData is created, use the
     * fist available opportunity to create and set IPHCommandBuilder. Once set it's
     * never updated.
     */
    private void maybeSetIphCommandBuilder(Tab tab) {
        if (mButtonData.getButtonSpec().getIPHCommandBuilder() != null || tab == null
                || !FeatureList.isInitialized()
                || !AdaptiveToolbarFeatures.isCustomizationEnabled()) {
            return;
        }

        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        IPHCommandBuilder iphCommandBuilder = new IPHCommandBuilder(tab.getContext().getResources(),
                FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_SHARE_FEATURE,
                /* stringId = */ R.string.adaptive_toolbar_button_share_iph,
                /* accessibilityStringId = */ R.string.adaptive_toolbar_button_share_iph)
                                                      .setHighlightParams(params);

        ButtonData.ButtonSpec currentSpec = mButtonData.getButtonSpec();
        ButtonData.ButtonSpec newSpec = new ButtonData.ButtonSpec(currentSpec.getDrawable(),
                currentSpec.getOnClickListener(), currentSpec.getContentDescriptionResId(),
                currentSpec.getSupportsTinting(), iphCommandBuilder,
                currentSpec.getButtonVariant());

        mButtonData.setButtonSpec(newSpec);
    }
}
