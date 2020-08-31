// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.app.Activity;
import android.content.Intent;
import android.os.ConditionVariable;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import com.google.android.gms.fido.fido2.api.common.ErrorCode;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.AuthenticatorStatus;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.test.mock.MockRenderFrameHost;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link Fido2CredentialRequestTest}.
 */

@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-experimental-web-platform-features", "enable-features=WebAuthentication",
        "ignore-certificate-errors"})
public class Fido2CredentialRequestTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    @Rule
    public JniMocker mocker = new JniMocker();

    private MockActivityWindowAndroid mWindowAndroid;
    private EmbeddedTestServer mTestServer;
    private String mUrl;
    private MockAuthenticatorRenderFrameHost mFrameHost;
    private Origin mOrigin;
    private AuthenticatorImpl.Natives mTestAuthenticatorImplJni;
    private AuthenticatorImpl mAuthenticatorImpl;
    private Fido2CredentialRequest mRequest;
    private PublicKeyCredentialCreationOptions mCreationOptions;
    private PublicKeyCredentialRequestOptions mRequestOptions;
    private static final String GOOGLE_PLAY_SERVICES_PACKAGE = "com.google.android.gms";
    private static final String FILLER_ERROR_MSG = "Error Error";
    private AuthenticatorCallback mCallback;
    private long mStartTimeMs;

    /**
     * This class constructs the parameters array that is used for testMakeCredential_with_param and
     * testGetAssertion_with_param as input parameters.
     */
    public static class ErrorTestParams implements ParameterProvider {
        private static List<ParameterSet> sErrorTestParams = Arrays.asList(
                new ParameterSet()
                        .value("SECURITY_ERR", FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.INVALID_DOMAIN))
                        .name("securityError"),
                new ParameterSet()
                        .value("TIMEOUT_ERR", FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR))
                        .name("timeoutError"),
                new ParameterSet()
                        .value("ENCODING_ERR", FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                        .name("encodingError"),
                new ParameterSet()
                        .value("NOT_ALLOWED_ERR", FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR))
                        .name("notAllowedError"),
                new ParameterSet()
                        .value("DATA_ERR", FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.ANDROID_NOT_SUPPORTED_ERROR))
                        .name("dataError"),
                new ParameterSet()
                        .value("NOT_SUPPORTED_ERR", FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.ANDROID_NOT_SUPPORTED_ERROR))
                        .name("notSupportedError"),
                new ParameterSet()
                        .value("CONSTRAINT_ERR", FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.CREDENTIAL_EXCLUDED))
                        .name("constraintErrorReRegistration"),
                new ParameterSet()
                        .value("INVALID_STATE_ERR", FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                        .name("invalidStateError"),
                new ParameterSet()
                        .value("UNKNOWN_ERR", FILLER_ERROR_MSG,
                                Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR))
                        .name("unknownError"));
        @Override
        public List<ParameterSet> getParameters() {
            return sErrorTestParams;
        }
    }

    /**
     * Mock class for tests.
     */
    private class MockActivityWindowAndroid extends ActivityWindowAndroid {
        private int mResultCode = Activity.RESULT_OK;
        private boolean mCancelableIntentSuccess = true;
        private Intent mResponseIntent;

        /**
         * Constructor.
         * @param activity The app activity.
         */
        public MockActivityWindowAndroid(ChromeActivity activity) {
            super(activity);
        }

        /**
         * Mocks sending the intent to GmsCore.
         * @param intent Fido2PendingIntent provided by the GmsCore API.
         * @param callback The intent callback.
         * @param contents The browser webcontents.
         * @return success status of the call.
         */
        @Override
        public int showCancelableIntent(Callback<Integer> intentTrigger,
                WindowAndroid.IntentCallback callback, Integer errorId) {
            // Bypass GmsCore and just call onIntentCompleted.
            if (mCancelableIntentSuccess) {
                callback.onIntentCompleted(this, mResultCode, mResponseIntent);
                return 0;
            }
            return WindowAndroid.START_INTENT_FAILURE;
        }

        /**
         *
         * @param callback The object that should have received the results.
         * @return success status of the call.
         */
        @Override
        public boolean removeIntentCallback(IntentCallback callback) {
            return true;
        }

        /**
         * Overrides the success status of sending the intent.
         * @param success
         */
        public void setCancelableIntentSuccess(boolean success) {
            mCancelableIntentSuccess = success;
        }

        /**
         * Overrides the result code returned in the callback.
         * @param resultCode
         */
        public void setResultCode(int resultCode) {
            mResultCode = resultCode;
        }

        /**
         * Overrides the intent returned in the callback.
         * @param intent
         */
        public void setResponseIntent(Intent intent) {
            mResponseIntent = intent;
        }
    }

    private static class AuthenticatorCallback extends HandlerResponseCallback {
        private Integer mStatus;
        private MakeCredentialAuthenticatorResponse mMakeCredentialResponse;
        private GetAssertionAuthenticatorResponse mGetAssertionAuthenticatorResponse;

        // Signals when request is complete.
        private final ConditionVariable mDone = new ConditionVariable();

        AuthenticatorCallback() {}

        @Override
        public void onRegisterResponse(
                Integer status, MakeCredentialAuthenticatorResponse response) {
            assert mStatus == null;
            mStatus = status;
            mMakeCredentialResponse = response;
            unblock();
        }

        @Override
        public void onSignResponse(Integer status, GetAssertionAuthenticatorResponse response) {
            assert mStatus == null;
            mStatus = status;
            mGetAssertionAuthenticatorResponse = response;
            unblock();
        }

        @Override
        public void onError(Integer status) {
            assert mStatus == null;
            mStatus = status;
            unblock();
        }

        public Integer getStatus() {
            return mStatus;
        }

        public MakeCredentialAuthenticatorResponse getMakeCredentialResponse() {
            return mMakeCredentialResponse;
        }

        public GetAssertionAuthenticatorResponse getGetAssertionResponse() {
            return mGetAssertionAuthenticatorResponse;
        }

        public void blockUntilCalled() {
            mDone.block();
        }

        private void unblock() {
            mDone.open();
        }
    }

    private static class MockFido2ApiHandler extends Fido2ApiHandler {
        private Fido2CredentialRequest mRequest;

        MockFido2ApiHandler(Fido2CredentialRequest request) {
            mRequest = request;
        }

        @Override
        protected void makeCredential(PublicKeyCredentialCreationOptions options,
                RenderFrameHost frameHost, Origin origin, HandlerResponseCallback callback) {
            mRequest.handleMakeCredentialRequest(options, frameHost, origin, callback);
        }

        @Override
        protected void getAssertion(PublicKeyCredentialRequestOptions options,
                RenderFrameHost frameHost, Origin origin, HandlerResponseCallback callback) {
            mRequest.handleGetAssertionRequest(options, frameHost, origin, callback);
        }

        @Override
        protected void isUserVerifyingPlatformAuthenticatorAvailable(
                RenderFrameHost frameHost, HandlerResponseCallback callback) {
            mRequest.handleIsUserVerifyingPlatformAuthenticatorAvailableRequest(
                    frameHost, callback);
        }
    }

    private static class TestAuthenticatorImplJni implements AuthenticatorImpl.Natives {
        private AuthenticatorCallback mCallback;

        TestAuthenticatorImplJni(AuthenticatorCallback callback) {
            mCallback = callback;
        }

        @Override
        public void invokeMakeCredentialResponse(
                long nativeInternalAuthenticatorAndroid, int status, ByteBuffer byteBuffer) {
            mCallback.onRegisterResponse(status,
                    byteBuffer == null
                            ? null
                            : MakeCredentialAuthenticatorResponse.deserialize(byteBuffer));
        }

        @Override
        public void invokeGetAssertionResponse(
                long nativeInternalAuthenticatorAndroid, int status, ByteBuffer byteBuffer) {
            mCallback.onSignResponse(status,
                    byteBuffer == null ? null
                                       : GetAssertionAuthenticatorResponse.deserialize(byteBuffer));
        }

        @Override
        public void invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                long nativeInternalAuthenticatorAndroid, boolean isUVPAA) {}
    }

    private static class MockOrigin extends Origin {
        private GURL mUrl;

        public void setUrl(String url) {
            mUrl = new GURL(url);
        }

        @Override
        public String getScheme() {
            return mUrl.getScheme();
        }

        @Override
        public String getHost() {
            return mUrl.getHost();
        }

        @Override
        public int getPort() {
            return Integer.parseInt(mUrl.getPort());
        }

        @Override
        public boolean isOpaque() {
            return false;
        }
    }

    private static class MockAuthenticatorRenderFrameHost extends MockRenderFrameHost {
        private String mLastUrl;
        private MockOrigin mLastOrigin;

        @Override
        public String getLastCommittedURL() {
            return mLastUrl;
        }

        @Override
        public Origin getLastCommittedOrigin() {
            return mLastOrigin;
        }

        public void setLastCommittedURL(String url) {
            mLastUrl = url;
            mLastOrigin = new MockOrigin();
            mLastOrigin.setUrl(url);
        }
    }

    @Before
    public void setUp() throws Exception {
        Assume.assumeTrue(gmsVersionSupported());
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        mCallback = new AuthenticatorCallback();
        mUrl = mTestServer.getURLWithHostName(
                "subdomain.example.test", "/content/test/data/android/authenticator.html");
        mActivityTestRule.loadUrl(mUrl);
        mFrameHost = new MockAuthenticatorRenderFrameHost();
        mFrameHost.setLastCommittedURL(mUrl);
        mOrigin = mFrameHost.getLastCommittedOrigin();
        mRequest = new Fido2CredentialRequest();
        mRequest.setWebContentsForTesting(new MockWebContents());
        Fido2ApiHandler.overrideInstanceForTesting(new MockFido2ApiHandler(mRequest));

        MockitoAnnotations.initMocks(this);
        mTestAuthenticatorImplJni = new TestAuthenticatorImplJni(mCallback);
        mocker.mock(AuthenticatorImplJni.TEST_HOOKS, mTestAuthenticatorImplJni);

        mAuthenticatorImpl = AuthenticatorImpl.create(0, mFrameHost);
        mCreationOptions = Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        mRequestOptions = Fido2ApiTestHelper.createDefaultGetAssertionOptions();
        ThreadUtils.runOnUiThreadBlocking(()
                                                  -> mWindowAndroid = new MockActivityWindowAndroid(
                                                             mActivityTestRule.getActivity()));
        mStartTimeMs = SystemClock.elapsedRealtime();
    }

    @After
    public void tearDown() {}

    /**
     * Used to enable early exit of tests on bots that don't support GmsCore v16.1+
     */
    private boolean gmsVersionSupported() {
        if (PackageUtils.getPackageVersion(
                    ContextUtils.getApplicationContext(), GOOGLE_PLAY_SERVICES_PACKAGE)
                >= Fido2ApiHandler.GMSCORE_MIN_VERSION) {
            return true;
        }
        return false;
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1083360")
    public void testConvertOriginToString_defaultPortRemoved() {
        MockOrigin origin = new MockOrigin();
        origin.setUrl("https://www.example.com:443");

        String parsedOrigin = mRequest.convertOriginToString(origin);
        Assert.assertEquals(parsedOrigin, "https://www.example.com");
    }

    @Test
    @SmallTest
    public void testMakeCredential_success() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_unsuccessfulAttemptToShowCancelableIntent() {
        mWindowAndroid.setCancelableIntentSuccess(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_missingExtra() {
        // An intent missing FIDO2_KEY_RESPONSE_EXTRA.
        mWindowAndroid.setResponseIntent(new Intent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_nullIntent() {
        // Don't set an intent to be returned at all.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_resultCanceled() {
        mWindowAndroid.setResultCode(Activity.RESULT_CANCELED);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_resultUnknown() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        mWindowAndroid.setResultCode(Activity.RESULT_FIRST_USER);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_nullRpIcon() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.relyingParty.icon = null;
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_nullUserIcon() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.user.icon = null;
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_noEligibleParameters() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.algorithmIdentifier = 1; // Not a valid algorithm identifier.
        parameters.type = PublicKeyCredentialType.PUBLIC_KEY;
        customOptions.publicKeyParameters = new PublicKeyCredentialParameters[] {parameters};

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.ALGORITHM_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_noEligibleParameters2() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.type = 10; // Not a valid type.
        customOptions.publicKeyParameters = new PublicKeyCredentialParameters[] {parameters};

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.ALGORITHM_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_parametersContainEligibleAndNoneligible() {
        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.algorithmIdentifier = 1; // Not a valid algorithm identifier.
        parameters.type = PublicKeyCredentialType.PUBLIC_KEY;
        PublicKeyCredentialParameters[] multiParams = new PublicKeyCredentialParameters[] {
                customOptions.publicKeyParameters[0], parameters};
        customOptions.publicKeyParameters = multiParams;
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_noExcludeCredentials() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.excludeCredentials = null;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredential_success() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRequest.setActivityWindowForTesting(mWindowAndroid);
            mAuthenticatorImpl.makeCredential(mCreationOptions,
                    (status, response) -> mCallback.onRegisterResponse(status, response));
        });

        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredential_resultCanceled() {
        mWindowAndroid.setResultCode(Activity.RESULT_CANCELED);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRequest.setActivityWindowForTesting(mWindowAndroid);
            mAuthenticatorImpl.makeCredential(mCreationOptions,
                    (status, response) -> mCallback.onRegisterResponse(status, response));
        });

        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredentialBridge_success() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRequest.setActivityWindowForTesting(mWindowAndroid);
            mAuthenticatorImpl.makeCredentialBridge(mCreationOptions.serialize());
        });

        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplMakeCredentialBridge_resultCanceled() {
        mWindowAndroid.setResultCode(Activity.RESULT_CANCELED);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRequest.setActivityWindowForTesting(mWindowAndroid);
            mAuthenticatorImpl.makeCredentialBridge(mCreationOptions.serialize());
        });
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Assert.assertNull(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertionWithoutUvmRequested_success() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }
    @Test
    @SmallTest
    public void testGetAssertionWithUvmRequestedWithoutUvmResponded_success() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequestOptions.userVerificationMethods = true;
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertionWithUvmRequestedWithUvmResponded_success() {
        mWindowAndroid.setResponseIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequestOptions.userVerificationMethods = true;
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_unsuccessfulAttemptToShowCancelableIntent() {
        mWindowAndroid.setCancelableIntentSuccess(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_missingExtra() {
        // An intent missing FIDO2_KEY_RESPONSE_EXTRA.
        mWindowAndroid.setResponseIntent(new Intent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_nullIntent() {
        // Don't set an intent to be returned at all.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_resultCanceled() {
        mWindowAndroid.setResultCode(Activity.RESULT_CANCELED);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_resultUnknown() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        mWindowAndroid.setResultCode(Activity.RESULT_FIRST_USER);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.UNKNOWN_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_unknownErrorCredentialNotRecognized() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createErrorIntent(
                ErrorCode.UNKNOWN_ERR, "Low level error 0x6a80"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.CREDENTIAL_NOT_RECOGNIZED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_appIdUsed() {
        PublicKeyCredentialRequestOptions customOptions = mRequestOptions;
        customOptions.appid = "www.example.com";
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulGetAssertionIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        GetAssertionAuthenticatorResponse response = mCallback.getGetAssertionResponse();
        Fido2ApiTestHelper.validateGetAssertionResponse(response);
        Assert.assertEquals(response.echoAppidExtension, true);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplGetAssertionWithUvmRequestedWithUvmResponded_success() {
        mWindowAndroid.setResponseIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());
        mRequestOptions.userVerificationMethods = true;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRequest.setActivityWindowForTesting(mWindowAndroid);
            mAuthenticatorImpl.getAssertion(mRequestOptions,
                    (status, response) -> mCallback.onSignResponse(status, response));
        });
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplGetAssertion_resultCanceled() {
        mWindowAndroid.setResultCode(Activity.RESULT_CANCELED);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRequest.setActivityWindowForTesting(mWindowAndroid);
            mAuthenticatorImpl.getAssertion(mRequestOptions,
                    (status, response) -> mCallback.onSignResponse(status, response));
        });
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplGetAssertionBridgeWithUvmRequestedWithUvmResponded_success() {
        mWindowAndroid.setResponseIntent(
                Fido2ApiTestHelper.createSuccessfulGetAssertionIntentWithUvm());
        mRequestOptions.userVerificationMethods = true;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRequest.setActivityWindowForTesting(mWindowAndroid);
            mAuthenticatorImpl.getAssertionBridge(mRequestOptions.serialize());
        });
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateGetAssertionResponse(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testAuthenticatorImplGetAssertionBridge_resultCanceled() {
        mWindowAndroid.setResultCode(Activity.RESULT_CANCELED);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRequest.setActivityWindowForTesting(mWindowAndroid);
            mAuthenticatorImpl.getAssertionBridge(mRequestOptions.serialize());
        });

        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.NOT_ALLOWED_ERROR));
        Assert.assertNull(mCallback.getGetAssertionResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationNone() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation = org.chromium.blink.mojom.AttestationConveyancePreference.NONE;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationIndirect() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation =
                org.chromium.blink.mojom.AttestationConveyancePreference.INDIRECT;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationDirect() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation = org.chromium.blink.mojom.AttestationConveyancePreference.DIRECT;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_attestationEnterprise() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createSuccessfulMakeCredentialIntent());
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));

        PublicKeyCredentialCreationOptions customOptions = mCreationOptions;
        customOptions.attestation =
                org.chromium.blink.mojom.AttestationConveyancePreference.ENTERPRISE;
        mRequest.handleMakeCredentialRequest(customOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.SUCCESS));
        Fido2ApiTestHelper.validateMakeCredentialResponse(mCallback.getMakeCredentialResponse());
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_invalidStateErrorDuplicateRegistration() {
        mWindowAndroid.setResponseIntent(
                Fido2ApiTestHelper.createErrorIntent(ErrorCode.INVALID_STATE_ERR,
                        "One of the excluded credentials exists on the local device"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));
        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(
                mCallback.getStatus(), Integer.valueOf(AuthenticatorStatus.CREDENTIAL_EXCLUDED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_emptyAllowCredentials1() {
        PublicKeyCredentialRequestOptions customOptions = mRequestOptions;
        customOptions.allowCredentials = null;

        // Passes conversion and gets rejected by GmsCore
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createErrorIntent(
                ErrorCode.NOT_ALLOWED_ERR, "Authentication request must have non-empty allowList"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.EMPTY_ALLOW_CREDENTIALS));
        // Verify the response returned immediately.
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_emptyAllowCredentials2() {
        PublicKeyCredentialRequestOptions customOptions = mRequestOptions;
        customOptions.allowCredentials = null;

        // Passes conversion and gets rejected by GmsCore
        mWindowAndroid.setResponseIntent(
                Fido2ApiTestHelper.createErrorIntent(ErrorCode.NOT_ALLOWED_ERR,
                        "Request doesn't have a valid list of allowed credentials."));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.EMPTY_ALLOW_CREDENTIALS));
        // Verify the response returned immediately.
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testMakeCredential_constraintErrorNoScreenlock() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createErrorIntent(
                ErrorCode.CONSTRAINT_ERR, "The device is not secured with any screen lock"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));
        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    public void testGetAssertion_constraintErrorNoScreenlock() {
        mWindowAndroid.setResponseIntent(Fido2ApiTestHelper.createErrorIntent(
                ErrorCode.CONSTRAINT_ERR, "The device is not secured with any screen lock"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(),
                Integer.valueOf(AuthenticatorStatus.USER_VERIFICATION_UNSUPPORTED));
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testMakeCredential_with_param(
            String errorCodeName, String errorMsg, Integer status) {
        mWindowAndroid.setResponseIntent(
                Fido2ApiTestHelper.createErrorIntent(ErrorCode.valueOf(errorCodeName), errorMsg));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));
        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testGetAssertion_with_param(String errorCodeName, String errorMsg, Integer status) {
        mWindowAndroid.setResponseIntent(
                Fido2ApiTestHelper.createErrorIntent(ErrorCode.valueOf(errorCodeName), errorMsg));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testMakeCredential_with_param_nullErrorMessage(
            String errorCodeName, String errorMsg, Integer status) {
        mWindowAndroid.setResponseIntent(
                Fido2ApiTestHelper.createErrorIntent(ErrorCode.valueOf(errorCodeName), null));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));
        mRequest.handleMakeCredentialRequest(mCreationOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }

    @Test
    @SmallTest
    @UseMethodParameter(ErrorTestParams.class)
    public void testGetAssertion_with_param_nullErrorMessage(
            String errorCodeName, String errorMsg, Integer status) {
        mWindowAndroid.setResponseIntent(
                Fido2ApiTestHelper.createErrorIntent(ErrorCode.valueOf(errorCodeName), null));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mRequest.setActivityWindowForTesting(mWindowAndroid));
        mRequest.handleGetAssertionRequest(mRequestOptions, mFrameHost, mOrigin, mCallback);
        mCallback.blockUntilCalled();
        Assert.assertEquals(mCallback.getStatus(), status);
        Fido2ApiTestHelper.verifyRespondedBeforeTimeout(mStartTimeMs);
    }
}
