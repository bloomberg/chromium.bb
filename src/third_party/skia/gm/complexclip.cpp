/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "sk_tool_utils.h"
#include "SkCanvas.h"
#include "SkPath.h"

namespace skiagm {

constexpr SkColor gPathColor = SK_ColorBLACK;
constexpr SkColor gClipAColor = SK_ColorBLUE;
constexpr SkColor gClipBColor = SK_ColorRED;

class ComplexClipGM : public GM {
public:
    ComplexClipGM(bool aaclip, bool saveLayer, bool invertDraw)
    : fDoAAClip(aaclip)
    , fDoSaveLayer(saveLayer)
    , fInvertDraw(invertDraw) {
        this->setBGColor(0xFFDEDFDE);
    }

protected:
    SkString onShortName() override {
        SkString str;
        str.printf("complexclip_%s%s%s",
                   fDoAAClip ? "aa" : "bw",
                   fDoSaveLayer ? "_layer" : "",
                   fInvertDraw ? "_invert" : "");
        return str;
    }

    SkISize onISize() override { return SkISize::Make(970, 780); }

    void onDraw(SkCanvas* canvas) override {
        SkPath path;
        path.moveTo(0,   50)
            .quadTo(0,   0,   50,  0)
            .lineTo(175, 0)
            .quadTo(200, 0,   200, 25)
            .lineTo(200, 150)
            .quadTo(200, 200, 150, 200)
            .lineTo(0,   200)
            .close()
            .moveTo(50,  50)
            .lineTo(150, 50)
            .lineTo(150, 125)
            .quadTo(150, 150, 125, 150)
            .lineTo(50,  150)
            .close();
        if (fInvertDraw) {
            path.setFillType(SkPath::kInverseEvenOdd_FillType);
        } else {
            path.setFillType(SkPath::kEvenOdd_FillType);
        }
        SkPaint pathPaint;
        pathPaint.setAntiAlias(true);
        pathPaint.setColor(gPathColor);

        SkPath clipA;
        clipA.addPoly({{10,  20}, {165, 22}, {70,  105}, {165, 177}, {-5,  180}}, false).close();

        SkPath clipB;
        clipB.addPoly({{40,  10}, {190, 15}, {195, 190}, {40,  185}, {155, 100}}, false).close();

        SkPaint paint;
        paint.setAntiAlias(true);
        sk_tool_utils::set_portable_typeface(&paint);
        paint.setTextSize(20);

        constexpr struct {
            SkClipOp fOp;
            const char*      fName;
        } gOps[] = { //extra spaces in names for measureText
            {kIntersect_SkClipOp,         "Isect "},
            {kDifference_SkClipOp,        "Diff " },
            {kUnion_SkClipOp,             "Union "},
            {kXOR_SkClipOp,               "Xor "  },
            {kReverseDifference_SkClipOp, "RDiff "}
        };

        canvas->translate(20, 20);
        canvas->scale(3 * SK_Scalar1 / 4, 3 * SK_Scalar1 / 4);

        if (fDoSaveLayer) {
            // We want the layer to appear symmetric relative to actual
            // device boundaries so we need to "undo" the effect of the
            // scale and translate
            SkRect bounds = SkRect::MakeLTRB(
              4.0f/3.0f * -20,
              4.0f/3.0f * -20,
              4.0f/3.0f * (this->getISize().fWidth - 20),
              4.0f/3.0f * (this->getISize().fHeight - 20));

            bounds.inset(100, 100);
            SkPaint boundPaint;
            boundPaint.setColor(SK_ColorRED);
            boundPaint.setStyle(SkPaint::kStroke_Style);
            canvas->drawRect(bounds, boundPaint);
            canvas->clipRect(bounds);
            canvas->saveLayer(&bounds, nullptr);
        }

        for (int invBits = 0; invBits < 4; ++invBits) {
            canvas->save();
            for (size_t op = 0; op < SK_ARRAY_COUNT(gOps); ++op) {
                this->drawHairlines(canvas, path, clipA, clipB);

                bool doInvA = SkToBool(invBits & 1);
                bool doInvB = SkToBool(invBits & 2);
                canvas->save();
                    // set clip
                    clipA.setFillType(doInvA ? SkPath::kInverseEvenOdd_FillType :
                                      SkPath::kEvenOdd_FillType);
                    clipB.setFillType(doInvB ? SkPath::kInverseEvenOdd_FillType :
                                      SkPath::kEvenOdd_FillType);
                    canvas->clipPath(clipA, fDoAAClip);
                    canvas->clipPath(clipB, gOps[op].fOp, fDoAAClip);

                    // In the inverse case we need to prevent the draw from covering the whole
                    // canvas.
                    if (fInvertDraw) {
                        SkRect rectClip = clipA.getBounds();
                        rectClip.join(path.getBounds());
                        rectClip.join(path.getBounds());
                        rectClip.outset(5, 5);
                        canvas->clipRect(rectClip);
                    }

                    // draw path clipped
                    canvas->drawPath(path, pathPaint);
                canvas->restore();


                SkScalar txtX = 45;
                paint.setColor(gClipAColor);
                const char* aTxt = doInvA ? "InvA " : "A ";
                canvas->drawString(aTxt, txtX, 220, paint);
                txtX += paint.measureText(aTxt, strlen(aTxt));
                paint.setColor(SK_ColorBLACK);
                canvas->drawString(gOps[op].fName, txtX, 220, paint);
                txtX += paint.measureText(gOps[op].fName, strlen(gOps[op].fName));
                paint.setColor(gClipBColor);
                const char* bTxt = doInvB ? "InvB " : "B ";
                canvas->drawString(bTxt, txtX, 220, paint);

                canvas->translate(250,0);
            }
            canvas->restore();
            canvas->translate(0, 250);
        }

        if (fDoSaveLayer) {
            canvas->restore();
        }
    }
private:
    void drawHairlines(SkCanvas* canvas, const SkPath& path,
                       const SkPath& clipA, const SkPath& clipB) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kStroke_Style);
        const SkAlpha fade = 0x33;

        // draw path in hairline
        paint.setColor(gPathColor); paint.setAlpha(fade);
        canvas->drawPath(path, paint);

        // draw clips in hair line
        paint.setColor(gClipAColor); paint.setAlpha(fade);
        canvas->drawPath(clipA, paint);
        paint.setColor(gClipBColor); paint.setAlpha(fade);
        canvas->drawPath(clipB, paint);
    }

    bool fDoAAClip;
    bool fDoSaveLayer;
    bool fInvertDraw;

    typedef GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM(return new ComplexClipGM(false, false, false);)
DEF_GM(return new ComplexClipGM(false, false, true);)
DEF_GM(return new ComplexClipGM(false, true, false);)
DEF_GM(return new ComplexClipGM(false, true, true);)
DEF_GM(return new ComplexClipGM(true, false, false);)
DEF_GM(return new ComplexClipGM(true, false, true);)
DEF_GM(return new ComplexClipGM(true, true, false);)
DEF_GM(return new ComplexClipGM(true, true, true);)
}
