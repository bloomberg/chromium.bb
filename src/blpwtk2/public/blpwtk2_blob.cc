/*
 * Copyright (C) 2015 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_blob.h>
#include <base/logging.h>  // for DCHECK
#include <cstring>

namespace blpwtk2 {

class Blob::Impl {
  public:
    virtual ~Impl() {}
    virtual void copyTo(void *dest) const = 0;
    virtual size_t size() const = 0;
};

Blob::~Blob()
{
    if (d_impl)
        delete d_impl;
}

void Blob::copyTo(void *dest) const
{
    DCHECK(d_impl);
    return d_impl->copyTo(dest);
}

size_t Blob::size() const
{
    DCHECK(d_impl);
    return d_impl->size();
}

// SkiaStreamBlob

class SkiaStreamBlob : public Blob::Impl {
    SkDynamicMemoryWStream d_skiaStream;

  public:
    SkDynamicMemoryWStream& skiaStream()
    {
        return d_skiaStream;
    }

    void copyTo(void *dest) const override
    {
        d_skiaStream.copyTo(dest);
    }

    size_t size() const override
    {
        return d_skiaStream.bytesWritten();
    }
};

SkDynamicMemoryWStream& Blob::makeSkStream()
{
    DCHECK(!d_impl);
    SkiaStreamBlob *blob = new SkiaStreamBlob();
    d_impl = blob;
    return blob->skiaStream();
}


// SkiaBitmapBlob

class SkiaBitmapBlob : public Blob::Impl {
    SkBitmap d_skiaBitmap;

public:
    SkBitmap& skiaBitmap()
    {
        return d_skiaBitmap;
    }

    void copyTo(void *dest) const override
    {
        BITMAPFILEHEADER fileHeader = {
            0x4d42,
            sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + size(),
            0,
            0,
            sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)
        };

        BITMAPINFOHEADER bmiHeader = {
            sizeof(BITMAPINFOHEADER),
            d_skiaBitmap.width(),
            -d_skiaBitmap.height(),
            1,
            32,
            BI_RGB,
        };

        char* dest_buffer = reinterpret_cast<char*>(dest);
        size_t offset = 0;

        memcpy(dest_buffer, &fileHeader, sizeof(fileHeader));
        offset += sizeof(fileHeader);

        memcpy(dest_buffer + offset, &bmiHeader, sizeof(bmiHeader));
        offset += sizeof(bmiHeader);

        const void* src_pixels = d_skiaBitmap.getPixels();
        size_t src_size = d_skiaBitmap.computeByteSize();
        std::memcpy(dest_buffer + offset, src_pixels, src_size);
    }

    size_t size() const override
    {
        return sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + d_skiaBitmap.computeByteSize();
    }
};

SkBitmap& Blob::makeSkBitmap()
{
    DCHECK(!d_impl);
    SkiaBitmapBlob *blob = new SkiaBitmapBlob();
    d_impl = blob;
    return blob->skiaBitmap();
}

// StorageDataElementBlob

class StorageDataElementBlob : public Blob::Impl {
    const network::DataElement& d_element;

public:
    StorageDataElementBlob(const network::DataElement& element)
    : d_element(element) {
    }

    void copyTo(void *dest) const override
    {
        std::memcpy(dest, d_element.bytes(), d_element.length());
    }

    size_t size() const override
    {
        return d_element.length();
    }
};

void Blob::makeStorageDataElement(const network::DataElement& element)
{
    DCHECK(!d_impl);
    StorageDataElementBlob *blob = new StorageDataElementBlob(element);
    d_impl = blob;
}

}  // close namespace blpwtk2

// vim: ts=4 et

