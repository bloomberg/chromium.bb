#ifndef NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_RSP_RECORDER_RSP_RECORDER_H_
#define NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_RSP_RECORDER_RSP_RECORDER_H_

#include "..\common\debug_socket.h"
#include "..\common\rsp_packetizer.h"
#include "..\common\rsp_session_log.h"

namespace rsp_recorder {
class Recorder;
class HostPacketConsumer : public rsp::PacketConsumer {
public: 
  HostPacketConsumer() : rec_(NULL) {}
  virtual void OnPacket(debug::Blob& body, bool valid_checksum);
  virtual void OnUnexpectedChar(char unexpected_char);
  virtual void OnBreak();

  rsp_recorder::Recorder* rec_;
};

class TargetPacketConsumer : public rsp::PacketConsumer {
public: 
  TargetPacketConsumer() : rec_(NULL) {}
  virtual void OnPacket(debug::Blob& body, bool valid_checksum);
  virtual void OnUnexpectedChar(char unexpected_char);
  virtual void OnBreak();

  rsp_recorder::Recorder* rec_;
};

class Recorder {
public:
  Recorder();
  ~Recorder();

  void SetSessionLogFileName(const std::string& name);
  void Start(int host_port,
             const std::string& target_hostname,
             int target_port);
  void DoWork();
  void Stop();
  bool IsRunning() const;

protected:
  enum State {STOPPED, CONNECTING, CONNECTED};

  void OnHostPacket(debug::Blob& body, bool valid_checksum);
  void OnHostUnexpectedChar(char unexpected_char);
  void OnHostBreak();
  void OnTargetPacket(debug::Blob& body, bool valid_checksum);
  void OnTargetUnexpectedChar(char unexpected_char);
  void OnTargetBreak();
  void OnError(const char* error_description);

  State state_;
  rsp::SessionLog session_log_;
  int host_port_;
  int target_port_;
  std::string target_hostname_;
  debug::ListeningSocket host_listening_socket_;
  debug::Socket host_connection_;
  debug::Socket target_connection_;
  rsp::Packetizer host_packetizer_;
  rsp::Packetizer target_packetizer_;
  HostPacketConsumer host_packet_consumer_;
  TargetPacketConsumer target_packet_consumer_;

  friend class HostPacketConsumer;
  friend class TargetPacketConsumer;
};
}  // namespace rsp_recorder

#endif  // NACL_SDK_BUILD_TOOLS_DEBUG_SERVER_RSP_RECORDER_RSP_RECORDER_H_