// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaValues_h
#define MediaValues_h

#include "core/css/MediaQueryEvaluator.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/rendering/style/RenderStyle.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;

class MediaValues : public RefCounted<MediaValues> {
public:
    enum MediaValuesMode { CachingMode,
        DynamicMode };

    enum PointerDeviceType { TouchPointer,
        MousePointer,
        NoPointer,
        UnknownPointer };


    static PassRefPtr<MediaValues> create(Document*, MediaValuesMode);
    static PassRefPtr<MediaValues> create(LocalFrame*, RenderStyle*, MediaValuesMode);
    static PassRefPtr<MediaValues> create(MediaValuesMode,
        int viewportWidth,
        int viewportHeight,
        int deviceWidth,
        int deviceHeight,
        float devicePixelRatio,
        int colorBitsPerComponent,
        int monochromeBitsPerComponent,
        PointerDeviceType,
        int defaultFontSize,
        bool threeDEnabled,
        bool scanMediaType,
        bool screenMediaType,
        bool printMediaType,
        bool strictMode);
    PassRefPtr<MediaValues> copy() const;
    bool isSafeToSendToAnotherThread() const;

    int viewportWidth() const;
    int viewportHeight() const;
    int deviceWidth() const;
    int deviceHeight() const;
    float devicePixelRatio() const;
    int colorBitsPerComponent() const;
    int monochromeBitsPerComponent() const;
    PointerDeviceType pointer() const;
    int defaultFontSize() const;
    bool threeDEnabled() const;
    bool scanMediaType() const;
    bool screenMediaType() const;
    bool printMediaType() const;
    bool strictMode() const;
    RenderStyle* style() const { return m_style.get(); }
    Document* document() const;

private:
    MediaValues(LocalFrame* frame, PassRefPtr<RenderStyle> style, MediaValuesMode mode)
        : m_style(style)
        , m_frame(frame)
        , m_mode(mode)
        , m_viewportWidth(0)
        , m_viewportHeight(0)
        , m_deviceWidth(0)
        , m_deviceHeight(0)
        , m_devicePixelRatio(0)
        , m_colorBitsPerComponent(0)
        , m_monochromeBitsPerComponent(0)
        , m_pointer(UnknownPointer)
        , m_defaultFontSize(0)
        , m_threeDEnabled(false)
        , m_scanMediaType(false)
        , m_screenMediaType(false)
        , m_printMediaType(false)
        , m_strictMode(false)
    {
    }

    RefPtr<RenderStyle> m_style;
    LocalFrame* m_frame;
    MediaValuesMode m_mode;

    // Members variables beyond this point must be thread safe, since they're copied to the parser thread
    int m_viewportWidth;
    int m_viewportHeight;
    int m_deviceWidth;
    int m_deviceHeight;
    float m_devicePixelRatio;
    int m_colorBitsPerComponent;
    int m_monochromeBitsPerComponent;
    PointerDeviceType m_pointer;
    int m_defaultFontSize;
    bool m_threeDEnabled;
    bool m_scanMediaType;
    bool m_screenMediaType;
    bool m_printMediaType;
    bool m_strictMode;
};

} // namespace
#endif
