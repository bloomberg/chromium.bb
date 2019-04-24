/*
* Copyright 2016 Google Inc.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#ifndef Slide_DEFINED
#define Slide_DEFINED

#include "SkRefCnt.h"
#include "SkSize.h"
#include "SkString.h"
#include "sk_app/Window.h"

class SkCanvas;
class SkAnimTimer;
class SkMetaData;

class Slide : public SkRefCnt {
public:
    virtual ~Slide() {}

    virtual SkISize getDimensions() const = 0;

    virtual void draw(SkCanvas* canvas) = 0;
    virtual bool animate(const SkAnimTimer&) { return false;  }
    virtual void load(SkScalar winWidth, SkScalar winHeight) {}
    virtual void resize(SkScalar winWidth, SkScalar winHeight) {}
    virtual void unload() {}

    virtual bool onChar(SkUnichar c) { return false; }
    virtual bool onMouse(SkScalar x, SkScalar y, sk_app::Window::InputState state,
                         uint32_t modifiers) { return false; }

    virtual bool onGetControls(SkMetaData*) { return false; }
    virtual void onSetControls(const SkMetaData&) {}

    SkString getName() { return fName; }


protected:
    SkString    fName;
};


#endif
