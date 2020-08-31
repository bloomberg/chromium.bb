// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.clipboard;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.BitmapDrawable;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Assert;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionBuilderForTest;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;

/**
 * Tests for {@link ClipboardSuggestionProcessor}.
 */
public class ClipboardSuggestionProcessorTest {
    private static final GURL TEST_URL = new GURL("http://url");
    private static final GURL ARABIC_URL = new GURL("http://site.com/مرحبا/123");

    @Mock
    SuggestionHost mSuggestionHost;
    @Mock
    LargeIconBridge mIconBridge;
    @Mock
    Resources mResources;

    private ClipboardSuggestionProcessor mProcessor;
    private OmniboxSuggestion mSuggestion;
    private PropertyModel mModel;
    private Bitmap mBitmap;
    private ViewGroup mRootView;
    private TextView mTitleTextView;
    private TextView mContentTextView;
    private int mLastSetTextDirection = -1;

    @CalledByNative
    private ClipboardSuggestionProcessorTest() {}

    @CalledByNative
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mBitmap = Bitmap.createBitmap(10, 5, Config.ARGB_8888);
        mProcessor = new ClipboardSuggestionProcessor(
                ContextUtils.getApplicationContext(), mSuggestionHost, () -> mIconBridge);
        mRootView = new LinearLayout(ContextUtils.getApplicationContext());
        mTitleTextView = new TextView(ContextUtils.getApplicationContext());
        mTitleTextView.setId(R.id.line_1);
        mContentTextView = new TextView(ContextUtils.getApplicationContext()) {
            @Override
            public void setTextDirection(int textDirection) {
                super.setTextDirection(textDirection);
                mLastSetTextDirection = textDirection;
            }
        };
        mContentTextView.setId(R.id.line_2);
        mRootView.addView(mTitleTextView);
        mRootView.addView(mContentTextView);
    }
    /** Create clipboard suggestion for test. */
    private void createClipboardSuggestion(int type, GURL url) {
        createClipboardSuggestion(type, url, null);
    }

    /** Create clipboard suggestion for test. */
    private void createClipboardSuggestion(int type, GURL url, byte[] clipboardImageData) {
        mSuggestion = OmniboxSuggestionBuilderForTest.searchWithType(type)
                              .setIsSearch(type != OmniboxSuggestionType.CLIPBOARD_URL)
                              .setUrl(url)
                              .setClipboardImageData(clipboardImageData)
                              .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, mModel, 0);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionViewProperties.TEXT_LINE_1_TEXT);
        SuggestionViewViewBinder.bind(
                mModel, mRootView, SuggestionCommonProperties.USE_DARK_COLORS);
        SuggestionViewViewBinder.bind(
                mModel, mRootView, SuggestionViewProperties.IS_SEARCH_SUGGESTION);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionViewProperties.TEXT_LINE_2_TEXT);
    }

    @CalledByNativeJavaTest
    public void clipboardSugestion_identifyUrlSuggestion() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
    }

    @CalledByNativeJavaTest
    public void clipboardSuggestion_doesNotRefine() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        Assert.assertNull(mModel.get(BaseSuggestionViewProperties.ACTION_CALLBACK));

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        Assert.assertNull(mModel.get(BaseSuggestionViewProperties.ACTION_CALLBACK));

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        Assert.assertNull(mModel.get(BaseSuggestionViewProperties.ACTION_CALLBACK));
    }

    @CalledByNativeJavaTest
    public void clipboardSuggestion_showsFaviconWhenAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, TEST_URL);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForUrl(eq(TEST_URL), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(mBitmap, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @CalledByNativeJavaTest
    public void clipboardSuggestion_showsFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, TEST_URL);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForUrl(eq(TEST_URL), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(null, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertEquals(icon1, icon2);
    }

    @CalledByNativeJavaTest
    public void clipobardSuggestion_urlAndTextDirection() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        // URL
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, ARABIC_URL);
        Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        verify(mIconBridge).getLargeIconForUrl(eq(ARABIC_URL), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(null, 0, false, 0);
        Assert.assertEquals(TextView.TEXT_DIRECTION_LTR, mLastSetTextDirection);

        // Text
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        Assert.assertEquals(TextView.TEXT_DIRECTION_INHERIT, mLastSetTextDirection);
    }

    @CalledByNativeJavaTest
    public void clipboardSuggestion_showsThumbnailWhenAvailable() {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        Assert.assertTrue(mBitmap.compress(Bitmap.CompressFormat.PNG, 100, baos));
        byte[] bitmapData = baos.toByteArray();
        createClipboardSuggestion(
                OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL(), bitmapData);
        SuggestionDrawableState icon = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon);

        // Since |icon| is Bitmap -> PNG -> Bitmap, the image changed, we just check the size to
        // make sure they are same.
        Assert.assertEquals(
                mBitmap.getWidth(), ((BitmapDrawable) icon.drawable).getBitmap().getWidth());
        Assert.assertEquals(
                mBitmap.getHeight(), ((BitmapDrawable) icon.drawable).getBitmap().getHeight());
    }

    @CalledByNativeJavaTest
    public void clipboardSuggestion_thumbnailShouldResizeIfTooLarge() {
        int size = ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);

        Bitmap largeBitmap = Bitmap.createBitmap(size * 2, size * 2, Config.ARGB_8888);
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        Assert.assertTrue(largeBitmap.compress(Bitmap.CompressFormat.PNG, 100, baos));
        byte[] bitmapData = baos.toByteArray();
        createClipboardSuggestion(
                OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL(), bitmapData);
        SuggestionDrawableState icon = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon);

        Assert.assertEquals(size, ((BitmapDrawable) icon.drawable).getBitmap().getWidth());
        Assert.assertEquals(size, ((BitmapDrawable) icon.drawable).getBitmap().getHeight());
    }
}
