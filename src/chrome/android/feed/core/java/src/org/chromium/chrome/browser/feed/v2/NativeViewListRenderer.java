// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ListContentManager;
import org.chromium.chrome.browser.xsurface.ListContentManagerObserver;

/**
 * Implementation of {@link HybridListRenderer} for list consisting all native views.
 */
public class NativeViewListRenderer extends RecyclerView.Adapter<NativeViewListRenderer.ViewHolder>
        implements HybridListRenderer, ListContentManagerObserver {
    /**
     * A ViewHolder for the underlying RecyclerView.
     */
    public static class ViewHolder extends RecyclerView.ViewHolder {
        ViewHolder(View v) {
            super(v);
        }
    }

    private final Context mContext;

    private ListContentManager mManager;
    private RecyclerView mView;

    public NativeViewListRenderer(Context mContext) {
        this.mContext = mContext;
    }

    /* RecyclerView.Adapter methods */
    @Override
    public int getItemCount() {
        return mManager.getItemCount();
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        if (mManager.isNativeView(position)) {
            mManager.bindNativeView(position, holder.itemView);
        }
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        // viewType is same as position.
        int position = viewType;
        if (mManager.isNativeView(position)) {
            View v = mManager.getNativeView(position, parent);
            return new ViewHolder(v);
        }
        return null;
    }

    @Override
    public int getItemViewType(int position) {
        return position;
    }

    /* HybridListRenderer methods */
    @Override
    public View bind(ListContentManager manager) {
        mManager = manager;
        mView = new RecyclerView(mContext);
        mView.setAdapter(this);
        mView.setLayoutManager(new LinearLayoutManager(mContext));
        mManager.addObserver(this);
        onItemRangeInserted(0, mManager.getItemCount());
        return mView;
    }

    @Override
    public void unbind() {
        mManager.removeObserver(this);
        onItemRangeRemoved(0, mManager.getItemCount());
        mManager = null;
    }

    @Override
    public void update(byte[] data) {}

    /* ListContentManagerObserver methods */
    @Override
    public void onItemRangeInserted(int startIndex, int count) {
        notifyItemRangeInserted(startIndex, count);
    }

    @Override
    public void onItemRangeRemoved(int startIndex, int count) {
        notifyItemRangeRemoved(startIndex, count);
        ;
    }

    @Override
    public void onItemRangeChanged(int startIndex, int count) {
        notifyItemRangeChanged(startIndex, count);
    }

    @Override
    public void onItemMoved(int curIndex, int newIndex) {
        notifyItemMoved(curIndex, newIndex);
    }
}
