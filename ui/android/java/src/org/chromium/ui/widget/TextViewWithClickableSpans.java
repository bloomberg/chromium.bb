// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;
import android.widget.TextView;

/**
 * ClickableSpan isn't accessible by default, so we create a simple subclass
 * of TextView that addresses this by adding click and longpress handlers.
 * If there's only one ClickableSpan, we activate it. If there's more than
 * one, we pop up a Spinner to disambiguate.
 */
public class TextViewWithClickableSpans extends TextView {
    public TextViewWithClickableSpans(Context context) {
        super(context);
        init();
    }

    public TextViewWithClickableSpans(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public TextViewWithClickableSpans(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                handleClick();
            }
        });
        setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                openDisambiguationMenu();
                return true;
            }
        });
    }

    private ClickableSpan[] getClickableSpans() {
        CharSequence text = getText();
        if (!(text instanceof SpannableString)) return null;

        SpannableString spannable = (SpannableString) text;
        return spannable.getSpans(0, spannable.length(), ClickableSpan.class);
    }

    private void handleClick() {
        ClickableSpan[] clickableSpans = getClickableSpans();
        if (clickableSpans == null || clickableSpans.length == 0) {
            return;
        } else if (clickableSpans.length == 1) {
            clickableSpans[0].onClick(this);
        } else {
            openDisambiguationMenu();
        }
    }

    private void openDisambiguationMenu() {
        ClickableSpan[] clickableSpans = getClickableSpans();
        if (clickableSpans == null || clickableSpans.length == 0)
            return;

        SpannableString spannable = (SpannableString) getText();
        PopupMenu popup = new PopupMenu(getContext(), this);
        Menu menu = popup.getMenu();
        for (final ClickableSpan clickableSpan : clickableSpans) {
            CharSequence itemText = spannable.subSequence(
                    spannable.getSpanStart(clickableSpan),
                    spannable.getSpanEnd(clickableSpan));
            MenuItem menuItem = menu.add(itemText);
            menuItem.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
                @Override
                public boolean onMenuItemClick(MenuItem menuItem) {
                    clickableSpan.onClick(TextViewWithClickableSpans.this);
                    return true;
                }
            });
        }

        popup.show();
    }
}
