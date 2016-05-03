// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a thin binary wrapper around the BattOr Agent
// library. This binary wrapper provides a means for non-C++ tracing
// controllers, such as Telemetry and Android Systrace, to issue high-level
// tracing commands to the BattOr through an interactive shell.
//
// Example usage of how an external trace controller might use this binary:
//
// 1) Telemetry's PowerTracingAgent is told to start recording power samples
// 2) PowerTracingAgent opens up a BattOr agent binary subprocess
// 3) PowerTracingAgent sends the subprocess the StartTracing message via
//    STDIN
// 4) PowerTracingAgent waits for the subprocess to write a line to STDOUT
//    ('Done.' if successful, some error message otherwise)
// 5) If the last command was successful, PowerTracingAgent waits for the
//    duration of the trace
// 6) When the tracing should end, PowerTracingAgent records the clock sync
//    start timestamp and sends the subprocess the
//    'RecordClockSyncMark <marker>' message via STDIN.
// 7) PowerTracingAgent waits for the subprocess to write a line to STDOUT
//    ('Done.' if successful, some error message otherwise)
// 8) If the last command was successful, PowerTracingAgent records the clock
//    sync end timestamp and sends the subprocess the StopTracing message via
//    STDIN
// 9) PowerTracingAgent continues to read trace output lines from STDOUT until
//    the binary exits with an exit code of 1 (indicating failure) or the
//    'Done.' line is printed to STDOUT, signaling the last line of the trace
// 10) PowerTracingAgent returns the battery trace to the Telemetry trace
//     controller

#include <stdint.h>

#include <fstream>
#include <iostream>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "tools/battor_agent/battor_agent.h"
#include "tools/battor_agent/battor_error.h"
#include "tools/battor_agent/battor_finder.h"

using std::endl;

namespace battor {

namespace {

const char kIoThreadName[] = "BattOr IO Thread";
const char kFileThreadName[] = "BattOr File Thread";
const char kUiThreadName[] = "BattOr UI Thread";

const char kUsage[] =
    "Start the battor_agent shell with:\n"
    "\n"
    "  battor_agent <switches>\n"
    "\n"
    "Switches: \n"
    "  --battor-path=<path> Uses the specified BattOr path.\n"
    "\n"
    "Once in the shell, you can issue the following commands:\n"
    "\n"
    "  StartTracing\n"
    "  StopTracing <optional file path>\n"
    "  SupportsExplicitClockSync\n"
    "  RecordClockSyncMarker <marker>\n"
    "  Exit\n"
    "  Help\n"
    "\n";

void PrintSupportsExplicitClockSync() {
  std::cout << BattOrAgent::SupportsExplicitClockSync() << endl;
}

// Logs the error and exits with an error code.
void HandleError(battor::BattOrError error) {
  if (error != BATTOR_ERROR_NONE)
    LOG(FATAL) << "Fatal error when communicating with the BattOr: "
               << BattOrErrorToString(error);
}

// Prints an error message and exits due to a required thread failing to start.
void ExitFromThreadStartFailure(const std::string& thread_name) {
  LOG(FATAL) << "Failed to start " << thread_name;
}

std::vector<std::string> TokenizeString(std::string cmd) {
  base::StringTokenizer tokenizer(cmd, " ");
  std::vector<std::string> tokens;
  while (tokenizer.GetNext())
    tokens.push_back(tokenizer.token());
  return tokens;
}

}  // namespace

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

  // Starts the interactive BattOr agent shell and eventually returns an exit
  // code.
  int Run(int argc, char* argv[]) {
    // If we don't have any BattOr to use, exit.
    std::string path = BattOrFinder::FindBattOr();
    if (path.empty()) {
      std::cout << "Unable to find a BattOr." << endl;
      exit(1);
    }

    SetUp(path);

    std::string cmd;
    for (;;) {
      std::getline(std::cin, cmd);

      if (cmd == "StartTracing") {
        StartTracing();
      } else if (cmd.find("StopTracing") != std::string::npos) {
        std::vector<std::string> tokens = TokenizeString(cmd);
        if (tokens[0] != "StopTracing" || tokens.size() > 2) {
          std::cout << "Invalid StopTracing command." << endl;
          std::cout << kUsage << endl;
          continue;
        }

        // tokens[1] contains the optional output file argument, which allows
        // users to dump the trace to a file instead instead of to STDOUT.
        std::string trace_output_file =
            tokens.size() == 2 ? tokens[1] : std::string();

        StopTracing(trace_output_file);
        break;
      } else if (cmd == "SupportsExplicitClockSync") {
        PrintSupportsExplicitClockSync();
      } else if (cmd.find("RecordClockSyncMarker") != std::string::npos) {
        std::vector<std::string> tokens = TokenizeString(cmd);
        if (tokens.size() != 2 || tokens[0] != "RecordClockSyncMarker") {
          std::cout << "Invalid RecordClockSyncMarker command." << endl;
          std::cout << kUsage << endl;
          continue;
        }

        RecordClockSyncMarker(tokens[1]);
      } else if (cmd == "Exit") {
        break;
      } else {
        std::cout << kUsage << endl;
      }
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
    done_.Wait();
  }

  // Performs any cleanup necessary after the BattOr binary is done running.
  void TearDown() {
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BattOrAgentBin::DeleteAgent, base::Unretained(this)));
    done_.Wait();
  }

  void StartTracing() {
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BattOrAgent::StartTracing, base::Unretained(agent_.get())));
    done_.Wait();
  }

  void OnStartTracingComplete(BattOrError error) override {
    if (error == BATTOR_ERROR_NONE)
      std::cout << "Done." << endl;
    else
      HandleError(error);

    done_.Signal();
  }

  void StopTracing(const std::string& trace_output_file) {
    trace_output_file_ = trace_output_file;
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BattOrAgent::StopTracing, base::Unretained(agent_.get())));
    done_.Wait();
    trace_output_file_ = std::string();
  }

  void OnStopTracingComplete(const std::string& trace,
                             BattOrError error) override {
    if (error == BATTOR_ERROR_NONE) {
      if (trace_output_file_.empty()) {
        std::cout << trace;
      } else {
        std::ofstream trace_stream(trace_output_file_);
        if (!trace_stream.is_open()) {
          std::cout << "Tracing output file could not be opened." << endl;
          exit(1);
        }
        trace_stream << trace;
        trace_stream.close();
      }
      std::cout << "Done." << endl;
    } else {
      HandleError(error);
    }

    done_.Signal();
  }

  void RecordClockSyncMarker(const std::string& marker) {
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&BattOrAgent::RecordClockSyncMarker,
                              base::Unretained(agent_.get()), marker));
    done_.Wait();
  }

  void OnRecordClockSyncMarkerComplete(BattOrError error) override {
    if (error == BATTOR_ERROR_NONE)
      std::cout << "Done." << endl;
    else
      HandleError(error);

    done_.Signal();
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
    done_.Signal();
  }

  // Postable task for deleting the BattOrAgent. See the comment for
  // CreateAgent() above regarding why this is necessary.
  void DeleteAgent() {
    agent_.reset(nullptr);
    done_.Signal();
  }

 private:
  // Event signaled when an async task has finished executing.
  base::WaitableEvent done_;

  // Threads needed for serial communication.
  base::Thread io_thread_;
  base::Thread file_thread_;
  base::Thread ui_thread_;

  // The agent capable of asynchronously communicating with the BattOr.
  std::unique_ptr<BattOrAgent> agent_;

  std::string trace_output_file_;
};

}  // namespace battor

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  battor::BattOrAgentBin bin;
  return bin.Run(argc, argv);
}
