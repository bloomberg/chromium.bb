// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Browser;
import android.support.test.InstrumentationRegistry;
import android.test.mock.MockPackageManager;

import androidx.appcompat.app.AlertDialog;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Function;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.external_intents.ExternalNavigationDelegate.IntentToAutofillAllowingAppResult;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingAsyncActionType;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Pattern;

/**
 * Instrumentation tests for {@link ExternalNavigationHandler}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
// clang-format off
@Batch(Batch.UNIT_TESTS)
public class ExternalNavigationHandlerTest {
    // clang-format on
    // Expectations
    private static final int IGNORE = 0x0;
    private static final int START_INCOGNITO = 0x1;
    private static final int START_WEBAPK = 0x2;
    private static final int START_FILE = 0x4;
    private static final int START_OTHER_ACTIVITY = 0x10;
    private static final int INTENT_SANITIZATION_EXCEPTION = 0x20;
    private static final int PROXY_FOR_INSTANT_APPS = 0x40;

    private static final boolean IS_CUSTOM_TAB_INTENT = true;
    private static final boolean SEND_TO_EXTERNAL_APPS = true;
    private static final boolean IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED = true;
    private static final boolean HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY = true;
    private static final boolean INTENT_STARTED_TASK = true;

    private static final String INTENT_APP_PACKAGE_NAME = "com.imdb.mobile";
    private static final String YOUTUBE_URL = "http://youtube.com/";
    private static final String YOUTUBE_MOBILE_URL = "http://m.youtube.com";
    private static final String YOUTUBE_PACKAGE_NAME = "youtube";

    private static final String SEARCH_RESULT_URL_FOR_TOM_HANKS =
            "https://www.google.com/search?q=tom+hanks";
    private static final String IMDB_WEBPAGE_FOR_TOM_HANKS = "http://m.imdb.com/name/nm0000158";
    private static final String INTENT_URL_WITH_FALLBACK_URL =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(IMDB_WEBPAGE_FOR_TOM_HANKS) + ";end";
    private static final String INTENT_URL_WITH_FALLBACK_URL_WITHOUT_PACKAGE_NAME =
            "intent:///name/nm0000158#Intent;scheme=imdb;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(IMDB_WEBPAGE_FOR_TOM_HANKS) + ";end";
    private static final String SOME_JAVASCRIPT_PAGE = "javascript:window.open(0);";
    private static final String INTENT_URL_WITH_JAVASCRIPT_FALLBACK_URL =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(SOME_JAVASCRIPT_PAGE) + ";end";
    private static final String IMDB_APP_INTENT_FOR_TOM_HANKS = "imdb:///name/nm0000158";
    private static final String INTENT_URL_WITH_CHAIN_FALLBACK_URL =
            "intent://scan/#Intent;scheme=zxing;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode("http://url.myredirector.com/aaa") + ";end";
    private static final String ENCODED_MARKET_REFERRER =
            "_placement%3D{placement}%26network%3D{network}%26device%3D{devicemodel}";
    private static final String INTENT_APP_NOT_INSTALLED_DEFAULT_MARKET_REFERRER =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;end";
    private static final String INTENT_APP_NOT_INSTALLED_WITH_MARKET_REFERRER =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;S."
            + ExternalNavigationHandler.EXTRA_MARKET_REFERRER + "=" + ENCODED_MARKET_REFERRER
            + ";end";
    private static final String INTENT_URL_FOR_CHROME_CUSTOM_TABS = "intent://example.com#Intent;"
            + "package=org.chromium.chrome;"
            + "action=android.intent.action.VIEW;"
            + "scheme=http;"
            + "S.android.support.customtabs.extra.SESSION=;"
            + "end;";
    private static final String INTENT_URL_FOR_CHROME = "intent://example.com#Intent;"
            + "package=org.chromium.chrome;"
            + "action=android.intent.action.VIEW;"
            + "scheme=http;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(YOUTUBE_URL) + ";end"
            + "end;";

    private static final String PLUS_STREAM_URL = "https://plus.google.com/stream";
    private static final String CALENDAR_URL = "http://www.google.com/calendar";
    private static final String KEEP_URL = "http://www.google.com/keep";

    private static final String TEXT_APP_1_PACKAGE_NAME = "text_app_1";
    private static final String TEXT_APP_2_PACKAGE_NAME = "text_app_2";

    private static final String WEBAPK_SCOPE = "https://www.template.com";
    private static final String WEBAPK_PACKAGE_PREFIX = "org.chromium.webapk";
    private static final String WEBAPK_PACKAGE_NAME = WEBAPK_PACKAGE_PREFIX + ".template";
    private static final String INVALID_WEBAPK_PACKAGE_NAME = WEBAPK_PACKAGE_PREFIX + ".invalid";

    private static final String[] SUPERVISOR_START_ACTIONS = {
            "com.google.android.instantapps.START", "com.google.android.instantapps.nmr1.INSTALL",
            "com.google.android.instantapps.nmr1.VIEW"};

    private static final String AUTOFILL_ASSISTANT_INTENT_URL_WITH_FALLBACK =
            "intent://www.example.com#Intent;scheme=https;"
            + "B.org.chromium.chrome.browser.autofill_assistant.ENABLED=true;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode("https://www.example.com") + ";end";

    private static final String AUTOFILL_ASSISTANT_INTENT_URL_WITHOUT_FALLBACK =
            "intent://www.example.com#Intent;scheme=https;"
            + "B.org.chromium.chrome.browser.autofill_assistant.ENABLED=true;"
            + "end;";

    private static final String AUTOFILL_ASSISTANT_INTENT_URL_WITH_SOMEAPP =
            "intent://someapp.com#Intent;scheme=https;"
            + "B.org.chromium.chrome.browser.autofill_assistant.ENABLED=true;"
            + "S.org.chromium.chrome.browser.autofill_assistant.ALLOW_APP=true;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode("https://someapp.com") + ";end";

    private static final String IS_INSTANT_APP_EXTRA = "IS_INSTANT_APP";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    AlertDialog mAlertDialog;

    private Context mContext;
    private final TestExternalNavigationDelegate mDelegate;
    private ExternalNavigationHandlerForTesting mUrlHandler;

    private Context mApplicationContextToRestore;

    public ExternalNavigationHandlerTest() {
        mDelegate = new TestExternalNavigationDelegate();
        mUrlHandler = new ExternalNavigationHandlerForTesting(mDelegate);
    }

    @Before
    public void setUp() {
        mApplicationContextToRestore = ContextUtils.getApplicationContext();

        mContext = new TestContext(InstrumentationRegistry.getTargetContext(), mDelegate);
        ContextUtils.initApplicationContextForTests(mContext);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        ContextUtils.initApplicationContextForTests(mApplicationContextToRestore);
        // Any tests showing the dialog should have closed the dialog, even if assertions in the
        // test failed.
        if (mUrlHandler.mShownIncognitoAlertDialog != null) {
            Assert.assertFalse(mUrlHandler.mShownIncognitoAlertDialog.isShowing());
        }
    }

    @Test
    @SmallTest
    public void testStartActivityToTrustedPackageWithoutUserGesture() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler handler = RedirectHandler.create();
        handler.updateNewUrlLoading(PageTransition.CLIENT_REDIRECT, false, false, 0, 0);

        checkUrl(YOUTUBE_URL)
                .withRedirectHandler(handler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        mDelegate.setIsCallingAppTrusted(true);

        checkUrl(YOUTUBE_URL)
                .withRedirectHandler(handler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testOrdinaryIncognitoUri() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // http://crbug.com/587306: Don't prompt the user for capturing URLs in incognito, just keep
        // it within the browser.
        checkUrl(YOUTUBE_URL)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testChromeReferrer() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // http://crbug.com/159153: Don't override http or https URLs from the NTP or bookmarks.
        checkUrl(YOUTUBE_URL)
                .withReferrer("chrome://about")
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
        checkUrl("tel:012345678")
                .withReferrer("chrome://about")
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testForwardBackNavigation() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // http://crbug.com/164194. We shouldn't show the intent picker on
        // forwards or backwards navigations.
        checkUrl(YOUTUBE_URL)
                .withPageTransition(PageTransition.LINK | PageTransition.FORWARD_BACK)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testRedirectFromFormSubmit() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // http://crbug.com/181186: We need to show the intent picker when we receive a redirect
        // following a form submit. OAuth of native applications rely on this.
        checkUrl("market://1234")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        checkUrl("http://youtube.com://")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // If the page matches the referrer, then continue loading in Chrome.
        checkUrl("http://youtube.com://")
                .withReferrer(YOUTUBE_URL)
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        // If the page does not match the referrer, then prompt an intent.
        checkUrl("http://youtube.com://")
                .withReferrer("http://google.com")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // It doesn't make sense to allow intent picker without redirect, since form data
        // is not encoded in the intent (although, in theory, it could be passed in as
        // an extra data in the intent).
        checkUrl("http://youtube.com://")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testRedirectFromFormSubmit_NoUserGesture() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // If the redirect is not associated with a user gesture, then continue loading in Chrome.
        checkUrl("market://1234")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(false)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
        checkUrl("http://youtube.com://")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(false)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testRedirectFromFormSubmit_NoUserGesture_OnIntentRedirectChain() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler redirectHandler = new RedirectHandler() {
            @Override
            public boolean isOnEffectiveIntentRedirectChain() {
                return true;
            }
        };

        // If the redirect is not associated with a user gesture but came from an incoming intent,
        // then allow those to launch external intents.
        checkUrl("market://1234")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(false)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        checkUrl("http://youtube.com://")
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(false)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testOrdinary_disableExternalIntentRequestsForUrl() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));
        mDelegate.setDisableExternalIntentRequests(true);

        checkUrl(YOUTUBE_URL).expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testIgnore() {
        // Ensure the following URLs are not broadcast for external navigation.
        String urlsToIgnore[] = new String[] {"about:test",
                "content:test", // Content URLs should not be exposed outside of Chrome.
                "chrome://history", "chrome-native://newtab", "devtools://foo",
                "intent:chrome-urls#Intent;package=com.android.chrome;scheme=about;end;",
                "intent:chrome-urls#Intent;package=com.android.chrome;scheme=chrome;end;",
                "intent://com.android.chrome.FileProvider/foo.html#Intent;scheme=content;end;",
                "intent:///x.mhtml#Intent;package=com.android.chrome;action=android.intent.action.VIEW;scheme=file;end;"};
        for (String url : urlsToIgnore) {
            checkUrl(url).expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
            checkUrl(url).withIsIncognito(true).expecting(
                    OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
        }
    }

    @Test
    @SmallTest
    public void testPageTransitionType() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        // Non-link page transition type are ignored.
        checkUrl(YOUTUBE_URL)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        checkUrl(YOUTUBE_URL)
                .withIsRedirect(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // http://crbug.com/143118 - Don't show the picker for directly typed URLs, unless
        // the URL results in a redirect.
        checkUrl(YOUTUBE_URL)
                .withPageTransition(PageTransition.TYPED)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        // http://crbug.com/162106 - Don't show the picker on reload.
        checkUrl(YOUTUBE_URL)
                .withPageTransition(PageTransition.RELOAD)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testWtai() {
        // Start the telephone application with the given number.
        checkUrl("wtai://wp/mc;0123456789")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY | INTENT_SANITIZATION_EXCEPTION);

        // These two cases are currently unimplemented.
        checkUrl("wtai://wp/sd;0123456789")
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE,
                        IGNORE | INTENT_SANITIZATION_EXCEPTION);
        checkUrl("wtai://wp/ap;0123456789")
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE,
                        IGNORE | INTENT_SANITIZATION_EXCEPTION);

        // Ignore other WTAI urls.
        checkUrl("wtai://wp/invalid")
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE,
                        IGNORE | INTENT_SANITIZATION_EXCEPTION);
    }

    @Test
    @SmallTest
    public void testRedirectToMarketWithReferrer() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        checkUrl(INTENT_APP_NOT_INSTALLED_WITH_MARKET_REFERRER)
                .withReferrer(KEEP_URL)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Uri uri = mDelegate.startActivityIntent.getData();
        Assert.assertEquals("market", uri.getScheme());
        Assert.assertEquals(Uri.decode(ENCODED_MARKET_REFERRER), uri.getQueryParameter("referrer"));
        Assert.assertEquals(Uri.parse(KEEP_URL),
                mDelegate.startActivityIntent.getParcelableExtra(Intent.EXTRA_REFERRER));
    }

    @Test
    @SmallTest
    public void testRedirectToMarketWithoutReferrer() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        checkUrl(INTENT_APP_NOT_INSTALLED_DEFAULT_MARKET_REFERRER)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Uri uri = mDelegate.startActivityIntent.getData();
        Assert.assertEquals("market", uri.getScheme());
        Assert.assertEquals(getPackageName(), uri.getQueryParameter("referrer"));
    }

    @Test
    @SmallTest
    public void testExternalUri() {
        checkUrl("tel:012345678")
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testTypedRedirectToExternalProtocol() {
        // http://crbug.com/169549
        checkUrl("market://1234")
                .withPageTransition(PageTransition.TYPED)
                .withIsRedirect(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // http://crbug.com/709217
        checkUrl("market://1234")
                .withPageTransition(PageTransition.FROM_ADDRESS_BAR)
                .withIsRedirect(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // http://crbug.com/143118
        checkUrl("market://1234")
                .withPageTransition(PageTransition.TYPED)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testIncomingIntentRedirect() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        int transitionTypeIncomingIntent = PageTransition.LINK | PageTransition.FROM_API;
        // http://crbug.com/149218
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transitionTypeIncomingIntent)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        // http://crbug.com/170925
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transitionTypeIncomingIntent)
                .withIsRedirect(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testIntentScheme() {
        String url = "intent:wtai://wp/#Intent;action=android.settings.SETTINGS;"
                + "component=package/class;end";
        String urlWithSel = "intent:wtai://wp/#Intent;SEL;action=android.settings.SETTINGS;"
                + "component=package/class;end";
        String urlWithNullData = "intent:#Intent;package=com.google.zxing.client.android;"
                + "action=android.settings.SETTINGS;end";

        checkUrl(url).expecting(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, START_OTHER_ACTIVITY);

        // http://crbug.com/370399
        checkUrl(urlWithSel)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        checkUrl(urlWithNullData)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testYouTubePairingCode() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        String mobileUrl = "http://m.youtube.com/watch?v=1234&pairingCode=5678";
        int transitionTypeIncomingIntent = PageTransition.LINK | PageTransition.FROM_API;
        final String[] goodUrls = {mobileUrl, "http://youtube.com?pairingCode=xyz",
                "http://youtube.com/tv?pairingCode=xyz",
                "http://youtube.com/watch?v=1234&version=3&autohide=1&pairingCode=xyz",
                "http://youtube.com/watch?v=1234&pairingCode=xyz&version=3&autohide=1"};
        final String[] badUrls = {"http://youtube.com.foo.com/tv?pairingCode=xyz",
                "http://youtube.com.foo.com?pairingCode=xyz", "http://youtube.com&pairingCode=xyz",
                "http://youtube.com/watch?v=1234#pairingCode=xyz"};

        // Make sure we don't override when faced with valid pairing code URLs.
        for (String url : goodUrls) {
            Assert.assertTrue(mUrlHandler.isYoutubePairingCode(new GURL(url)));
        }
        for (String url : badUrls) {
            Assert.assertFalse(mUrlHandler.isYoutubePairingCode(new GURL(url)));
        }
        // http://crbug/386600 - it makes no sense to switch activities for pairing code URLs.
        checkUrl(mobileUrl).withIsRedirect(true).expecting(
                OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
        checkUrl(mobileUrl)
                .withPageTransition(transitionTypeIncomingIntent)
                .withIsRedirect(true)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testInitialIntent() throws URISyntaxException {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();
        Intent ytIntent = Intent.parseUri(YOUTUBE_URL, Intent.URI_INTENT_SCHEME);
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        int transTypeLinkFromIntent = PageTransition.LINK | PageTransition.FROM_API;

        // Ignore if url is redirected, transition type is IncomingIntent and a new intent doesn't
        // have any new resolver.
        redirectHandler.updateIntent(ytIntent, !IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        // Do not ignore if a new intent has any new resolver.
        redirectHandler.updateIntent(fooIntent, !IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // Do not ignore if a new intent cannot be handled by Chrome.
        redirectHandler.updateIntent(fooIntent, !IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl("intent://myownurl")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testInitialIntentHeadingToChrome() throws URISyntaxException {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        fooIntent.setPackage(mContext.getPackageName());
        int transTypeLinkFromIntent = PageTransition.LINK | PageTransition.FROM_API;

        // Ignore if an initial Intent was heading to Chrome.
        redirectHandler.updateIntent(fooIntent, !IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        // Do not ignore if the URI has an external protocol.
        redirectHandler.updateIntent(fooIntent, !IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl("market://1234")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testIntentForCustomTab() throws URISyntaxException {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();
        int transTypeLinkFromIntent = PageTransition.LINK | PageTransition.FROM_API;

        // In Custom Tabs, if the first url is not a redirect, stay in chrome.
        Intent barIntent = Intent.parseUri(YOUTUBE_URL, Intent.URI_INTENT_SCHEME);
        barIntent.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(barIntent, IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        // In Custom Tabs, if the first url is a redirect, don't allow it to intent out.
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        fooIntent.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(fooIntent, IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        // In Custom Tabs, if the external handler extra is present, intent out if the first
        // url is a redirect.
        Intent extraIntent2 = Intent.parseUri(YOUTUBE_URL, Intent.URI_INTENT_SCHEME);
        extraIntent2.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(extraIntent2, IS_CUSTOM_TAB_INTENT, SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Intent extraIntent3 = Intent.parseUri(YOUTUBE_URL, Intent.URI_INTENT_SCHEME);
        extraIntent3.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(extraIntent3, IS_CUSTOM_TAB_INTENT, SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        // External intent for a user-initiated navigation should always be allowed.
        redirectHandler.updateIntent(fooIntent, IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        // Simulate a real user navigation.
        redirectHandler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime() + 1, 0);
        checkUrl(YOUTUBE_URL)
                .withPageTransition(PageTransition.LINK)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testInstantAppsIntent_customTabRedirect() throws Exception {
        RedirectHandler redirectHandler = RedirectHandler.create();
        int transTypeLinkFromIntent = PageTransition.LINK | PageTransition.FROM_API;

        // In Custom Tabs, if the first url is a redirect, don't allow it to intent out, unless
        // the redirect is going to Instant Apps.
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        fooIntent.putExtra(CustomTabsIntent.EXTRA_ENABLE_INSTANT_APPS, true);
        fooIntent.setPackage(mContext.getPackageName());
        redirectHandler.updateIntent(fooIntent, IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);

        mDelegate.setCanHandleWithInstantApp(true);
        checkUrl("http://instantappenabled.com")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, IGNORE);
    }

    @Test
    @SmallTest
    public void testCCTIntentUriDoesNotFireCCTAndLoadInChrome_InIncognito() throws Exception {
        mUrlHandler.mResolveInfoContainsSelf = true;
        mDelegate.setCanLoadUrlInTab(false);
        checkUrl(INTENT_URL_FOR_CHROME_CUSTOM_TABS)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals("http://example.com/", mUrlHandler.mNewUrlAfterClobbering);
    }

    @Test
    @SmallTest
    public void testCCTIntentUriFiresCCT_InRegular() throws Exception {
        checkUrl(INTENT_URL_FOR_CHROME_CUSTOM_TABS)
                .withIsIncognito(false)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        Assert.assertNotNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testChromeIntentUriDoesNotFireAndLoadsInChrome_InIncognito() throws Exception {
        mUrlHandler.mResolveInfoContainsSelf = true;
        mDelegate.setCanLoadUrlInTab(false);
        checkUrl(INTENT_URL_FOR_CHROME)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals("http://example.com/", mUrlHandler.mNewUrlAfterClobbering);

        mUrlHandler.mResolveInfoContainsSelf = false;
        checkUrl(INTENT_URL_FOR_CHROME)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals(YOUTUBE_URL, mUrlHandler.mNewUrlAfterClobbering);
    }

    @Test
    @SmallTest
    public void testInstantAppsIntent_incomingIntentRedirect() throws Exception {
        int transTypeLinkFromIntent = PageTransition.LINK | PageTransition.FROM_API;
        RedirectHandler redirectHandler = RedirectHandler.create();
        Intent fooIntent =
                Intent.parseUri("http://instantappenabled.com", Intent.URI_INTENT_SCHEME);
        redirectHandler.updateIntent(fooIntent, !IS_CUSTOM_TAB_INTENT, !SEND_TO_EXTERNAL_APPS,
                IS_CCT_EXTERNAL_LINK_HANDLING_ENABLED, !INTENT_STARTED_TASK);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);

        mDelegate.setCanHandleWithInstantApp(true);
        checkUrl("http://goo.gl/1234")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, IGNORE);

        // URL that cannot be handled with instant apps should stay in Chrome.
        mDelegate.setCanHandleWithInstantApp(false);
        checkUrl("http://goo.gl/1234")
                .withPageTransition(transTypeLinkFromIntent)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testInstantAppsIntent_handleNavigation() {
        mDelegate.setCanHandleWithInstantApp(false);
        checkUrl("http://maybeinstantapp.com")
                .withPageTransition(PageTransition.LINK)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        mDelegate.setCanHandleWithInstantApp(true);
        checkUrl("http://maybeinstantapp.com")
                .withPageTransition(PageTransition.LINK)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, IGNORE);
    }

    @Test
    @SmallTest
    public void testHandlingOfInstantApps() {
        String intentUrl = "intent://buzzfeed.com/tasty#Intent;scheme=http;"
                + "package=com.google.android.instantapps.supervisor;"
                + "action=com.google.android.instantapps.START;"
                + "S.com.google.android.instantapps.FALLBACK_PACKAGE="
                + "com.android.chrome;S.com.google.android.instantapps.INSTANT_APP_PACKAGE="
                + "com.yelp.android;S.android.intent.extra.REFERRER_NAME="
                + "https%3A%2F%2Fwww.google.com;end";

        mUrlHandler.mIsSerpReferrer = true;
        mDelegate.setIsIntentToInstantApp(true);
        checkUrl(intentUrl).expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_OTHER_ACTIVITY | PROXY_FOR_INSTANT_APPS);
        Assert.assertTrue(
                mDelegate.startActivityIntent.getBooleanExtra(IS_INSTANT_APP_EXTRA, false));

        // Check that we block all instant app intent:// URLs not from SERP.
        mUrlHandler.mIsSerpReferrer = false;
        checkUrl(intentUrl).expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        // Check that that just having the SERP referrer alone doesn't cause intents to be treated
        // as intents to instant apps if the delegate indicates that they shouldn't be.
        mUrlHandler.mIsSerpReferrer = true;
        mDelegate.setIsIntentToInstantApp(false);
        checkUrl(intentUrl).expecting(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, START_OTHER_ACTIVITY);
        Assert.assertFalse(
                mDelegate.startActivityIntent.getBooleanExtra(IS_INSTANT_APP_EXTRA, true));
    }

    @Test
    @SmallTest
    public void testFallbackUrl_IntentResolutionSucceeds() {
        // IMDB app is installed.
        mDelegate.add(new IntentActivity("imdb:", INTENT_APP_PACKAGE_NAME));

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Intent invokedIntent = mDelegate.startActivityIntent;
        Assert.assertEquals(IMDB_APP_INTENT_FOR_TOM_HANKS, invokedIntent.getData().toString());
        Assert.assertNull("The invoked intent should not have browser_fallback_url\n",
                invokedIntent.getStringExtra(ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL));
        Assert.assertNull(mUrlHandler.mNewUrlAfterClobbering);
        Assert.assertNull(mUrlHandler.mReferrerUrlForClobbering);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_IntentResolutionSucceedsInIncognito() {
        // IMDB app is installed.
        mDelegate.add(new IntentActivity("imdb:", INTENT_APP_PACKAGE_NAME));
        mUrlHandler.mCanShowIncognitoDialog = true;

        // Expect that the user is prompted to leave incognito mode.
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withIsIncognito(true)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);

        Assert.assertNull(mUrlHandler.mNewUrlAfterClobbering);
        Assert.assertNull(mUrlHandler.mReferrerUrlForClobbering);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_FallbackToWebApk() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        mDelegate.add(new IntentActivity(IMDB_WEBPAGE_FOR_TOM_HANKS, WEBAPK_PACKAGE_NAME));
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, START_WEBAPK);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_DontFallbackToWebApkMultipleHandlers() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        mDelegate.add(new IntentActivity(IMDB_WEBPAGE_FOR_TOM_HANKS, WEBAPK_PACKAGE_NAME));
        mDelegate.add(new IntentActivity(IMDB_WEBPAGE_FOR_TOM_HANKS, TEXT_APP_1_PACKAGE_NAME));
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mUrlHandler.mNewUrlAfterClobbering);
        Assert.assertEquals(SEARCH_RESULT_URL_FOR_TOM_HANKS, mUrlHandler.mReferrerUrlForClobbering);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_IntentResolutionFails() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        // When intent resolution fails, we should not start an activity, but instead clobber
        // the current tab.
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mUrlHandler.mNewUrlAfterClobbering);
        Assert.assertEquals(SEARCH_RESULT_URL_FOR_TOM_HANKS, mUrlHandler.mReferrerUrlForClobbering);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_FallbackToMarketApp() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        String intent = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/apps/details?id=com.imdb.mobile"
                + "&referrer=mypage;end";
        checkUrl(intent).expecting(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, START_OTHER_ACTIVITY);

        Assert.assertEquals("market://details?id=com.imdb.mobile&referrer=mypage",
                mDelegate.startActivityIntent.getDataString());

        String intentNoRef = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/apps/details?id=com.imdb.mobile;end";
        checkUrl(intentNoRef)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertEquals("market://details?id=com.imdb.mobile&referrer=" + getPackageName(),
                mDelegate.startActivityIntent.getDataString());

        String intentBadUrl = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/search?q=pub:imdb;end";
        checkUrl(intentBadUrl)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
    }

    @Test
    @MediumTest
    public void testFallbackUrl_FallbackToMarketApp_Incognito() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme("market");
        ActivityMonitor monitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);
        Intent dummyIntent = new Intent(mApplicationContextToRestore, DummyUiActivity.class);
        dummyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity activity =
                InstrumentationRegistry.getInstrumentation().startActivitySync(dummyIntent);
        mDelegate.setContext(activity);
        mDelegate.setCanLoadUrlInTab(true);
        try {
            mDelegate.setCanResolveActivityForExternalSchemes(false);
            String playUrl = "https://play.google.com/store/apps/details?id=com.imdb.mobile"
                    + "&referrer=mypage";

            String intent = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                    + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                    + Uri.encode(playUrl, null) + ";end;";

            mUrlHandler.mCanShowIncognitoDialog = true;
            ThreadUtils.runOnUiThreadBlocking(() -> {
                checkUrl(intent).withIsIncognito(true).withHasUserGesture(true).expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);
                Assert.assertNull(mDelegate.startActivityIntent);
                Assert.assertNull(mUrlHandler.mNewUrlAfterClobbering);
                mUrlHandler.mShownIncognitoAlertDialog.cancel();
            });
            // Cancel callback is posted, so continue after posting to the task queue.
            ThreadUtils.runOnUiThreadBlocking(() -> {
                Assert.assertEquals(playUrl, mUrlHandler.mNewUrlAfterClobbering);
                mUrlHandler.mNewUrlAfterClobbering = null;

                checkUrl(intent).withIsIncognito(true).withHasUserGesture(true).expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);
                Assert.assertNull(mDelegate.startActivityIntent);
                mUrlHandler.mShownIncognitoAlertDialog.getButton(DialogInterface.BUTTON_POSITIVE)
                        .performClick();
            });
            // Click callback is posted, so continue after posting to the task queue.
            ThreadUtils.runOnUiThreadBlocking(() -> {
                Assert.assertNull(mUrlHandler.mNewUrlAfterClobbering);
                Assert.assertEquals(1, monitor.getHits());
                Assert.assertEquals("market://details?id=com.imdb.mobile&referrer=mypage",
                        mDelegate.startActivityIntent.getDataString());
            });
        } finally {
            if (mUrlHandler.mShownIncognitoAlertDialog != null) {
                mUrlHandler.mShownIncognitoAlertDialog.cancel();
            }
            activity.finish();
            InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
        }
    }

    @Test
    @MediumTest
    public void testFallbackUrl_ChromeCanHandle_Incognito() {
        mDelegate.add(new IntentActivity("https", "package"));
        Intent dummyIntent = new Intent(mApplicationContextToRestore, DummyUiActivity.class);
        dummyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity activity =
                InstrumentationRegistry.getInstrumentation().startActivitySync(dummyIntent);
        mDelegate.setContext(activity);
        mDelegate.setCanLoadUrlInTab(true);
        try {
            String intent = "intent://example.com#Intent;scheme=https;"
                    + "S.browser_fallback_url=http%3A%2F%2Fgoogle.com;end";

            mUrlHandler.mResolveInfoContainsSelf = true;
            mUrlHandler.mCanShowIncognitoDialog = true;
            ThreadUtils.runOnUiThreadBlocking(() -> {
                checkUrl(intent).withIsIncognito(true).withHasUserGesture(true).expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);
                Assert.assertNull(mDelegate.startActivityIntent);
                Assert.assertNull(mUrlHandler.mNewUrlAfterClobbering);
                mUrlHandler.mShownIncognitoAlertDialog.cancel();
            });
            // Cancel callback is posted, so continue after posting to the task queue.
            ThreadUtils.runOnUiThreadBlocking(() -> {
                Assert.assertEquals("https://example.com/", mUrlHandler.mNewUrlAfterClobbering);
                mUrlHandler.mNewUrlAfterClobbering = null;
                mUrlHandler.mResolveInfoContainsSelf = false;

                checkUrl(intent).withIsIncognito(true).withHasUserGesture(true).expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);
                Assert.assertNull(mDelegate.startActivityIntent);
                mUrlHandler.mShownIncognitoAlertDialog.cancel();
            });
            // Click callback is posted, so continue after posting to the task queue.
            ThreadUtils.runOnUiThreadBlocking(() -> {
                Assert.assertEquals("http://google.com/", mUrlHandler.mNewUrlAfterClobbering);
            });
        } finally {
            if (mUrlHandler.mShownIncognitoAlertDialog != null) {
                mUrlHandler.mShownIncognitoAlertDialog.cancel();
            }
            activity.finish();
        }
    }

    @Test
    @MediumTest
    public void testFallbackUrl_FallbackToMarketApp_Incognito_DelegateHandleDialogPresentation() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme("market");
        ActivityMonitor monitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);
        Intent dummyIntent = new Intent(mApplicationContextToRestore, DummyUiActivity.class);
        dummyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity activity =
                InstrumentationRegistry.getInstrumentation().startActivitySync(dummyIntent);
        mDelegate.setContext(activity);
        mDelegate.setCanLoadUrlInTab(true);
        mDelegate.setShouldPresentLeavingIncognitoDialog(true);
        try {
            mDelegate.setCanResolveActivityForExternalSchemes(false);
            String playUrl = "https://play.google.com/store/apps/details?id=com.imdb.mobile"
                    + "&referrer=mypage";

            String intent = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                    + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                    + Uri.encode(playUrl, null) + ";end;";

            mUrlHandler.mCanShowIncognitoDialog = true;
            ThreadUtils.runOnUiThreadBlocking(() -> {
                checkUrl(intent).withIsIncognito(true).withHasUserGesture(true).expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);
                Assert.assertNull(mDelegate.startActivityIntent);
                Assert.assertNull(mUrlHandler.mNewUrlAfterClobbering);

                // Verify that the incognito dialog was not shown.
                Assert.assertNull(mUrlHandler.mShownIncognitoAlertDialog);

                // Verify that the delegate was given the opportunity to present the dialog.
                Assert.assertNotNull(mDelegate.incognitoDialogUserDecisionCallback);

                // Inform the handler that the user decided not to launch the intent and verify that
                // the appropriate URL is navigated to in the browser.
                mDelegate.incognitoDialogUserDecisionCallback.onResult(new Boolean(false));
                Assert.assertEquals(playUrl, mUrlHandler.mNewUrlAfterClobbering);
                mUrlHandler.mNewUrlAfterClobbering = null;
                mDelegate.incognitoDialogUserDecisionCallback = null;

                checkUrl(intent).withIsIncognito(true).withHasUserGesture(true).expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);
                Assert.assertNull(mDelegate.startActivityIntent);

                // Verify that the incognito dialog was not shown.
                Assert.assertNull(mUrlHandler.mShownIncognitoAlertDialog);

                // Verify that the delegate was given the opportunity to present the dialog.
                Assert.assertNotNull(mDelegate.incognitoDialogUserDecisionCallback);

                // Inform the handler that the user decided to launch the intent and verify that
                // the intent was launched.
                mDelegate.incognitoDialogUserDecisionCallback.onResult(new Boolean(true));
                Assert.assertNull(mUrlHandler.mNewUrlAfterClobbering);
                Assert.assertEquals(1, monitor.getHits());
                Assert.assertEquals("market://details?id=com.imdb.mobile&referrer=mypage",
                        mDelegate.startActivityIntent.getDataString());
            });
        } finally {
            activity.finish();
            InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
        }
    }

    @Test
    @MediumTest
    public void testFallbackUrl_ChromeCanHandle_Incognito_DelegateHandleDialogPresentation() {
        mDelegate.add(new IntentActivity("https", "package"));
        Intent dummyIntent = new Intent(mApplicationContextToRestore, DummyUiActivity.class);
        dummyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity activity =
                InstrumentationRegistry.getInstrumentation().startActivitySync(dummyIntent);
        mDelegate.setContext(activity);
        mDelegate.setCanLoadUrlInTab(true);
        mDelegate.setShouldPresentLeavingIncognitoDialog(true);
        try {
            String intent = "intent://example.com#Intent;scheme=https;"
                    + "S.browser_fallback_url=http%3A%2F%2Fgoogle.com;end";

            mUrlHandler.mResolveInfoContainsSelf = true;
            mUrlHandler.mCanShowIncognitoDialog = true;
            ThreadUtils.runOnUiThreadBlocking(() -> {
                checkUrl(intent).withIsIncognito(true).withHasUserGesture(true).expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);
                Assert.assertNull(mDelegate.startActivityIntent);
                Assert.assertNull(mUrlHandler.mNewUrlAfterClobbering);

                // Verify that the incognito dialog was not shown.
                Assert.assertNull(mUrlHandler.mShownIncognitoAlertDialog);

                // Verify that the delegate was given the opportunity to present the dialog.
                Assert.assertNotNull(mDelegate.incognitoDialogUserDecisionCallback);

                // Inform the handler that the user decided not to launch the intent and verify that
                // the appropriate URL is navigated to in the browser.
                mDelegate.incognitoDialogUserDecisionCallback.onResult(new Boolean(false));
                Assert.assertEquals("https://example.com/", mUrlHandler.mNewUrlAfterClobbering);

                mUrlHandler.mNewUrlAfterClobbering = null;
                mUrlHandler.mResolveInfoContainsSelf = false;
                mDelegate.incognitoDialogUserDecisionCallback = null;

                checkUrl(intent).withIsIncognito(true).withHasUserGesture(true).expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);
                Assert.assertNull(mDelegate.startActivityIntent);

                // Verify that the incognito dialog was not shown.
                Assert.assertNull(mUrlHandler.mShownIncognitoAlertDialog);

                // Verify that the delegate was given the opportunity to present the dialog.
                Assert.assertNotNull(mDelegate.incognitoDialogUserDecisionCallback);

                // Inform the handler that the user decided not to launch the intent and verify that
                // the appropriate URL is navigated to in the browser.
                mDelegate.incognitoDialogUserDecisionCallback.onResult(new Boolean(false));
                Assert.assertEquals("http://google.com/", mUrlHandler.mNewUrlAfterClobbering);
            });
        } finally {
            activity.finish();
        }
    }

    @Test
    @SmallTest
    public void testFallbackUrl_SubframeFallbackToMarketApp() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        RedirectHandler redirectHandler = RedirectHandler.create();
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0);
        String intent = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/apps/details?id=com.imdb.mobile"
                + "&referrer=mypage;end";
        checkUrl(intent)
                .withIsMainFrame(false)
                .withHasUserGesture(true)
                .withRedirectHandler(redirectHandler)
                .withPageTransition(PageTransition.LINK)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        Assert.assertEquals("market://details?id=com.imdb.mobile&referrer=mypage",
                mDelegate.startActivityIntent.getDataString());

        redirectHandler = RedirectHandler.create();
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0);
        String intentBadUrl = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/search?q=pub:imdb;end";
        checkUrl(intentBadUrl)
                .withIsMainFrame(false)
                .withHasUserGesture(true)
                .withRedirectHandler(redirectHandler)
                .withPageTransition(PageTransition.LINK)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_RedirectToIntentToMarket() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        RedirectHandler redirectHandler = RedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0);
        checkUrl("http://goo.gl/abcdefg")
                .withPageTransition(PageTransition.LINK)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, true, 0, 0);
        String realIntent = "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
                + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
                + "https://play.google.com/store/apps/details?id=com.imdb.mobile"
                + "&referrer=mypage;end";

        checkUrl(realIntent)
                .withPageTransition(PageTransition.LINK)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertEquals("market://details?id=com.imdb.mobile&referrer=mypage",
                mDelegate.startActivityIntent.getDataString());
    }

    @Test
    @SmallTest
    public void testFallbackUrl_DontFallbackForAutoSubframe() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        mDelegate.add(new IntentActivity(IMDB_WEBPAGE_FOR_TOM_HANKS, WEBAPK_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();
        redirectHandler.updateNewUrlLoading(PageTransition.AUTO_SUBFRAME, true, true, 0, 0);

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withIsMainFrame(false)
                .withHasUserGesture(true)
                .withRedirectHandler(redirectHandler)
                .withPageTransition(PageTransition.AUTO_SUBFRAME)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_NoExternalFallbackWithoutGesture() {
        mDelegate.add(new IntentActivity(IMDB_WEBPAGE_FOR_TOM_HANKS, WEBAPK_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 0);

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withHasUserGesture(false)
                .withRedirectHandler(redirectHandler)
                .withPageTransition(PageTransition.LINK)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_IntentResolutionFailsWithoutPackageName() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        // Fallback URL should work even when package name isn't given.
        checkUrl(INTENT_URL_WITH_FALLBACK_URL_WITHOUT_PACKAGE_NAME)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mUrlHandler.mNewUrlAfterClobbering);
        Assert.assertEquals(SEARCH_RESULT_URL_FOR_TOM_HANKS, mUrlHandler.mReferrerUrlForClobbering);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_FallbackShouldNotWarnOnIncognito() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
        Assert.assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mUrlHandler.mNewUrlAfterClobbering);
        Assert.assertEquals(SEARCH_RESULT_URL_FOR_TOM_HANKS, mUrlHandler.mReferrerUrlForClobbering);
    }

    @Test
    @SmallTest
    public void testFallbackUrl_IgnoreJavascriptFallbackUrl() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivityForExternalSchemes(false);
        mUrlHandler.mCanShowIncognitoDialog = true;

        // Will be redirected to market since package is given.
        checkUrl(INTENT_URL_WITH_JAVASCRIPT_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);

        Intent invokedIntent = mUrlHandler.mStartActivityInIncognitoIntent;
        Assert.assertTrue(invokedIntent.getData().toString().startsWith("market://"));
        Assert.assertEquals(null, mUrlHandler.mNewUrlAfterClobbering);
        Assert.assertEquals(null, mUrlHandler.mReferrerUrlForClobbering);
    }

    @Test
    @SmallTest
    public void testFallback_UseFallbackUrlForRedirectionFromTypedInUrl() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0);
        checkUrl("http://goo.gl/abcdefg")
                .withPageTransition(PageTransition.TYPED)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 0);
        checkUrl(INTENT_URL_WITH_FALLBACK_URL_WITHOUT_PACKAGE_NAME)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        // Now the user opens a link.
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testIgnoreEffectiveRedirectFromIntentFallbackUrl() {
        // We cannot resolve any intent, so fall-back URL will be used.
        mDelegate.setCanResolveActivityForExternalSchemes(false);

        RedirectHandler redirectHandler = RedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0);
        checkUrl(INTENT_URL_WITH_CHAIN_FALLBACK_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        // As a result of intent resolution fallback, we have clobberred the current tab.
        // The fall-back URL was HTTP-schemed, but it was effectively redirected to a new intent
        // URL using javascript. However, we do not allow chained fallback intent, so we do NOT
        // override URL loading here.
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 0);
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        // Now enough time (2 seconds) have passed.
        // New URL loading should not be affected.
        // (The URL happened to be the same as previous one.)
        // TODO(changwan): this is not likely cause flakiness, but it may be better to refactor
        // systemclock or pass the new time as parameter.
        long lastUserInteractionTimeInMillis = SystemClock.elapsedRealtime() + 2 * 1000L;
        redirectHandler.updateNewUrlLoading(
                PageTransition.LINK, false, true, lastUserInteractionTimeInMillis, 1);
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);
    }

    @Test
    @SmallTest
    public void testIgnoreEffectiveRedirectFromUserTyping() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(PageTransition.TYPED)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, true, false, 0, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withPageTransition(PageTransition.TYPED)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testNavigationFromLinkWithoutUserGesture() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testChromeAppInBackground() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));
        mDelegate.setIsChromeAppInForeground(false);
        checkUrl(YOUTUBE_URL).expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testNotChromeAppInForegroundRequired() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));
        mDelegate.setIsChromeAppInForeground(false);
        checkUrl(YOUTUBE_URL)
                .withChromeAppInForegroundRequired(false)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testCreatesIntentsToOpenInNewTab() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        mUrlHandler = new ExternalNavigationHandlerForTesting(mDelegate);
        ExternalNavigationParams params =
                new ExternalNavigationParams.Builder(new GURL(YOUTUBE_MOBILE_URL), false)
                        .setOpenInNewTab(true)
                        .build();
        OverrideUrlLoadingResult result = mUrlHandler.shouldOverrideUrlLoading(params);
        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, result.getResultType());
        Assert.assertTrue(mDelegate.startActivityIntent != null);
        Assert.assertTrue(
                mDelegate.startActivityIntent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
    }

    @Test
    @SmallTest
    public void testCanExternalAppHandleUrl() {
        mDelegate.setCanResolveActivityForExternalSchemes(false);
        mDelegate.add(new IntentActivity("someapp", "someapp"));

        Assert.assertTrue(mUrlHandler.canExternalAppHandleUrl(new GURL("someapp://someapp.com/")));

        Assert.assertTrue(mUrlHandler.canExternalAppHandleUrl(new GURL("wtai://wp/mc;0123456789")));
        Assert.assertTrue(mUrlHandler.canExternalAppHandleUrl(
                new GURL("intent:/#Intent;scheme=noapp;package=com.noapp;end")));
        Assert.assertFalse(mUrlHandler.canExternalAppHandleUrl(new GURL("noapp://noapp.com/")));
    }

    @Test
    @SmallTest
    public void testPlusAppRefresh() {
        mDelegate.add(new IntentActivity(PLUS_STREAM_URL, "plus"));

        checkUrl(PLUS_STREAM_URL)
                .withReferrer(PLUS_STREAM_URL)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testSameDomainDifferentApps() {
        mDelegate.add(new IntentActivity(CALENDAR_URL, "calendar"));

        checkUrl(CALENDAR_URL)
                .withReferrer(KEEP_URL)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testFormSubmitSameDomain() {
        mDelegate.add(new IntentActivity(CALENDAR_URL, "calendar"));

        checkUrl(CALENDAR_URL)
                .withReferrer(KEEP_URL)
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testBackgroundTabNavigation() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        checkUrl(YOUTUBE_URL)
                .withIsBackgroundTabNavigation(true)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testBackgroundTabNavigationWithIntentLaunchesInBackgroundTabsAllowed() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        checkUrl(YOUTUBE_URL)
                .withIsBackgroundTabNavigation(true)
                .withAllowIntentLaunchesInBackgroundTabs(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testPdfDownloadHappensInChrome() {
        mDelegate.add(new IntentActivity(CALENDAR_URL, "calendar"));

        checkUrl(CALENDAR_URL + "/file.pdf")
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testIntentToPdfFileOpensApp() {
        checkUrl("intent://yoursite.com/mypdf.pdf#Intent;action=VIEW;category=BROWSABLE;"
                + "scheme=http;package=com.adobe.reader;end;")
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testUsafeIntentFlagsFiltered() {
        checkUrl("intent:#Intent;package=com.test.package;launchFlags=0x7FFFFFFF;end;")
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        Assert.assertEquals(ExternalNavigationHandler.ALLOWED_INTENT_FLAGS,
                mDelegate.startActivityIntent.getFlags());
    }

    @Test
    @SmallTest
    public void testIntentWithFileSchemeFiltered() {
        checkUrl("intent://#Intent;package=com.test.package;scheme=file;end;")
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testIntentWithNoSchemeLaunched() {
        checkUrl("intent:#Intent;package=com.test.package;end;")
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testIntentWithEmptySchemeLaunched() {
        checkUrl("intent://#Intent;package=com.test.package;scheme=;end;")
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    @Test
    @SmallTest
    public void testIntentWithWeirdSchemeLaunched() {
        checkUrl("intent://#Intent;package=com.test.package;scheme=w3irD;end;")
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        // Schemes on Android are case-sensitive, so ensure the scheme is passed through as-is.
        Assert.assertEquals("w3irD", mDelegate.startActivityIntent.getScheme());
    }

    @Test
    @SmallTest
    public void testIntentWithMissingReferrer() {
        mDelegate.add(new IntentActivity("http://refertest.com", "refertest"));
        mDelegate.add(new IntentActivity("https://refertest.com", "refertest"));

        // http://crbug.com/702089: Don't override links within the same host/domain.
        // This is an issue for HTTPS->HTTP because there's no referrer, so we fall back on the
        // WebContents.lastCommittedUrl.

        checkUrl("http://refertest.com")
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        mUrlHandler.mLastCommittedUrl = new GURL("https://refertest.com");
        checkUrl("https://refertest.com")
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testReferrerExtra() {
        mDelegate.add(new IntentActivity(YOUTUBE_URL, YOUTUBE_PACKAGE_NAME));

        String referrer = "http://www.google.com/";
        checkUrl(YOUTUBE_URL + ":90/foo/bar")
                .withReferrer(referrer)
                .withPageTransition(PageTransition.FORM_SUBMIT)
                .withIsRedirect(true)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        Assert.assertEquals(Uri.parse(referrer),
                mDelegate.startActivityIntent.getParcelableExtra(Intent.EXTRA_REFERRER));
    }

    @Test
    @SmallTest
    public void testNavigationFromReload() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();

        redirectHandler.updateNewUrlLoading(PageTransition.RELOAD, false, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SmallTest
    public void testNavigationWithForwardBack() {
        mDelegate.add(new IntentActivity(YOUTUBE_MOBILE_URL, YOUTUBE_PACKAGE_NAME));

        RedirectHandler redirectHandler = RedirectHandler.create();

        redirectHandler.updateNewUrlLoading(
                PageTransition.FORM_SUBMIT | PageTransition.FORWARD_BACK, false, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, false, 1, 0);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withIsRedirect(true)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 1);
        checkUrl(YOUTUBE_MOBILE_URL)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    @Test
    @SuppressLint("SdCardPath")
    @SmallTest
    public void testFileAccess() {
        String fileUrl = "file:///sdcard/Downloads/test.html";

        mUrlHandler.mShouldRequestFileAccess = false;
        // Verify no overrides if file access is allowed (under different load conditions).
        checkUrl(fileUrl).expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
        checkUrl(fileUrl)
                .withPageTransition(PageTransition.RELOAD)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
        checkUrl(fileUrl)
                .withPageTransition(PageTransition.AUTO_TOPLEVEL)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        mUrlHandler.mShouldRequestFileAccess = true;
        // Verify that the file intent action is triggered if file access is not allowed.
        checkUrl(fileUrl)
                .withPageTransition(PageTransition.AUTO_TOPLEVEL)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_BROWSER_NAVIGATION, START_FILE);
    }

    @Test
    @SmallTest
    public void testSms_DispatchIntentToDefaultSmsApp() {
        final String referer = "https://www.google.com/";
        mDelegate.add(new IntentActivity("sms", TEXT_APP_1_PACKAGE_NAME));
        mDelegate.add(new IntentActivity("sms", TEXT_APP_2_PACKAGE_NAME));
        mUrlHandler.defaultSmsPackageName = TEXT_APP_2_PACKAGE_NAME;

        checkUrl("sms:+012345678?body=hello%20there")
                .withReferrer(referer)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Assert.assertEquals(TEXT_APP_2_PACKAGE_NAME, mDelegate.startActivityIntent.getPackage());
    }

    @Test
    @SmallTest
    public void testSms_DefaultSmsAppDoesNotHandleIntent() {
        final String referer = "https://www.google.com/";
        mDelegate.add(new IntentActivity("sms", TEXT_APP_1_PACKAGE_NAME));
        mDelegate.add(new IntentActivity("sms", TEXT_APP_2_PACKAGE_NAME));
        // Note that this package does not resolve the intent.
        mUrlHandler.defaultSmsPackageName = "text_app_3";

        checkUrl("sms:+012345678?body=hello%20there")
                .withReferrer(referer)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Assert.assertNull(mDelegate.startActivityIntent.getPackage());
    }

    @Test
    @SmallTest
    public void testSms_DispatchIntentSchemedUrlToDefaultSmsApp() {
        final String referer = "https://www.google.com/";
        mDelegate.add(new IntentActivity("sms", TEXT_APP_1_PACKAGE_NAME));
        mDelegate.add(new IntentActivity("sms", TEXT_APP_2_PACKAGE_NAME));
        mUrlHandler.defaultSmsPackageName = TEXT_APP_2_PACKAGE_NAME;

        checkUrl("intent://012345678?body=hello%20there/#Intent;scheme=sms;end")
                .withReferrer(referer)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Assert.assertEquals(TEXT_APP_2_PACKAGE_NAME, mDelegate.startActivityIntent.getPackage());
    }

    /**
     * Test that tapping a link which falls solely in the scope of a WebAPK launches a WebAPK
     * without showing the intent picker.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_BypassIntentPicker() {
        mDelegate.add(new IntentActivity(WEBAPK_SCOPE, WEBAPK_PACKAGE_NAME));

        checkUrl(WEBAPK_SCOPE)
                .expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, START_WEBAPK);
    }

    /**
     * Test that tapping a link which falls in the scope of multiple intent handlers, one of which
     * is a WebAPK, shows the intent picker.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_ShowIntentPickerMultipleIntentHandlers() {
        final String scope = "https://www.webapk.with.native.com";
        mDelegate.add(new IntentActivity(scope, WEBAPK_PACKAGE_PREFIX + ".with.native"));
        mDelegate.add(new IntentActivity(scope, "com.webapk.with.native.android"));

        checkUrl(scope).expecting(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, START_OTHER_ACTIVITY);
    }

    /**
     * Test that tapping a link which falls solely into the scope of a different WebAPK launches a
     * WebAPK without showing the intent picker.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_BypassIntentPickerFromAnotherWebApk() {
        final String scope1 = "https://www.webapk.with.native.com";
        final String scope1WebApkPackageName = WEBAPK_PACKAGE_PREFIX + ".with.native";
        final String scope1NativeAppPackageName = "com.webapk.with.native.android";
        final String scope2 = "https://www.template.com";
        mDelegate.add(new IntentActivity(scope1, scope1WebApkPackageName));
        mDelegate.add(new IntentActivity(scope1, scope1NativeAppPackageName));
        mDelegate.add(new IntentActivity(scope2, WEBAPK_PACKAGE_NAME));
        mDelegate.setReferrerWebappPackageName(scope1WebApkPackageName);

        checkUrl(scope2).withReferrer(scope1).expecting(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, START_WEBAPK);
    }

    /**
     * Test that a link which falls into the scope of an invalid WebAPK (e.g. it was incorrectly
     * signed) does not get any special WebAPK handling. The first time that the user taps on the
     * link, the intent picker should be shown.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_ShowIntentPickerInvalidWebApk() {
        mDelegate.add(new IntentActivity(WEBAPK_SCOPE, INVALID_WEBAPK_PACKAGE_NAME));
        checkUrl(WEBAPK_SCOPE)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
    }

    /**
     * Tests that a WebAPK isn't launched from an initial Intent if the delegate doesn't say it
     * should.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_InitialIntent_DelegateReturnsFalse() {
        mDelegate.add(new IntentActivity(WEBAPK_SCOPE, WEBAPK_PACKAGE_NAME));

        int transitionTypeIncomingIntent = PageTransition.LINK | PageTransition.FROM_API;
        checkUrl(WEBAPK_SCOPE)
                .withPageTransition(transitionTypeIncomingIntent)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
    }

    /**
     * Tests that a WebAPK is launched from an initial Intent if the delegate says it should.
     */
    @Test
    @SmallTest
    public void testLaunchWebApk_InitialIntent_DelegateReturnsTrue() {
        mDelegate.add(new IntentActivity(WEBAPK_SCOPE, WEBAPK_PACKAGE_NAME));
        mDelegate.setShouldLaunchWebApksOnInitialIntent(true);

        int transitionTypeIncomingIntent = PageTransition.LINK | PageTransition.FROM_API;
        checkUrl(WEBAPK_SCOPE)
                .withPageTransition(transitionTypeIncomingIntent)
                .expecting(
                        OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT, START_WEBAPK);
    }

    @Test
    @SmallTest
    public void testMarketIntent_MarketInstalled() {
        checkUrl("market://1234")
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
        Assert.assertTrue(mDelegate.startActivityIntent.getScheme().startsWith("market"));
    }

    @Test
    @SmallTest
    public void testMarketIntent_MarketNotInstalled() {
        mDelegate.setCanResolveActivityForMarket(false);
        checkUrl("market://1234").expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testMarketIntent_ShowDialogIncognitoMarketInstalled() {
        mUrlHandler.mCanShowIncognitoDialog = true;
        checkUrl("market://1234")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);

        Assert.assertTrue(mUrlHandler.mStartIncognitoIntentCalled);
    }

    @Test
    @SmallTest
    public void testMarketIntent_DontShowDialogIncognitoMarketNotInstalled() {
        mDelegate.setCanResolveActivityForMarket(false);
        checkUrl("market://1234")
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        Assert.assertFalse(mUrlHandler.mStartIncognitoIntentCalled);
    }

    @Test
    @SmallTest
    public void testUserGesture_Regular() {
        // IMDB app is installed.
        mDelegate.add(new IntentActivity("imdb:", INTENT_APP_PACKAGE_NAME));

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withHasUserGesture(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        Assert.assertTrue(mDelegate.maybeSetRequestMetadataCalled);
        Assert.assertFalse(mUrlHandler.mStartIncognitoIntentCalled);
    }

    @Test
    @SmallTest
    public void testUserGesture_Incognito() {
        // IMDB app is installed.
        mDelegate.add(new IntentActivity("imdb:", INTENT_APP_PACKAGE_NAME));

        mUrlHandler.mCanShowIncognitoDialog = true;
        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withHasUserGesture(true)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
                        OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH, START_INCOGNITO);
        Assert.assertTrue(mDelegate.maybeSetRequestMetadataCalled);
        Assert.assertTrue(mUrlHandler.mStartIncognitoIntentCalled);
    }

    @Test
    @SmallTest
    public void testRendererInitiated() {
        // IMDB app is installed.
        mDelegate.add(new IntentActivity("imdb:", INTENT_APP_PACKAGE_NAME));

        checkUrl(INTENT_URL_WITH_FALLBACK_URL)
                .withReferrer(SEARCH_RESULT_URL_FOR_TOM_HANKS)
                .withIsRendererInitiated(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);
        Assert.assertTrue(mDelegate.maybeSetRequestMetadataCalled);
    }

    @Test
    @SmallTest
    public void testAutofillAssistantIntentWithFallback_InRegular() {
        RedirectHandler redirectHandler = RedirectHandler.create();

        mDelegate.setIsIntentToAutofillAssistant(true);
        checkUrl(AUTOFILL_ASSISTANT_INTENT_URL_WITH_FALLBACK)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertTrue(redirectHandler.shouldNotOverrideUrlLoading());
        Assert.assertNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testAutofillAssistantIntentWithFallback_InIncognito() {
        mDelegate.setIsIntentToAutofillAssistant(true);
        checkUrl(AUTOFILL_ASSISTANT_INTENT_URL_WITH_FALLBACK)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testAutofillAssistantIntentWithoutFallback_InRegular() {
        mDelegate.setIsIntentToAutofillAssistant(true);
        checkUrl(AUTOFILL_ASSISTANT_INTENT_URL_WITHOUT_FALLBACK)
                .withIsIncognito(false)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testAutofillAssistantIntentWithoutFallback_InIncognito() {
        mDelegate.setIsIntentToAutofillAssistant(true);
        checkUrl(AUTOFILL_ASSISTANT_INTENT_URL_WITHOUT_FALLBACK)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testAutofillAssistantIntentWithAppOverrideNow_InRegular() {
        mDelegate.add(new IntentActivity("https://someapp.com", "someapp"));

        mDelegate.setIsIntentToAutofillAssistant(true);
        mDelegate.setAutofillAssistantAllowAppOverrideResult(
                IntentToAutofillAllowingAppResult.DEFER_TO_APP_NOW);
        checkUrl(AUTOFILL_ASSISTANT_INTENT_URL_WITH_SOMEAPP)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
                        START_OTHER_ACTIVITY);

        Assert.assertNotNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testAutofillAssistantIntentWithAppOverrideLater_InRegular() {
        mDelegate.add(new IntentActivity("https://someapp.com", "someapp"));

        RedirectHandler redirectHandler = RedirectHandler.create();

        mUrlHandler.mIsGoogleReferrer = true;
        mDelegate.setIsIntentToAutofillAssistant(true);
        mDelegate.setAutofillAssistantAllowAppOverrideResult(
                IntentToAutofillAllowingAppResult.DEFER_TO_APP_LATER);
        checkUrl(AUTOFILL_ASSISTANT_INTENT_URL_WITH_SOMEAPP)
                .withRedirectHandler(redirectHandler)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertFalse(redirectHandler.shouldNotOverrideUrlLoading());
        Assert.assertFalse(
                redirectHandler
                        .getAndClearShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain());
        Assert.assertNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testAutofillAssistantIntentWithAppOverride_InIncognito() {
        mDelegate.add(new IntentActivity("https://someapp.com", "someapp"));

        mDelegate.setIsIntentToAutofillAssistant(true);
        mDelegate.setAutofillAssistantAllowAppOverrideResult(
                IntentToAutofillAllowingAppResult.DEFER_TO_APP_NOW);
        checkUrl(AUTOFILL_ASSISTANT_INTENT_URL_WITH_SOMEAPP)
                .withIsIncognito(true)
                .expecting(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        Assert.assertNull(mDelegate.startActivityIntent);
    }

    @Test
    @SmallTest
    public void testIsDownload_noSystemDownloadManager() {
        Assert.assertTrue("pdf should be a download, no viewer in Android Chrome",
                mUrlHandler.isPdfDownload(new GURL("http://somesampeleurldne.com/file.pdf")));
        Assert.assertFalse("URL is not a file, but web page",
                mUrlHandler.isPdfDownload(new GURL("http://somesampleurldne.com/index.html")));
        Assert.assertFalse("URL is not a file url",
                mUrlHandler.isPdfDownload(
                        new GURL("http://somesampeleurldne.com/not.a.real.extension")));
        Assert.assertFalse("URL is an image, can be viewed in Chrome",
                mUrlHandler.isPdfDownload(new GURL("http://somesampleurldne.com/image.jpg")));
        Assert.assertFalse("URL is a text file can be viewed in Chrome",
                mUrlHandler.isPdfDownload(new GURL("http://somesampleurldne.com/copy.txt")));
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_NoResolveInfo() {
        String packageName = "";
        List<ResolveInfo> resolveInfos = new ArrayList<ResolveInfo>();
        Assert.assertEquals(0,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(
                                resolveInfos, packageName, HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_NoPathOrAuthority() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(0,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(
                                resolveInfos, packageName, HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithPath() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataPath("somepath", 2);
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(1,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(
                                resolveInfos, packageName, HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithAuthority() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(1,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(
                                resolveInfos, packageName, HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithAuthority_Wildcard_Host() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("*", null);
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(0,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(
                                resolveInfos, packageName, HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());

        ResolveInfo infoWildcardSubDomain = new ResolveInfo();
        infoWildcardSubDomain.filter = new IntentFilter();
        infoWildcardSubDomain.filter.addDataAuthority("http://*.google.com", "80");
        List<ResolveInfo> resolveInfosWildcardSubDomain = makeResolveInfos(infoWildcardSubDomain);
        Assert.assertEquals(1,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(resolveInfosWildcardSubDomain,
                                packageName, HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithTargetPackage_Matching() {
        String packageName = "com.android.chrome";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = packageName;
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(1,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(
                                resolveInfos, packageName, HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithTargetPackage_NotMatching() {
        String packageName = "com.android.chrome";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = "com.foo.bar";
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(0,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(
                                resolveInfos, packageName, HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializeHandler_withEphemeralResolver() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataPath("somepath", 2);
        info.activityInfo = new ActivityInfo();

        // See IntentUtils.isInstantAppResolveInfo
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            info.isInstantAppAvailable = true;
        } else {
            info.activityInfo.name = IntentUtils.EPHEMERAL_INSTALLER_CLASS;
        }
        info.activityInfo.packageName = "com.google.android.gms";
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        // Whether ephemeral resolver is counted as a specialized handler should be dependent on the
        // value passed for |handlesInstantAppLaunchingInternally|.
        Assert.assertEquals(0,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(
                                resolveInfos, packageName, HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());
        Assert.assertEquals(1,
                ExternalNavigationHandler
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName,
                                !HANDLES_INSTANT_APP_LAUNCHING_INTERNALLY)
                        .size());
    }

    @Test
    @SmallTest
    public void testExceptions() {
        // Test that we don't crash under various bad intent URIs.
        String numberFormatException = "intent://foo#Intent;scheme=https;i.FOO=0.1;end";
        String uriSyntaxException = "intent://foo#Intent;scheme=https;invalid=asdf;end";
        String indexOutOfBoundsException = "intent://foo#Intent;scheme=https;c.%;end";

        checkUrl(numberFormatException).expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
        checkUrl(uriSyntaxException).expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);
        checkUrl(indexOutOfBoundsException)
                .expecting(OverrideUrlLoadingResultType.NO_OVERRIDE, IGNORE);

        Assert.assertFalse(mUrlHandler.canExternalAppHandleUrl(new GURL(numberFormatException)));
        Assert.assertFalse(mUrlHandler.canExternalAppHandleUrl(new GURL(uriSyntaxException)));
        Assert.assertFalse(
                mUrlHandler.canExternalAppHandleUrl(new GURL(indexOutOfBoundsException)));
    }

    private static List<ResolveInfo> makeResolveInfos(ResolveInfo... infos) {
        return Arrays.asList(infos);
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo ai = new ActivityInfo();
        ai.packageName = packageName;
        ai.name = "Name: " + packageName;
        ResolveInfo ri = new ResolveInfo();
        ri.activityInfo = ai;
        return ri;
    }

    private static ResolveInfo newSpecializedResolveInfo(
            String packageName, IntentActivity activity) {
        ResolveInfo info = newResolveInfo(packageName);
        info.filter = new IntentFilter(Intent.ACTION_VIEW);
        info.filter.addDataAuthority(activity.mUrlPrefix, null);
        return info;
    }

    private static class IntentActivity {
        private String mUrlPrefix;
        private String mPackageName;
        private boolean mIsWebApk;

        public IntentActivity(String urlPrefix, String packageName) {
            mUrlPrefix = urlPrefix;
            mPackageName = packageName;
        }

        public String urlPrefix() {
            return mUrlPrefix;
        }

        public String packageName() {
            return mPackageName;
        }

        public boolean isSpecialized() {
            // Specialized if URL prefix is more than just a scheme.
            return Pattern.compile("[^:/]+://.+").matcher(mUrlPrefix).matches();
        }
    }

    private class ExternalNavigationHandlerForTesting extends ExternalNavigationHandler {
        public String defaultSmsPackageName;
        public GURL mLastCommittedUrl;
        public boolean mIsSerpReferrer;
        public boolean mIsGoogleReferrer;
        public boolean mShouldRequestFileAccess;
        public String mNewUrlAfterClobbering;
        public String mReferrerUrlForClobbering;
        public boolean mStartFileIntentCalled;
        public Intent mStartActivityInIncognitoIntent;
        public boolean mStartIncognitoIntentCalled;
        public boolean mCanShowIncognitoDialog;
        public AlertDialog mShownIncognitoAlertDialog;
        public boolean mResolveInfoContainsSelf;

        public ExternalNavigationHandlerForTesting(ExternalNavigationDelegate delegate) {
            super(delegate);
        }

        @Override
        protected boolean isValidWebApk(String packageName) {
            return packageName.startsWith(WEBAPK_PACKAGE_PREFIX)
                    && !packageName.equals(INVALID_WEBAPK_PACKAGE_NAME);
        }

        @Override
        public boolean blockExternalFormRedirectsWithoutGesture() {
            return true;
        }

        @Override
        protected boolean canLaunchIncognitoIntent(Intent intent, Context context) {
            mStartActivityInIncognitoIntent = intent;
            mStartIncognitoIntentCalled = true;
            return mCanShowIncognitoDialog;
        }

        @Override
        protected AlertDialog showLeavingIncognitoAlert(Context context,
                ExternalNavigationParams params, Intent intent, GURL fallbackUrl, boolean proxy) {
            if (context == null) return mAlertDialog;
            mShownIncognitoAlertDialog =
                    super.showLeavingIncognitoAlert(context, params, intent, fallbackUrl, proxy);
            return mShownIncognitoAlertDialog;
        }

        @Override
        protected String getDefaultSmsPackageNameFromSystem() {
            return defaultSmsPackageName;
        }

        @Override
        protected GURL getLastCommittedUrl() {
            return mLastCommittedUrl;
        }

        @Override
        protected boolean isSerpReferrer() {
            return mIsSerpReferrer;
        }

        @Override
        protected boolean isGoogleReferrer() {
            return mIsGoogleReferrer;
        }

        @Override
        protected boolean shouldRequestFileAccess(GURL url) {
            return mShouldRequestFileAccess;
        }

        @Override
        protected void startFileIntent(ExternalNavigationParams params) {
            mStartFileIntentCalled = true;
        }

        @Override
        protected OverrideUrlLoadingResult clobberCurrentTab(GURL url, GURL referrerUrl) {
            mNewUrlAfterClobbering = url.getSpec();
            mReferrerUrlForClobbering = referrerUrl.getSpec();
            return OverrideUrlLoadingResult.forClobberingTab();
        }

        @Override
        public boolean isYoutubePairingCode(GURL url) {
            return super.isYoutubePairingCode(url);
        }

        @Override
        protected boolean resolveInfoContainsSelf(List<ResolveInfo> resolveInfos) {
            return mResolveInfoContainsSelf;
        }
    };

    private static class TestExternalNavigationDelegate implements ExternalNavigationDelegate {
        public List<ResolveInfo> queryIntentActivities(Intent intent) {
            List<ResolveInfo> list = new ArrayList<>();
            String dataString = intent.getDataString();
            if (intent.getScheme() != null) {
                if (dataString.startsWith("http://") || dataString.startsWith("https://")) {
                    list.add(newResolveInfo("chrome"));
                }
                for (IntentActivity intentActivity : mIntentActivities) {
                    if (dataString.startsWith(intentActivity.urlPrefix())) {
                        list.add(newSpecializedResolveInfo(
                                intentActivity.packageName(), intentActivity));
                    }
                }
                if (!list.isEmpty()) return list;

                String schemeString = intent.getScheme();
                boolean isMarketScheme = schemeString != null && schemeString.startsWith("market");
                if (mCanResolveActivityForMarket && isMarketScheme) {
                    list.add(newResolveInfo("market"));
                    return list;
                }
                if (mCanResolveActivityForExternalSchemes && !isMarketScheme) {
                    list.add(newResolveInfo(intent.getData().getScheme()));
                }
            } else if (mCanResolveActivityForExternalSchemes) {
                // Scheme-less intents (eg. Action-based intents like opening Settings).
                list.add(newResolveInfo("package"));
            }
            return list;
        }

        @Override
        public Context getContext() {
            return mContext;
        }

        @Override
        public boolean willAppHandleIntent(Intent intent) {
            String chromePackageName = ContextUtils.getApplicationContext().getPackageName();
            if (chromePackageName.equals(intent.getPackage())
                    || (intent.getComponent() != null
                            && chromePackageName.equals(intent.getComponent().getPackageName()))) {
                return true;
            }

            List<ResolveInfo> resolveInfos = queryIntentActivities(intent);
            return resolveInfos.size() == 1
                    && resolveInfos.get(0).activityInfo.packageName.contains("chrome");
        }

        @Override
        public boolean shouldDisableExternalIntentRequestsForUrl(GURL url) {
            return mShouldDisableExternalIntentRequests;
        }

        @Override
        public boolean handlesInstantAppLaunchingInternally() {
            return false;
        }

        @Override
        public void dispatchAuthenticatedIntent(Intent intent) {}

        @Override
        public void didStartActivity(Intent intent) {
            startActivityIntent = intent;
        }

        @Override
        public @StartActivityIfNeededResult int maybeHandleStartActivityIfNeeded(
                Intent intent, boolean proxy) {
            // For simplicity, don't distinguish between startActivityIfNeeded and startActivity
            // until a test requires this distinction.
            startActivityIntent = intent;
            mCalledWithProxy = proxy;
            return StartActivityIfNeededResult.HANDLED_WITH_ACTIVITY_START;
        }

        @Override
        public boolean canLoadUrlInCurrentTab() {
            return mCanLoadUrlInTab;
        }

        @Override
        public void closeTab() {}

        @Override
        public boolean isIncognito() {
            return false;
        }

        @Override
        public boolean hasCustomLeavingIncognitoDialog() {
            return mShouldPresentLeavingIncognitoDialog;
        }

        @Override
        public void presentLeavingIncognitoModalDialog(Callback<Boolean> onUserDecision) {
            incognitoDialogUserDecisionCallback = onUserDecision;
        }

        @Override
        public void loadUrlIfPossible(LoadUrlParams loadUrlParams) {}

        @Override
        public void maybeSetWindowId(Intent intent) {}

        @Override
        public void maybeSetPendingReferrer(Intent intent, GURL referrerUrl) {
            // This is used in a test to check that ExternalNavigationHandler correctly passes
            // this data to the delegate when the referrer URL is non-null.
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(referrerUrl.getSpec()));
        }

        @Override
        public void maybeAdjustInstantAppExtras(Intent intent, boolean isIntentToInstantApp) {
            if (isIntentToInstantApp) {
                intent.putExtra(IS_INSTANT_APP_EXTRA, true);
            } else {
                intent.putExtra(IS_INSTANT_APP_EXTRA, false);
            }
        }

        @Override
        public void maybeSetRequestMetadata(Intent intent, boolean hasUserGesture,
                boolean isRendererInitiated, Origin initiatorOrigin) {
            maybeSetRequestMetadataCalled = true;
        }

        @Override
        public void maybeSetPendingIncognitoUrl(Intent intent) {}

        @Override
        public boolean isApplicationInForeground() {
            return mIsChromeAppInForeground;
        }

        @Override
        public boolean maybeLaunchInstantApp(
                GURL url, GURL referrerUrl, boolean isIncomingRedirect, boolean isSerpReferrer) {
            return mCanHandleWithInstantApp;
        }

        @Override
        public WindowAndroid getWindowAndroid() {
            return null;
        }

        @Override
        public WebContents getWebContents() {
            return null;
        }

        @Override
        public boolean hasValidTab() {
            return false;
        }

        @Override
        public boolean canCloseTabOnIncognitoIntentLaunch() {
            return false;
        }

        @Override
        public boolean isIntentForTrustedCallingApp(Intent intent) {
            return mIsCallingAppTrusted;
        }

        @Override
        public boolean isIntentToInstantApp(Intent intent) {
            return mIsIntentToInstantApp;
        }

        @Override
        public boolean isIntentToAutofillAssistant(Intent intent) {
            return mIsIntentToAutofillAssistant;
        }

        @Override
        public @IntentToAutofillAllowingAppResult int isIntentToAutofillAssistantAllowingApp(
                ExternalNavigationParams params, Intent targetIntent,
                Function<Intent, Boolean> canExternalAppHandleIntent) {
            return mAutofillAssistantAllowAppOverrideResult;
        }

        @Override
        public boolean handleWithAutofillAssistant(ExternalNavigationParams params,
                Intent targetIntent, GURL browserFallbackUrl, boolean isGoogleReferrer) {
            return mHandleWithAutofillAssistant;
        }

        @Override
        public boolean shouldLaunchWebApksOnInitialIntent() {
            return mShouldLaunchWebApksOnInitialIntent;
        }

        public void reset() {
            startActivityIntent = null;
            startIncognitoIntentCalled = false;
            mCalledWithProxy = false;
        }

        public void setContext(Context context) {
            mContext = context;
        }

        public void add(IntentActivity handler) {
            mIntentActivities.add(handler);
        }

        public void setCanResolveActivityForExternalSchemes(boolean value) {
            mCanResolveActivityForExternalSchemes = value;
        }

        public void setCanResolveActivityForMarket(boolean value) {
            mCanResolveActivityForMarket = value;
        }

        public void setIsChromeAppInForeground(boolean value) {
            mIsChromeAppInForeground = value;
        }

        public void setReferrerWebappPackageName(String webappPackageName) {
            mReferrerWebappPackageName = webappPackageName;
        }

        public String getReferrerWebappPackageName() {
            return mReferrerWebappPackageName;
        }

        public void setCanHandleWithInstantApp(boolean value) {
            mCanHandleWithInstantApp = value;
        }

        public void setHandleIntentWithAutofillAssistant(boolean value) {
            mHandleWithAutofillAssistant = value;
        }

        public void setIsCallingAppTrusted(boolean trusted) {
            mIsCallingAppTrusted = trusted;
        }

        public void setDisableExternalIntentRequests(boolean disable) {
            mShouldDisableExternalIntentRequests = disable;
        }

        public void setIsIntentToInstantApp(boolean value) {
            mIsIntentToInstantApp = value;
        }

        public void setIsIntentToAutofillAssistant(boolean value) {
            mIsIntentToAutofillAssistant = value;
        }

        public void setAutofillAssistantAllowAppOverrideResult(
                @IntentToAutofillAllowingAppResult int value) {
            mAutofillAssistantAllowAppOverrideResult = value;
        }

        public void setCanLoadUrlInTab(boolean value) {
            mCanLoadUrlInTab = value;
        }

        public void setShouldPresentLeavingIncognitoDialog(boolean value) {
            mShouldPresentLeavingIncognitoDialog = value;
        }

        public void setShouldLaunchWebApksOnInitialIntent(boolean value) {
            mShouldLaunchWebApksOnInitialIntent = value;
        }

        public Intent startActivityIntent;
        public boolean startIncognitoIntentCalled;
        public boolean maybeSetRequestMetadataCalled;
        public Callback<Boolean> incognitoDialogUserDecisionCallback;

        private String mReferrerWebappPackageName;

        private ArrayList<IntentActivity> mIntentActivities = new ArrayList<IntentActivity>();
        private boolean mCanResolveActivityForExternalSchemes = true;
        private boolean mCanResolveActivityForMarket = true;
        private boolean mCanHandleWithInstantApp;
        private boolean mHandleWithAutofillAssistant;
        public boolean mCalledWithProxy;
        public boolean mIsChromeAppInForeground = true;
        private boolean mIsCallingAppTrusted;
        private boolean mShouldDisableExternalIntentRequests;
        private boolean mIsIntentToInstantApp;
        private boolean mIsIntentToAutofillAssistant;
        private @IntentToAutofillAllowingAppResult int mAutofillAssistantAllowAppOverrideResult;
        private boolean mCanLoadUrlInTab;
        private boolean mShouldPresentLeavingIncognitoDialog;
        private boolean mShouldLaunchWebApksOnInitialIntent;
        private Context mContext;
    }

    private void checkIntentSanity(Intent intent, String name) {
        Assert.assertTrue("The invoked " + name + " doesn't have the BROWSABLE category set\n",
                intent.hasCategory(Intent.CATEGORY_BROWSABLE));
        Assert.assertNull("The invoked " + name + " should not have a Component set\n",
                intent.getComponent());
    }

    private ExternalNavigationTestParams checkUrl(String url) {
        return new ExternalNavigationTestParams(url);
    }

    private class ExternalNavigationTestParams {
        private final String mUrl;

        private String mReferrerUrl;
        private boolean mIsIncognito;
        private int mPageTransition = PageTransition.LINK;
        private boolean mIsRedirect;
        private boolean mChromeAppInForegroundRequired = true;
        private boolean mIsBackgroundTabNavigation;
        private boolean mIntentLaunchesAllowedInBackgroundTabs;
        private boolean mHasUserGesture;
        private RedirectHandler mRedirectHandler;
        private boolean mIsRendererInitiated;
        private boolean mIsMainFrame = true;

        private ExternalNavigationTestParams(String url) {
            mUrl = url;
        }

        public ExternalNavigationTestParams withReferrer(String referrerUrl) {
            mReferrerUrl = referrerUrl;
            return this;
        }

        public ExternalNavigationTestParams withIsIncognito(boolean isIncognito) {
            mIsIncognito = isIncognito;
            return this;
        }

        public ExternalNavigationTestParams withPageTransition(int pageTransition) {
            mPageTransition = pageTransition;
            return this;
        }

        public ExternalNavigationTestParams withIsRedirect(boolean isRedirect) {
            mIsRedirect = isRedirect;
            return this;
        }

        public ExternalNavigationTestParams withHasUserGesture(boolean hasGesture) {
            mHasUserGesture = hasGesture;
            return this;
        }

        public ExternalNavigationTestParams withChromeAppInForegroundRequired(
                boolean foregroundRequired) {
            mChromeAppInForegroundRequired = foregroundRequired;
            return this;
        }

        public ExternalNavigationTestParams withIsBackgroundTabNavigation(
                boolean isBackgroundTabNavigation) {
            mIsBackgroundTabNavigation = isBackgroundTabNavigation;
            return this;
        }

        public ExternalNavigationTestParams withAllowIntentLaunchesInBackgroundTabs(
                boolean allowIntentLaunchesInBackgroundTabs) {
            mIntentLaunchesAllowedInBackgroundTabs = allowIntentLaunchesInBackgroundTabs;
            return this;
        }

        public ExternalNavigationTestParams withRedirectHandler(RedirectHandler handler) {
            mRedirectHandler = handler;
            return this;
        }

        public ExternalNavigationTestParams withIsRendererInitiated(boolean isRendererInitiated) {
            mIsRendererInitiated = isRendererInitiated;
            return this;
        }

        public ExternalNavigationTestParams withIsMainFrame(boolean isMainFrame) {
            mIsMainFrame = isMainFrame;
            return this;
        }

        public void expecting(
                @OverrideUrlLoadingResultType int expectedOverrideResult, int otherExpectation) {
            expecting(expectedOverrideResult, OverrideUrlLoadingAsyncActionType.NO_ASYNC_ACTION,
                    otherExpectation);
        }

        public void expecting(@OverrideUrlLoadingResultType int expectedOverrideResult,
                @OverrideUrlLoadingAsyncActionType int expectedOverrideAsyncAction,
                int otherExpectation) {
            boolean expectStartIncognito = (otherExpectation & START_INCOGNITO) != 0;
            boolean expectStartActivity =
                    (otherExpectation & (START_WEBAPK | START_OTHER_ACTIVITY)) != 0;
            boolean expectStartWebApk = (otherExpectation & START_WEBAPK) != 0;
            boolean expectStartOtherActivity = (otherExpectation & START_OTHER_ACTIVITY) != 0;
            boolean expectStartFile = (otherExpectation & START_FILE) != 0;
            boolean expectSaneIntent = expectStartOtherActivity
                    && (otherExpectation & INTENT_SANITIZATION_EXCEPTION) == 0;
            boolean expectProxyForIA = (otherExpectation & PROXY_FOR_INSTANT_APPS) != 0;

            mDelegate.reset();

            ExternalNavigationParams params =
                    new ExternalNavigationParams
                            .Builder(new GURL(mUrl), mIsIncognito, new GURL(mReferrerUrl),
                                    mPageTransition, mIsRedirect)
                            .setApplicationMustBeInForeground(mChromeAppInForegroundRequired)
                            .setRedirectHandler(mRedirectHandler)
                            .setIsBackgroundTabNavigation(mIsBackgroundTabNavigation)
                            .setIntentLaunchesAllowedInBackgroundTabs(
                                    mIntentLaunchesAllowedInBackgroundTabs)
                            .setIsMainFrame(mIsMainFrame)
                            .setNativeClientPackageName(mDelegate.getReferrerWebappPackageName())
                            .setHasUserGesture(mHasUserGesture)
                            .setIsRendererInitiated(mIsRendererInitiated)
                            .build();
            OverrideUrlLoadingResult result = mUrlHandler.shouldOverrideUrlLoading(params);
            boolean startActivityCalled = false;
            boolean startWebApkCalled = false;

            // Incognito intent launching gets caught by the test URL handler, whereas non-incgonito
            // intent launching gets caught by the test delegate.
            Intent startActivityIntent = mDelegate.startActivityIntent;

            if (startActivityIntent != null) {
                startActivityCalled = true;
                String packageName = startActivityIntent.getPackage();
                if (packageName != null) {
                    startWebApkCalled = packageName.startsWith(WEBAPK_PACKAGE_PREFIX);
                }
            }

            Assert.assertEquals(expectedOverrideResult, result.getResultType());
            Assert.assertEquals(expectedOverrideAsyncAction, result.getAsyncActionType());
            Assert.assertEquals(expectStartIncognito, mUrlHandler.mStartIncognitoIntentCalled);
            Assert.assertEquals(expectStartActivity, startActivityCalled);
            Assert.assertEquals(expectStartWebApk, startWebApkCalled);
            Assert.assertEquals(expectStartFile, mUrlHandler.mStartFileIntentCalled);
            Assert.assertEquals(expectProxyForIA, mDelegate.mCalledWithProxy);

            if (startActivityCalled && expectSaneIntent) {
                checkIntentSanity(startActivityIntent, "Intent");
                if (startActivityIntent.getSelector() != null) {
                    checkIntentSanity(startActivityIntent.getSelector(), "Intent's selector");
                }
            }
        }
    }

    private static String getPackageName() {
        return ContextUtils.getApplicationContext().getPackageName();
    }

    private static class TestPackageManager extends MockPackageManager {
        private TestExternalNavigationDelegate mDelegate;

        public TestPackageManager(TestExternalNavigationDelegate delegate) {
            mDelegate = delegate;
        }

        @Override
        public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
            return mDelegate.queryIntentActivities(intent);
        }
    }

    private static class TestContext extends ContextWrapper {
        private PackageManager mPackageManager;

        public TestContext(Context baseContext, TestExternalNavigationDelegate delegate) {
            super(baseContext);
            mPackageManager = new TestPackageManager(delegate);
        }

        @Override
        public Context getApplicationContext() {
            return this;
        }

        @Override
        public PackageManager getPackageManager() {
            return mPackageManager;
        }

        @Override
        public String getPackageName() {
            return "test.app.name";
        }

        @Override
        public void startActivities(Intent[] intents, Bundle options) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void startActivity(Intent intent) {
        }

        @Override
        public void startActivity(Intent intent, Bundle options) {
            throw new UnsupportedOperationException();
        }
    }
}
