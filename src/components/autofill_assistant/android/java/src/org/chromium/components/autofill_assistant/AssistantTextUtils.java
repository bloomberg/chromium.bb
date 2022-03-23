// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;
import android.content.Context;
import android.graphics.Typeface;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.StyleSpan;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Common text utilities used by autofill assistant.
 */
public class AssistantTextUtils {
    private static final String TAG = "AutofillAssistant";

    /** Bold tags of the form <b>...</b>. */
    private static final SpanApplier.SpanInfo BOLD_SPAN_INFO =
            new SpanApplier.SpanInfo("<b>", "</b>", new StyleSpan(android.graphics.Typeface.BOLD));
    /** Italic tags of the form <i>...</i>. */
    private static final SpanApplier.SpanInfo ITALIC_SPAN_INFO =
            new SpanApplier.SpanInfo("<i>", "</i>", new StyleSpan(Typeface.ITALIC));
    /** Links of the form <link0>...</link0>. */
    private static final Pattern LINK_START_PATTERN = Pattern.compile("<link(\\d+)>");

    /**
     * Sets the text of {@code view} by automatically applying all special appearance tags in {@code
     * text}.
     *
     * Special appearance tags are of the form <b></b> or <link0></link0>. For links, a callback can
     * be provided.
     */
    public static void applyVisualAppearanceTags(
            TextView view, String text, @Nullable Callback<Integer> linkCallback) {
        SpannableString textWithSpans =
                applyVisualAppearanceTags(text, view.getContext(), linkCallback);
        view.setText(textWithSpans);
        if (textWithSpans.getSpans(0, textWithSpans.length(), NoUnderlineClickableSpan.class).length
                != 0) {
            view.setMovementMethod(LinkMovementMethod.getInstance());
        }
    }

    /**
     * Returns a {@link SpannableString} version of {@code text}, with the appropriate appearance
     * tags.
     */
    public static SpannableString applyVisualAppearanceTags(
            String text, Context context, @Nullable Callback<Integer> linkCallback) {
        List<SpanApplier.SpanInfo> spans = new ArrayList<>();
        if (text.contains("<b>")) {
            spans.add(BOLD_SPAN_INFO);
        }
        if (text.contains("<i>")) {
            spans.add(ITALIC_SPAN_INFO);
        }

        // We first collect the link IDs into a set to allow multiple links with same ID.
        Set<Integer> linkIds = new HashSet<>();
        Matcher matcher = LINK_START_PATTERN.matcher(text);
        while (matcher.find()) {
            linkIds.add(Integer.parseInt(matcher.group(1)));
        }

        for (Integer linkId : linkIds) {
            spans.add(new SpanApplier.SpanInfo("<link" + linkId + ">", "</link" + linkId + ">",
                    new NoUnderlineClickableSpan(context, (unusedView) -> {
                        if (linkCallback != null) {
                            linkCallback.onResult(linkId);
                        }
                    })));
        }

        List<SpanApplier.SpanInfo> successfulSpans = new ArrayList<>();
        for (SpanApplier.SpanInfo info : spans) {
            try {
                SpanApplier.applySpans(text, info);

                // Apply successful. Add the info to the list.
                successfulSpans.add(info);
            } catch (IllegalArgumentException e) {
                Log.d(TAG, "Mismatching span", e);
            }
        }
        return SpanApplier.applySpans(
                text, successfulSpans.toArray(new SpanApplier.SpanInfo[successfulSpans.size()]));
    }
}
