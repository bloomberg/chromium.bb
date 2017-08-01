// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a wrapper to run the virtual me2me session within a
// proper PAM session. It will generally be run as root and drop privileges to
// the specified user before running the me2me session script.

// Usage: user-session start [--foreground] [user]
//
// Options:
//   --foreground  - Don't daemonize.
//   user          - Create a session for the specified user. Required when
//                   running as root, not allowed when running as a normal user.

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
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

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/strings/string_piece.h"

namespace {

const char kPamName[] = "chrome-remote-desktop";
const char kScriptName[] = "chrome-remote-desktop";
const char kStartCommand[] = "start";
const char kForegroundFlag[] = "--foreground";

// This template will be formatted by strftime and then used by mkstemp
const char kLogFileTemplate[] =
    "/tmp/chrome_remote_desktop_%Y%m%d_%H%M%S_XXXXXX";

const char kUsageMessage[] =
    "This program is not intended to be run by end users. To configure Chrome\n"
    "Remote Desktop, please install the app from the Chrome Web Store:\n"
    "https://chrome.google.com/remotedesktop\n";

void PrintUsage() {
  std::fputs(kUsageMessage, stderr);
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

std::string FindScriptPath() {
  base::FilePath path;
  bool result = base::ReadSymbolicLink(base::FilePath("/proc/self/exe"), &path);
  PCHECK(result) << "Failed to determine binary location";
  CHECK(path.IsAbsolute()) << "Retrieved binary location not absolute";

  return path.DirName().Append(kScriptName).value();
}

// Execs the me2me script.
// This function is called after forking and dropping privileges. It never
// returns.
void ExecMe2MeScript(base::EnvironmentMap environment,
                     const struct passwd* pwinfo) {
  // By convention, a login shell is signified by preceeding the shell name in
  // argv[0] with a '-'.
  std::string shell_name =
      '-' + base::FilePath(pwinfo->pw_shell).BaseName().value();

  base::Optional<std::string> escaped_script_path =
      ShellEscapeArgument(FindScriptPath());
  CHECK(escaped_script_path) << "Could not escape script path";

  std::string shell_arg =
      *escaped_script_path + " --start --foreground --keep-parent-env";

  environment["USER"] = pwinfo->pw_name;
  environment["LOGNAME"] = pwinfo->pw_name;
  environment["HOME"] = pwinfo->pw_dir;
  environment["SHELL"] = pwinfo->pw_shell;
  if (!environment.count("PATH")) {
    environment["PATH"] = "/bin:/usr/bin";
  }

  std::vector<std::string> env_strings;
  for (const auto& env_var : environment) {
    env_strings.emplace_back(env_var.first + "=" + env_var.second);
  }

  std::vector<const char*> arg_ptrs = {shell_name.c_str(), "-c",
                                       shell_arg.c_str(), nullptr};
  std::vector<const char*> env_ptrs;
  env_ptrs.reserve(env_strings.size() + 1);
  for (const auto& env_string : env_strings) {
    env_ptrs.push_back(env_string.c_str());
  }
  env_ptrs.push_back(nullptr);

  execve(pwinfo->pw_shell, const_cast<char* const*>(arg_ptrs.data()),
         const_cast<char* const*>(env_ptrs.data()));
  PLOG(FATAL) << "Failed to exec login shell " << pwinfo->pw_shell;
}

// Runs the me2me script in a PAM session. Exits the program on failure.
// If chown_log is true, the owner and group of the file associated with stdout
// will be changed to the target user. If match_uid is specified, this function
// will fail if the final user id does not match the one provided.
void ExecuteSession(std::string user,
                    bool chown_log,
                    base::Optional<uid_t> match_uid) {
  PamHandle pam_handle(kPamName, user.c_str(), &kPamConversation);
  CHECK(pam_handle.IsInitialized()) << "Failed to initialize PAM";

  // Make sure the account is valid and enabled.
  pam_handle.CheckReturnCode(pam_handle.AccountManagement(0), "Account check");

  // PAM may remap the user at any stage.
  user = pam_handle.GetUser().value_or(std::move(user));

  // setcred explicitly does not handle user id or group membership, and
  // specifies that they should be established before calling setcred. Only the
  // real user id is set here, as we still require root privileges. PAM modules
  // may use getpwnam, so pwinfo can only be assumed valid until the next PAM
  // call.
  errno = 0;
  struct passwd* pwinfo = getpwnam(user.c_str());
  PCHECK(pwinfo != nullptr) << "getpwnam failed";
  PCHECK(setreuid(pwinfo->pw_uid, -1) == 0) << "setreuid failed";
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

  // Fetch pwinfo again, as it may have been invalidated or the user name might
  // have been remapped.
  pwinfo = getpwnam(user.c_str());
  PCHECK(pwinfo != nullptr) << "getpwnam failed";

  if (match_uid && pwinfo->pw_uid != *match_uid) {
    LOG(FATAL) << "PAM remapped username to one with a different user ID.";
  }

  if (chown_log) {
    int result = fchown(STDOUT_FILENO, pwinfo->pw_uid, pwinfo->pw_gid);
    PLOG_IF(WARNING, result != 0) << "Failed to change log file owner";
  }

  pid_t child_pid = fork();
  PCHECK(child_pid >= 0) << "fork failed";
  if (child_pid == 0) {
    PCHECK(setuid(pwinfo->pw_uid) == 0) << "setuid failed";
    PCHECK(chdir(pwinfo->pw_dir) == 0) << "chdir to $HOME failed";
    base::Optional<base::EnvironmentMap> pam_environment =
        pam_handle.GetEnvironment();
    CHECK(pam_environment) << "Failed to get environment from PAM";
    ExecMe2MeScript(std::move(*pam_environment), pwinfo);  // Never returns
  } else {
    // waitpid will return if the child is ptraced, so loop until the process
    // actually exits.
    int status;
    do {
      pid_t wait_result = waitpid(child_pid, &status, 0);

      // Die if wait fails so we don't close the PAM session while the child is
      // still running.
      PCHECK(wait_result >= 0) << "wait failed";
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == EXIT_SUCCESS) {
        LOG(INFO) << "Child exited successfully";
      } else {
        LOG(WARNING) << "Child exited with status " << WEXITSTATUS(status);
      }
    } else if (WIFSIGNALED(status)) {
      LOG(WARNING) << "Child terminated by signal " << WTERMSIG(status);
    }

    // Best effort PAM cleanup
    if (pam_handle.CloseSession(0) != PAM_SUCCESS) {
      LOG(WARNING) << "Failed to close PAM session";
    }
    ignore_result(pam_handle.SetCredentials(PAM_DELETE_CRED));
  }
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

// Find the username for the current user. If either USER or LOGNAME is set to
// a user matching our real user id, we return that. Otherwise, we use getpwuid
// to attempt a reverse lookup. Note: It's possible for multiple usernames to
// share the same user id (e.g., to allow a user to have logins with different
// home directories or group membership, but be considered the same user as far
// as file permissions are concerned). Consulting USER/LOGNAME allows us to pick
// the correct entry in these circumstances.
std::string FindCurrentUsername() {
  uid_t real_uid = getuid();
  struct passwd* pwinfo;
  for (const char* var : {"USER", "LOGNAME"}) {
    const char* value = getenv(var);
    if (value) {
      pwinfo = getpwnam(value);
      // USER and LOGNAME can be overridden, so make sure the value is valid
      // and matches the UID of the invoking user.
      if (pwinfo && pwinfo->pw_uid == real_uid) {
        return pwinfo->pw_name;
      }
    }
  }
  errno = 0;
  pwinfo = getpwuid(real_uid);
  PCHECK(pwinfo) << "getpwuid failed";
  return pwinfo->pw_name;
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
  if (argc < 2 || std::strcmp(argv[1], kStartCommand) != 0) {
    PrintUsage();
    std::exit(EXIT_FAILURE);
  }

  // Skip initial args
  argc -= 2;
  argv += 2;

  bool foreground = false;
  if (argc >= 1 && std::strcmp(argv[0], kForegroundFlag) == 0) {
    foreground = true;
    argc -= 1;
    argv += 1;
  }

  if (argc > 1) {
    std::fputs("Too many command-line arguments.\n", stderr);
    std::exit(EXIT_FAILURE);
  }

  uid_t real_uid = getuid();
  std::string user;

  // Note: This logic is security sensitive. It is imperative that a non-root
  // user is not allowed to specify an arbitrary target user.
  if (argc == 0) {
    if (real_uid == 0) {
      std::fputs("Target user must be specified when run as root.\n", stderr);
      std::exit(EXIT_FAILURE);
    }
    user = FindCurrentUsername();
  } else {
    if (real_uid != 0) {
      std::fputs("Target user may not be specified by non-root users.\n",
                 stderr);
      std::exit(EXIT_FAILURE);
    }
    user = argv[0];
  }

  if (!foreground) {
    Daemonize();
  }

  // Daemonizing redirects stdout to a log file, which we want to be owned by
  // the target user.
  bool chown_stdout = !foreground;
  ExecuteSession(std::move(user), chown_stdout,
                 real_uid != 0 ? base::make_optional(real_uid) : base::nullopt);
}
