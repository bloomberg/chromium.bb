// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_TRACING_MODEL_H_
#define ASH_SYSTEM_MODEL_TRACING_MODEL_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class ASH_EXPORT TracingObserver {
 public:
  virtual ~TracingObserver() {}

  // Notifies when tracing mode changes.
  virtual void OnTracingModeChanged() = 0;
};

// Model to store whether users enable performance tracing at chrome://slow.
class ASH_EXPORT TracingModel {
 public:
  TracingModel();
  ~TracingModel();

  void AddObserver(TracingObserver* observer);
  void RemoveObserver(TracingObserver* observer);

  void SetIsTracing(bool is_tracing);

  bool is_tracing() const { return is_tracing_; }

 private:
  void NotifyChanged();

  // True if performance tracing is enabled.
  bool is_tracing_ = false;

  base::ObserverList<TracingObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(TracingModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_TRACING_MODEL_H_
