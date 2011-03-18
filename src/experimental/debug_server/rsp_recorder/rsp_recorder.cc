#include "rsp_recorder.h"

namespace {
const int kListenMs = 20;
const int kReadWaitMs = 20;
const int kBufferSize = 512;
}

namespace rsp_recorder {
Recorder::Recorder()
  : state_(STOPPED),
    host_port_(0),
    target_port_(0) {
  host_packet_consumer_.rec_ = this;
  target_packet_consumer_.rec_ = this;
}

Recorder::~Recorder() {
  Stop();
  session_log_.Close();
}

void Recorder::SetSessionLogFileName(const std::string& name) {
  session_log_.OpenToWrite(name);
}

void Recorder::Start(int host_port,
                     const std::string& target_hostname,
                     int target_port) {
  Stop();
  host_port_ = host_port;
  target_port_ = target_port;
  target_hostname_ = target_hostname;
  host_listening_socket_.Setup(host_port_);
  host_packetizer_.SetPacketConsumer(&host_packet_consumer_);
  target_packetizer_.SetPacketConsumer(&target_packet_consumer_);
  state_ = CONNECTING;
}
void Recorder::Stop() {
  host_listening_socket_.Close();
  host_connection_.Close();
  target_connection_.Close();
  state_ = STOPPED;
}

bool Recorder::IsRunning() const {
  return (STOPPED != state_);
}

void Recorder::DoWork() {
  if (STOPPED == state_)
    return;

  // Recorder always connects to the target first, then accepts
  // connection from the host (gdb).
  if (CONNECTING == state_) {
    if (!target_connection_.IsConnected()) {
      target_connection_.ConnectTo(target_hostname_, target_port_);
      if (target_connection_.IsConnected())
        printf("Connected to target on [%s:%d]\n",
               target_hostname_.c_str(),
               target_port_);
    } else if (!host_connection_.IsConnected()) {
      host_listening_socket_.Accept(&host_connection_, kListenMs);
      if (host_connection_.IsConnected())
        printf("Got connection from host\n");
    } else {
      state_ = CONNECTED;
    }
    return;
  }

  if (CONNECTED == state_) {
    if (!target_connection_.IsConnected() || !host_connection_.IsConnected()) {
      Stop();
      return;
    }
  }

  char buff[kBufferSize];
	size_t read_bytes = host_connection_.Read(buff,
                                            sizeof(buff) - 1,
                                            kReadWaitMs);
  if (read_bytes > 0) {
    buff[read_bytes] = 0;
    printf("h>%s\n", buff);
    host_packetizer_.OnData(buff, read_bytes);
    target_connection_.WriteAll(buff, read_bytes);
  }

	read_bytes = target_connection_.Read(buff, sizeof(buff) - 1, kReadWaitMs);
  if (read_bytes > 0) {
    buff[read_bytes] = 0;
    printf("t>%s\n", buff);
    target_packetizer_.OnData(buff, read_bytes);
    host_connection_.WriteAll(buff, read_bytes);
  }
}

void Recorder::OnError(const char* error_description) {
  printf("Recorder ERROR!!! : [%s]\n", error_description);
  fflush(stdout);
  Stop();
}

void HostPacketConsumer::OnPacket(debug::Blob& body, bool valid_checksum) {
  rec_->OnHostPacket(body, valid_checksum);
}

void HostPacketConsumer::OnUnexpectedChar(char unexpected_char) {
  rec_->OnHostUnexpectedChar(unexpected_char);
}

void HostPacketConsumer::OnBreak() {
  rec_->OnHostBreak();
}

void TargetPacketConsumer::OnPacket(debug::Blob& body, bool valid_checksum) {
  rec_->OnTargetPacket(body, valid_checksum);
}

void TargetPacketConsumer::OnUnexpectedChar(char unexpected_char) {
  rec_->OnTargetUnexpectedChar(unexpected_char);
}

void TargetPacketConsumer::OnBreak() {
  rec_->OnTargetBreak();
}

void Recorder::OnHostPacket(debug::Blob& body, bool valid_checksum) {
  if (!valid_checksum)
    OnError("Invalid checksum on message from host.");
  else {
    std::string body_str = body.ToString();
    printf("H>[%s] chk=%s\n",
           body_str.c_str(),
           (valid_checksum ? "ok" : "err"));
    session_log_.Add('H', body);
  }
}

void Recorder::OnHostUnexpectedChar(char unexpected_char) {
  printf("H>enexpected [%c]\n", unexpected_char);
  OnError("Received unexpected character from host.");
}

void Recorder::OnHostBreak() {
  printf("H>Brk\n");
  debug::Blob body;
  body.PushBack(0x03); // Ctrl-C
  session_log_.Add('H', body);
}

void Recorder::OnTargetPacket(debug::Blob& body, bool valid_checksum) {
  std::string body_str = body.ToString();
  printf("T>[%s] chk=%s\n", body_str.c_str(), (valid_checksum ? "ok" : "err"));
  session_log_.Add('T', body);
}

void Recorder::OnTargetUnexpectedChar(char unexpected_char) {
  printf("T>enexpected [%c]\n", unexpected_char);
  OnError("Received unexpected character from targer.");
}

void Recorder::OnTargetBreak() {
  printf("T>Brk\n");
  OnError("Received Ctrl-C from targer.");
}
}  // namespace rsp_recorder
