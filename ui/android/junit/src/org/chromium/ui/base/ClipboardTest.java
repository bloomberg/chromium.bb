// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.content.Intent;
import android.text.SpannableString;
import android.text.style.RelativeSizeSpan;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests logic in the Clipboard class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClipboardTest {
    private static final String PLAIN_TEXT = "plain";
    private static final String HTML_TEXT = "<span style=\"color: red;\">HTML</span>";

    @BeforeClass
    public static void beforeClass() {
        RecordHistogram.setDisabledForTests(true);
        RecordUserAction.setDisabledForTests(true);
    }

    @AfterClass
    public static void afterClass() {
        RecordHistogram.setDisabledForTests(false);
        RecordUserAction.setDisabledForTests(false);
    }

    @Test
    public void testGetClipboardContentChangeTimeInMillis() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
        // Upon launch, the clipboard should be at start of epoch, i.e., ancient.
        Clipboard clipboard = Clipboard.getInstance();
        assertEquals(0, clipboard.getClipboardContentChangeTimeInMillis());
        // After updating the clipboard, it should have a new time.
        clipboard.onPrimaryClipChanged();
        assertTrue(clipboard.getClipboardContentChangeTimeInMillis() > 0);
    }

    @Test
    public void testGetHTMLTextAndGetCoercedText() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
        Clipboard clipboard = Clipboard.getInstance();

        // HTML text
        ClipData html = ClipData.newHtmlText("html", PLAIN_TEXT, HTML_TEXT);
        clipboard.setPrimaryClipNoException(html);
        assertEquals(PLAIN_TEXT, clipboard.getCoercedText());
        assertEquals(HTML_TEXT, clipboard.getHTMLText());

        // Plain text without span
        ClipData plainTextNoSpan = ClipData.newPlainText("plain", PLAIN_TEXT);
        clipboard.setPrimaryClipNoException(plainTextNoSpan);
        assertEquals(PLAIN_TEXT, clipboard.getCoercedText());
        assertNull(clipboard.getHTMLText());

        // Plain text with span
        SpannableString spanned = new SpannableString(PLAIN_TEXT);
        spanned.setSpan(new RelativeSizeSpan(2f), 0, 5, 0);
        ClipData plainTextSpan = ClipData.newPlainText("plain", spanned);
        clipboard.setPrimaryClipNoException(plainTextSpan);
        assertEquals(PLAIN_TEXT, clipboard.getCoercedText());
        assertNotNull(clipboard.getHTMLText());

        // Intent
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        ClipData intentClip = ClipData.newIntent("intent", intent);
        clipboard.setPrimaryClipNoException(intentClip);
        assertNotNull(clipboard.getCoercedText());
        assertNull(clipboard.getHTMLText());
    }
}
