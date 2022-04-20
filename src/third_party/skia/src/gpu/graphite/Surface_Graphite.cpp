/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/Surface_Graphite.h"

#include "include/gpu/graphite/Recorder.h"
#include "include/gpu/graphite/SkStuff.h"
#include "src/gpu/graphite/Device.h"
#include "src/gpu/graphite/Image_Graphite.h"

namespace skgpu::graphite {

Surface::Surface(sk_sp<Device> device)
        : SkSurface_Base(device->width(), device->height(), &device->surfaceProps())
        , fDevice(std::move(device)) {
}

Surface::~Surface() {}

Recorder* Surface::onGetRecorder() {
    return fDevice->recorder();
}

SkCanvas* Surface::onNewCanvas() { return new SkCanvas(fDevice); }

sk_sp<SkSurface> Surface::onNewSurface(const SkImageInfo& ii) {
    return MakeGraphite(fDevice->recorder(), ii);
}

sk_sp<SkImage> Surface::onNewImageSnapshot(const SkIRect* subset) {
    SkImageInfo ii = subset ? this->imageInfo().makeDimensions(subset->size())
                            : this->imageInfo();

    // TODO: create a real proxy view
    sk_sp<TextureProxy> proxy(new TextureProxy(ii.dimensions(), {}));
    TextureProxyView tpv(std::move(proxy));

    return sk_sp<Image>(new Image(tpv, ii.colorInfo()));
}

void Surface::onWritePixels(const SkPixmap& pixmap, int x, int y) {
    fDevice->writePixels(pixmap, x, y);
}

bool Surface::onCopyOnWrite(ContentChangeMode) { return true; }

bool Surface::onReadPixels(Context* context,
                           Recorder* recorder,
                           const SkPixmap& dst,
                           int srcX,
                           int srcY) {
    return fDevice->readPixels(context, recorder, dst, srcX, srcY);
}

} // namespace skgpu::graphite
