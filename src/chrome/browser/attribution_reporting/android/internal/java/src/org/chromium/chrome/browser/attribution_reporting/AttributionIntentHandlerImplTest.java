// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.eq;

import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.view.InputEvent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for AttributionIntentHandlerImpl. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AttributionIntentHandlerImplTest {
    private AttributionIntentHandlerImpl mAttributionIntentHandlerImpl;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private PendingIntent mPendingIntent;

    @Mock
    private InputEvent mInputEvent;

    @Captor
    private ArgumentCaptor<Intent> mPendingIntentSendCaptor;

    private String mPackageName = "packageName";
    private byte mPackageMac[];
    private boolean mInputEventValid = true;

    @Before
    public void setUp() {
        mAttributionIntentHandlerImpl =
                new AttributionIntentHandlerImpl((inputEvent) -> mInputEventValid);
        mPackageMac = AttributionIntentHandlerImpl.sHasher.doFinal(
                ApiCompatibilityUtils.getBytesUtf8(mPackageName));
    }

    private Intent makeValidAttributionIntent() {
        Intent intent = new Intent(AttributionConstants.ACTION_APP_ATTRIBUTION);
        byte packageMac[] = AttributionIntentHandlerImpl.sHasher.doFinal(
                ApiCompatibilityUtils.getBytesUtf8(mPackageName));
        intent.putExtra(AttributionIntentHandlerImpl.EXTRA_PACKAGE_NAME, mPackageName);
        intent.putExtra(AttributionIntentHandlerImpl.EXTRA_PACKAGE_MAC, packageMac);
        intent.putExtra(AttributionIntentHandlerImpl.EXTRA_ORIGINAL_INTENT, new Intent());

        intent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID, "1234");
        intent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION, "https://example.com");
        intent.putExtra(AttributionConstants.EXTRA_INPUT_EVENT, mInputEvent);
        return intent;
    }

    @Test
    public void testHandleOuterAttributionIntent() throws Exception {
        Intent intent = new Intent();
        intent.setData(Uri.parse("https://www.example.com"));
        intent.putExtra("testKey", "testValue");
        intent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_INTENT, mPendingIntent);
        Mockito.when(mPendingIntent.getCreatorPackage()).thenReturn(mPackageName);

        Assert.assertTrue(mAttributionIntentHandlerImpl.handleOuterAttributionIntent(intent));

        Mockito.verify(mPendingIntent)
                .send(eq(ContextUtils.getApplicationContext()), eq(0),
                        mPendingIntentSendCaptor.capture());
        intent.removeExtra(AttributionConstants.EXTRA_ATTRIBUTION_INTENT);
        Intent fillIn = mPendingIntentSendCaptor.getValue();
        Intent originalIntent =
                fillIn.getParcelableExtra(AttributionIntentHandlerImpl.EXTRA_ORIGINAL_INTENT);
        Assert.assertEquals(intent.toUri(0), originalIntent.toUri(0));
        Assert.assertEquals(mPackageName,
                fillIn.getStringExtra(AttributionIntentHandlerImpl.EXTRA_PACKAGE_NAME));
        byte[] expectedMac = AttributionIntentHandlerImpl.sHasher.doFinal(
                ApiCompatibilityUtils.getBytesUtf8(mPackageName));
        Assert.assertArrayEquals(expectedMac,
                fillIn.getByteArrayExtra(AttributionIntentHandlerImpl.EXTRA_PACKAGE_MAC));
    }

    @Test
    public void testHandleOuterAttributionIntent_canceledPendingIntent() throws Exception {
        Intent intent = new Intent();
        intent.putExtra(AttributionConstants.EXTRA_ATTRIBUTION_INTENT, mPendingIntent);
        String packageName = "packageName";
        Mockito.when(mPendingIntent.getCreatorPackage()).thenReturn(packageName);
        Mockito.doThrow(new PendingIntent.CanceledException())
                .when(mPendingIntent)
                .send(anyObject(), anyInt(), anyObject());

        Assert.assertFalse(mAttributionIntentHandlerImpl.handleOuterAttributionIntent(intent));
    }

    @Test
    public void testHandleOuterAttributionIntent_NoAttribution() throws Exception {
        Assert.assertFalse(
                mAttributionIntentHandlerImpl.handleOuterAttributionIntent(new Intent()));
    }

    @Test
    public void testIsValidAttributionIntent_valid() {
        Assert.assertTrue(mAttributionIntentHandlerImpl.isValidAttributionIntent(mPackageName,
                mPackageMac, new Intent(), "event", "https://example.com", mInputEvent));
    }

    @Test
    public void testIsValidAttributionIntent_wrongMac() {
        mPackageMac[mPackageMac.length - 1] = (byte) ~mPackageMac[mPackageMac.length - 1];
        Assert.assertFalse(mAttributionIntentHandlerImpl.isValidAttributionIntent(mPackageName,
                mPackageMac, new Intent(), "event", "https://example.com", mInputEvent));
    }

    @Test
    public void testIsValidAttributionIntent_noIntent() {
        Assert.assertFalse(mAttributionIntentHandlerImpl.isValidAttributionIntent(
                mPackageName, mPackageMac, null, "event", "https://example.com", mInputEvent));
    }

    @Test
    public void testIsValidAttributionIntent_noEventId() {
        Assert.assertFalse(mAttributionIntentHandlerImpl.isValidAttributionIntent(
                mPackageName, mPackageMac, new Intent(), null, "https://example.com", mInputEvent));
    }

    @Test
    public void testIsValidAttributionIntent_noAttributionDestitation() {
        Assert.assertFalse(mAttributionIntentHandlerImpl.isValidAttributionIntent(
                mPackageName, mPackageMac, new Intent(), "event", null, mInputEvent));
    }

    @Test
    public void testIsValidAttributionIntent_noInputEvent() {
        Assert.assertFalse(mAttributionIntentHandlerImpl.isValidAttributionIntent(
                mPackageName, mPackageMac, new Intent(), "event", "https://example.com", null));
    }

    @Test
    public void testIsValidAttributionIntent_invalidInputEvent() {
        mInputEventValid = false;
        Assert.assertFalse(mAttributionIntentHandlerImpl.isValidAttributionIntent(mPackageName,
                mPackageMac, new Intent(), "event", "https://example.com", mInputEvent));
    }

    @Test
    public void testHandleInnerAttributionIntent() {
        Intent originalIntent = new Intent(Intent.ACTION_VIEW);
        originalIntent.setData(Uri.parse("https://www.example.com"));
        originalIntent.putExtra("testKey", "testValue");
        Intent intent = makeValidAttributionIntent();
        intent.putExtra(AttributionIntentHandlerImpl.EXTRA_ORIGINAL_INTENT, originalIntent);

        Intent result = mAttributionIntentHandlerImpl.handleInnerAttributionIntent(intent);

        Assert.assertEquals(result.toUri(0), originalIntent.toUri(0));
    }

    @Test
    public void testHandleInnerAttributionIntent_invalid() {
        Intent originalIntent = new Intent(Intent.ACTION_VIEW);
        originalIntent.setData(Uri.parse("https://www.example.com"));
        originalIntent.putExtra("testKey", "testValue");
        Intent intent = makeValidAttributionIntent();
        intent.putExtra(AttributionIntentHandlerImpl.EXTRA_ORIGINAL_INTENT, originalIntent);
        intent.putExtra(AttributionIntentHandlerImpl.EXTRA_PACKAGE_NAME, "wrongPackage");

        // Even for an invalid attribution intent, we should still un-wrap to the original intent.
        Intent result = mAttributionIntentHandlerImpl.handleInnerAttributionIntent(intent);

        Assert.assertEquals(result.toUri(0), originalIntent.toUri(0));
    }

    @Test
    public void testHandleInnerAttributionIntent_noAttributionAction() {
        Intent intent = makeValidAttributionIntent();
        // Replace the attribution action with a VIEW action.
        intent.setAction(Intent.ACTION_VIEW);
        Assert.assertNull(mAttributionIntentHandlerImpl.handleInnerAttributionIntent(intent));
    }
}
