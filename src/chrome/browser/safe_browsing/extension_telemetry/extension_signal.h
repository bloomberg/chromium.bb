// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_SIGNAL_H_

#include "extensions/common/extension_id.h"

namespace safe_browsing {

// Signal types.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ExtensionSignalType {
  kTabsExecuteScript = 0,
  kMaxValue = kTabsExecuteScript,
};

// An abstract signal. Subclasses provide type-specific functionality to
// enable processing by the extension telemetry service.
class ExtensionSignal {
 public:
  virtual ~ExtensionSignal() = default;

  // Returns the type of the signal.
  virtual ExtensionSignalType GetType() const = 0;

  const extensions::ExtensionId& extension_id() { return extension_id_; }

 protected:
  explicit ExtensionSignal(const extensions::ExtensionId& extension_id)
      : extension_id_(extension_id) {}

  const extensions::ExtensionId extension_id_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_SIGNAL_H_
