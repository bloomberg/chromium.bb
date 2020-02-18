// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.datareduction.DataReductionProxyUma;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Handles the First Run Experience sequences shown to the user launching Chrome for the first time.
 * It supports only a simple format of FRE:
 *   [Welcome]
 *   [Intro pages...]
 *   [Sign-in page]
 * The activity might be run more than once, e.g. 1) for ToS and sign-in, and 2) for intro.
 */
public class FirstRunActivity extends FirstRunActivityBase implements FirstRunPageDelegate {
    /** Alerted about various events when FirstRunActivity performs them. */
    public interface FirstRunActivityObserver {
        /** See {@link #onFlowIsKnown}. */
        void onFlowIsKnown(Bundle freProperties);

        /** See {@link #acceptTermsOfService}. */
        void onAcceptTermsOfService();

        /** See {@link #jumpToPage}. */
        void onJumpToPage(int position);

        /** Called when First Run is completed. */
        void onUpdateCachedEngineName();

        /** See {@link #abortFirstRunExperience}. */
        void onAbortFirstRunExperience();
    }

    // UMA constants.
    private static final int SIGNIN_SETTINGS_DEFAULT_ACCOUNT = 0;
    private static final int SIGNIN_SETTINGS_ANOTHER_ACCOUNT = 1;
    private static final int SIGNIN_ACCEPT_DEFAULT_ACCOUNT = 2;
    private static final int SIGNIN_ACCEPT_ANOTHER_ACCOUNT = 3;
    private static final int SIGNIN_NO_THANKS = 4;
    private static final int SIGNIN_MAX = 5;
    private static final EnumeratedHistogramSample sSigninChoiceHistogram =
            new EnumeratedHistogramSample("MobileFre.SignInChoice", SIGNIN_MAX);

    private static final int FRE_PROGRESS_STARTED = 0;
    private static final int FRE_PROGRESS_WELCOME_SHOWN = 1;
    private static final int FRE_PROGRESS_DATA_SAVER_SHOWN = 2;
    private static final int FRE_PROGRESS_SIGNIN_SHOWN = 3;
    private static final int FRE_PROGRESS_COMPLETED_SIGNED_IN = 4;
    private static final int FRE_PROGRESS_COMPLETED_NOT_SIGNED_IN = 5;
    private static final int FRE_PROGRESS_DEFAULT_SEARCH_ENGINE_SHOWN = 6;
    private static final int FRE_PROGRESS_MAX = 7;
    private static final EnumeratedHistogramSample sMobileFreProgressMainIntentHistogram =
            new EnumeratedHistogramSample("MobileFre.Progress.MainIntent", FRE_PROGRESS_MAX);
    private static final EnumeratedHistogramSample sMobileFreProgressViewIntentHistogram =
            new EnumeratedHistogramSample("MobileFre.Progress.ViewIntent", FRE_PROGRESS_MAX);

    private static FirstRunActivityObserver sObserver;

    private boolean mShowWelcomePage = true;

    private String mResultSignInAccountName;
    private boolean mResultIsDefaultAccount;
    private boolean mResultShowSignInSettings;

    private boolean mFlowIsKnown;
    private boolean mPostNativePageSequenceCreated;
    private boolean mNativeSideIsInitialized;
    private Set<FirstRunFragment> mPagesToNotifyOfNativeInit;
    private boolean mDeferredCompleteFRE;

    private FirstRunViewPager mPager;

    private FirstRunFlowSequencer mFirstRunFlowSequencer;

    private Bundle mFreProperties;

    /**
     * Whether the first run activity was launched as a result of the user launching Chrome from the
     * Android app list.
     */
    private boolean mLaunchedFromChromeIcon;

    private final List<FirstRunPage> mPages = new ArrayList<>();
    private final List<Integer> mFreProgressStates = new ArrayList<>();

    /**
     * The pager adapter, which provides the pages to the view pager widget.
     */
    private FirstRunPagerAdapter mPagerAdapter;

    /**
     * Defines a sequence of pages to be shown (depending on parameters etc).
     */
    private void createPageSequence() {
        // An optional welcome page.
        if (mShowWelcomePage) {
            mPages.add(new ToSAndUMAFirstRunFragment.Page());
            mFreProgressStates.add(FRE_PROGRESS_WELCOME_SHOWN);
        }

        // Other pages will be created by createPostNativePageSequence() after
        // native has been initialized.
    }

    private void createPostNativePageSequence() {
        // Note: Can't just use POST_NATIVE_SETUP_NEEDED for the early return, because this
        // populates |mPages| which needs to be done even even if onNativeInitialized() was
        // performed in a previous session.
        if (mPostNativePageSequenceCreated) return;
        mFirstRunFlowSequencer.onNativeInitialized(mFreProperties);

        boolean notifyAdapter = false;
        // An optional Data Saver page.
        if (mFreProperties.getBoolean(SHOW_DATA_REDUCTION_PAGE)) {
            mPages.add(new DataReductionProxyFirstRunFragment.Page());
            mFreProgressStates.add(FRE_PROGRESS_DATA_SAVER_SHOWN);
            notifyAdapter = true;
        }

        // An optional page to select a default search engine.
        if (mFreProperties.getBoolean(SHOW_SEARCH_ENGINE_PAGE)) {
            mPages.add(new DefaultSearchEngineFirstRunFragment.Page());
            mFreProgressStates.add(FRE_PROGRESS_DEFAULT_SEARCH_ENGINE_SHOWN);
            notifyAdapter = true;
        }

        // An optional sign-in page.
        if (mFreProperties.getBoolean(SHOW_SIGNIN_PAGE)) {
            mPages.add(SigninFirstRunFragment::new);
            mFreProgressStates.add(FRE_PROGRESS_SIGNIN_SHOWN);
            notifyAdapter = true;
        }

        if (notifyAdapter && mPagerAdapter != null) {
            mPagerAdapter.notifyDataSetChanged();
        }
        mPostNativePageSequenceCreated = true;
    }

    @Override
    protected Bundle transformSavedInstanceStateForOnCreate(Bundle savedInstanceState) {
        // We pass null to Activity.onCreate() so that it doesn't automatically restore
        // the FragmentManager state - as that may cause fragments to be loaded that have
        // dependencies on native before native has been loaded (and then crash). Instead,
        // these fragments will be recreated manually by us and their progression restored
        // from |mFreProperties| which we still get from getSavedInstanceState() below.
        return null;
    }

    /**
     * Creates the content view for this activity.
     * The only thing subclasses can do is wrapping the view returned by super implementation
     * in some extra layout.
     */
    @CallSuper
    protected View createContentView() {
        mPager = new FirstRunViewPager(this);
        mPager.setId(R.id.fre_pager);
        mPager.setOffscreenPageLimit(3);
        return mPager;
    }

    @Override
    public void triggerLayoutInflation() {
        initializeStateFromLaunchData();

        setFinishOnTouchOutside(true);

        setContentView(createContentView());

        mFirstRunFlowSequencer = new FirstRunFlowSequencer(this) {
            @Override
            public void onFlowIsKnown(Bundle freProperties) {
                mFlowIsKnown = true;
                if (freProperties == null) {
                    completeFirstRunExperience();
                    return;
                }

                mFreProperties = freProperties;
                mShowWelcomePage = mFreProperties.getBoolean(SHOW_WELCOME_PAGE);
                if (TextUtils.isEmpty(mResultSignInAccountName)) {
                    mResultSignInAccountName = mFreProperties.getString(
                            SigninFirstRunFragment.FORCE_SIGNIN_ACCOUNT_TO);
                }

                createPageSequence();
                if (mNativeSideIsInitialized) {
                    createPostNativePageSequence();
                }

                if (mPages.size() == 0) {
                    completeFirstRunExperience();
                    return;
                }

                mPagerAdapter = new FirstRunPagerAdapter(getSupportFragmentManager(), mPages);
                stopProgressionIfNotAcceptedTermsOfService();
                mPager.setAdapter(mPagerAdapter);

                if (mNativeSideIsInitialized) {
                    skipPagesIfNecessary();
                }

                if (sObserver != null) sObserver.onFlowIsKnown(mFreProperties);
                recordFreProgressHistogram(mFreProgressStates.get(0));
            }
        };
        mFirstRunFlowSequencer.start();
        FirstRunStatus.setFirstRunTriggered(true);
        recordFreProgressHistogram(FRE_PROGRESS_STARTED);
        onInitialLayoutInflationComplete();
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        Runnable onNativeFinished = new Runnable() {
            @Override
            public void run() {
                if (isActivityFinishingOrDestroyed()) return;

                onNativeDependenciesFullyInitialized();
            }
        };
        TemplateUrlServiceFactory.get().runWhenLoaded(onNativeFinished);
    }

    public boolean isNativeSideIsInitializedForTest() {
        return mNativeSideIsInitialized;
    }

    private void onNativeDependenciesFullyInitialized() {
        mNativeSideIsInitialized = true;
        if (mDeferredCompleteFRE) {
            completeFirstRunExperience();
            mDeferredCompleteFRE = false;
        } else if (mFlowIsKnown) {
            // Note: If mFlowIsKnown is false, then we're not ready to create the post native page
            // sequence - in that case this will be done when onFlowIsKnown() gets called.
            createPostNativePageSequence();
            if (mPagesToNotifyOfNativeInit != null) {
                for (FirstRunFragment page : mPagesToNotifyOfNativeInit) {
                    page.onNativeInitialized();
                }
            }
            mPagesToNotifyOfNativeInit = null;
            skipPagesIfNecessary();
        }
    }

    // Activity:

    @Override
    public void onAttachFragment(Fragment fragment) {
        if (!(fragment instanceof FirstRunFragment)) return;

        FirstRunFragment page = (FirstRunFragment) fragment;
        if (mNativeSideIsInitialized) {
            page.onNativeInitialized();
            return;
        }

        if (mPagesToNotifyOfNativeInit == null) {
            mPagesToNotifyOfNativeInit = new HashSet<>();
        }
        mPagesToNotifyOfNativeInit.add(page);
    }

    @Override
    public void onRestoreInstanceState(Bundle state) {
        // Don't automatically restore state here. This is a counterpart to the override
        // of transformSavedInstanceStateForOnCreate() as the two need to be consistent.
        // The default implementation of this would restore the state of the views, which
        // would otherwise cause a crash in ViewPager used to manage fragments - as it
        // expects consistency between the states restored by onCreate() and this method.
        // Activity doesn't check for null on the parameter, so pass an empty bundle.
        super.onRestoreInstanceState(new Bundle());
    }

    @Override
    public void onStart() {
        super.onStart();
        stopProgressionIfNotAcceptedTermsOfService();
    }

    @Override
    public void onBackPressed() {
        // Terminate if we are still waiting for the native or for Android EDU / GAIA Child checks.
        if (mPagerAdapter == null) {
            abortFirstRunExperience();
            return;
        }

        Object currentItem = mPagerAdapter.instantiateItem(mPager, mPager.getCurrentItem());
        if (currentItem instanceof FirstRunFragment) {
            FirstRunFragment page = (FirstRunFragment) currentItem;
            if (page.interceptBackPressed()) return;
        }

        if (mPager.getCurrentItem() == 0) {
            abortFirstRunExperience();
        } else {
            mPager.setCurrentItem(mPager.getCurrentItem() - 1, false);
        }
    }

    // FirstRunPageDelegate:
    @Override
    public Bundle getProperties() {
        return mFreProperties;
    }

    @Override
    public void advanceToNextPage() {
        jumpToPage(mPager.getCurrentItem() + 1);
    }

    @Override
    public void abortFirstRunExperience() {
        finish();

        notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), false);
        if (sObserver != null) sObserver.onAbortFirstRunExperience();
    }

    @Override
    public void completeFirstRunExperience() {
        if (!mNativeSideIsInitialized) {
            mDeferredCompleteFRE = true;
            return;
        }
        if (!TextUtils.isEmpty(mResultSignInAccountName)) {
            final int choice;
            if (mResultShowSignInSettings) {
                choice = mResultIsDefaultAccount ? SIGNIN_SETTINGS_DEFAULT_ACCOUNT
                                                 : SIGNIN_SETTINGS_ANOTHER_ACCOUNT;
            } else {
                choice = mResultIsDefaultAccount ? SIGNIN_ACCEPT_DEFAULT_ACCOUNT
                                                 : SIGNIN_ACCEPT_ANOTHER_ACCOUNT;
            }
            sSigninChoiceHistogram.record(choice);
            recordFreProgressHistogram(FRE_PROGRESS_COMPLETED_SIGNED_IN);
        } else {
            recordFreProgressHistogram(FRE_PROGRESS_COMPLETED_NOT_SIGNED_IN);
        }

        FirstRunFlowSequencer.markFlowAsCompleted(
                mResultSignInAccountName, mResultShowSignInSettings);

        if (DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo()) {
            if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
                DataReductionProxyUma
                        .dataReductionProxyUIAction(DataReductionProxyUma.ACTION_FRE_ENABLED);
                DataReductionPromoUtils.saveFrePromoOptOut(false);
            } else {
                DataReductionProxyUma
                        .dataReductionProxyUIAction(DataReductionProxyUma.ACTION_FRE_DISABLED);
                DataReductionPromoUtils.saveFrePromoOptOut(true);
            }
        }

        // Update the search engine name cached by the widget.
        SearchWidgetProvider.updateCachedEngineName();
        if (sObserver != null) sObserver.onUpdateCachedEngineName();

        if (!sendFirstRunCompletePendingIntent()) {
            finish();
        } else {
            ApplicationStatus.registerStateListenerForAllActivities(new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    boolean shouldFinish = false;
                    if (activity == FirstRunActivity.this) {
                        shouldFinish = (newState == ActivityState.STOPPED
                                || newState == ActivityState.DESTROYED);
                    } else {
                        shouldFinish = newState == ActivityState.RESUMED;
                    }
                    if (shouldFinish) {
                        finish();
                        ApplicationStatus.unregisterActivityStateListener(this);
                    }
                }
            });
        }
    }

    @Override
    public void refuseSignIn() {
        sSigninChoiceHistogram.record(SIGNIN_NO_THANKS);
        mResultSignInAccountName = null;
        mResultShowSignInSettings = false;
    }

    @Override
    public void acceptSignIn(String accountName, boolean isDefaultAccount, boolean openSettings) {
        mResultSignInAccountName = accountName;
        mResultIsDefaultAccount = isDefaultAccount;
        mResultShowSignInSettings = openSettings;
    }

    @Override
    public boolean didAcceptTermsOfService() {
        boolean result = FirstRunUtils.didAcceptTermsOfService();
        if (sObserver != null) sObserver.onAcceptTermsOfService();
        return result;
    }

    @Override
    public void acceptTermsOfService(boolean allowCrashUpload) {
        // If default is true then it corresponds to opt-out and false corresponds to opt-in.
        UmaUtils.recordMetricsReportingDefaultOptIn(!DEFAULT_METRICS_AND_CRASH_REPORTING);
        FirstRunUtils.acceptTermsOfService(allowCrashUpload);
        FirstRunStatus.setSkipWelcomePage(true);
        flushPersistentData();
        stopProgressionIfNotAcceptedTermsOfService();
        jumpToPage(mPager.getCurrentItem() + 1);
    }

    /** Initialize local state from launch intent and from saved instance state. */
    private void initializeStateFromLaunchData() {
        if (getIntent() != null) {
            mLaunchedFromChromeIcon =
                    getIntent().getBooleanExtra(EXTRA_COMING_FROM_CHROME_ICON, false);
        }
    }

    /**
     * Transitions to a given page.
     * @return Whether the transition to a given page was allowed.
     * @param position A page index to transition to.
     */
    private boolean jumpToPage(int position) {
        if (sObserver != null) sObserver.onJumpToPage(position);

        if (mShowWelcomePage && !didAcceptTermsOfService()) {
            return position == 0;
        }
        if (position >= mPagerAdapter.getCount()) {
            completeFirstRunExperience();
            return false;
        }
        mPager.setCurrentItem(position, false);
        recordFreProgressHistogram(mFreProgressStates.get(position));
        return true;
    }

    private void stopProgressionIfNotAcceptedTermsOfService() {
        if (mPagerAdapter == null) return;
        mPagerAdapter.setStopAtTheFirstPage(mShowWelcomePage && !didAcceptTermsOfService());
    }

    private void skipPagesIfNecessary() {
        if (mPagerAdapter == null) return;

        boolean shouldSkip = mPages.get(mPager.getCurrentItem()).shouldSkipPageOnCreate();
        while (shouldSkip) {
            if (!jumpToPage(mPager.getCurrentItem() + 1)) return;
            shouldSkip = mPages.get(mPager.getCurrentItem()).shouldSkipPageOnCreate();
        }
    }

    private void recordFreProgressHistogram(int state) {
        if (mLaunchedFromChromeIcon) {
            sMobileFreProgressMainIntentHistogram.record(state);
        } else {
            sMobileFreProgressViewIntentHistogram.record(state);
        }
    }

    @Override
    public void showInfoPage(@StringRes int url) {
        CustomTabActivity.showInfoPage(
                this, LocalizationUtils.substituteLocalePlaceholder(getString(url)));
    }

    @VisibleForTesting
    public static void setObserverForTest(FirstRunActivityObserver observer) {
        assert sObserver == null;
        sObserver = observer;
    }
}
