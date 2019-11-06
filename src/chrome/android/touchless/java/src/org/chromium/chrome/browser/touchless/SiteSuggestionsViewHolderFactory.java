// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * ViewHolderFactory for touchless suggestion tiles.
 */
public class SiteSuggestionsViewHolderFactory implements RecyclerViewAdapter.ViewHolderFactory<
        SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder> {
    /**
     * Actual ViewHolder for a touchless suggestion.
     */
    public static class SiteSuggestionsViewHolder extends RecyclerView.ViewHolder {
        public SiteSuggestionsViewHolder(View view) {
            super(view);
        }
    }

    @Override
    public SiteSuggestionsViewHolder createViewHolder(ViewGroup parent, int viewType) {
        return new SiteSuggestionsViewHolder(
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touchless_suggestions_tile_view, parent, false));
    }
}
