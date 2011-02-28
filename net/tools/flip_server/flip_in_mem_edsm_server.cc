// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <iostream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/timer.h"
#include "net/tools/flip_server/acceptor_thread.h"
#include "net/tools/flip_server/constants.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/output_ordering.h"
#include "net/tools/flip_server/sm_connection.h"
#include "net/tools/flip_server/sm_interface.h"
#include "net/tools/flip_server/spdy_interface.h"
#include "net/tools/flip_server/streamer_interface.h"
#include "net/tools/flip_server/split.h"

using std::cout;
using std::cerr;

// If true, then disables the nagle algorithm);
bool FLAGS_disable_nagle = true;

// The number of times that accept() will be called when the
//  alarm goes off when the accept_using_alarm flag is set to true.
//  If set to 0, accept() will be performed until the accept queue
//  is completely drained and the accept() call returns an error);
int32 FLAGS_accepts_per_wake = 0;

// The size of the TCP accept backlog);
int32 FLAGS_accept_backlog_size = 1024;

// If set to false a single socket will be used. If set to true
//  then a new socket will be created for each accept thread.
//  Note that this only works with kernels that support
//  SO_REUSEPORT);
bool FLAGS_reuseport = false;

// Flag to force spdy, even if NPN is not negotiated.
bool FLAGS_force_spdy = false;

// The amount of time the server delays before sending back the
//  reply);
double FLAGS_server_think_time_in_s = 0;

net::FlipConfig g_proxy_config;

////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> &split(const std::string &s,
                                char delim,
                                std::vector<std::string> &elems) {
  std::stringstream ss(s);
  std::string item;
  while(std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  return split(s, delim, elems);
}

bool GotQuitFromStdin() {
  // Make stdin nonblocking. Yes this is done each time. Oh well.
  fcntl(0, F_SETFL, O_NONBLOCK);
  char c;
  std::string maybequit;
  while (read(0, &c, 1) > 0) {
    maybequit += c;
  }
  if (maybequit.size()) {
    VLOG(1) << "scanning string: \"" << maybequit << "\"";
  }
  return (maybequit.size() > 1 &&
          (maybequit.c_str()[0] == 'q' ||
           maybequit.c_str()[0] == 'Q'));
}

const char* BoolToStr(bool b) {
  if (b)
    return "true";
  return "false";
}

////////////////////////////////////////////////////////////////////////////////

static bool wantExit = false;
static bool wantLogClose = false;
void SignalHandler(int signum)
{
  switch(signum) {
    case SIGTERM:
    case SIGINT:
      wantExit = true;
      break;
    case SIGHUP:
      wantLogClose = true;
      break;
  }
}

static int OpenPidFile(const char *pidfile)
{
  int fd;
  struct stat pid_stat;
  int ret;

  fd = open(pidfile, O_RDWR | O_CREAT, 0600);
  if (fd == -1) {
      cerr << "Could not open pid file '" << pidfile << "' for reading.\n";
      exit(1);
  }

  ret = flock(fd, LOCK_EX | LOCK_NB);
  if (ret == -1) {
    if (errno == EWOULDBLOCK) {
      cerr << "Flip server is already running.\n";
    } else {
      cerr << "Error getting lock on pid file: " << strerror(errno) << "\n";
    }
    exit(1);
  }

  if (fstat(fd, &pid_stat) == -1) {
    cerr << "Could not stat pid file '" << pidfile << "': " << strerror(errno)
         << "\n";
  }
  if (pid_stat.st_size != 0) {
    if (ftruncate(fd, pid_stat.st_size) == -1) {
      cerr << "Could not truncate pid file '" << pidfile << "': "
           << strerror(errno) << "\n";
    }
  }

  char pid_str[8];
  snprintf(pid_str, sizeof(pid_str), "%d", getpid());
  int bytes = static_cast<int>(strlen(pid_str));
  if (write(fd, pid_str, strlen(pid_str)) != bytes) {
    cerr << "Could not write pid file: " << strerror(errno) << "\n";
    close(fd);
    exit(1);
  }

  return fd;
}

int main (int argc, char**argv)
{
  unsigned int i = 0;
  bool wait_for_iface = false;
  int pidfile_fd;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGTERM, SignalHandler);
  signal(SIGINT, SignalHandler);
  signal(SIGHUP, SignalHandler);

  CommandLine::Init(argc, argv);
  CommandLine cl(argc, argv);

  if (cl.HasSwitch("help") || argc < 2) {
    cout << argv[0] << " <options>\n";
    cout << "  Proxy options:\n";
    cout << "\t--proxy<1..n>=\"<listen ip>,<listen port>,"
         << "<ssl cert filename>,\n"
         << "\t               <ssl key filename>,<http server ip>,"
         << "<http server port>,\n"
         << "\t               [https server ip],[https server port],"
         << "<spdy only 0|1>\"\n";
    cout << "\t  * The https server ip and port may be left empty if they are"
         << " the same as\n"
         << "\t    the http server fields.\n";
    cout << "\t  * spdy only prevents non-spdy https connections from being"
         << " passed\n"
         << "\t    through the proxy listen ip:port.\n";
    cout << "\t--forward-ip-header=<header name>\n";
    cout << "\n  Server options:\n";
    cout << "\t--spdy-server=\"<listen ip>,<listen port>,[ssl cert filename],"
         << "\n\t               [ssl key filename]\"\n";
    cout << "\t--http-server=\"<listen ip>,<listen port>,[ssl cert filename],"
         << "\n\t               [ssl key filename]\"\n";
    cout << "\t  * Leaving the ssl cert and key fields empty will disable ssl"
         << " for the\n"
         << "\t    http and spdy flip servers\n";
    cout << "\n  Global options:\n";
    cout << "\t--logdest=<file|system|both>\n";
    cout << "\t--logfile=<logfile>\n";
    cout << "\t--wait-for-iface\n";
    cout << "\t  * The flip server will block until the listen ip has been"
         << " raised.\n";
    cout << "\t--ssl-session-expiry=<seconds> (default is 300)\n";
    cout << "\t--ssl-disable-compression\n";
    cout << "\t--idle-timeout=<seconds> (default is 300)\n";
    cout << "\t--pidfile=<filepath> (default /var/run/flip-server.pid)\n";
    cout << "\t--help\n";
    exit(0);
  }

  if (cl.HasSwitch("pidfile")) {
    pidfile_fd = OpenPidFile(cl.GetSwitchValueASCII("pidfile").c_str());
  } else {
    pidfile_fd = OpenPidFile(PIDFILE);
  }

  net::OutputOrdering::set_server_think_time_in_s(FLAGS_server_think_time_in_s);

  if (cl.HasSwitch("forward-ip-header")) {
    net::SpdySM::set_forward_ip_header(
        cl.GetSwitchValueASCII("forward-ip-header"));
    net::StreamerSM::set_forward_ip_header(
        cl.GetSwitchValueASCII("forward-ip-header"));
  }

  if (cl.HasSwitch("logdest")) {
    std::string log_dest_value = cl.GetSwitchValueASCII("logdest");
    if (log_dest_value.compare("file") == 0) {
      g_proxy_config.log_destination_ = logging::LOG_ONLY_TO_FILE;
    } else if (log_dest_value.compare("system") == 0) {
      g_proxy_config.log_destination_ = logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG;
    } else if (log_dest_value.compare("both") == 0) {
      g_proxy_config.log_destination_ =
        logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
    } else {
      LOG(FATAL) << "Invalid logging destination value: " << log_dest_value;
    }
  } else {
    g_proxy_config.log_destination_ = logging::LOG_NONE;
  }

  if (cl.HasSwitch("logfile")) {
    g_proxy_config.log_filename_ = cl.GetSwitchValueASCII("logfile");
    if (g_proxy_config.log_destination_ == logging::LOG_NONE) {
      g_proxy_config.log_destination_ = logging::LOG_ONLY_TO_FILE;
    }
  } else if (g_proxy_config.log_destination_ == logging::LOG_ONLY_TO_FILE ||
             g_proxy_config.log_destination_ ==
             logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG) {
    LOG(FATAL) << "Logging destination requires a log file to be specified.";
  }

  if (cl.HasSwitch("wait-for-iface")) {
    wait_for_iface = true;
  }

  if (cl.HasSwitch("ssl-session-expiry")) {
    std::string session_expiry = cl.GetSwitchValueASCII("ssl-session-expiry");
    g_proxy_config.ssl_session_expiry_ = atoi(session_expiry.c_str());
  }

  if (cl.HasSwitch("ssl-disable-compression")) {
    g_proxy_config.ssl_disable_compression_ = true;
  }

  if (cl.HasSwitch("idle-timeout")) {
    g_proxy_config.idle_socket_timeout_s_ =
      atoi(cl.GetSwitchValueASCII("idle-timeout").c_str());
  }

  if (cl.HasSwitch("force_spdy"))
    net::SMConnection::set_force_spdy(true);

  InitLogging(g_proxy_config.log_filename_.c_str(),
              g_proxy_config.log_destination_,
              logging::DONT_LOCK_LOG_FILE,
              logging::APPEND_TO_OLD_LOG_FILE,
              logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  LOG(INFO) << "Flip SPDY proxy started with configuration:";
  LOG(INFO) << "Logging destination     : " << g_proxy_config.log_destination_;
  LOG(INFO) << "Log file                : " << g_proxy_config.log_filename_;
  LOG(INFO) << "Forward IP Header       : "
            << (net::SpdySM::forward_ip_header().length() ?
               net::SpdySM::forward_ip_header() : "<disabled>");
  LOG(INFO) << "Wait for interfaces     : " << (wait_for_iface?"true":"false");
  LOG(INFO) << "Accept backlog size     : " << FLAGS_accept_backlog_size;
  LOG(INFO) << "Accepts per wake        : " << FLAGS_accepts_per_wake;
  LOG(INFO) << "Disable nagle           : "
            << (FLAGS_disable_nagle?"true":"false");
  LOG(INFO) << "Reuseport               : "
            << (FLAGS_reuseport?"true":"false");
  LOG(INFO) << "Force SPDY              : "
            << (FLAGS_force_spdy?"true":"false");
  LOG(INFO) << "SSL session expiry      : "
            << g_proxy_config.ssl_session_expiry_;
  LOG(INFO) << "SSL disable compression : "
            << g_proxy_config.ssl_disable_compression_;
  LOG(INFO) << "Connection idle timeout : "
            << g_proxy_config.idle_socket_timeout_s_;

  // Proxy Acceptors
  while (true) {
    i += 1;
    std::stringstream name;
    name << "proxy" << i;
    if (!cl.HasSwitch(name.str())) {
      break;
    }
    std::string value = cl.GetSwitchValueASCII(name.str());
    std::vector<std::string> valueArgs = split(value, ',');
    CHECK_EQ((unsigned int)9, valueArgs.size());
    int spdy_only = atoi(valueArgs[8].c_str());
    // If wait_for_iface is enabled, then this call will block
    // indefinitely until the interface is raised.
    g_proxy_config.AddAcceptor(net::FLIP_HANDLER_PROXY,
                               valueArgs[0], valueArgs[1],
                               valueArgs[2], valueArgs[3],
                               valueArgs[4], valueArgs[5],
                               valueArgs[6], valueArgs[7],
                               spdy_only,
                               FLAGS_accept_backlog_size,
                               FLAGS_disable_nagle,
                               FLAGS_accepts_per_wake,
                               FLAGS_reuseport,
                               wait_for_iface,
                               NULL);
  }

  // Spdy Server Acceptor
  net::MemoryCache spdy_memory_cache;
  if (cl.HasSwitch("spdy-server")) {
    spdy_memory_cache.AddFiles();
    std::string value = cl.GetSwitchValueASCII("spdy-server");
    std::vector<std::string> valueArgs = split(value, ',');
    g_proxy_config.AddAcceptor(net::FLIP_HANDLER_SPDY_SERVER,
                               valueArgs[0], valueArgs[1],
                               valueArgs[2], valueArgs[3],
                               "", "", "", "",
                               0,
                               FLAGS_accept_backlog_size,
                               FLAGS_disable_nagle,
                               FLAGS_accepts_per_wake,
                               FLAGS_reuseport,
                               wait_for_iface,
                               &spdy_memory_cache);
  }

  // Spdy Server Acceptor
  net::MemoryCache http_memory_cache;
  if (cl.HasSwitch("http-server")) {
    http_memory_cache.AddFiles();
    std::string value = cl.GetSwitchValueASCII("http-server");
    std::vector<std::string> valueArgs = split(value, ',');
    g_proxy_config.AddAcceptor(net::FLIP_HANDLER_HTTP_SERVER,
                               valueArgs[0], valueArgs[1],
                               valueArgs[2], valueArgs[3],
                               "", "", "", "",
                               0,
                               FLAGS_accept_backlog_size,
                               FLAGS_disable_nagle,
                               FLAGS_accepts_per_wake,
                               FLAGS_reuseport,
                               wait_for_iface,
                               &http_memory_cache);
  }

  std::vector<net::SMAcceptorThread*> sm_worker_threads_;

  for (i = 0; i < g_proxy_config.acceptors_.size(); i++) {
    net::FlipAcceptor *acceptor = g_proxy_config.acceptors_[i];

    sm_worker_threads_.push_back(
        new net::SMAcceptorThread(acceptor,
                                  (net::MemoryCache *)acceptor->memory_cache_));
    // Note that spdy_memory_cache is not threadsafe, it is merely
    // thread compatible. Thus, if ever we are to spawn multiple threads,
    // we either must make the MemoryCache threadsafe, or use
    // a separate MemoryCache for each thread.
    //
    // The latter is what is currently being done as we spawn
    // a separate thread for each http and spdy server acceptor.

    sm_worker_threads_.back()->InitWorker();
    sm_worker_threads_.back()->Start();
  }

  while (!wantExit) {
    // Close logfile when HUP signal is received. Logging system will
    // automatically reopen on next log message.
    if ( wantLogClose ) {
      wantLogClose = false;
      VLOG(1) << "HUP received, reopening log file.";
      logging::CloseLogFile();
    }
    if (GotQuitFromStdin()) {
      for (unsigned int i = 0; i < sm_worker_threads_.size(); ++i) {
        sm_worker_threads_[i]->Quit();
      }
      for (unsigned int i = 0; i < sm_worker_threads_.size(); ++i) {
        sm_worker_threads_[i]->Join();
      }
      break;
    }
    usleep(1000*10);  // 10 ms
  }

  unlink(PIDFILE);
  close(pidfile_fd);
  return 0;
}

