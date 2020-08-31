// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ResolveInfo;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.components.external_intents.ExternalNavigationDelegate.StartActivityIfNeededResult;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * WebLayer's implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    private final TabImpl mTab;
    private boolean mTabDestroyed;

    public ExternalNavigationDelegateImpl(TabImpl tab) {
        assert tab != null;
        mTab = tab;
    }

    public void onTabDestroyed() {
        mTabDestroyed = true;
    }

    @Override
    public Activity getActivityContext() {
        return ContextUtils.activityFromContext(mTab.getBrowser().getContext());
    }

    @Override
    public boolean willAppHandleIntent(Intent intent) {
        return false;
    }

    @Override
    public boolean shouldDisableExternalIntentRequestsForUrl(String url) {
        return false;
    }

    @Override
    public boolean handlesInstantAppLaunchingInternally() {
        return false;
    }

    @Override
    public void dispatchAuthenticatedIntent(Intent intent) {
        // This method should never be invoked in WebLayer as this class always returns false for
        // isIntentToInstantApp().
        assert false;
    }

    @Override
    public void didStartActivity(Intent intent) {}

    @Override
    public @StartActivityIfNeededResult int maybeHandleStartActivityIfNeeded(
            Intent intent, boolean proxy) {
        assert !proxy
            : "|proxy| should be true only for instant apps, which WebLayer doesn't handle";

        boolean isExternalProtocol = !UrlUtilities.isAcceptedScheme(intent.toUri(0));
        boolean hasDefaultHandler = hasDefaultHandler(intent);

        // Match CCT's custom behavior of keeping http(s) URLs with no default handler in the app.
        // TODO(blundell): If/when CCT eliminates its special handling of this case, eliminate it
        // from WebLayer as well.
        if (!isExternalProtocol && !hasDefaultHandler) {
            return StartActivityIfNeededResult.HANDLED_WITHOUT_ACTIVITY_START;
        }

        // Otherwise defer to ExternalNavigationHandler's default logic.
        return StartActivityIfNeededResult.DID_NOT_HANDLE;
    }

    @Override
    public boolean startIncognitoIntent(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final boolean needsToCloseTab, final boolean proxy) {
        // TODO(crbug.com/1063399): Determine if this behavior should be refined.
        ExternalNavigationHandler.startActivity(intent, proxy, this);
        return true;
    }

    // This method should never be invoked as WebLayer does not handle incoming intents.
    @Override
    public @OverrideUrlLoadingResult int handleIncognitoIntentTargetingSelf(
            final Intent intent, final String referrerUrl, final String fallbackUrl) {
        assert false;
        return OverrideUrlLoadingResult.NO_OVERRIDE;
    }

    @Override
    public void loadUrlIfPossible(LoadUrlParams loadUrlParams) {
        if (!hasValidTab()) return;
        mTab.loadUrl(loadUrlParams);
    }

    @Override
    public boolean isApplicationInForeground() {
        return mTab.getBrowser().isResumed();
    }

    @Override
    public void maybeSetWindowId(Intent intent) {}

    @Override
    public boolean supportsCreatingNewTabs() {
        // In WebLayer all URLs that ExternalNavigationHandler loads internally are loaded within
        // the current tab; this flow is sufficient for WebLayer from a UX POV, and there is no
        // reason to add the complexity of a flow to create new tabs here. In particular, in Chrome
        // that new tab creation is done by launching an activity targeted at the Chrome package.
        // This would not work for WebLayer as the embedder does not in general handle incoming
        // browsing intents.
        return false;
    }

    @Override
    public void loadUrlInNewTab(final String url, final boolean launchIncognito) {
        // Should never be invoked based on the implementation of supportsCreatingNewTabs().
        assert false;
    }

    @Override
    public boolean canLoadUrlInCurrentTab() {
        return true;
    }

    @Override
    public void closeTab() {
        InterceptNavigationDelegateClientImpl.closeTab(mTab);
    }

    @Override
    public boolean isIncognito() {
        return mTab.getProfile().isIncognito();
    }

    @Override
    public void maybeAdjustInstantAppExtras(Intent intent, boolean isIntentToInstantApp) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetUserGesture(Intent intent) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetPendingReferrer(Intent intent, String referrerUrl) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetPendingIncognitoUrl(Intent intent) {}

    @Override
    public boolean maybeLaunchInstantApp(
            String url, String referrerUrl, boolean isIncomingRedirect, boolean isSerpReferrer) {
        return false;
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        return mTab.getBrowser().getWindowAndroid();
    }

    @Override
    public WebContents getWebContents() {
        return mTab.getWebContents();
    }

    @Override
    public boolean hasValidTab() {
        assert mTab != null;
        return !mTabDestroyed;
    }

    @Override
    public boolean isIntentForTrustedCallingApp(Intent intent) {
        return false;
    }

    @Override
    public boolean isIntentToInstantApp(Intent intent) {
        return false;
    }

    @Override
    public boolean isIntentToAutofillAssistant(Intent intent) {
        return false;
    }

    @Override
    public boolean isValidWebApk(String packageName) {
        // TODO(crbug.com/1063874): Determine whether to refine this.
        return false;
    }

    @Override
    public boolean handleWithAutofillAssistant(ExternalNavigationParams params, Intent targetIntent,
            String browserFallbackUrl, boolean isGoogleReferrer) {
        return false;
    }

    /**
     * Resolve the default external handler of an intent.
     * @return Whether the default external handler is found.
     */
    private boolean hasDefaultHandler(Intent intent) {
        ResolveInfo info = PackageManagerUtils.resolveActivity(intent, 0);
        if (info == null) return false;
        return info.match != 0;
    }
}
