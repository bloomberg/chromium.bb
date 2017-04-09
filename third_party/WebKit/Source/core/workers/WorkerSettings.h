// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerSettings_h
#define WorkerSettings_h

#include "core/CoreExport.h"
#include "core/frame/Settings.h"

namespace blink {

class CORE_EXPORT WorkerSettings {
 public:
  explicit WorkerSettings(Settings*);

  bool DisableReadingFromCanvas() const { return disable_reading_from_canvas_; }

 private:
  void CopyFlagValuesFromSettings(Settings*);
  void SetDefaultValues();

  // The settings that are to be copied from main thread to worker thread
  // These setting's flag values must remain unchanged throughout the document
  // lifecycle.
  // We hard-code the flags as there're very few flags at this moment.
  bool disable_reading_from_canvas_;
};

}  // namespace blink

#endif  // WorkerSettings_h
