/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "gm.h"
#include "sk_tool_utils.h"
#include "SkColorPriv.h"
#include "SkShader.h"
#include "SkCanvas.h"
#include "SkUTF.h"

namespace skiagm {

static SkBitmap make_bitmap() {
    SkBitmap bm;
    bm.allocN32Pixels(1, 1);
    *bm.getAddr32(0, 0) = SkPackARGB32(0x80, 0x80, 0, 0);
    return bm;
}

class TinyBitmapGM : public GM {
public:
    TinyBitmapGM() {
        this->setBGColor(0xFFDDDDDD);
    }

protected:
    SkString onShortName() {
        return SkString("tinybitmap");
    }

    virtual SkISize onISize() { return SkISize::Make(100, 100); }

    virtual void onDraw(SkCanvas* canvas) {
        SkBitmap bm = make_bitmap();
        SkPaint paint;
        paint.setAlpha(0x80);
        paint.setShader(SkShader::MakeBitmapShader(bm, SkShader::kRepeat_TileMode,
                                                   SkShader::kMirror_TileMode));
        canvas->drawPaint(paint);
    }

private:
    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

static GM* MyFactory(void*) { return new TinyBitmapGM; }
static GMRegistry reg(MyFactory);

}
