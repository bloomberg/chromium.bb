// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_IME_INPUT_EVENT_DEV_H_
#define PPAPI_CPP_DEV_IME_INPUT_EVENT_DEV_H_

#include <utility>

#include "ppapi/c/dev/ppb_ime_input_event_dev.h"
#include "ppapi/cpp/input_event.h"

/// @file
/// This file defines the API used to handle IME input events.

namespace pp {

class Var;

class IMEInputEvent_Dev : public InputEvent {
 public:
  /// Constructs an is_null() IME input event object.
  IMEInputEvent_Dev();

  /// Constructs an IME input event object from the provided generic input
  /// event. If the given event is itself is_null() or is not an IME input
  /// event, the object will be is_null().
  ///
  /// @param[in] event A generic input event.
  explicit IMEInputEvent_Dev(const InputEvent& event);

  /// Returns the composition text as a UTF-8 string for the given IME event.
  ///
  /// @return A string var representing the composition text. For non-IME
  /// input events the return value will be an undefined var.
  Var GetText() const;

  /// Returns the number of segments in the composition text.
  ///
  /// @return The number of segments. For events other than COMPOSITION_UPDATE,
  /// returns 0.
  uint32_t GetSegmentNumber() const;

  /// Returns the start and the end position of the index-th segment in the
  /// composition text. The positions are given by byte-indices of the string
  /// GetText(). They always satisfy 0 <= .first < .second <= (Length of
  /// GetText()) and GetSegmentAt(index).first < GetSegmentAt(index+1).first.
  /// When the event is not COMPOSITION_UPDATE or index >= GetSegmentNumber(),
  /// returns (0, 0).
  ///
  /// @param[in] index An integer indicating a segment.
  ///
  /// @return A pair of integers representing the index-th segment.
  std::pair<uint32_t, uint32_t> GetSegmentAt(uint32_t index) const;

  /// Returns the index of the current target segment of composition.
  ///
  /// @return An integer indicating the index of the target segment. When there
  /// is no active target segment, or the event is not COMPOSITION_UPDATE,
  /// returns -1.
  int32_t GetTargetSegment() const;

  /// Returns the range selected by caret in the composition text.
  ///
  /// @return A pair of integers indicating the selection range.
  std::pair<uint32_t, uint32_t> GetSelection() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_IME_INPUT_EVENT_DEV_H_
