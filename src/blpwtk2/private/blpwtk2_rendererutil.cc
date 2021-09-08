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

#include <blpwtk2_rendererutil.h>
#include <blpwtk2_blob.h>
#include <blpwtk2_statics.h>

#include <base/logging.h>  // for DCHECK

#include <content/renderer/render_widget.h>
#include <content/public/browser/native_web_keyboard_event.h>
#include <ui/events/event.h>

#include <content/public/renderer/render_view.h>
#include <third_party/blink/public/web/web_view.h>
#include <third_party/blink/public/web/web_frame.h>
#include <skia/ext/platform_canvas.h>
#include <third_party/skia/include/core/SkDocument.h>
#include <third_party/skia/include/core/SkStream.h>
#include <pdf/pdf.h>
#include <ui/gfx/geometry/size.h>
#include <ui/events/blink/web_input_event.h>
#include <ui/aura/window.h>
#include <ui/aura/client/screen_position_client.h>

namespace blpwtk2 {

void RendererUtil::handleInputEvents(blink::WebWidget *widget, const WebView::InputEvent *events, size_t eventsCount)
{
    for (size_t i=0; i < eventsCount; ++i) {
        ui::LatencyInfo latency_info;

        const WebView::InputEvent *event = events + i;
        MSG msg = {
            event->hwnd,
            event->message,
            event->wparam,
            event->lparam,
            GetMessageTime()
        };

        switch (event->message) {
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYUP:
        case WM_IME_CHAR:
        case WM_SYSCHAR:
        case WM_CHAR: {
          ui::KeyEvent uiKeyboardEvent(msg);
          content::NativeWebKeyboardEvent blinkKeyboardEvent(&uiKeyboardEvent);

          constexpr int masktOutModifiers =
              ~(blink::WebInputEvent::kShiftKey |
                blink::WebInputEvent::kControlKey |
                blink::WebInputEvent::kAltKey |
                blink::WebInputEvent::kMetaKey |
                blink::WebInputEvent::kIsAutoRepeat |
                blink::WebInputEvent::kIsKeyPad |
                blink::WebInputEvent::kIsLeft |
                blink::WebInputEvent::kIsRight |
                blink::WebInputEvent::kNumLockOn |
                blink::WebInputEvent::kCapsLockOn);

          int modifiers = blinkKeyboardEvent.GetModifiers() & masktOutModifiers;

          if (event->shiftKey)
            modifiers |= blink::WebInputEvent::kShiftKey;

          if (event->controlKey)
            modifiers |= blink::WebInputEvent::kControlKey;

          if (event->altKey)
            modifiers |= blink::WebInputEvent::kAltKey;

          if (event->metaKey)
            modifiers |= blink::WebInputEvent::kMetaKey;

          if (event->isAutoRepeat)
            modifiers |= blink::WebInputEvent::kIsAutoRepeat;

          if (event->isKeyPad)
            modifiers |= blink::WebInputEvent::kIsKeyPad;

          if (event->isLeft)
            modifiers |= blink::WebInputEvent::kIsLeft;

          if (event->isRight)
            modifiers |= blink::WebInputEvent::kIsRight;

          if (event->numLockOn)
            modifiers |= blink::WebInputEvent::kNumLockOn;

          if (event->capsLockOn)
            modifiers |= blink::WebInputEvent::kCapsLockOn;

          blinkKeyboardEvent.SetModifiers(modifiers);
          widget->HandleInputEvent(blink::WebCoalescedInputEvent(blinkKeyboardEvent, latency_info));
        } break;

        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP: {
            ui::MouseEvent uiMouseEvent(msg);
            blink::WebMouseEvent blinkMouseEvent = 
                            ui::MakeWebMouseEvent(uiMouseEvent);
            widget->HandleInputEvent(blink::WebCoalescedInputEvent(blinkMouseEvent, latency_info));
        } break;

        case WM_MOUSEWHEEL: {
            ui::MouseWheelEvent uiMouseWheelEvent(msg);
            blink::WebMouseWheelEvent blinkMouseWheelEvent =
                   ui::MakeWebMouseWheelEvent(uiMouseWheelEvent);
            widget->HandleInputEvent(blink::WebCoalescedInputEvent(blinkMouseWheelEvent, latency_info));
        } break;
        }
    }
}



// patch section: screen printing


// patch section: print to pdf



}  // close namespace blpwtk2

// vim: ts=4 et

