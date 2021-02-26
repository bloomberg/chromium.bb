/*
* Copyright 2020 Google LLC
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#ifndef SkMatrixProvider_DEFINED
#define SkMatrixProvider_DEFINED

#include "include/core/SkM44.h"
#include "include/core/SkMatrix.h"

class SkMatrixProvider {
public:
    SkMatrixProvider(const SkMatrix& localToDevice)
        : fLocalToDevice(localToDevice)
        , fLocalToDevice33(localToDevice) {}

    SkMatrixProvider(const SkM44& localToDevice)
        : fLocalToDevice(localToDevice)
        , fLocalToDevice33(localToDevice.asM33()) {}

    virtual ~SkMatrixProvider() {}

    // These should return the "same" matrix, as either a 3x3 or 4x4. Most sites in Skia still
    // call localToDevice, and operate on SkMatrix.
    const SkMatrix& localToDevice() const { return fLocalToDevice33; }
    const SkM44& localToDevice44() const { return fLocalToDevice; }

    virtual bool getLocalToMarker(uint32_t id, SkM44* localToMarker) const = 0;

private:
    friend class SkBaseDevice;

    SkM44    fLocalToDevice;
    SkMatrix fLocalToDevice33;  // Cached SkMatrix version of above, for legacy usage
};

class SkOverrideDeviceMatrixProvider : public SkMatrixProvider {
public:
    SkOverrideDeviceMatrixProvider(const SkMatrixProvider& parent, const SkMatrix& localToDevice)
        : SkMatrixProvider(localToDevice)
        , fParent(parent) {}

    bool getLocalToMarker(uint32_t id, SkM44* localToMarker) const override {
        return fParent.getLocalToMarker(id, localToMarker);
    }

private:
    const SkMatrixProvider& fParent;
};

class SkPostTranslateMatrixProvider : public SkMatrixProvider {
public:
    SkPostTranslateMatrixProvider(const SkMatrixProvider& parent, SkScalar dx, SkScalar dy)
            : SkMatrixProvider(SkM44::Translate(dx, dy) * parent.localToDevice44())
            , fParent(parent) {}

    // Assume that the post-translation doesn't apply to any marked matrices
    bool getLocalToMarker(uint32_t id, SkM44* localToMarker) const override {
        return fParent.getLocalToMarker(id, localToMarker);
    }

private:
    const SkMatrixProvider& fParent;
};

class SkPreConcatMatrixProvider : public SkMatrixProvider {
public:
    SkPreConcatMatrixProvider(const SkMatrixProvider& parent, const SkMatrix& preMatrix)
            : SkMatrixProvider(parent.localToDevice44() * SkM44(preMatrix))
            , fParent(parent)
            , fPreMatrix(preMatrix) {}

    bool getLocalToMarker(uint32_t id, SkM44* localToMarker) const override {
        if (fParent.getLocalToMarker(id, localToMarker)) {
            if (localToMarker) {
                localToMarker->preConcat(fPreMatrix);
            }
            return true;
        }
        return false;
    }

private:
    const SkMatrixProvider& fParent;
    const SkMatrix          fPreMatrix;
};

class SkSimpleMatrixProvider : public SkMatrixProvider {
public:
    SkSimpleMatrixProvider(const SkMatrix& localToDevice)
        : SkMatrixProvider(localToDevice) {}

    bool getLocalToMarker(uint32_t, SkM44*) const override { return false; }
};

#endif
