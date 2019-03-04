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
#include <vector>

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
#include <third_party/blink/public/web/web_local_frame.h>
#include <skia/ext/platform_canvas.h>
#include <third_party/skia/include/core/SkDocument.h>
#include <third_party/skia/include/core/SkStream.h>
#include <pdf/pdf.h>
#include <ui/gfx/geometry/size.h>
#include <ui/events/blink/web_input_event.h>
#include <ui/aura/window.h>
#include <ui/aura/client/screen_position_client.h>
#include <components/printing/renderer/print_render_frame_helper.h>
#include <v8.h>

namespace blpwtk2 {

// This function is copied from
// content/browser/renderer_host/renderer_widget_host_view_event_handler.cc
gfx::PointF GetScreenLocationFromEvent(const ui::LocatedEvent& event)
{
    aura::Window* root =
        static_cast<aura::Window*>(event.target())->GetRootWindow();
    aura::client::ScreenPositionClient *spc =
        aura::client::GetScreenPositionClient(root);
    gfx::PointF screen_location(event.root_location());
    if (!spc) {
        return screen_location;
    }
    spc->ConvertPointToScreen(root, &screen_location);
    return screen_location;
}


void RendererUtil::handleInputEvents(content::RenderWidget *rw, const WebView::InputEvent *events, size_t eventsCount)
{
    for (size_t i=0; i < eventsCount; ++i) {
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
          rw->bbHandleInputEvent(blinkKeyboardEvent);
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
            blink::WebMouseEvent blinkMouseEvent = ui::MakeWebMouseEvent(
                    uiMouseEvent,
                    base::Bind(&GetScreenLocationFromEvent));
            rw->bbHandleInputEvent(blinkMouseEvent);
        } break;

        case WM_MOUSEWHEEL: {
            ui::MouseWheelEvent uiMouseWheelEvent(msg);
            blink::WebMouseWheelEvent blinkMouseWheelEvent =
                ui::MakeWebMouseWheelEvent(uiMouseWheelEvent,
                                           base::Bind(&GetScreenLocationFromEvent));
            rw->bbHandleInputEvent(blinkMouseWheelEvent);
        } break;
        }
    }
}



// patch section: screen printing


// patch section: print to pdf



String RendererUtil::printToPDF(
    content::RenderView* renderView, const std::string& propertyName)
{
    blpwtk2::String returnVal;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);

    for (auto* frame = renderView->GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      if (auto* local_frame = frame->ToWebLocalFrame()) {
        v8::Local<v8::Context> jsContext =
            local_frame->MainWorldScriptContext();
        v8::Local<v8::Object> winObject = jsContext->Global();

        if (winObject->Has(
                v8::String::NewFromUtf8(isolate, propertyName.c_str()))) {
          std::vector<char> buffer = printing::PrintRenderFrameHelper::Get(
                                         renderView->GetMainRenderFrame())
                                         ->PrintToPDF(local_frame);
          returnVal.assign(buffer.data(), buffer.size());
          break;
        }
      }
    }

    return returnVal;
}

}  // close namespace blpwtk2

// vim: ts=4 et

