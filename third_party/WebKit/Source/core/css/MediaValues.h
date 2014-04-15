// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaValues_h
#define MediaValues_h

#include "core/frame/LocalFrame.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Document;
class RenderStyle;
class CSSPrimitiveValue;

class MediaValues : public RefCounted<MediaValues> {
public:

    enum MediaValuesMode {
        CachingMode,
        DynamicMode
    };

    enum PointerDeviceType {
        TouchPointer,
        MousePointer,
        NoPointer,
        UnknownPointer
    };

    virtual ~MediaValues() { }

    virtual PassRefPtr<MediaValues> copy() const = 0;
    virtual bool isSafeToSendToAnotherThread() const = 0;
    virtual bool computeLength(double value, unsigned short type, int& result) const = 0;

    virtual int viewportWidth() const = 0;
    virtual int viewportHeight() const = 0;
    virtual int deviceWidth() const = 0;
    virtual int deviceHeight() const = 0;
    virtual float devicePixelRatio() const = 0;
    virtual int colorBitsPerComponent() const = 0;
    virtual int monochromeBitsPerComponent() const = 0;
    virtual PointerDeviceType pointer() const = 0;
    virtual bool threeDEnabled() const = 0;
    virtual bool scanMediaType() const = 0;
    virtual bool screenMediaType() const = 0;
    virtual bool printMediaType() const = 0;
    virtual bool strictMode() const = 0;
    virtual Document* document() const = 0;
    virtual bool hasValues() const = 0;

protected:
    static Document* getExecutingDocument(Document&);

    int calculateViewportWidth(LocalFrame*, RenderStyle*) const;
    int calculateViewportHeight(LocalFrame*, RenderStyle*) const;
    int calculateDeviceWidth(LocalFrame*) const;
    int calculateDeviceHeight(LocalFrame*) const;
    bool calculateStrictMode(LocalFrame*) const;
    float calculateDevicePixelRatio(LocalFrame*) const;
    int calculateColorBitsPerComponent(LocalFrame*) const;
    int calculateMonochromeBitsPerComponent(LocalFrame*) const;
    int calculateDefaultFontSize(RenderStyle*) const;
    int calculateComputedFontSize(RenderStyle*) const;
    bool calculateHasXHeight(RenderStyle*) const;
    double calculateXHeight(RenderStyle*) const;
    double calculateZeroWidth(RenderStyle*) const;
    bool calculateScanMediaType(LocalFrame*) const;
    bool calculateScreenMediaType(LocalFrame*) const;
    bool calculatePrintMediaType(LocalFrame*) const;
    bool calculateThreeDEnabled(LocalFrame*) const;
    float calculateEffectiveZoom(RenderStyle*) const;
    MediaValues::PointerDeviceType calculateLeastCapablePrimaryPointerDeviceType(LocalFrame*) const;

};

} // namespace

#endif // MediaValues_h
