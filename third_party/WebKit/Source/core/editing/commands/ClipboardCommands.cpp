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

#include "core/editing/commands/ClipboardCommands.h"

#include "core/clipboard/DataTransferAccessPolicy.h"
#include "core/clipboard/Pasteboard.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/commands/EditingCommandsUtilities.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/ClipboardEvent.h"
#include "core/events/TextEvent.h"
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/forms/TextControlElement.h"
#include "platform/PasteMode.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

bool ClipboardCommands::CanReadClipboard(LocalFrame& frame,
                                         EditorCommandSource source) {
  if (source == EditorCommandSource::kMenuOrKeyBinding)
    return true;
  Settings* const settings = frame.GetSettings();
  const bool default_value = settings &&
                             settings->GetJavaScriptCanAccessClipboard() &&
                             settings->GetDOMPasteAllowed();
  if (!frame.GetContentSettingsClient())
    return default_value;
  return frame.GetContentSettingsClient()->AllowReadFromClipboard(
      default_value);
}

bool ClipboardCommands::CanWriteClipboard(LocalFrame& frame,
                                          EditorCommandSource source) {
  if (source == EditorCommandSource::kMenuOrKeyBinding)
    return true;
  Settings* const settings = frame.GetSettings();
  const bool default_value =
      (settings && settings->GetJavaScriptCanAccessClipboard()) ||
      Frame::HasTransientUserActivation(&frame);
  if (!frame.GetContentSettingsClient())
    return default_value;
  return frame.GetContentSettingsClient()->AllowWriteToClipboard(default_value);
}

bool ClipboardCommands::CanSmartReplaceWithPasteboard(LocalFrame& frame,
                                                      Pasteboard* pasteboard) {
  return frame.GetEditor().SmartInsertDeleteEnabled() &&
         pasteboard->CanSmartReplace();
}

Element* ClipboardCommands::FindEventTargetForClipboardEvent(
    LocalFrame& frame,
    EditorCommandSource source) {
  // https://www.w3.org/TR/clipboard-apis/#fire-a-clipboard-event says:
  //  "Set target to be the element that contains the start of the selection in
  //   document order, or the body element if there is no selection or cursor."
  // We treat hidden selections as "no selection or cursor".
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      frame.Selection().IsHidden())
    return frame.Selection().GetDocument().body();

  return FindEventTargetFrom(
      frame, frame.Selection().ComputeVisibleSelectionInDOMTree());
}

// Returns true if Editor should continue with default processing.
bool ClipboardCommands::DispatchClipboardEvent(LocalFrame& frame,
                                               const AtomicString& event_type,
                                               DataTransferAccessPolicy policy,
                                               EditorCommandSource source,
                                               PasteMode paste_mode) {
  Element* const target = FindEventTargetForClipboardEvent(frame, source);
  if (!target)
    return true;

  DataTransfer* const data_transfer =
      DataTransfer::Create(DataTransfer::kCopyAndPaste, policy,
                           policy == DataTransferAccessPolicy::kWritable
                               ? DataObject::Create()
                               : DataObject::CreateFromPasteboard(paste_mode));

  Event* const evt = ClipboardEvent::Create(event_type, data_transfer);
  target->DispatchEvent(evt);
  const bool no_default_processing = evt->defaultPrevented();
  if (no_default_processing && policy == DataTransferAccessPolicy::kWritable) {
    Pasteboard::GeneralPasteboard()->WriteDataObject(
        data_transfer->GetDataObject());
  }

  // Invalidate clipboard here for security.
  data_transfer->SetAccessPolicy(DataTransferAccessPolicy::kNumb);
  return !no_default_processing;
}

bool ClipboardCommands::DispatchCopyOrCutEvent(LocalFrame& frame,
                                               EditorCommandSource source,
                                               const AtomicString& event_type) {
  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (IsInPasswordField(
          frame.Selection().ComputeVisibleSelectionInDOMTree().Start()))
    return true;

  return DispatchClipboardEvent(frame, event_type,
                                DataTransferAccessPolicy::kWritable, source,
                                PasteMode::kAllMimeTypes);
}

bool ClipboardCommands::DispatchPasteEvent(LocalFrame& frame,
                                           PasteMode paste_mode,
                                           EditorCommandSource source) {
  return DispatchClipboardEvent(frame, EventTypeNames::paste,
                                DataTransferAccessPolicy::kReadable, source,
                                paste_mode);
}

// WinIE uses onbeforecut and onbeforepaste to enables the cut and paste menu
// items. They also send onbeforecopy, apparently for symmetry, but it doesn't
// affect the menu items. We need to use onbeforecopy as a real menu enabler
// because we allow elements that are not normally selectable to implement
// copy/paste (like divs, or a document body).

bool ClipboardCommands::EnabledCopy(LocalFrame& frame,
                                    Event*,
                                    EditorCommandSource source) {
  if (!CanWriteClipboard(frame, source))
    return false;
  return !DispatchCopyOrCutEvent(frame, source, EventTypeNames::beforecopy) ||
         frame.GetEditor().CanCopy();
}

bool ClipboardCommands::EnabledCut(LocalFrame& frame,
                                   Event*,
                                   EditorCommandSource source) {
  if (!CanWriteClipboard(frame, source))
    return false;
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;
  return !DispatchCopyOrCutEvent(frame, source, EventTypeNames::beforecut) ||
         frame.GetEditor().CanCut();
}

bool ClipboardCommands::EnabledPaste(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource source) {
  if (!CanReadClipboard(frame, source))
    return false;
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;
  return frame.GetEditor().CanPaste();
}

static Pasteboard::SmartReplaceOption GetSmartReplaceOption(
    const LocalFrame& frame) {
  if (frame.GetEditor().SmartInsertDeleteEnabled() &&
      frame.Selection().Granularity() == TextGranularity::kWord)
    return Pasteboard::kCanSmartReplace;
  return Pasteboard::kCannotSmartReplace;
}

void ClipboardCommands::WriteSelectionToPasteboard(LocalFrame& frame) {
  const KURL& url = frame.GetDocument()->Url();
  const String html = frame.Selection().SelectedHTMLForClipboard();
  const String plain_text = frame.SelectedTextForClipboard();
  Pasteboard::GeneralPasteboard()->WriteHTML(html, url, plain_text,
                                             GetSmartReplaceOption(frame));
}

bool ClipboardCommands::PasteSupported(LocalFrame* frame) {
  const Settings* const settings = frame->GetSettings();
  const bool default_value = settings &&
                             settings->GetJavaScriptCanAccessClipboard() &&
                             settings->GetDOMPasteAllowed();
  if (!frame->GetContentSettingsClient())
    return default_value;
  return frame->GetContentSettingsClient()->AllowReadFromClipboard(
      default_value);
}

bool ClipboardCommands::ExecuteCopy(LocalFrame& frame,
                                    Event*,
                                    EditorCommandSource source,
                                    const String&) {
  if (!DispatchCopyOrCutEvent(frame, source, EventTypeNames::copy))
    return true;
  if (!frame.GetEditor().CanCopy())
    return true;

  // Since copy is a read-only operation it succeeds anytime a selection
  // is *visible*. In contrast to cut or paste, the selection does not
  // need to be focused - being visible is enough.
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      frame.Selection().IsHidden())
    return true;

  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // A 'copy' event handler might have dirtied the layout so we need to update
  // before we obtain the selection.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (EnclosingTextControl(
          frame.Selection().ComputeVisibleSelectionInDOMTree().Start())) {
    Pasteboard::GeneralPasteboard()->WritePlainText(
        frame.SelectedTextForClipboard(), GetSmartReplaceOption(frame));
    return true;
  }
  const Document* const document = frame.GetDocument();
  if (HTMLImageElement* image_element =
          ImageElementFromImageDocument(document)) {
    WriteImageNodeToPasteboard(Pasteboard::GeneralPasteboard(), *image_element,
                               document->title());
    return true;
  }
  WriteSelectionToPasteboard(frame);
  return true;
}

bool ClipboardCommands::CanDeleteRange(const EphemeralRange& range) {
  if (range.IsCollapsed())
    return false;

  const Node& start_container = *range.StartPosition().ComputeContainerNode();
  const Node& end_container = *range.EndPosition().ComputeContainerNode();

  return HasEditableStyle(start_container) && HasEditableStyle(end_container);
}

static DeleteMode ConvertSmartReplaceOptionToDeleteMode(
    Pasteboard::SmartReplaceOption smart_replace_option) {
  if (smart_replace_option == Pasteboard::kCanSmartReplace)
    return DeleteMode::kSmart;
  DCHECK_EQ(smart_replace_option, Pasteboard::kCannotSmartReplace);
  return DeleteMode::kSimple;
}

bool ClipboardCommands::ExecuteCut(LocalFrame& frame,
                                   Event*,
                                   EditorCommandSource source,
                                   const String&) {
  if (!DispatchCopyOrCutEvent(frame, source, EventTypeNames::cut))
    return true;
  if (!frame.GetEditor().CanCut())
    return true;

  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // A 'cut' event handler might have dirtied the layout so we need to update
  // before we obtain the selection.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return true;

  if (!CanDeleteRange(frame.GetEditor().SelectedRange()))
    return true;
  if (EnclosingTextControl(
          frame.Selection().ComputeVisibleSelectionInDOMTree().Start())) {
    const String plain_text = frame.SelectedTextForClipboard();
    Pasteboard::GeneralPasteboard()->WritePlainText(
        plain_text, GetSmartReplaceOption(frame));
  } else {
    WriteSelectionToPasteboard(frame);
  }

  if (source == EditorCommandSource::kMenuOrKeyBinding) {
    if (DispatchBeforeInputDataTransfer(
            FindEventTargetForClipboardEvent(frame, source),
            InputEvent::InputType::kDeleteByCut,
            nullptr) != DispatchEventResult::kNotCanceled)
      return true;
    // 'beforeinput' event handler may destroy target frame.
    if (frame.GetDocument()->GetFrame() != frame)
      return true;
  }
  frame.GetEditor().DeleteSelectionWithSmartDelete(
      ConvertSmartReplaceOptionToDeleteMode(GetSmartReplaceOption(frame)),
      InputEvent::InputType::kDeleteByCut);

  return true;
}

void ClipboardCommands::PasteAsFragment(LocalFrame& frame,
                                        DocumentFragment* pasting_fragment,
                                        bool smart_replace,
                                        bool match_style,
                                        EditorCommandSource source) {
  Element* const target = FindEventTargetForClipboardEvent(frame, source);
  if (!target)
    return;
  target->DispatchEvent(TextEvent::CreateForFragmentPaste(
      frame.DomWindow(), pasting_fragment, smart_replace, match_style));
}

void ClipboardCommands::PasteAsPlainTextWithPasteboard(
    LocalFrame& frame,
    Pasteboard* pasteboard,
    EditorCommandSource source) {
  Element* const target = FindEventTargetForClipboardEvent(frame, source);
  if (!target)
    return;
  target->DispatchEvent(TextEvent::CreateForPlainTextPaste(
      frame.DomWindow(), pasteboard->PlainText(),
      CanSmartReplaceWithPasteboard(frame, pasteboard)));
}

ClipboardCommands::FragmentAndPlainText
ClipboardCommands::GetFragmentFromClipboard(LocalFrame& frame,
                                            Pasteboard* pasteboard) {
  DocumentFragment* fragment = nullptr;
  if (pasteboard->IsHTMLAvailable()) {
    unsigned fragment_start = 0;
    unsigned fragment_end = 0;
    KURL url;
    const String markup =
        pasteboard->ReadHTML(url, fragment_start, fragment_end);
    if (!markup.IsEmpty()) {
      DCHECK(frame.GetDocument());
      fragment = CreateFragmentFromMarkupWithContext(
          *frame.GetDocument(), markup, fragment_start, fragment_end, url,
          kDisallowScriptingAndPluginContent);
    }
  }
  if (fragment)
    return std::make_pair(fragment, false);

  const String text = pasteboard->PlainText();
  if (text.IsEmpty())
    return std::make_pair(fragment, false);

  // TODO(editing-dev): Use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. See http://crbug.com/590369 for more details.
  // |SelectedRange| requires clean layout for visible selection
  // normalization.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  fragment = CreateFragmentFromText(frame.GetEditor().SelectedRange(), text);
  return std::make_pair(fragment, true);
}

void ClipboardCommands::PasteWithPasteboard(LocalFrame& frame,
                                            Pasteboard* pasteboard,
                                            EditorCommandSource source) {
  const ClipboardCommands::FragmentAndPlainText fragment_and_plain_text =
      GetFragmentFromClipboard(frame, pasteboard);

  if (!fragment_and_plain_text.first)
    return;

  PasteAsFragment(frame, fragment_and_plain_text.first,
                  CanSmartReplaceWithPasteboard(frame, pasteboard),
                  fragment_and_plain_text.second, source);
}

void ClipboardCommands::Paste(LocalFrame& frame, EditorCommandSource source) {
  DCHECK(frame.GetDocument());
  if (!DispatchPasteEvent(frame, PasteMode::kAllMimeTypes, source))
    return;
  if (!frame.GetEditor().CanPaste())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // A 'paste' event handler might have dirtied the layout so we need to update
  // before we obtain the selection.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return;

  ResourceFetcher* const loader = frame.GetDocument()->Fetcher();
  ResourceCacheValidationSuppressor validation_suppressor(loader);

  const PasteMode paste_mode = frame.GetEditor().CanEditRichly()
                                   ? PasteMode::kAllMimeTypes
                                   : PasteMode::kPlainTextOnly;

  if (source == EditorCommandSource::kMenuOrKeyBinding) {
    DataTransfer* data_transfer = DataTransfer::Create(
        DataTransfer::kCopyAndPaste, DataTransferAccessPolicy::kReadable,
        DataObject::CreateFromPasteboard(paste_mode));

    if (DispatchBeforeInputDataTransfer(
            FindEventTargetForClipboardEvent(frame, source),
            InputEvent::InputType::kInsertFromPaste,
            data_transfer) != DispatchEventResult::kNotCanceled)
      return;
    // 'beforeinput' event handler may destroy target frame.
    if (frame.GetDocument()->GetFrame() != frame)
      return;
  }

  if (paste_mode == PasteMode::kAllMimeTypes) {
    PasteWithPasteboard(frame, Pasteboard::GeneralPasteboard(), source);
    return;
  }
  PasteAsPlainTextWithPasteboard(frame, Pasteboard::GeneralPasteboard(),
                                 source);
}

bool ClipboardCommands::ExecutePaste(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource source,
                                     const String&) {
  Paste(frame, source);
  return true;
}

bool ClipboardCommands::ExecutePasteGlobalSelection(LocalFrame& frame,
                                                    Event*,
                                                    EditorCommandSource source,
                                                    const String&) {
  if (!frame.GetEditor().Behavior().SupportsGlobalSelection())
    return false;
  DCHECK_EQ(source, EditorCommandSource::kMenuOrKeyBinding);

  const bool old_selection_mode =
      Pasteboard::GeneralPasteboard()->IsSelectionMode();
  Pasteboard::GeneralPasteboard()->SetSelectionMode(true);
  Paste(frame, source);
  Pasteboard::GeneralPasteboard()->SetSelectionMode(old_selection_mode);
  return true;
}

bool ClipboardCommands::ExecutePasteAndMatchStyle(LocalFrame& frame,
                                                  Event*,
                                                  EditorCommandSource source,
                                                  const String&) {
  if (!DispatchPasteEvent(frame, PasteMode::kPlainTextOnly, source))
    return false;
  if (!frame.GetEditor().CanPaste())
    return false;

  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // A 'paste' event handler might have dirtied the layout so we need to update
  // before we obtain the selection.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;

  PasteAsPlainTextWithPasteboard(frame, Pasteboard::GeneralPasteboard(),
                                 source);
  return true;
}

}  // namespace blink
