// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.Context;
import android.text.Html;
import android.text.Spanned;
import android.text.style.CharacterStyle;
import android.text.style.ParagraphStyle;
import android.text.style.UpdateAppearance;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.ui.R;
import org.chromium.ui.widget.Toast;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

/**
 * Simple proxy that provides C++ code with an access pathway to the Android clipboard.
 */
@JNINamespace("ui")
public class Clipboard implements ClipboardManager.OnPrimaryClipChangedListener {
    private static Clipboard sInstance;

    private static final String TAG = "Clipboard";

    // Necessary for coercing clipboard contents to text if they require
    // access to network resources, etceteras (e.g., URI in clipboard)
    private final Context mContext;

    private final ClipboardManager mClipboardManager;

    // A message hasher that's used to hash clipboard contents so we can tell
    // when a clipboard changes without storing the full contents.
    private MessageDigest mMd5Hasher;
    // The hash of the current clipboard.
    // TODO(mpearson): unsuppress this warning once saving and restoring
    // the hash from prefs is added.
    @SuppressFBWarnings("URF_UNREAD_FIELD")
    private byte[] mClipboardMd5;
    // The time when the clipboard was last updated.  Set to 0 if unknown.
    private long mClipboardChangeTime;

    /**
     * Get the singleton Clipboard instance (creating it if needed).
     */
    @CalledByNative
    public static Clipboard getInstance() {
        if (sInstance == null) {
            sInstance = new Clipboard();
        }
        return sInstance;
    }

    private Clipboard() {
        mContext = ContextUtils.getApplicationContext();
        mClipboardManager =
                (ClipboardManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CLIPBOARD_SERVICE);
        mClipboardManager.addPrimaryClipChangedListener(this);
        try {
            mMd5Hasher = MessageDigest.getInstance("MD5");
            mClipboardMd5 = weakMd5Hash();
        } catch (NoSuchAlgorithmException e) {
            Log.e(TAG,
                    "Unable to construct MD5 MessageDigest: %s; assume "
                            + "clipboard last update time is start of epoch.",
                    e);
            mMd5Hasher = null;
            mClipboardMd5 = new byte[] {};
        }
        RecordHistogram.recordBooleanHistogram("Clipboard.ConstructedHasher", mMd5Hasher != null);
        mClipboardChangeTime = 0;
    }

    /**
     * Emulates the behavior of the now-deprecated
     * {@link android.text.ClipboardManager#getText()} by invoking
     * {@link android.content.ClipData.Item#coerceToText(Context)} on the first
     * item in the clipboard (if any) and returning the result as a string.
     * <p>
     * This is quite different than simply calling {@link Object#toString()} on
     * the clip; consumers of this API should familiarize themselves with the
     * process described in
     * {@link android.content.ClipData.Item#coerceToText(Context)} before using
     * this method.
     *
     * @return a string representation of the first item on the clipboard, if
     *         the clipboard currently has an item and coercion of the item into
     *         a string is possible; otherwise, <code>null</code>
     */
    @SuppressWarnings("javadoc")
    @CalledByNative
    public String getCoercedText() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug.com/654802 and b/31501780)
        try {
            return mClipboardManager.getPrimaryClip()
                    .getItemAt(0)
                    .coerceToText(mContext)
                    .toString();
        } catch (Exception e) {
            return null;
        }
    }

    // TODO(ctzsm): Remove this method after Android API is updated
    private boolean hasStyleSpan(Spanned spanned) {
        Class<?>[] styleClasses = {
                CharacterStyle.class, ParagraphStyle.class, UpdateAppearance.class};
        for (Class<?> clazz : styleClasses) {
            if (spanned.nextSpanTransition(-1, spanned.length(), clazz) < spanned.length()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Gets the HTML text of top item on the primary clip on the Android clipboard.
     *
     * @return a Java string with the html text if any, or null if there is no html
     *         text or no entries on the primary clip.
     */
    @CalledByNative
    public String getHTMLText() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug/654802 and b/31501780)
        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            ClipDescription description = clipData.getDescription();
            if (description.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML)) {
                return clipData.getItemAt(0).getHtmlText();
            }

            if (description.hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN)) {
                Spanned spanned = (Spanned) clipData.getItemAt(0).getText();
                if (hasStyleSpan(spanned)) {
                    return Html.toHtml(spanned);
                }
            }
        } catch (Exception e) {
            return null;
        }
        return null;
    }

    /**
     * Gets the time the clipboard content last changed.
     *
     * This is calculated according to the device's clock.  E.g., it continues
     * increasing when the device is suspended.  Likewise, it can be in the
     * future if the user's clock updated after this information was recorded.
     *
     * @return a Java long recording the last changed time in milliseconds since
     * epoch, or 0 if the time could not be determined.
     */
    @CalledByNative
    public long getClipboardContentChangeTimeInMillis() {
        return mClipboardChangeTime;
    }

    /**
     * Emulates the behavior of the now-deprecated
     * {@link android.text.ClipboardManager#setText(CharSequence)}, setting the
     * clipboard's current primary clip to a plain-text clip that consists of
     * the specified string.
     * @param text  will become the content of the clipboard's primary clip
     */
    @SuppressFBWarnings("UPM_UNCALLED_PRIVATE_METHOD")
    @CalledByNative
    public void setText(final String text) {
        setPrimaryClipNoException(ClipData.newPlainText("text", text));
    }

    /**
     * Writes HTML to the clipboard, together with a plain-text representation
     * of that very data.
     *
     * @param html  The HTML content to be pasted to the clipboard.
     * @param text  Plain-text representation of the HTML content.
     */
    @SuppressFBWarnings("UPM_UNCALLED_PRIVATE_METHOD")
    @CalledByNative
    private void setHTMLText(final String html, final String text) {
        setPrimaryClipNoException(ClipData.newHtmlText("html", text, html));
    }

    /**
     * Clears the Clipboard Primary clip.
     *
     */
    @SuppressFBWarnings("UPM_UNCALLED_PRIVATE_METHOD")
    @CalledByNative
    private void clear() {
        setPrimaryClipNoException(ClipData.newPlainText(null, null));
    }

    public void setPrimaryClipNoException(ClipData clip) {
        try {
            mClipboardManager.setPrimaryClip(clip);
        } catch (Exception ex) {
            // Ignore any exceptions here as certain devices have bugs and will fail.
            String text = mContext.getString(R.string.copy_to_clipboard_failure_message);
            Toast.makeText(mContext, text, Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * Updates mClipboardMd5 and mClipboardChangeTime when the clipboard updates.
     *
     * Implements OnPrimaryClipChangedListener to listen for clipboard updates.
     */
    @Override
    public void onPrimaryClipChanged() {
        if (mMd5Hasher == null) return;
        RecordUserAction.record("MobileClipboardChanged");
        mClipboardMd5 = weakMd5Hash();
        // Always update the clipboard change time even if the clipboard
        // content hasn't changed.  This is because if the user put something
        // in the clipboard recently (even if it was not necessary because it
        // was already there), that content should be considered recent.
        mClipboardChangeTime = System.currentTimeMillis();
    }

    /**
     * Returns a weak hash of getCoercedText().
     *
     * @return a Java byte[] with the weak hash.
     */
    private byte[] weakMd5Hash() {
        if (getCoercedText() == null) {
            return new byte[] {};
        }
        // Compute a hash consisting of the first 4 bytes of the MD5 hash of
        // getCoercedText().  This value is used to detect clipboard content
        // change. Keeping only 4 bytes is a privacy requirement to introduce
        // collision and allow deniability of having copied a given string.
        return Arrays.copyOfRange(mMd5Hasher.digest(getCoercedText().getBytes()), 0, 4);
    }
}
