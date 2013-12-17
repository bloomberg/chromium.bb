/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/editing/Editor.h"

#include "core/events/KeyboardEvent.h"
#include "core/frame/Frame.h"
#include "core/page/EditorClient.h"
#include "platform/PlatformKeyboardEvent.h"

namespace WebCore {

bool Editor::handleEditingKeyboardEvent(KeyboardEvent* evt)
{
    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    // do not treat this as text input if it's a system key event
    if (!keyEvent || keyEvent->isSystemKey())
        return false;

    String commandName = behavior().interpretKeyEvent(*evt);
    Command command = this->command(commandName);

    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how
        // commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately
        // (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        if (command.isTextInsertion() || commandName.isEmpty())
            return false;
        if (command.execute(evt)) {
            client().didExecuteCommand(commandName);
            return true;
        }
        return false;
    }

    if (command.execute(evt)) {
        client().didExecuteCommand(commandName);
        return true;
    }

    // Here we need to filter key events.
    // On Gtk/Linux, it emits key events with ASCII text and ctrl on for ctrl-<x>.
    // In Webkit, EditorClient::handleKeyboardEvent in
    // WebKit/gtk/WebCoreSupport/EditorClientGtk.cpp drop such events.
    // On Mac, it emits key events with ASCII text and meta on for Command-<x>.
    // These key events should not emit text insert event.
    // Alt key would be used to insert alternative character, so we should let
    // through. Also note that Ctrl-Alt combination equals to AltGr key which is
    // also used to insert alternative character.
    // http://code.google.com/p/chromium/issues/detail?id=10846
    // Windows sets both alt and meta are on when "Alt" key pressed.
    // http://code.google.com/p/chromium/issues/detail?id=2215
    // Also, we should not rely on an assumption that keyboards don't
    // send ASCII characters when pressing a control key on Windows,
    // which may be configured to do it so by user.
    // See also http://en.wikipedia.org/wiki/Keyboard_Layout
    // FIXME(ukai): investigate more detail for various keyboard layout.
    if (evt->keyEvent()->text().length() == 1) {
        UChar ch = evt->keyEvent()->text()[0U];

        // Don't insert null or control characters as they can result in
        // unexpected behaviour
        if (ch < ' ')
            return false;
#if !OS(WIN)
        // Don't insert ASCII character if ctrl w/o alt or meta is on.
        // On Mac, we should ignore events when meta is on (Command-<x>).
        if (ch < 0x80) {
            if (evt->keyEvent()->ctrlKey() && !evt->keyEvent()->altKey())
                return false;
#if OS(MACOSX)
            if (evt->keyEvent()->metaKey())
            return false;
#endif
        }
#endif
    }

    if (!canEdit())
        return false;

    return insertText(evt->keyEvent()->text(), evt);
}

void Editor::handleKeyboardEvent(KeyboardEvent* evt)
{
    // Give the embedder a chance to handle the keyboard event.
    if (client().handleKeyboardEvent() || handleEditingKeyboardEvent(evt))
        evt->setDefaultHandled();
}

} // namesace WebCore
