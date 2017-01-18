/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/dom/Document.h"

#include "core/editing/Editor.h"
#include "core/events/ScopedEventQueue.h"
#include "core/html/TextControlElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/Histogram.h"
#include "wtf/AutoReset.h"
#include "wtf/StdLibExtras.h"

namespace blink {

namespace {

Editor::Command command(Document* document, const String& commandName) {
  LocalFrame* frame = document->frame();
  if (!frame || frame->document() != document)
    return Editor::Command();

  document->updateStyleAndLayoutTree();
  return frame->editor().createCommand(commandName, CommandFromDOM);
}

}  // namespace

bool Document::execCommand(const String& commandName,
                           bool,
                           const String& value,
                           ExceptionState& exceptionState) {
  if (!isHTMLDocument() && !isXHTMLDocument()) {
    exceptionState.throwDOMException(
        InvalidStateError, "execCommand is only supported on HTML documents.");
    return false;
  }
  if (focusedElement() && isTextControlElement(*focusedElement()))
    UseCounter::count(*this, UseCounter::ExecCommandOnInputOrTextarea);

  // We don't allow recursive |execCommand()| to protect against attack code.
  // Recursive call of |execCommand()| could be happened by moving iframe
  // with script triggered by insertion, e.g. <iframe src="javascript:...">
  // <iframe onload="...">. This usage is valid as of the specification
  // although, it isn't common use case, rather it is used as attack code.
  if (m_isRunningExecCommand) {
    String message =
        "We don't execute document.execCommand() this time, because it is "
        "called recursively.";
    addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
    return false;
  }
  AutoReset<bool> executeScope(&m_isRunningExecCommand, true);

  // Postpone DOM mutation events, which can execute scripts and change
  // DOM tree against implementation assumption.
  EventQueueScope eventQueueScope;
  Editor::tidyUpHTMLStructure(*this);
  Editor::Command editorCommand = command(this, commandName);

  DEFINE_STATIC_LOCAL(SparseHistogram, editorCommandHistogram,
                      ("WebCore.Document.execCommand"));
  editorCommandHistogram.sample(editorCommand.idForHistogram());
  return editorCommand.execute(value);
}

bool Document::queryCommandEnabled(const String& commandName,
                                   ExceptionState& exceptionState) {
  if (!isHTMLDocument() && !isXHTMLDocument()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "queryCommandEnabled is only supported on HTML documents.");
    return false;
  }

  return command(this, commandName).isEnabled();
}

bool Document::queryCommandIndeterm(const String& commandName,
                                    ExceptionState& exceptionState) {
  if (!isHTMLDocument() && !isXHTMLDocument()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "queryCommandIndeterm is only supported on HTML documents.");
    return false;
  }

  return command(this, commandName).state() == MixedTriState;
}

bool Document::queryCommandState(const String& commandName,
                                 ExceptionState& exceptionState) {
  if (!isHTMLDocument() && !isXHTMLDocument()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "queryCommandState is only supported on HTML documents.");
    return false;
  }

  return command(this, commandName).state() == TrueTriState;
}

bool Document::queryCommandSupported(const String& commandName,
                                     ExceptionState& exceptionState) {
  if (!isHTMLDocument() && !isXHTMLDocument()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "queryCommandSupported is only supported on HTML documents.");
    return false;
  }

  return command(this, commandName).isSupported();
}

String Document::queryCommandValue(const String& commandName,
                                   ExceptionState& exceptionState) {
  if (!isHTMLDocument() && !isXHTMLDocument()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "queryCommandValue is only supported on HTML documents.");
    return "";
  }

  return command(this, commandName).value();
}

}  // namespace blink
