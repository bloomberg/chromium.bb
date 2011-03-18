#ifndef NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_RSP_PLAYER_RSP_PLAYER_H_
#define NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_RSP_PLAYER_RSP_PLAYER_H_

#include "..\common\rsp_session_log.h"
#include "..\common\rsp_packetizer.h"
#include "..\common\debug_socket.h"
#include "..\common\debug_blob.h"

namespace rsp_player {

class Player : public rsp::PacketConsumer {
public:
  Player();
  ~Player();

  bool SetSessionLogFileName(const std::string& name);
  void Start(const std::string& target_host, int target_port);
  void DoWork();
  void Stop();
  bool IsRunning() const;
  bool IsFailed() const;
  void GetErrorDescription(std::string* error) const;

protected:
  enum State {STOPPED, 
              CONNECTING, 
              CONNECTED, 
              WAIT_FOR_ACK, 
              WAIT_FOR_TARGET_REPLY, 
              END_FAILED, 
              END_SUCCESS};

  void SetState(State state);
  void SetCurrentThreadId(const debug::Blob& id);
  void OnPacket(debug::Blob& body, bool valid_checksum);
  void OnUnexpectedChar(char unexpected_char);
  void OnBreak();
  void OnLoggedMsgFromHost(debug::Blob& msg_in);
  void OnLoggedMsgFromTarget(debug::Blob& msg_in);
  void UpdateDebuggeeThread(bool msg_is_from_host, const debug::Blob& message);
  void StartTimer(int seconds);
  bool MatchReceivedMessageToLogged(const debug::Blob& received_message,
                                    const debug::Blob& logged_message);
  void CheckTimers();
  void OnError(const char* error_description);
  void PatchHostMessage(const debug::Blob& msg_in, debug::Blob* msg_out);

  State state_;
  std::string error_description_;
  rsp::SessionLog session_log_;
  std::string target_host_;
  int target_port_;
  debug::Socket target_connection_;
  rsp::Packetizer target_packetizer_;
  debug::Blob logged_message_from_taget_;
  debug::Blob last_message_to_target_;
  debug::Blob current_thread_id_;
  time_t timer_ts_;
};
}  // namespace rsp_player

#endif  // NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_RSP_PLAYER_RSP_PLAYER_H_