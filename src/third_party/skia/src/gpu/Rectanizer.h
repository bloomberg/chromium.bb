/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_Rectanizer_DEFINED
#define skgpu_Rectanizer_DEFINED

#include "include/gpu/GrTypes.h"

struct SkIPoint16;

namespace skgpu {

class Rectanizer {
public:
    Rectanizer(int width, int height) : fWidth(width), fHeight(height) {
        SkASSERT(width >= 0);
        SkASSERT(height >= 0);
    }

    virtual ~Rectanizer() {}

    virtual void reset() = 0;

    int width() const { return fWidth; }
    int height() const { return fHeight; }

    // Attempt to add a rect. Return true on success; false on failure. If
    // successful the position in the atlas is returned in 'loc'.
    virtual bool addRect(int width, int height, SkIPoint16* loc) = 0;
    virtual float percentFull() const = 0;

    /**
     *  Our factory, which returns the subclass du jour
     */
    static Rectanizer* Factory(int width, int height);

private:
    const int fWidth;
    const int fHeight;
};

} // End of namespace skgpu

#endif
