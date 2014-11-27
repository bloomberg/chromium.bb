// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageEncoder_h
#define ImageEncoder_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

class SkBitmap;

namespace blink {

class PLATFORM_EXPORT ImageEncoder {
    WTF_MAKE_NONCOPYABLE(ImageEncoder);
public:
    class PLATFORM_EXPORT RawImageBytes {
        // This object should be passed as a const reference, which ensures the
        // lifetime of the underlying image data pointed to by |m_data|.
        WTF_MAKE_NONCOPYABLE(RawImageBytes);
    public:
        RawImageBytes(const IntSize& size, const void* data) : m_size(size), m_data(static_cast<const unsigned char*>(data)) { }
        IntSize size() const { return m_size; }
        const unsigned char* data() const { return m_data; }

    private:
        IntSize m_size;
        const unsigned char* m_data;
    };

    static String toDataURL(const SkBitmap&, const String& mimeType, const double* quality);
    static String toDataURL(const RawImageBytes&, const String& mimeType, const double* quality);
};

} // namespace blink

#endif // ImageEncoder_h
