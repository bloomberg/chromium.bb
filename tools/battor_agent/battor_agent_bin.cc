// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a thin binary wrapper around the BattOr Agent
// library. This binary wrapper provides a means for non-C++ tracing
// controllers, such as Telemetry and Android Systrace, to issue high-level
// tracing commands to the BattOr..

#include <stdint.h>

#include <iostream>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "tools/battor_agent/battor_agent.h"
#include "tools/battor_agent/battor_error.h"
#include "tools/battor_agent/battor_finder.h"

using std::cout;
using std::endl;

namespace {

const char kIoThreadName[] = "BattOr IO Thread";
const char kFileThreadName[] = "BattOr File Thread";
const char kUiThreadName[] = "BattOr UI Thread";
const int32_t kBattOrCommandTimeoutSeconds = 10;

void PrintUsage() {
  cout << "Usage: battor_agent <command> <arguments>" << endl
       << endl
       << "Commands:" << endl
       << endl
       << "  StartTracing <path>" << endl
       << "  StopTracing <path>" << endl
       << "  SupportsExplicitClockSync" << endl
       << "  RecordClockSyncMarker <path> <marker>" << endl
       << "  IssueClockSyncMarker <path>" << endl
       << "  Help" << endl;
}

void PrintSupportsExplicitClockSync() {
  cout << battor::BattOrAgent::SupportsExplicitClockSync() << endl;
}

// Retrieves argument argnum from the argument list, or an empty string if the
// argument doesn't exist.
std::string GetArg(int argnum, int argc, char* argv[]) {
  if (argnum >= argc) {
    return std::string();
  }

  return argv[argnum];
}

// Checks if an error occurred and, if it did, prints the error and exits
// with an error code.
void CheckError(battor::BattOrError error) {
  if (error != battor::BATTOR_ERROR_NONE)
    LOG(FATAL) << "Fatal error when communicating with the BattOr: " << error;
}

// Prints an error message and exits due to a required thread failing to start.
void ExitFromThreadStartFailure(const std::string& thread_name) {
  LOG(FATAL) << "Failed to start " << thread_name;
}

}  // namespace

namespace battor {

// Wrapper class containing all state necessary for an independent binary to
// use a BattOrAgent to communicate with a BattOr.
class BattOrAgentBin : public BattOrAgent::Listener {
 public:
  BattOrAgentBin()
      : done_(false, false),
        io_thread_(kIoThreadName),
        file_thread_(kFileThreadName),
        ui_thread_(kUiThreadName) {}

  ~BattOrAgentBin() { DCHECK(!agent_); }

  // Runs the BattOr binary and returns the exit code.
  int Run(int argc, char* argv[]) {
    std::string cmd = GetArg(1, argc, argv);
    if (cmd.empty()) {
      PrintUsage();
      exit(1);
    }

    // SupportsExplicitClockSync doesn't need to use the serial connection, so
    // handle it separately.
    if (cmd == "SupportsExplicitClockSync") {
      PrintSupportsExplicitClockSync();
      return 0;
    }

    std::string path = GetArg(2, argc, argv);
    // If no path is specified, see if we can find a BattOr and use that.
    if (path.empty())
      path = BattOrFinder::FindBattOr();

    // If we don't have any BattOr to use, exit.
    if (path.empty()) {
      cout << "Unable to find a BattOr, and no explicit BattOr path was "
              "specified."
           << endl;
      exit(1);
    }

    SetUp(path);

    if (cmd == "StartTracing") {
      StartTracing();
    } else if (cmd == "StopTracing") {
      StopTracing();
    } else if (cmd == "RecordClockSyncMarker") {
      // TODO(charliea): Write RecordClockSyncMarker.
    } else if (cmd == "IssueClockSyncMarker") {
      // TODO(charliea): Write IssueClockSyncMarker.
    } else {
      TearDown();
      PrintUsage();
      return 1;
    }

    TearDown();
    return 0;
  }

  // Performs any setup necessary for the BattOr binary to run.
  void SetUp(const std::string& path) {
    // TODO(charliea): Investigate whether it's possible to replace this
    // separate thread with a combination of MessageLoopForIO and RunLoop.
    base::Thread::Options io_thread_options;
    io_thread_options.message_loop_type = base::MessageLoopForIO::TYPE_IO;
    if (!io_thread_.StartWithOptions(io_thread_options)) {
      ExitFromThreadStartFailure(kIoThreadName);
    }

    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BattOrAgentBin::CreateAgent, base::Unretained(this), path));
    CheckError(AwaitResult());
  }

  // Performs any cleanup necessary after the BattOr binary is done running.
  void TearDown() {
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BattOrAgentBin::DeleteAgent, base::Unretained(this)));
    CheckError(AwaitResult());
  }

  void StartTracing() {
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BattOrAgent::StartTracing, base::Unretained(agent_.get())));
    CheckError(AwaitResult());
  }

  void OnStartTracingComplete(BattOrError error) override {
    error_ = error;
    done_.Signal();
  }

  void StopTracing() {
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BattOrAgent::StopTracing, base::Unretained(agent_.get())));
    CheckError(AwaitResult());
  }

  void OnStopTracingComplete(const std::string& trace,
                             BattOrError error) override {
    error_ = error;

    if (error == BATTOR_ERROR_NONE)
      cout << trace << endl;

    done_.Signal();
  }

  void OnRecordClockSyncMarkerComplete(BattOrError error) override {
    // TODO(charliea): Implement RecordClockSyncMarker for this binary. This
    // will probably involve reading an external file for the actual sample
    // number to clock sync ID map.
    NOTREACHED();
  }

  // Postable task for creating the BattOrAgent. Because the BattOrAgent has
  // uber thread safe dependencies, all interactions with it, including creating
  // and deleting it, MUST happen on the IO thread.
  void CreateAgent(const std::string& path) {
    // In Chrome, we already have file and UI threads running. Because the
    // Chrome serial libraries rely on having those threads available, we have
    // to spin up our own because we're in a separate binary.
    if (!file_thread_.Start())
      ExitFromThreadStartFailure(kFileThreadName);

    base::Thread::Options ui_thread_options;
    ui_thread_options.message_loop_type = base::MessageLoopForIO::TYPE_UI;
    if (!ui_thread_.StartWithOptions(ui_thread_options)) {
      ExitFromThreadStartFailure(kUiThreadName);
    }

    agent_.reset(new BattOrAgent(path, this, file_thread_.task_runner(),
                                 ui_thread_.task_runner()));
    error_ = BATTOR_ERROR_NONE;
    done_.Signal();
  }

  // Postable task for deleting the BattOrAgent. See the comment for
  // CreateAgent() above regarding why this is necessary.
  void DeleteAgent() {
    agent_.reset(nullptr);
    error_ = BATTOR_ERROR_NONE;
    done_.Signal();
  }

  // Waits until the previously executed command has finished executing.
  BattOrError AwaitResult() {
    if (!done_.TimedWait(
            base::TimeDelta::FromSeconds(kBattOrCommandTimeoutSeconds)))
      return BATTOR_ERROR_TIMEOUT;

    return error_;
  }

 private:
  // Event signaled when an async task has finished executing.
  base::WaitableEvent done_;

  // The error from the last async command that finished.
  BattOrError error_;

  // Threads needed for serial communication.
  base::Thread io_thread_;
  base::Thread file_thread_;
  base::Thread ui_thread_;

  // The agent capable of asynchronously communicating with the BattOr.
  scoped_ptr<BattOrAgent> agent_;
};

}  // namespace battor

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  battor::BattOrAgentBin bin;
  return bin.Run(argc, argv);
}
