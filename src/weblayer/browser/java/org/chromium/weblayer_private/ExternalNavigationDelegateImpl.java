// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.RemoteException;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Function;
import org.chromium.base.PackageManagerUtils;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.components.external_intents.ExternalNavigationDelegate.StartActivityIfNeededResult;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.ExternalIntentInIncognitoUserDecision;

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
    public Context getContext() {
        return mTab.getBrowser().getContext();
    }

    @Override
    public boolean willAppHandleIntent(Intent intent) {
        return false;
    }

    @Override
    public boolean shouldDisableExternalIntentRequestsForUrl(GURL url) {
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

        // Defer to ExternalNavigationHandler's default logic.
        return StartActivityIfNeededResult.DID_NOT_HANDLE;
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
    public boolean hasCustomLeavingIncognitoDialog() {
        return mTab.getExternalIntentInIncognitoCallbackProxy() != null;
    }

    @Override
    public void presentLeavingIncognitoModalDialog(Callback<Boolean> onUserDecision) {
        try {
            mTab.getExternalIntentInIncognitoCallbackProxy().onExternalIntentInIncognito(
                    (Integer result) -> {
                        @ExternalIntentInIncognitoUserDecision
                        int userDecision = result.intValue();
                        switch (userDecision) {
                            case ExternalIntentInIncognitoUserDecision.ALLOW:
                                onUserDecision.onResult(Boolean.valueOf(true));
                                break;
                            case ExternalIntentInIncognitoUserDecision.DENY:
                                onUserDecision.onResult(Boolean.valueOf(false));
                                break;
                            default:
                                assert false;
                        }
                    });
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    public void maybeAdjustInstantAppExtras(Intent intent, boolean isIntentToInstantApp) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetRequestMetadata(Intent intent, boolean hasUserGesture,
            boolean isRendererInitiated, @Nullable Origin initiatorOrigin) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetPendingReferrer(Intent intent, GURL referrerUrl) {}

    @Override
    // This is relevant only if the intent ends up being handled by this app, which does not happen
    // for WebLayer.
    public void maybeSetPendingIncognitoUrl(Intent intent) {}

    @Override
    public boolean maybeLaunchInstantApp(
            GURL url, GURL referrerUrl, boolean isIncomingRedirect, boolean isSerpReferrer) {
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
    public boolean canCloseTabOnIncognitoIntentLaunch() {
        return hasValidTab();
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
    public @IntentToAutofillAllowingAppResult int isIntentToAutofillAssistantAllowingApp(
            ExternalNavigationParams params, Intent targetIntent,
            Function<Intent, Boolean> canExternalAppHandleIntent) {
        return IntentToAutofillAllowingAppResult.NONE;
    }

    @Override
    public boolean handleWithAutofillAssistant(ExternalNavigationParams params, Intent targetIntent,
            GURL browserFallbackUrl, boolean isGoogleReferrer) {
        return false;
    }

    @Override
    public boolean shouldLaunchWebApksOnInitialIntent() {
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
