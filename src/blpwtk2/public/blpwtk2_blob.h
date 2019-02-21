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

#ifndef INCLUDED_BLPWTK2_BLOB_H
#define INCLUDED_BLPWTK2_BLOB_H

#include <blpwtk2_config.h>

#ifdef BLPWTK2_IMPLEMENTATION
#include <third_party/skia/include/core/SkStream.h>
#include <third_party/skia/include/core/SkBitmap.h>
#include <services/network/public/cpp/data_element.h>

#endif // BLPWTK2_IMPLEMENTATION

namespace blpwtk2 {

// This class is a representation of a binary blob.  The purpose of this class
// is to provide a way to transfer a binary blob from blpwtk2 to the embedder.
class BLPWTK2_EXPORT Blob {
  public:
    class Impl;

    Blob() : d_impl(0) {}
    ~Blob();

    void copyTo(void *dest) const;
    size_t size() const;

#ifdef BLPWTK2_IMPLEMENTATION
    SkDynamicMemoryWStream& makeSkStream();
    SkBitmap& makeSkBitmap();
    void makeStorageDataElement(const network::DataElement& element);
#endif  // BLPWTK2_IMPLEMENTATION

  private:
    Impl *d_impl;

    Blob(const Blob& original);
    Blob& operator=(const Blob& original);
};

}  // close namespace blpwtk2
#endif  // INCLUDED_BLPWTK2_BLOB_H

// vim: ts=4 et

