// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/input_conversion.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/trace.h"
#include "tools/gn/value.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#endif

#if defined(OS_POSIX)
#include <fcntl.h>
#include <unistd.h>

#include "base/posix/file_descriptor_shuffle.h"
#endif

namespace functions {

namespace {

const char kNoExecSwitch[] = "no-exec";

#if defined(OS_WIN)
bool ExecProcess(const CommandLine& cmdline,
                 const base::FilePath& startup_dir,
                 std::string* std_out,
                 std::string* std_err,
                 int* exit_code) {
  SECURITY_ATTRIBUTES sa_attr;
  // Set the bInheritHandle flag so pipe handles are inherited.
  sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa_attr.bInheritHandle = TRUE;
  sa_attr.lpSecurityDescriptor = NULL;

  // Create the pipe for the child process's STDOUT.
  HANDLE out_read = NULL;
  HANDLE out_write = NULL;
  if (!CreatePipe(&out_read, &out_write, &sa_attr, 0)) {
    NOTREACHED() << "Failed to create pipe";
    return false;
  }
  base::win::ScopedHandle scoped_out_read(out_read);
  base::win::ScopedHandle scoped_out_write(out_write);

  // Create the pipe for the child process's STDERR.
  HANDLE err_read = NULL;
  HANDLE err_write = NULL;
  if (!CreatePipe(&err_read, &err_write, &sa_attr, 0)) {
    NOTREACHED() << "Failed to create pipe";
    return false;
  }
  base::win::ScopedHandle scoped_err_read(err_read);
  base::win::ScopedHandle scoped_err_write(err_write);

  // Ensure the read handle to the pipe for STDOUT/STDERR is not inherited.
  if (!SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0)) {
    NOTREACHED() << "Failed to disabled pipe inheritance";
    return false;
  }
  if (!SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0)) {
    NOTREACHED() << "Failed to disabled pipe inheritance";
    return false;
  }

  base::FilePath::StringType cmdline_str(cmdline.GetCommandLineString());

  STARTUPINFO start_info = {};

  start_info.cb = sizeof(STARTUPINFO);
  start_info.hStdOutput = out_write;
  // Keep the normal stdin.
  start_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  // FIXME(brettw) set stderr here when we actually read it below.
  //start_info.hStdError = err_write;
  start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  start_info.dwFlags |= STARTF_USESTDHANDLES;

  // Create the child process.
  PROCESS_INFORMATION temp_process_info = {};
  if (!CreateProcess(NULL,
                     &cmdline_str[0],
                     NULL, NULL,
                     TRUE,  // Handles are inherited.
                     0, NULL,
                     startup_dir.value().c_str(),
                     &start_info, &temp_process_info)) {
    return false;
  }
  base::win::ScopedProcessInformation proc_info(temp_process_info);

  // Close our writing end of pipes now. Otherwise later read would not be able
  // to detect end of child's output.
  scoped_out_write.Close();
  scoped_err_write.Close();

  // Read output from the child process's pipe for STDOUT
  const int kBufferSize = 1024;
  char buffer[kBufferSize];

  // FIXME(brettw) read from stderr here! This is complicated because we want
  // to read both of them at the same time, probably need overlapped I/O.
  // Also uncomment start_info code above.
  for (;;) {
    DWORD bytes_read = 0;
    BOOL success = ReadFile(out_read, buffer, kBufferSize, &bytes_read, NULL);
    if (!success || bytes_read == 0)
      break;
    std_out->append(buffer, bytes_read);
  }

  // Let's wait for the process to finish.
  WaitForSingleObject(proc_info.process_handle(), INFINITE);

  DWORD dw_exit_code;
  GetExitCodeProcess(proc_info.process_handle(), &dw_exit_code);
  *exit_code = static_cast<int>(dw_exit_code);

  return true;
}
#else
bool ExecProcess(const CommandLine& cmdline,
                 const base::FilePath& startup_dir,
                 std::string* std_out,
                 std::string* std_err,
                 int* exit_code) {
  *exit_code = EXIT_FAILURE;

  std::vector<std::string> argv = cmdline.argv();

  int pipe_fd[2];
  pid_t pid;
  base::InjectiveMultimap fd_shuffle1, fd_shuffle2;
  scoped_ptr<char*[]> argv_cstr(new char*[argv.size() + 1]);

  fd_shuffle1.reserve(3);
  fd_shuffle2.reserve(3);

  if (pipe(pipe_fd) < 0)
    return false;

  switch (pid = fork()) {
    case -1:  // error
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      return false;
    case 0:  // child
      {
        // DANGER: no calls to malloc are allowed from now on:
        // http://crbug.com/36678

        // Obscure fork() rule: in the child, if you don't end up doing exec*(),
        // you call _exit() instead of exit(). This is because _exit() does not
        // call any previously-registered (in the parent) exit handlers, which
        // might do things like block waiting for threads that don't even exist
        // in the child.
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null < 0)
          _exit(127);

        fd_shuffle1.push_back(
            base::InjectionArc(pipe_fd[1], STDOUT_FILENO, true));
        fd_shuffle1.push_back(
            base::InjectionArc(dev_null, STDERR_FILENO, true));
        fd_shuffle1.push_back(
            base::InjectionArc(dev_null, STDIN_FILENO, true));
        // Adding another element here? Remeber to increase the argument to
        // reserve(), above.

        for (size_t i = 0; i < fd_shuffle1.size(); ++i)
          fd_shuffle2.push_back(fd_shuffle1[i]);

        if (!ShuffleFileDescriptors(&fd_shuffle1))
          _exit(127);

        base::SetCurrentDirectory(startup_dir);

        // TODO(brettw) the base version GetAppOutput does a
        // CloseSuperfluousFds call here. Do we need this?

        for (size_t i = 0; i < argv.size(); i++)
          argv_cstr[i] = const_cast<char*>(argv[i].c_str());
        argv_cstr[argv.size()] = NULL;
        execvp(argv_cstr[0], argv_cstr.get());
        _exit(127);
      }
    default:  // parent
      {
        // Close our writing end of pipe now. Otherwise later read would not
        // be able to detect end of child's output (in theory we could still
        // write to the pipe).
        close(pipe_fd[1]);

        char buffer[256];
        ssize_t bytes_read = 0;

        while (true) {
          bytes_read = HANDLE_EINTR(read(pipe_fd[0], buffer, sizeof(buffer)));
          if (bytes_read <= 0)
            break;
          std_out->append(buffer, bytes_read);
        }
        close(pipe_fd[0]);

        return base::WaitForExitCode(pid, exit_code);
      }
  }

  return false;
}
#endif

}  // namespace

const char kExecScript[] = "exec_script";
const char kExecScript_HelpShort[] =
    "exec_script: Synchronously run a script and return the output.";
const char kExecScript_Help[] =
    "exec_script: Synchronously run a script and return the output.\n"
    "\n"
    "  exec_script(filename,\n"
    "              arguments = [],\n"
    "              input_conversion = \"\",\n"
    "              file_dependencies = [])\n"
    "\n"
    "  Runs the given script, returning the stdout of the script. The build\n"
    "  generation will fail if the script does not exist or returns a nonzero\n"
    "  exit code.\n"
    "\n"
    "  The current directory when executing the script will be the root\n"
    "  build directory. If you are passing file names, you will want to use\n"
    "  the rebase_path() function to make file names relative to this\n"
    "  path (see \"gn help rebase_path\").\n"
    "\n"
    "Arguments:\n"
    "\n"
    "  filename:\n"
    "      File name of python script to execute. Non-absolute names will\n"
    "      be treated as relative to the current build file.\n"
    "\n"
    "  arguments:\n"
    "      A list of strings to be passed to the script as arguments.\n"
    "      May be unspecified or the empty list which means no arguments.\n"
    "\n"
    "  input_conversion:\n"
    "      Controls how the file is read and parsed.\n"
    "      See \"gn help input_conversion\".\n"
    "\n"
    "      If unspecified, defaults to the empty string which causes the\n"
    "      script result to be discarded. exec script will return None.\n"
    "\n"
    "  dependencies:\n"
    "      (Optional) A list of files that this script reads or otherwise\n"
    "      depends on. These dependencies will be added to the build result\n"
    "      such that if any of them change, the build will be regenerated and\n"
    "      the script will be re-run.\n"
    "\n"
    "      The script itself will be an implicit dependency so you do not\n"
    "      need to list it.\n"
    "\n"
    "Example:\n"
    "\n"
    "  all_lines = exec_script(\n"
    "      \"myscript.py\", [some_input], \"list lines\",\n"
    "      [ rebase_path(\"data_file.txt\", root_build_dir) ])\n"
    "\n"
    "  # This example just calls the script with no arguments and discards\n"
    "  # the result.\n"
    "  exec_script(\"//foo/bar/myscript.py\")\n";

Value RunExecScript(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    Err* err) {
  if (args.size() < 1 || args.size() > 4) {
    *err = Err(function->function(), "Wrong number of arguments to exec_script",
               "I expected between one and four arguments.");
    return Value();
  }

  const Settings* settings = scope->settings();
  const BuildSettings* build_settings = settings->build_settings();
  const SourceDir& cur_dir = scope->GetSourceDir();

  // Find the python script to run.
  if (!args[0].VerifyTypeIs(Value::STRING, err))
    return Value();
  SourceFile script_source =
      cur_dir.ResolveRelativeFile(args[0].string_value());
  base::FilePath script_path = build_settings->GetFullPath(script_source);
  if (!build_settings->secondary_source_path().empty() &&
      !base::PathExists(script_path)) {
    // Fall back to secondary source root when the file doesn't exist.
    script_path = build_settings->GetFullPathSecondary(script_source);
  }

  ScopedTrace trace(TraceItem::TRACE_SCRIPT_EXECUTE, script_source.value());
  trace.SetToolchain(settings->toolchain_label());

  // Add all dependencies of this script, including the script itself, to the
  // build deps.
  g_scheduler->AddGenDependency(script_path);
  if (args.size() == 4) {
    const Value& deps_value = args[3];
    if (!deps_value.VerifyTypeIs(Value::LIST, err))
      return Value();

    for (size_t i = 0; i < deps_value.list_value().size(); i++) {
      if (!deps_value.list_value()[0].VerifyTypeIs(Value::STRING, err))
        return Value();
      g_scheduler->AddGenDependency(
          build_settings->GetFullPath(cur_dir.ResolveRelativeFile(
              deps_value.list_value()[0].string_value())));
    }
  }

  // Make the command line.
  const base::FilePath& python_path = build_settings->python_path();
  CommandLine cmdline(python_path);
  cmdline.AppendArgPath(script_path);

  if (args.size() >= 2) {
    // Optional command-line arguments to the script.
    const Value& script_args = args[1];
    if (!script_args.VerifyTypeIs(Value::LIST, err))
      return Value();
    for (size_t i = 0; i < script_args.list_value().size(); i++) {
      if (!script_args.list_value()[i].VerifyTypeIs(Value::STRING, err))
        return Value();
      cmdline.AppendArg(script_args.list_value()[i].string_value());
    }
  }

  // Log command line for debugging help.
  trace.SetCommandLine(cmdline);
  base::TimeTicks begin_exec;
  if (g_scheduler->verbose_logging()) {
#if defined(OS_WIN)
    g_scheduler->Log("Pythoning",
                     base::UTF16ToUTF8(cmdline.GetCommandLineString()));
#else
    g_scheduler->Log("Pythoning", cmdline.GetCommandLineString());
#endif
    begin_exec = base::TimeTicks::Now();
  }

  base::FilePath startup_dir =
      build_settings->GetFullPath(build_settings->build_dir());
  // The first time a build is run, no targets will have been written so the
  // build output directory won't exist. We need to make sure it does before
  // running any scripts with this as its startup directory, although it will
  // be relatively rare that the directory won't exist by the time we get here.
  //
  // If this shows up on benchmarks, we can cache whether we've done this
  // or not and skip creating the directory.
  base::CreateDirectory(startup_dir);

  // Execute the process.
  // TODO(brettw) set the environment block.
  std::string output;
  std::string stderr_output;  // TODO(brettw) not hooked up, see above.
  int exit_code = 0;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kNoExecSwitch)) {
    if (!ExecProcess(cmdline, startup_dir,
                     &output, &stderr_output, &exit_code)) {
      *err = Err(function->function(), "Could not execute python.",
          "I was trying to execute \"" + FilePathToUTF8(python_path) + "\".");
      return Value();
    }
  }
  if (g_scheduler->verbose_logging()) {
    g_scheduler->Log("Pythoning", script_source.value() + " took " +
        base::Int64ToString(
            (base::TimeTicks::Now() - begin_exec).InMilliseconds()) +
        "ms");
  }

  // TODO(brettw) maybe we need stderr also for reasonable stack dumps.
  if (exit_code != 0) {
    std::string msg = "Current dir: " + FilePathToUTF8(startup_dir) +
        "\nCommand: " + FilePathToUTF8(cmdline.GetCommandLineString()) +
        "\nReturned " + base::IntToString(exit_code);
    if (!output.empty())
      msg += " and printed out:\n\n" + output;
    else
      msg += ".";
    *err = Err(function->function(), "Script returned non-zero exit code.",
               msg);
    return Value();
  }

  // Default to None value for the input conversion if unspecified.
  return ConvertInputToValue(scope->settings(), output, function,
                             args.size() >= 3 ? args[2] : Value(), err);
}

}  // namespace functions
