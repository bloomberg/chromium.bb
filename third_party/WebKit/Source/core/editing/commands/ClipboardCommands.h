/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Igalia S.L.
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

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipboardCommands_h
#define ClipboardCommands_h

#include "core/editing/Forward.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DocumentFragment;
class Element;
class Event;
class LocalFrame;
class Pasteboard;

enum class DataTransferAccessPolicy;
enum class EditorCommandSource;
enum class PasteMode;

// This class provides static functions about commands related to clipboard.
class ClipboardCommands {
  STATIC_ONLY(ClipboardCommands);

 public:
  static bool EnabledCopy(LocalFrame&, Event*, EditorCommandSource);
  static bool EnabledCut(LocalFrame&, Event*, EditorCommandSource);
  static bool EnabledPaste(LocalFrame&, Event*, EditorCommandSource);

  // Returns |bool| value for Document#execCommand().
  static bool ExecuteCopy(LocalFrame&,
                          Event*,
                          EditorCommandSource,
                          const String&);
  static bool ExecuteCut(LocalFrame&,
                         Event*,
                         EditorCommandSource,
                         const String&);
  static bool ExecutePaste(LocalFrame&,
                           Event*,
                           EditorCommandSource,
                           const String&);
  static bool ExecutePasteGlobalSelection(LocalFrame&,
                                          Event*,
                                          EditorCommandSource,
                                          const String&);
  static bool ExecutePasteAndMatchStyle(LocalFrame&,
                                        Event*,
                                        EditorCommandSource,
                                        const String&);

  static bool PasteSupported(LocalFrame*);

  static bool CanReadClipboard(LocalFrame&, EditorCommandSource);
  static bool CanWriteClipboard(LocalFrame&, EditorCommandSource);

 private:
  static bool CanSmartReplaceWithPasteboard(LocalFrame&, Pasteboard*);
  static bool CanDeleteRange(const EphemeralRange&);
  static Element* FindEventTargetForClipboardEvent(LocalFrame&,
                                                   EditorCommandSource);

  // Returns true if Editor should continue with default processing.
  static bool DispatchClipboardEvent(LocalFrame&,
                                     const AtomicString&,
                                     DataTransferAccessPolicy,
                                     EditorCommandSource,
                                     PasteMode);
  static bool DispatchCopyOrCutEvent(LocalFrame&,
                                     EditorCommandSource,
                                     const AtomicString&);
  static bool DispatchPasteEvent(LocalFrame&, PasteMode, EditorCommandSource);

  static void WriteSelectionToPasteboard(LocalFrame&);
  static void Paste(LocalFrame&, EditorCommandSource);
  static void PasteAsFragment(LocalFrame&,
                              DocumentFragment*,
                              bool smart_replace,
                              bool match_style,
                              EditorCommandSource);
  static void PasteAsPlainTextWithPasteboard(LocalFrame&,
                                             Pasteboard*,
                                             EditorCommandSource);
  static void PasteWithPasteboard(LocalFrame&,
                                  Pasteboard*,
                                  EditorCommandSource);

  using FragmentAndPlainText = std::pair<DocumentFragment*, const bool>;
  static FragmentAndPlainText GetFragmentFromClipboard(LocalFrame&,
                                                       Pasteboard*);
};

}  // namespace blink

#endif
