// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi.DIMENSION_UNKNOWN;
import static com.google.android.libraries.feed.common.Validators.checkState;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import com.google.android.libraries.feed.piet.AdapterFactory.SingletonKeySupplier;
import com.google.android.libraries.feed.piet.ui.AspectRatioScalingImageView;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ImageElement;
import com.google.search.now.ui.piet.ElementsProto.Visibility;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.ImagesProto.ImageSource;

/** An {@link ElementAdapter} for {@code ImageElement} elements. */
class ImageElementAdapter extends ElementAdapter<AspectRatioScalingImageView, ImageElement> {
    private static final String TAG = "ImageElementAdapter";

    /*@Nullable*/ private LoadImageCallback currentlyLoadingImage;

    @VisibleForTesting
    ImageElementAdapter(Context context, AdapterParameters parameters) {
        super(context, parameters, createView(context), KeySupplier.SINGLETON_KEY);
    }

    @Override
    ImageElement getModelFromElement(Element baseElement) {
        if (!baseElement.hasImageElement()) {
            throw new PietFatalException(ErrorCode.ERR_MISSING_ELEMENT_CONTENTS,
                    String.format("Missing ImageElement; has %s", baseElement.getElementsCase()));
        }
        return baseElement.getImageElement();
    }

    @Override
    void onCreateAdapter(ImageElement model, Element baseElement, FrameContext frameContext) {
        StyleProvider style = getElementStyle();
        Context context = getContext();

        widthPx = style.getWidthSpecPx(context);

        if (style.hasHeight()) {
            heightPx = style.getHeightSpecPx(context);
        } else {
            // Defaults to a square when only the width is defined.
            // TODO: This is not cross-platform standard; should probably get rid of this.
            heightPx = widthPx > 0 ? widthPx : StyleProvider.DIMENSION_NOT_SET;
        }
    }

    @Override
    void onBindModel(ImageElement model, Element baseElement, FrameContext frameContext) {
        Image image;
        switch (model.getContentCase()) {
            case IMAGE:
                image = model.getImage();
                break;
            case IMAGE_BINDING:
                BindingValue binding = frameContext.getImageBindingValue(model.getImageBinding());
                if (!binding.hasImage()) {
                    if (model.getImageBinding().getIsOptional()) {
                        setVisibilityOnView(Visibility.GONE);
                        return;
                    } else {
                        throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                String.format(
                                        "Image binding %s had no content", binding.getBindingId()));
                    }
                }
                image = binding.getImage();
                break;
            default:
                throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                        String.format("Unsupported or missing content in ImageElement: %s",
                                model.getContentCase()));
        }

        if (getElementStyle().hasPreLoadFill()) {
            getBaseView().setImageDrawable(getElementStyle().createPreLoadFill());
        }

        image = frameContext.filterImageSourcesByMediaQueryCondition(image);

        getBaseView().setDefaultAspectRatio(getAspectRatio(image));

        checkState(currentlyLoadingImage == null, "An image loading callback exists; unbind first");
        Integer overlayColor = getElementStyle().hasColor() ? getElementStyle().getColor() : null;
        LoadImageCallback loadImageCallback = createLoadImageCallback(
                getElementStyle().getScaleType(), overlayColor, frameContext);
        currentlyLoadingImage = loadImageCallback;
        getParameters().hostProviders.getAssetProvider().getImage(image,
                convertDimensionForImageLoader(widthPx), convertDimensionForImageLoader(heightPx),
                loadImageCallback);
    }

    private int convertDimensionForImageLoader(int dimension) {
        return dimension < 0 ? DIMENSION_UNKNOWN : dimension;
    }

    @VisibleForTesting
    static float getAspectRatio(Image image) {
        for (ImageSource source : image.getSourcesList()) {
            if (source.getHeightPx() > 0 && source.getWidthPx() > 0) {
                return ((float) source.getWidthPx()) / source.getHeightPx();
            }
        }
        return 0.0f;
    }

    @Override
    void onUnbindModel() {
        if (currentlyLoadingImage != null) {
            currentlyLoadingImage.cancel();
            currentlyLoadingImage = null;
        }
        ImageView imageView = getBaseView();
        if (imageView != null) {
            imageView.setImageDrawable(null);
        }
    }

    @VisibleForTesting
    LoadImageCallback createLoadImageCallback(
            ScaleType scaleType, /*@Nullable*/ Integer overlayColor, FrameContext frameContext) {
        return new LoadImageCallback(getBaseView(), scaleType, overlayColor,
                getElementStyle().getFadeInImageOnLoad(), getParameters(), frameContext);
    }

    private static AspectRatioScalingImageView createView(Context context) {
        AspectRatioScalingImageView imageView = new AspectRatioScalingImageView(context);
        imageView.setCropToPadding(true);
        return imageView;
    }

    static class KeySupplier extends SingletonKeySupplier<ImageElementAdapter, ImageElement> {
        @Override
        public String getAdapterTag() {
            return TAG;
        }

        @Override
        public ImageElementAdapter getAdapter(Context context, AdapterParameters parameters) {
            return new ImageElementAdapter(context, parameters);
        }
    }
}
