// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/pending_user_input.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {
namespace scheduler {

typedef HashMap<AtomicString, PendingUserInputType> PendingInputTypeMap;

namespace {

void PopulatePendingInputTypeMap(PendingInputTypeMap& map) {
  map = {
      {"click", PendingUserInputType::kClick},
      {"dblclick", PendingUserInputType::kDblClick},
      {"mousedown", PendingUserInputType::kMouseDown},
      {"mouseenter", PendingUserInputType::kMouseEnter},
      {"mouseleave", PendingUserInputType::kMouseLeave},
      {"mousemove", PendingUserInputType::kMouseMove},
      {"mouseout", PendingUserInputType::kMouseOut},
      {"mouseover", PendingUserInputType::kMouseOver},
      {"mouseup", PendingUserInputType::kMouseUp},
      {"wheel", PendingUserInputType::kWheel},
      {"keydown", PendingUserInputType::kKeyDown},
      {"keyup", PendingUserInputType::kKeyUp},
      {"touchstart", PendingUserInputType::kTouchStart},
      {"touchend", PendingUserInputType::kTouchEnd},
      {"touchmove", PendingUserInputType::kTouchMove},
      {"touchcancel", PendingUserInputType::kTouchCancel},
  };
}

}  // namespace

void PendingUserInput::Monitor::OnEnqueue(WebInputEvent::Type type) {
  DCHECK_NE(type, WebInputEvent::kUndefined);
  DCHECK_LE(type, WebInputEvent::kTypeLast);

  size_t& counter = counters_[type];
  counter++;
}

void PendingUserInput::Monitor::OnDequeue(WebInputEvent::Type type) {
  DCHECK_NE(type, WebInputEvent::kUndefined);
  DCHECK_LE(type, WebInputEvent::kTypeLast);

  size_t& counter = counters_[type];
  DCHECK_GT(counter, size_t{0});
  counter--;
}

PendingUserInputInfo PendingUserInput::Monitor::Info() const {
  PendingUserInputType mask = PendingUserInputType::kNone;
  for (int type = WebInputEvent::kTypeFirst + 1;
       type <= WebInputEvent::kTypeLast; type++) {
    const size_t& counter = counters_[type];
    DCHECK_GE(counter, size_t{0});
    if (counter == 0)
      continue;

    mask = mask |
           TypeFromWebInputEventType(static_cast<WebInputEvent::Type>(type));
  }
  return PendingUserInputInfo(mask);
}

// static
PendingUserInputType PendingUserInput::TypeFromString(
    const AtomicString& pending_user_input_type) {
  DEFINE_STATIC_LOCAL(PendingInputTypeMap, kPendingInputTypeMap, ());
  if (kPendingInputTypeMap.IsEmpty()) {
    PopulatePendingInputTypeMap(kPendingInputTypeMap);
    DCHECK(!kPendingInputTypeMap.IsEmpty());
  }

  const auto type_iter = kPendingInputTypeMap.find(pending_user_input_type);
  if (type_iter != kPendingInputTypeMap.end())
    return type_iter->value;
  return PendingUserInputType::kNone;
}

// static
PendingUserInputType PendingUserInput::TypeFromWebInputEventType(
    WebInputEvent::Type type) {
  using Type = PendingUserInputType;
  switch (type) {
    case WebInputEvent::Type::kMouseDown:
      return Type::kMouseDown;
    case WebInputEvent::Type::kMouseUp:
      return Type::kClick | Type::kDblClick | Type::kMouseUp;
    case WebInputEvent::Type::kMouseMove:
      return Type::kMouseMove;
    case WebInputEvent::Type::kMouseEnter:
      return Type::kMouseEnter;
    case WebInputEvent::Type::kMouseLeave:
      return Type::kMouseLeave;
    case WebInputEvent::Type::kMouseWheel:
      return Type::kWheel;

    case WebInputEvent::Type::kKeyDown:
    case WebInputEvent::Type::kRawKeyDown:
    case WebInputEvent::Type::kChar:
      return Type::kKeyDown;
    case WebInputEvent::Type::kKeyUp:
      return Type::kKeyUp;

    case WebInputEvent::Type::kTouchStart:
    case WebInputEvent::Type::kGestureTapDown:
      return Type::kTouchStart | Type::kMouseDown;
    case WebInputEvent::Type::kTouchMove:
      return Type::kTouchMove;
    case WebInputEvent::Type::kTouchEnd:
    case WebInputEvent::Type::kGestureTap:
      return Type::kTouchEnd | Type::kClick | Type::kDblClick | Type::kMouseUp;
    case WebInputEvent::Type::kTouchCancel:
    case WebInputEvent::Type::kGestureTapCancel:
      return Type::kTouchCancel;

    default:
      return Type::kNone;
  }
}

}  // namespace scheduler
}  // namespace blink
