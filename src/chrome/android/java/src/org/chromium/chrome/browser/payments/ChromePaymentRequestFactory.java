// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.OriginSecurityChecker;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestServiceUtil;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.FeaturePolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.services.service_manager.InterfaceFactory;

/**
 * Creates an instance of PaymentRequest for use in Chrome.
 */
public class ChromePaymentRequestFactory implements InterfaceFactory<PaymentRequest> {
    // Tests can inject behaviour on future PaymentRequests via these objects.
    public static ChromePaymentRequestService.Delegate sDelegateForTest;
    @Nullable
    private static ChromePaymentRequestDelegateImplObserverForTest sObserverForTest;
    private final RenderFrameHost mRenderFrameHost;

    /** Observes the {@link ChromePaymentRequestDelegateImpl} for testing. */
    @VisibleForTesting
    /* package */ interface ChromePaymentRequestDelegateImplObserverForTest {
        /**
         * Called after an instance of {@link ChromePaymentRequestDelegateImpl} has just been
         * created.
         * @param delegateImpl The {@link ChromePaymentRequestDelegateImpl}.
         */
        void onCreatedChromePaymentRequestDelegateImpl(
                ChromePaymentRequestDelegateImpl delegateImpl);
    }

    /**
     * Production implementation of the ChromePaymentRequestService's Delegate. Gives true answers
     * about the system.
     */
    @VisibleForTesting
    public static class ChromePaymentRequestDelegateImpl
            implements ChromePaymentRequestService.Delegate {
        private final TwaPackageManagerDelegate mPackageManagerDelegate =
                new TwaPackageManagerDelegate();
        private final RenderFrameHost mRenderFrameHost;
        private boolean mSkipUiForBasicCard;

        private ChromePaymentRequestDelegateImpl(RenderFrameHost renderFrameHost) {
            mRenderFrameHost = renderFrameHost;
        }

        @Override
        public boolean isOffTheRecord() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (liveWebContents == null) return true;
            Profile profile = Profile.fromWebContents(liveWebContents);
            if (profile == null) return true;
            return profile.isOffTheRecord();
        }

        @Override
        public String getInvalidSslCertificateErrorMessage() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (liveWebContents == null) return null;
            if (!OriginSecurityChecker.isSchemeCryptographic(
                        liveWebContents.getLastCommittedUrl())) {
                return null;
            }
            return SslValidityChecker.getInvalidSslCertificateErrorMessage(liveWebContents);
        }

        @Override
        public boolean prefsCanMakePayment() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            return liveWebContents != null
                    && UserPrefs.get(Profile.fromWebContents(liveWebContents))
                               .getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED);
        }

        @Override
        public boolean skipUiForBasicCard() {
            return mSkipUiForBasicCard; // Only tests may set it to true.
        }

        @Override
        @Nullable
        public String getTwaPackageName() {
            WebContents liveWebContents =
                    PaymentRequestServiceUtil.getLiveWebContents(mRenderFrameHost);
            if (liveWebContents == null) return null;
            ChromeActivity activity = ChromeActivity.fromWebContents(liveWebContents);
            return activity != null ? mPackageManagerDelegate.getTwaPackageName(activity) : null;
        }

        @VisibleForTesting
        public void setSkipUiForBasicCard() {
            mSkipUiForBasicCard = true;
        }
    }

    /**
     * Builds a factory for PaymentRequest.
     *
     * @param renderFrameHost The host of the frame that has invoked the PaymentRequest API.
     */
    public ChromePaymentRequestFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    /** Set an observer for the payment request service, cannot be null. */
    @VisibleForTesting
    public static void setChromePaymentRequestDelegateImplObserverForTest(
            ChromePaymentRequestDelegateImplObserverForTest observer) {
        assert observer != null;
        sObserverForTest = observer;
    }

    @Override
    public PaymentRequest createImpl() {
        if (mRenderFrameHost == null) return new InvalidPaymentRequest();
        if (!mRenderFrameHost.isFeatureEnabled(FeaturePolicyFeature.PAYMENT)) {
            mRenderFrameHost.getRemoteInterfaces().onConnectionError(
                    new MojoException(MojoResult.PERMISSION_DENIED));
            return null;
        }

        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS)) {
            return new InvalidPaymentRequest();
        }

        ChromePaymentRequestService.Delegate delegate;
        if (sDelegateForTest != null) {
            delegate = sDelegateForTest;
        } else {
            ChromePaymentRequestDelegateImpl delegateImpl =
                    new ChromePaymentRequestDelegateImpl(mRenderFrameHost);
            if (sObserverForTest != null) {
                sObserverForTest.onCreatedChromePaymentRequestDelegateImpl(/*delegateImpl=*/
                        delegateImpl);
            }
            delegate = delegateImpl;
        }

        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        if (webContents == null || webContents.isDestroyed()) return new InvalidPaymentRequest();

        return PaymentRequestService.createPaymentRequest(mRenderFrameHost,
                /*isOffTheRecord=*/delegate.isOffTheRecord(), delegate,
                (paymentRequestService)
                        -> new ChromePaymentRequestService(paymentRequestService, delegate));
    }
}
