// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_NEW_SCREENCAST_PRECONDITION_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_NEW_SCREENCAST_PRECONDITION_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

// The new screencast button state in the Projector SWA.
// Ensure that this enum class is synchronized with
// NewScreencastPreconditionState enum in
// //ash/webui/projector_app/resources/communication/message_types.js.
enum class ASH_PUBLIC_EXPORT NewScreencastPreconditionState {
  // The new screencast button is visible but is disabled.
  kDisabled = 1,
  // The new screencast button is enabled and the user can create new ones now.
  kEnabled = 2
};

// The reason for the new screencast button state.
// Ensure that this enum class is synchronized with
// NewScreencastPreconditionReason enum in
// //ash/webui/projector_app/resources/communication/message_types.js.
enum class ASH_PUBLIC_EXPORT NewScreencastPreconditionReason {
  kSodaInstallationError = 0,
  kOnDeviceSpeechRecognitionNotSupported = 1,
  kUserLocaleNotSupported = 2,
  kInProjectorSession = 3,
  kScreenRecordingInProgress = 4,
  kSodaDownloadInProgress = 5,
  kOutOfDiskSpace = 6,
  kNoMic = 7,
  kOthers = 8
};

// Struct used to provide the new screen cast precondition state and the reasons
// for such a state.
struct ASH_PUBLIC_EXPORT NewScreencastPrecondition {
  NewScreencastPrecondition();
  NewScreencastPrecondition(
      NewScreencastPreconditionState new_state,
      const std::vector<NewScreencastPreconditionReason>& new_state_reason);
  NewScreencastPrecondition(const NewScreencastPrecondition&);
  NewScreencastPrecondition& operator=(const NewScreencastPrecondition&);
  ~NewScreencastPrecondition();

  base::Value ToValue() const;
  bool operator==(const NewScreencastPrecondition& rhs) const;

  NewScreencastPreconditionState state;
  std::vector<NewScreencastPreconditionReason> reasons;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_NEW_SCREENCAST_PRECONDITION_H_
