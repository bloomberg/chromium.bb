/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Sample.h"
#include "SkBitmap.h"
#include "SkBlurMask.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkGradientShader.h"
#include "SkMaskFilter.h"
#include "SkUTF.h"

class BlurView : public Sample {
    SkBitmap    fBM;
public:
    BlurView() {}

protected:
    virtual bool onQuery(Sample::Event* evt) {
        if (Sample::TitleQ(*evt)) {
            Sample::TitleR(evt, "Blur");
            return true;
        }
        return this->INHERITED::onQuery(evt);
    }

    void drawBG(SkCanvas* canvas) {
        canvas->drawColor(0xFFDDDDDD);
    }

    virtual void onDrawContent(SkCanvas* canvas) {
        drawBG(canvas);

        SkBlurStyle NONE = SkBlurStyle(-999);
        static const struct {
            SkBlurStyle fStyle;
            int         fCx, fCy;
        } gRecs[] = {
            { NONE,                                 0,  0 },
            { kInner_SkBlurStyle,  -1,  0 },
            { kNormal_SkBlurStyle,  0,  1 },
            { kSolid_SkBlurStyle,   0, -1 },
            { kOuter_SkBlurStyle,   1,  0 },
        };

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setTextSize(25);
        canvas->translate(-40, 0);

        paint.setColor(SK_ColorBLUE);
        for (size_t i = 0; i < SK_ARRAY_COUNT(gRecs); i++) {
            if (gRecs[i].fStyle != NONE) {
                paint.setMaskFilter(SkMaskFilter::MakeBlur(gRecs[i].fStyle,
                                    SkBlurMask::ConvertRadiusToSigma(SkIntToScalar(20))));
            } else {
                paint.setMaskFilter(nullptr);
            }
            canvas->drawCircle(200 + gRecs[i].fCx*100.f,
                               200 + gRecs[i].fCy*100.f, 50, paint);
        }
        // draw text
        {
            paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle,
                                                       SkBlurMask::ConvertRadiusToSigma(4)));
            SkScalar x = SkIntToScalar(70);
            SkScalar y = SkIntToScalar(400);
            paint.setColor(SK_ColorBLACK);
            canvas->drawString("Hamburgefons Style", x, y, paint);
            canvas->drawString("Hamburgefons Style", x, y + SkIntToScalar(50), paint);
            paint.setMaskFilter(nullptr);
            paint.setColor(SK_ColorWHITE);
            x -= SkIntToScalar(2);
            y -= SkIntToScalar(2);
            canvas->drawString("Hamburgefons Style", x, y, paint);
        }
    }

private:
    typedef Sample INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_SAMPLE( return new BlurView(); )
