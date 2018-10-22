// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.text;

import android.support.annotation.Nullable;
import android.text.SpannableString;

import java.util.Arrays;

/**
 * Applies spans to an HTML-looking string and returns the resulting SpannableString.
 * Note: This does not support duplicate, nested or overlapping spans.
 *
 * Example:
 *
 *   String input = "Click to view the <tos>terms of service</tos> or <pn>privacy notice</pn>";
 *   ClickableSpan tosSpan = ...;
 *   ClickableSpan privacySpan = ...;
 *   SpannableString output = SpanApplier.applySpans(input,
 *           new Span("<tos>", "</tos>", tosSpan), new Span("<pn>", "</pn>", privacySpan));
 */
public class SpanApplier {

    /**
     * Associates a span with the range of text between a start and an end tag.
     */
    public static final class SpanInfo implements Comparable<SpanInfo> {
        final String mStartTag;
        final String mEndTag;
        final @Nullable Object mSpan;
        int mStartTagIndex;
        int mEndTagIndex;

        /**
         * @param startTag The start tag, e.g. "<tos>".
         * @param endTag The end tag, e.g. "</tos>".
         * @param span The span to apply to the text between the start and end tags. May be null,
         *         then SpanApplier will just remove start and end tags without applying any span.
         */
        public SpanInfo(String startTag, String endTag, @Nullable Object span) {
            mStartTag = startTag;
            mEndTag = endTag;
            mSpan = span;
        }

        @Override
        public int compareTo(SpanInfo other) {
            return this.mStartTagIndex < other.mStartTagIndex ? -1
                    : (this.mStartTagIndex == other.mStartTagIndex ? 0 : 1);
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof SpanInfo)) return false;

            return compareTo((SpanInfo) other) == 0;
        }

        @Override
        public int hashCode() {
            return 0;
        }
    }

    /**
     * Applies spans to an HTML-looking string and returns the resulting SpannableString.
     * If a span cannot be applied (e.g. because the start tag isn't in the input string), then
     * a RuntimeException will be thrown.
     *
     * @param input The input string.
     * @param spans The Spans which will be applied to the string.
     * @return A SpannableString with the given spans applied.
     */
    public static SpannableString applySpans(String input, SpanInfo... spans) {
        for (SpanInfo span : spans) {
            span.mStartTagIndex = input.indexOf(span.mStartTag);
            span.mEndTagIndex = input.indexOf(span.mEndTag,
                    span.mStartTagIndex + span.mStartTag.length());
        }

        // Sort the spans from first to last.
        Arrays.sort(spans);

        // Copy the input text to the output, but omit the start and end tags.
        // Update startTagIndex and endTagIndex for each Span as we go.
        int inputIndex = 0;
        StringBuilder output = new StringBuilder(input.length());

        for (SpanInfo span : spans) {
            // Fail if there is a span without a start or end tag or if there are nested
            // or overlapping spans.
            if (span.mStartTagIndex == -1 || span.mEndTagIndex == -1
                    || span.mStartTagIndex < inputIndex) {
                span.mStartTagIndex = -1;
                String error = String.format("Input string is missing tags %s%s: %s",
                        span.mStartTag, span.mEndTag, input);
                throw new IllegalArgumentException(error);
            }

            output.append(input, inputIndex, span.mStartTagIndex);
            inputIndex = span.mStartTagIndex + span.mStartTag.length();
            span.mStartTagIndex = output.length();

            output.append(input, inputIndex, span.mEndTagIndex);
            inputIndex = span.mEndTagIndex + span.mEndTag.length();
            span.mEndTagIndex = output.length();
        }
        output.append(input, inputIndex, input.length());

        SpannableString spannableString = new SpannableString(output);
        for (SpanInfo span : spans) {
            if (span.mStartTagIndex != -1 && span.mSpan != null) {
                spannableString.setSpan(span.mSpan, span.mStartTagIndex, span.mEndTagIndex, 0);
            }
        }

        return spannableString;
    }
}

