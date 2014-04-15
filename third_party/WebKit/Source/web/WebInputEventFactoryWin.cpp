/*
 * Copyright (C) 2006-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebInputEventFactory.h"

#include "WebInputEvent.h"

#include "wtf/Assertions.h"

namespace blink {

// WebKeyboardEvent -----------------------------------------------------------

static bool isKeyDown(WPARAM wparam)
{
    return GetKeyState(wparam) & 0x8000;
}

static int getLocationModifier(WPARAM wparam, LPARAM lparam)
{
    int modifier = 0;
    switch (wparam) {
    case VK_RETURN:
        if ((lparam >> 16) & KF_EXTENDED)
            modifier = WebInputEvent::IsKeyPad;
        break;
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
        if (!((lparam >> 16) & KF_EXTENDED))
            modifier = WebInputEvent::IsKeyPad;
        break;
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
    case VK_DIVIDE:
    case VK_MULTIPLY:
    case VK_SUBTRACT:
    case VK_ADD:
    case VK_DECIMAL:
    case VK_CLEAR:
        modifier = WebInputEvent::IsKeyPad;
        break;
    case VK_SHIFT:
        if (isKeyDown(VK_LSHIFT))
            modifier = WebInputEvent::IsLeft;
        else if (isKeyDown(VK_RSHIFT))
            modifier = WebInputEvent::IsRight;
        break;
    case VK_CONTROL:
        if (isKeyDown(VK_LCONTROL))
            modifier = WebInputEvent::IsLeft;
        else if (isKeyDown(VK_RCONTROL))
            modifier = WebInputEvent::IsRight;
        break;
    case VK_MENU:
        if (isKeyDown(VK_LMENU))
            modifier = WebInputEvent::IsLeft;
        else if (isKeyDown(VK_RMENU))
            modifier = WebInputEvent::IsRight;
        break;
    case VK_LWIN:
        modifier = WebInputEvent::IsLeft;
        break;
    case VK_RWIN:
        modifier = WebInputEvent::IsRight;
        break;
    }

    ASSERT(!modifier
           || modifier == WebInputEvent::IsKeyPad
           || modifier == WebInputEvent::IsLeft
           || modifier == WebInputEvent::IsRight);
    return modifier;
}

// Loads the state for toggle keys into the event.
static void SetToggleKeyState(WebInputEvent* event)
{
    // Low bit set from GetKeyState indicates "toggled".
    if (::GetKeyState(VK_NUMLOCK) & 1)
        event->modifiers |= WebInputEvent::NumLockOn;
    if (::GetKeyState(VK_CAPITAL) & 1)
        event->modifiers |= WebInputEvent::CapsLockOn;
}

WebKeyboardEvent WebInputEventFactory::keyboardEvent(HWND hwnd, UINT message,
                                                     WPARAM wparam, LPARAM lparam)
{
    WebKeyboardEvent result;

    // TODO(pkasting): http://b/1117926 Are we guaranteed that the message that
    // GetMessageTime() refers to is the same one that we're passed in? Perhaps
    // one of the construction parameters should be the time passed by the
    // caller, who would know for sure.
    result.timeStampSeconds = GetMessageTime() / 1000.0;

    result.windowsKeyCode = static_cast<int>(wparam);
    // Record the scan code (along with other context bits) for this key event.
    result.nativeKeyCode = static_cast<int>(lparam);

    switch (message) {
    case WM_SYSKEYDOWN:
        result.isSystemKey = true;
    case WM_KEYDOWN:
        result.type = WebInputEvent::RawKeyDown;
        break;
    case WM_SYSKEYUP:
        result.isSystemKey = true;
    case WM_KEYUP:
        result.type = WebInputEvent::KeyUp;
        break;
    case WM_IME_CHAR:
        result.type = WebInputEvent::Char;
        break;
    case WM_SYSCHAR:
        result.isSystemKey = true;
        result.type = WebInputEvent::Char;
    case WM_CHAR:
        result.type = WebInputEvent::Char;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (result.type == WebInputEvent::Char || result.type == WebInputEvent::RawKeyDown) {
        result.text[0] = result.windowsKeyCode;
        result.unmodifiedText[0] = result.windowsKeyCode;
    }
    if (result.type != WebInputEvent::Char)
        result.setKeyIdentifierFromWindowsKeyCode();

    if (GetKeyState(VK_SHIFT) & 0x8000)
        result.modifiers |= WebInputEvent::ShiftKey;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        result.modifiers |= WebInputEvent::ControlKey;
    if (GetKeyState(VK_MENU) & 0x8000)
        result.modifiers |= WebInputEvent::AltKey;
    // NOTE: There doesn't seem to be a way to query the mouse button state in
    // this case.

    if (LOWORD(lparam) > 1)
        result.modifiers |= WebInputEvent::IsAutoRepeat;

    result.modifiers |= getLocationModifier(wparam, lparam);

    SetToggleKeyState(&result);
    return result;
}

bool WebInputEventFactory::isSystemKeyEvent(const WebKeyboardEvent& event)
{
    // According to MSDN:
    // http://msdn.microsoft.com/en-us/library/ms646286(VS.85).aspx
    // Key events with Alt modifier and F10 are system key events.
    // We just emulate this behavior. It's necessary to prevent webkit from
    // processing keypress event generated by alt-d, etc.
    return event.modifiers & WebInputEvent::AltKey || event.windowsKeyCode == VK_F10;
}

} // namespace blink
