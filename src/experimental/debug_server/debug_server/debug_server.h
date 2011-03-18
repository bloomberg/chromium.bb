#pragma once
#include <string>
#include "..\common\debug_socket.h"
#include "debug_core.h"

namespace debug {
namespace rsp {
class Server {
public:
  Server();
  ~Server();

  bool Start(int listen_port,
             const std::string& cmd,
             const std::string& work_dir,
             std::string* error);
  void DoWork();
  void Stop();
  bool IsRunning() const;

 protected:
  void OnHostConnected();
  void OnHostDisconnected();

  debug::ListeningSocket listening_socket_;
  debug::Socket host_connection_;
  bool host_is_connected_;
  debug::Core debug_core_;
};
}  // namespace rsp
}  // namespace debug
