// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransferableImageBitmap_h
#define TransferableImageBitmap_h

#include "bindings/core/v8/Transferable.h"
#include "core/CoreExport.h"
#include "core/frame/ImageBitmap.h"

namespace blink {

class CORE_EXPORT TransferableImageBitmap : public Transferable {
public:
    ~TransferableImageBitmap() override { }
    void append(ImageBitmap* imageBitmap)
    {
        m_imageBitmapArray.append(imageBitmap);
    }
    bool contains(ImageBitmap* imageBitmap)
    {
        return m_imageBitmapArray.contains(imageBitmap);
    }
    static TransferableImageBitmap* ensure(TransferableArray& transferables)
    {
        if (transferables.size() <= TransferableImageBitmapType) {
            transferables.resize(TransferableImageBitmapType + 1);
            transferables[TransferableImageBitmapType] = new TransferableImageBitmap();
        }
        return static_cast<TransferableImageBitmap*>(transferables[TransferableImageBitmapType].get());
    }
    static TransferableImageBitmap* get(TransferableArray& transferables)
    {
        if (transferables.size() <= TransferableImageBitmapType)
            return nullptr;
        return static_cast<TransferableImageBitmap*>(transferables[TransferableImageBitmapType].get());
    }
    HeapVector<Member<ImageBitmap>, 1>& getArray()
    {
        return m_imageBitmapArray;
    }
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_imageBitmapArray);
        Transferable::trace(visitor);
    }
private:
    HeapVector<Member<ImageBitmap>, 1> m_imageBitmapArray;
};

} // namespace blink

#endif // TransferableImageBitmap_h
