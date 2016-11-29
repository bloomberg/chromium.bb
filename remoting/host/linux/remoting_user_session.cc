// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a wrapper to run the virtual me2me session within a
// proper PAM session. It will generally be run as root and drop privileges to
// the specified user before running the me2me session script.

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <security/pam_appl.h>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/strings/string_piece.h"

namespace {

const char kPamName[] = "chrome-remote-desktop";

const char kHelpSwitchName[] = "help";
const char kQuestionSwitchName[] = "?";
const char kUserSwitchName[] = "user";
const char kScriptSwitchName[] = "me2me-script";
const char kForegroundSwitchName[] = "foreground";

// This template will be formatted by strftime and then used by mkstemp
const char kLogFileTemplate[] =
    "/tmp/chrome_remote_desktop_%Y%m%d_%H%M%S_XXXXXX";

const char kUsageMessage[] =
    "Usage: %s [options]\n"
    "\n"
    "Options:\n"
    "  --help, -?               - Print this message.\n"
    "  --user=<user>            - Create session as the specified user. "
                                 "(Must run as root.)\n"
    "  --me2me-script=<script>  - Location of the me2me python script "
                                 "(required)\n"
    "  --foreground             - Don't daemonize.\n";

void PrintUsage(const base::FilePath& program_name) {
  std::printf(kUsageMessage, program_name.MaybeAsASCII().c_str());
}

// Shell-escapes a single argument in a way that is compatible with various
// different shells. Returns nullopt when argument contains a newline, which
// can't be represented in a cross-shell fashion.
base::Optional<std::string> ShellEscapeArgument(
    const base::StringPiece argument) {
  std::string result;
  for (char character : argument) {
    // csh in particular doesn't provide a good way to handle this
    if (character == '\n') {
      return base::nullopt;
    }

    // Some shells ascribe special meaning to some escape sequences such as \t,
    // so don't escape any alphanumerics. (Also cuts down on verbosity.) This is
    // similar to the approach sudo takes.
    if (!((character >= '0' && character <= '9') ||
          (character >= 'A' && character <= 'Z') ||
          (character >= 'a' && character <= 'z') ||
          (character == '-' || character == '_'))) {
      result.push_back('\\');
    }
    result.push_back(character);
  }
  return result;
}

// PAM conversation function. Since the wrapper runs in a non-interactive
// context, log any messages, but return an error if asked to provide user
// input.
extern "C" int Converse(int num_messages,
                        const struct pam_message** messages,
                        struct pam_response** responses,
                        void* context) {
  bool failed = false;

  for (int i = 0; i < num_messages; ++i) {
    // This is correct for the PAM included with Linux, OS X, and BSD. However,
    // apparently Solaris and HP/UX require instead `&(*msg)[i]`. That is, they
    // disagree as to which level of indirection contains the array.
    const pam_message* message = messages[i];

    switch (message->msg_style) {
      case PAM_PROMPT_ECHO_OFF:
      case PAM_PROMPT_ECHO_ON:
        LOG(WARNING) << "PAM requested user input (unsupported): "
                     << (message->msg ? message->msg : "");
        failed = true;
        break;
      case PAM_TEXT_INFO:
        LOG(INFO) << "[PAM] " << (message->msg ? message->msg : "");
        break;
      case PAM_ERROR_MSG:
        // Error messages from PAM are not necessarily fatal to the operation,
        // as the module may be optional.
        LOG(WARNING) << "[PAM] " << (message->msg ? message->msg : "");
        break;
      default:
        LOG(WARNING) << "Encountered unknown PAM message style";
        failed = true;
        break;
    }
  }

  if (failed)
    return PAM_CONV_ERR;

  pam_response* response_list = static_cast<pam_response*>(
      std::calloc(num_messages, sizeof(*response_list)));

  if (response_list == nullptr)
    return PAM_BUF_ERR;

  *responses = response_list;
  return PAM_SUCCESS;
}

const struct pam_conv kPamConversation = {Converse, nullptr};

// Wrapper class for working with PAM and cleaning up in an RAII fashion
class PamHandle {
 public:
  // Attempts to initialize PAM transaction. Check the result with IsInitialized
  // before calling any other member functions.
  PamHandle(const char* service_name,
            const char* user,
            const struct pam_conv* pam_conversation) {
    last_return_code_ =
        pam_start(service_name, user, pam_conversation, &pam_handle_);
    if (last_return_code_ != PAM_SUCCESS) {
      pam_handle_ = nullptr;
    }
  }

  // Terminates PAM transaction
  ~PamHandle() {
    if (pam_handle_ != nullptr) {
      pam_end(pam_handle_, last_return_code_);
    }
  }

  // Checks whether the PAM transaction was successfully initialized. Only call
  // other member functions if this returns true.
  bool IsInitialized() const { return pam_handle_ != nullptr; }

  // Performs account validation
  int AccountManagement(int flags) {
    return last_return_code_ = pam_acct_mgmt(pam_handle_, flags);
  }

  // Establishes or deletes PAM user credentials
  int SetCredentials(int flags) {
    return last_return_code_ = pam_setcred(pam_handle_, flags);
  }

  // Starts user session
  int OpenSession(int flags) {
    return last_return_code_ = pam_open_session(pam_handle_, flags);
  }

  // Ends user session
  int CloseSession(int flags) {
    return last_return_code_ = pam_close_session(pam_handle_, flags);
  }

  // Returns the current username according to PAM. It is possible for PAM
  // modules to change this from the initial value passed to the constructor.
  base::Optional<std::string> GetUser() {
    const char* user;
    last_return_code_ = pam_get_item(pam_handle_, PAM_USER,
                                     reinterpret_cast<const void**>(&user));
    if (last_return_code_ != PAM_SUCCESS || user == nullptr)
      return base::nullopt;
    return std::string(user);
  }

  // Obtains the list of environment variables provided by PAM modules.
  base::Optional<base::EnvironmentMap> GetEnvironment() {
    char** environment = pam_getenvlist(pam_handle_);

    if (environment == nullptr)
      return base::nullopt;

    base::EnvironmentMap environment_map;

    for (char** variable = environment; *variable != nullptr; ++variable) {
      char* delimiter = std::strchr(*variable, '=');
      if (delimiter != nullptr) {
        environment_map[std::string(*variable, delimiter)] =
            std::string(delimiter + 1);
      }
      std::free(*variable);
    }
    std::free(environment);

    return environment_map;
  }

  // Returns a description of the given return code
  const char* ErrorString(int return_code) {
    return pam_strerror(pam_handle_, return_code);
  }

  // Logs a fatal error if return_code isn't PAM_SUCCESS
  void CheckReturnCode(int return_code, base::StringPiece what) {
    if (return_code != PAM_SUCCESS) {
      LOG(FATAL) << "[PAM] " << what << ": " << ErrorString(return_code);
    }
  }

 private:
  pam_handle_t* pam_handle_ = nullptr;
  int last_return_code_ = PAM_SUCCESS;

  DISALLOW_COPY_AND_ASSIGN(PamHandle);
};

// Runs the me2me script in a PAM session. Exits the program on failure.
// If chown_log is true, the owner and group of the file associated with stdout
// will be changed to the target user.
void ExecuteSession(std::string user, base::StringPiece script_path,
                    bool chown_log) {
  //////////////////////////////////////////////////////////////////////////////
  // Set up the PAM session
  //////////////////////////////////////////////////////////////////////////////

  PamHandle pam_handle(kPamName, user.c_str(), &kPamConversation);
  CHECK(pam_handle.IsInitialized()) << "Failed to initialize PAM";

  // Make sure the account is valid and enabled.
  pam_handle.CheckReturnCode(pam_handle.AccountManagement(0), "Account check");

  // PAM may remap the user at any stage.
  user = pam_handle.GetUser().value_or(std::move(user));

  // setcred explicitly does not handle group membership, and specifies that
  // group membership should be established before calling setcred. PAM modules
  // may also use getpwnam, so pwinfo can only be assumed valid until the next
  // PAM call.
  errno = 0;
  struct passwd* pwinfo = getpwnam(user.c_str());
  PCHECK(pwinfo != nullptr) << "getpwnam failed";
  PCHECK(setgid(pwinfo->pw_gid) == 0) << "setgid failed";
  PCHECK(initgroups(pwinfo->pw_name, pwinfo->pw_gid) == 0)
      << "initgroups failed";

  // The documentation states that setcred should be called before open_session,
  // as done here, but it may be worth noting that `login` calls open_session
  // first.
  pam_handle.CheckReturnCode(pam_handle.SetCredentials(PAM_ESTABLISH_CRED),
                              "Set credentials");

  pam_handle.CheckReturnCode(pam_handle.OpenSession(0), "Open session");

  // The above may have remapped the user.
  user =  pam_handle.GetUser().value_or(std::move(user));

  base::Optional<base::EnvironmentMap> pam_environment =
      pam_handle.GetEnvironment();
  CHECK(pam_environment) << "Failed to get environment from PAM";

  //////////////////////////////////////////////////////////////////////////////
  // Prepare to execute remoting session process
  //////////////////////////////////////////////////////////////////////////////

  // Callback to be run in child process after fork and before exec.
  // chdir is called manually instead of using LaunchOptions.current_directory
  // because it should take place after setuid. (This both makes sure the user
  // has the proper permissions and also apparently avoids some obscure errors
  // that can occur when accessing some network filesystems as the wrong user.)
  class PreExecDelegate : public base::LaunchOptions::PreExecDelegate {
   public:
    void RunAsyncSafe() override {
      // Use RAW_CHECK to avoid allocating post-fork.
      RAW_CHECK(setuid(pwinfo_->pw_uid) == 0);
      RAW_CHECK(chdir(pwinfo_->pw_dir) == 0);
    }

    PreExecDelegate(struct passwd* pwinfo) : pwinfo_(pwinfo) {}

   private:
    struct passwd* pwinfo_;
  };

  // Fetch pwinfo again, as it may have been invalidated or the user name might
  // have been remapped.
  pwinfo = getpwnam(user.c_str());
  PCHECK(pwinfo != nullptr) << "getpwnam failed";

  if (chown_log) {
    int result = fchown(STDOUT_FILENO, pwinfo->pw_uid, pwinfo->pw_gid);
    PLOG_IF(WARNING, result != 0) << "Failed to change log file owner";
  }

  PreExecDelegate pre_exec_delegate(pwinfo);

  base::LaunchOptions launch_options;

  // Required to allow suid binaries to function in the session.
  launch_options.allow_new_privs = true;

  launch_options.kill_on_parent_death = true;

  launch_options.clear_environ = true;
  launch_options.environ = std::move(*pam_environment);
  launch_options.environ["USER"] = pwinfo->pw_name;
  launch_options.environ["LOGNAME"] = pwinfo->pw_name;
  launch_options.environ["HOME"] = pwinfo->pw_dir;
  launch_options.environ["SHELL"] = pwinfo->pw_shell;
  if (!launch_options.environ.count("PATH")) {
    launch_options.environ["PATH"] = "/bin:/usr/bin";
  }

  launch_options.pre_exec_delegate = &pre_exec_delegate;

  // By convention, a login shell is signified by preceeding the shell name in
  // argv[0] with a '-'.
  base::CommandLine command_line(base::FilePath(
      '-' + base::FilePath(pwinfo->pw_shell).BaseName().value()));

  base::Optional<std::string> escaped_script_path =
      ShellEscapeArgument(script_path);

  CHECK(escaped_script_path) << "Could not escape script path";

  command_line.AppendSwitch("-c");
  command_line.AppendArg(*escaped_script_path +
                         " --start --foreground --keep-parent-env");

  // Tell LaunchProcess where to find the executable, since argv[0] doesn't
  // point to it.
  launch_options.real_path = base::FilePath(pwinfo->pw_shell);

  //////////////////////////////////////////////////////////////////////////////
  // We're ready to execute the remoting session
  //////////////////////////////////////////////////////////////////////////////

  base::Process child = base::LaunchProcess(command_line, launch_options);

  if (child.IsValid()) {
    int exit_code = 0;
    // Die if wait fails so we don't close the PAM session while the child is
    // still running.
    CHECK(child.WaitForExit(&exit_code)) << "Failed to wait for child process";
    LOG_IF(WARNING, exit_code != 0) << "Child did not exit normally";
  }

  // Best effort PAM cleanup
  if (pam_handle.CloseSession(0) != PAM_SUCCESS) {
    LOG(WARNING) << "Failed to close PAM session";
  }
  ignore_result(pam_handle.SetCredentials(PAM_DELETE_CRED));
}

// Opens a temp file for logging. Exits the program on failure.
int OpenLogFile() {
  char logfile[265];
  std::time_t time = std::time(nullptr);
  CHECK_NE(time, (std::time_t)(-1));
  // Safe because we're single threaded
  std::tm* localtime = std::localtime(&time);
  CHECK_NE(std::strftime(logfile, sizeof(logfile), kLogFileTemplate, localtime),
           (std::size_t) 0);

  mode_t mode = umask(0177);
  int fd = mkstemp(logfile);
  PCHECK(fd != -1);
  umask(mode);

  return fd;
}

// Daemonizes the process. Output is redirected to a file. Exits the program on
// failure.
//
// This logic is mostly the same as daemonize() in linux_me2me_host.py. Log-
// file redirection especially should be kept in sync. Note that this does
// not currently wait for the host to start successfully before exiting the
// parent process like the Python script does, as that functionality is
// probably not useful at boot, where the wrapper is expected to be used. If
// it turns out to be desired, it can be implemented by setting up a pipe and
// passing a file descriptor to the Python script.
void Daemonize() {

  int log_fd = OpenLogFile();
  int devnull_fd = open("/dev/null", O_RDONLY);
  PCHECK(devnull_fd != -1);

  PCHECK(dup2(devnull_fd, STDIN_FILENO) != -1);
  PCHECK(dup2(log_fd, STDOUT_FILENO) != -1);
  PCHECK(dup2(log_fd, STDERR_FILENO) != -1);

  // Close all file descriptors except stdio, including any we may have
  // inherited.
  base::CloseSuperfluousFds(base::InjectiveMultimap());

  // Allow parent to exit, and ensure we're not a session leader so setsid can
  // succeed
  pid_t pid = fork();
  PCHECK(pid != -1);

  if (pid != 0) {
    std::exit(EXIT_SUCCESS);
  }

  // Start a new process group and session with no controlling terminal.
  PCHECK(setsid() != -1);

  // Fork again so we're no longer a session leader and can't get a controlling
  // terminal.
  pid = fork();
  PCHECK(pid != -1);

  if (pid != 0) {
    std::exit(EXIT_SUCCESS);
  }

  // We don't want to change to the target user's home directory until we've
  // dropped privileges, so change to / to make sure we're not keeping any other
  // directory in use.
  PCHECK(chdir("/") == 0);

  // Done!
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    PrintUsage(command_line->GetProgram());
    std::exit(EXIT_SUCCESS);
  }

  base::FilePath script_path =
      command_line->GetSwitchValuePath(kScriptSwitchName);
  std::string user = command_line->GetSwitchValueNative(kUserSwitchName);
  bool foreground = command_line->HasSwitch(kForegroundSwitchName);

  if (script_path.empty()) {
    std::fputs("The path to the me2me python script is required.\n", stderr);
    std::exit(EXIT_FAILURE);
  }

  if (user.empty()) {
    std::fputs("The target user must be specified.\n", stderr);
    std::exit(EXIT_FAILURE);
  }

  if (!foreground) {
    Daemonize();
  }

  ExecuteSession(std::move(user), script_path.value(), !foreground);
}
