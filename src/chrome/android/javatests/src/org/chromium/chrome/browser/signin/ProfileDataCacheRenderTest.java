// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.support.annotation.Px;
import android.support.test.filters.MediumTest;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.widget.ChromeImageView;

import java.io.IOException;

/**
 * Tests for ProfileDataCache image scaling. Leverages RenderTest instead of reimplementing
 * bitmap comparison to simplify access to the compared images on buildbots (via result_details).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ProfileDataCacheRenderTest extends DummyUiActivityTestCase {
    // Anything different than logo_avatar_anonymous size should work.
    // TODO(https://crbug.com/967770): Make parameterized and test scaling to different sizes.
    private static final @Px int IMAGE_SIZE = 64;

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    private FrameLayout mContentView;
    private ImageView mImageView;
    private FakeProfileDataSource mProfileDataSource;
    private ProfileDataCache mProfileDataCache;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            mContentView = new FrameLayout(activity);
            mImageView = new ChromeImageView(activity);
            mContentView.addView(mImageView, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            activity.setContentView(mContentView);

            mProfileDataSource = new FakeProfileDataSource();
            mProfileDataCache =
                    new ProfileDataCache(getActivity(), IMAGE_SIZE, null, mProfileDataSource);
            // ProfileDataCache only populates the cache when an observer is added.
            mProfileDataCache.addObserver(accountId -> {});
        });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPlaceholderIsScaled() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String accountName = "no.data.for.this.account@example.com";
            DisplayableProfileData displayableProfileData =
                    mProfileDataCache.getProfileDataOrDefault(accountName);
            Drawable placeholderImage = displayableProfileData.getImage();
            assertEquals(IMAGE_SIZE, placeholderImage.getIntrinsicHeight());
            assertEquals(IMAGE_SIZE, placeholderImage.getIntrinsicWidth());
            mImageView.setImageDrawable(placeholderImage);
        });

        mRenderTestRule.render(mImageView, "profile_data_cache_placeholder");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAvatarIsScaled() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String accountName = "test@example.com";
            ProfileDataSource.ProfileData profileData = new ProfileDataSource.ProfileData(
                    accountName, createAvatar(), "Full Name", "Given Name");
            mProfileDataSource.setProfileData(accountName, profileData);

            DisplayableProfileData displayableProfileData =
                    mProfileDataCache.getProfileDataOrDefault(accountName);
            Drawable placeholderImage = displayableProfileData.getImage();
            assertEquals(IMAGE_SIZE, placeholderImage.getIntrinsicHeight());
            assertEquals(IMAGE_SIZE, placeholderImage.getIntrinsicWidth());
            mImageView.setImageDrawable(placeholderImage);
        });

        mRenderTestRule.render(mImageView, "profile_data_cache_avatar");
    }

    /**
     * Creates a simple dummy bitmap to use as the avatar picture. The avatar is intentionally
     * asymmetric to test scaling.
     */
    private Bitmap createAvatar() {
        final int avatarSize = 100;
        assertNotEquals("Should be different to test scaling", IMAGE_SIZE, avatarSize);

        Bitmap result = Bitmap.createBitmap(avatarSize, avatarSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(result);
        canvas.drawColor(Color.RED);

        Paint paint = new Paint();
        paint.setAntiAlias(true);

        paint.setColor(Color.BLUE);
        canvas.drawCircle(0, 0, avatarSize, paint);

        return result;
    }
}
