// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InputEvent_h
#define InputEvent_h

#include "core/clipboard/DataTransfer.h"
#include "core/dom/Range.h"
#include "core/dom/StaticRange.h"
#include "core/events/InputEventInit.h"
#include "core/events/UIEvent.h"

namespace blink {

class InputEvent final : public UIEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static InputEvent* Create(const AtomicString& type,
                            const InputEventInit& initializer) {
    return new InputEvent(type, initializer);
  }

  // https://w3c.github.io/input-events/#h-interface-inputevent-attributes
  enum class InputType {
    kNone,
    // Insertion.
    kInsertText,
    kInsertLineBreak,
    kInsertParagraph,
    kInsertOrderedList,
    kInsertUnorderedList,
    kInsertHorizontalRule,
    kInsertFromPaste,
    kInsertFromDrop,
    kInsertFromYank,
    kInsertTranspose,
    kInsertReplacementText,
    kInsertCompositionText,
    // Deletion.
    kDeleteWordBackward,
    kDeleteWordForward,
    kDeleteSoftLineBackward,
    kDeleteSoftLineForward,
    kDeleteHardLineBackward,
    kDeleteHardLineForward,
    kDeleteContentBackward,
    kDeleteContentForward,
    kDeleteByCut,
    kDeleteByDrag,
    // History.
    kHistoryUndo,
    kHistoryRedo,
    // Formatting.
    kFormatBold,
    kFormatItalic,
    kFormatUnderline,
    kFormatStrikeThrough,
    kFormatSuperscript,
    kFormatSubscript,
    kFormatJustifyCenter,
    kFormatJustifyFull,
    kFormatJustifyRight,
    kFormatJustifyLeft,
    kFormatIndent,
    kFormatOutdent,
    kFormatRemove,
    kFormatSetBlockTextDirection,

    // Add new input types immediately above this line.
    kNumberOfInputTypes,
  };

  enum EventCancelable : bool {
    kNotCancelable = false,
    kIsCancelable = true,
  };

  enum EventIsComposing : bool {
    kNotComposing = false,
    kIsComposing = true,
  };

  static InputEvent* CreateBeforeInput(InputType,
                                       const String& data,
                                       EventCancelable,
                                       EventIsComposing,
                                       const StaticRangeVector*);
  static InputEvent* CreateBeforeInput(InputType,
                                       DataTransfer*,
                                       EventCancelable,
                                       EventIsComposing,
                                       const StaticRangeVector*);
  static InputEvent* CreateInput(InputType,
                                 const String& data,
                                 EventIsComposing,
                                 const StaticRangeVector*);

  String inputType() const;
  const String& data() const { return data_; }
  DataTransfer* dataTransfer() const { return data_transfer_.Get(); }
  bool isComposing() const { return is_composing_; }
  // Returns a copy of target ranges during event dispatch, and returns an empty
  // vector after dispatch.
  StaticRangeVector getTargetRanges() const;

  bool IsInputEvent() const override;

  DispatchEventResult DispatchEvent(EventDispatcher&) override;

  virtual void Trace(blink::Visitor*);

 private:
  InputEvent(const AtomicString&, const InputEventInit&);

  InputType input_type_;
  String data_;
  Member<DataTransfer> data_transfer_;
  bool is_composing_;

  // We have to stored |Range| internally and only expose |StaticRange|, please
  // see comments in |dispatchEvent()|.
  RangeVector ranges_;
};

DEFINE_EVENT_TYPE_CASTS(InputEvent);

}  // namespace blink

#endif  // InputEvent_h
