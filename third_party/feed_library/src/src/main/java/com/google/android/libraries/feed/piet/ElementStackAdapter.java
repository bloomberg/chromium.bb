// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.piet.StyleProvider.DIMENSION_NOT_SET;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.piet.AdapterFactory.SingletonKeySupplier;
import com.google.search.now.ui.piet.ElementsProto.Content;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ElementStack;

import java.util.List;

/** Container adapter that stacks its children in a FrameLayout. */
class ElementStackAdapter extends ElementContainerAdapter<FrameLayout, ElementStack> {
    private static final String TAG = "ElementStackAdapter";

    private ElementStackAdapter(Context context, AdapterParameters parameters) {
        super(context, parameters, createView(context));
    }

    @Override
    public void onBindModel(ElementStack stack, Element baseElement, FrameContext frameContext) {
        super.onBindModel(stack, baseElement, frameContext);
        for (ElementAdapter<?, ?> childAdapter : childAdapters) {
            updateChildLayoutParams(childAdapter);
        }
    }

    private void updateChildLayoutParams(ElementAdapter<? extends View, ?> adapter) {
        FrameLayout.LayoutParams childParams = new FrameLayout.LayoutParams(0, 0);

        childParams.width = adapter.getComputedWidthPx() == DIMENSION_NOT_SET
                ? LayoutParams.WRAP_CONTENT
                : adapter.getComputedWidthPx();
        childParams.height = adapter.getComputedHeightPx() == DIMENSION_NOT_SET
                ? LayoutParams.WRAP_CONTENT
                : adapter.getComputedHeightPx();

        adapter.getElementStyle().applyMargins(getContext(), childParams);

        childParams.gravity = adapter.getGravity(Gravity.CENTER);

        adapter.setLayoutParams(childParams);
    }

    @Override
    List<Content> getContentsFromModel(ElementStack model) {
        return model.getContentsList();
    }

    @Override
    ElementStack getModelFromElement(Element baseElement) {
        return baseElement.getElementStack();
    }

    private static FrameLayout createView(Context context) {
        FrameLayout view = new FrameLayout(context);
        view.setLayoutParams(
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        return view;
    }

    static class KeySupplier extends SingletonKeySupplier<ElementStackAdapter, ElementStack> {
        @Override
        public String getAdapterTag() {
            return TAG;
        }

        @Override
        public ElementStackAdapter getAdapter(Context context, AdapterParameters parameters) {
            return new ElementStackAdapter(context, parameters);
        }
    }
}
