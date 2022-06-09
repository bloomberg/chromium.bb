// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_ROUTINE_H_

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"

namespace chromeos {
namespace network_diagnostics {

using RoutineResultCallback = base::OnceCallback<void(mojom::RoutineResultPtr)>;

// Defines the key components of a network diagnostics routine. This class is
// expected to be implemented by every network diagnostics routine.
class NetworkDiagnosticsRoutine {
 public:
  NetworkDiagnosticsRoutine();
  NetworkDiagnosticsRoutine(const NetworkDiagnosticsRoutine&) = delete;
  NetworkDiagnosticsRoutine& operator=(const NetworkDiagnosticsRoutine&) =
      delete;
  virtual ~NetworkDiagnosticsRoutine();

  // Returns the type of network diagnostic routine.
  virtual mojom::RoutineType Type() = 0;

  // Determines whether this test is capable of being run.
  virtual bool CanRun();

  // Runs the routine.
  void RunRoutine(RoutineResultCallback callback);

 protected:
  // Implemented routine logic.
  virtual void Run() = 0;

  // Analyze the results gathered by the function and execute the callback.
  virtual void AnalyzeResultsAndExecuteCallback() = 0;

  // Runs the routine callback and returns the result.
  void ExecuteCallback();

  void set_verdict(mojom::RoutineVerdict verdict) { result_.verdict = verdict; }

  void set_problems(mojom::RoutineProblemsPtr problems) {
    result_.problems = std::move(problems);
  }

  void set_result_value(mojom::RoutineResultValuePtr result_value) {
    result_.result_value = std::move(result_value);
  }

 private:
  mojom::RoutineResult result_;
  RoutineResultCallback callback_;
  friend class NetworkDiagnosticsRoutineTest;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_ROUTINE_H_
