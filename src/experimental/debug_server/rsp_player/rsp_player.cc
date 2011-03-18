#include "rsp_player.h"
#include <time.h>

#pragma warning(disable : 4996)

namespace {
const int kReadWaitMs = 20;
const int kTargetWaitSecs = 10;
const int kBufferSize = 512;
}

namespace rsp_player {
Player::Player() 
    : state_(STOPPED),
      target_port_(0),
      timer_ts_(0) {
  SetState(STOPPED);
}

Player::~Player() {
  Stop();
  session_log_.Close();
}

bool Player::SetSessionLogFileName(const std::string& name) {
  return session_log_.OpenToRead(name);
}

void Player::SetState(State state) {
  state_ = state;
}

void Player::SetCurrentThreadId(const debug::Blob& id) {
  current_thread_id_ = id;
}

bool Player::IsFailed() const {
  return (END_FAILED == state_);
}

void Player::GetErrorDescription(std::string* error) const {
  if (NULL != error)
    *error = error_description_;
}

void Player::Start(const std::string& target_host, int target_port) {
  target_host_ = target_host;
  target_port_ = target_port;
  target_packetizer_.SetPacketConsumer(this);
  error_description_.clear();
  SetState(CONNECTING);
}

void Player::DoWork() {
  if (!IsRunning())
    return;

  if (CONNECTING == state_) {
    if (!target_connection_.IsConnected()) {
      printf("Connecting to [%s:%d] ... ", target_host_.c_str(), target_port_);
      target_connection_.ConnectTo(target_host_.c_str(), target_port_);
      if (target_connection_.IsConnected()) {
        printf("Connected to target on port %d\n", target_port_);
        SetState(CONNECTED);
      } else {
        printf("fail.\n");
      }
      fflush(stdout);
    } 
    return;
  }

  if (!target_connection_.IsConnected()) {
    OnError("Connection to target is dropped.");
    return;
  }

  if (CONNECTED == state_) {
    char record_type = 0;
    debug::Blob record;
    if (!session_log_.GetNextRecord(&record_type, &record)) {
      Stop();
      SetState(END_SUCCESS);
      return;
    }
    if ('H' == record_type)
      OnLoggedMsgFromHost(record);
    else if ('T' == record_type)
      OnLoggedMsgFromTarget(record);
    return;
  }

  if (WAIT_FOR_TARGET_REPLY == state_) {
    char buff[kBufferSize];
	  size_t read_bytes = target_connection_.Read(buff, sizeof(buff), kReadWaitMs);
    if (read_bytes > 0) {
      target_packetizer_.OnData(buff, read_bytes);
      return;
    }
  }
  if (WAIT_FOR_ACK == state_) {
    char buff[1];
	  int read_bytes = target_connection_.Read(buff, sizeof(buff), kReadWaitMs);
    if (read_bytes > 0) {
      if (buff[0] == '+') {
        SetState(CONNECTED);
      } else {
        OnError("Expected ack, but received something else.");
        return;
      }
    }
  }
  CheckTimers();
}

void Player::Stop() {
  SetState(STOPPED);
  target_connection_.Close();
}

bool Player::IsRunning() const {
  return !((STOPPED == state_) ||
          (END_FAILED == state_) ||
          (END_SUCCESS == state_));
}

void Player::OnPacket(debug::Blob& body, bool valid_checksum) {
  printf("T>[%s]\n", body.ToString().c_str());

  target_connection_.WriteAll("+", 1); // send ack to target
  UpdateDebuggeeThread(false, body);
  if (MatchReceivedMessageToLogged(body, logged_message_from_taget_)) {
    SetState(CONNECTED);
  } else {
    std::string exp_str = logged_message_from_taget_.ToString();
    std::string recv_str = body.ToString();
    printf("Expected: [%s]\nReceived: [%s]\n",
           exp_str.c_str(),
           recv_str.c_str());
    OnError("Message from target is different from what was logged.");
  }
}

void Player::OnLoggedMsgFromHost(debug::Blob& msg) {
  UpdateDebuggeeThread(true, msg);

  debug::Blob patched_msg;
  PatchHostMessage(msg, &patched_msg);
  last_message_to_target_ = patched_msg;
  std::string str = patched_msg.ToString();
  printf("H>[%s]\n", str.c_str());

  debug::Blob msg_out;
  rsp::PacketUtils::AddEnvelope(patched_msg, &msg_out);
  target_connection_.WriteAll(msg_out);
  SetState(WAIT_FOR_ACK);
  StartTimer(kTargetWaitSecs);
}

void Player::OnLoggedMsgFromTarget(debug::Blob& record) {
  logged_message_from_taget_ = record;
  SetState(WAIT_FOR_TARGET_REPLY);
  StartTimer(kTargetWaitSecs);
}

void Player::OnError(const char* error_description) {
  printf("Player ERROR!!! : [%s]\n", error_description);
  fflush(stdout);
  Stop();
  SetState(END_FAILED);
  error_description_ = error_description;
}

void Player::StartTimer(int seconds) {
  timer_ts_ = time(0) + seconds;
}

void Player::CheckTimers() {
  if (0 != timer_ts_) {
    if (timer_ts_ < time(0)) {
      timer_ts_ = 0;
      if (WAIT_FOR_TARGET_REPLY == state_)
        OnError("Wait for target reply timed out.");
      else if (WAIT_FOR_ACK == state_)
        OnError("Wait for target ack timed out.");
    }
  }
}

void Player::OnUnexpectedChar(char unexpected_char) {
  OnError("Unexpected character received from target.");
}

void Player::OnBreak() {
  OnError("Break received from target.");
}

void Player::UpdateDebuggeeThread(bool msg_is_from_host,
                              const debug::Blob& message) {
  debug::Blob msg = message;
  bool expect_id_list = last_message_to_target_.HasPrefix("qfDebuggeeThread") ||
                        last_message_to_target_.HasPrefix("qsDebuggeeThread");

  if (expect_id_list &&
     !msg_is_from_host &&
     (msg.HasPrefix("m") || msg.HasPrefix("l"))) {
    msg.PopFront();
    if (msg.Size() > 0)
      SetCurrentThreadId(msg);
  }

  if (!msg_is_from_host && msg.HasPrefix("QC")) {
    // target replies with current thread id
    msg.PopFront();
    msg.PopFront();
    SetCurrentThreadId(msg);
  }
}

bool Player::MatchReceivedMessageToLogged(const debug::Blob& received_message,
                                          const debug::Blob& logged_message) {
  if (last_message_to_target_.HasPrefix("g"))
    // Content of registers might change from run to run, so don't compare them.
    return (received_message.Size() == logged_message.Size()); 

  bool expect_id_list = last_message_to_target_.HasPrefix("qfDebuggeeThread") ||
                        last_message_to_target_.HasPrefix("qsDebuggeeThread");

  if (expect_id_list)
    return received_message.Compare(logged_message, 1);

  if (received_message.HasPrefix("QC"))
    return received_message.Compare(logged_message, 2);
  
  return received_message.Compare(logged_message);
}

void Player::PatchHostMessage(const debug::Blob& msg_in, debug::Blob* msg_out) {
  debug::Blob msg = msg_in;
  if (current_thread_id_.Size() > 0) {
    if (msg.HasPrefix("H")) {
      char c1 = msg[0];
      char c2 = msg[1];
      msg.PopFront();
      msg.PopFront();
      bool thread_id_specified = !msg.HasPrefix("0") && !msg.HasPrefix("-1");
      msg.PushFront(c2);
      msg.PushFront(c1);
      if (thread_id_specified) {
        while (msg.Size() > 2)
          msg.PopBack();
        msg.Append(current_thread_id_);
      }
    } else if (msg.HasPrefix("T")) {
      while (msg.Size() > 1)
        msg.PopBack();
      msg.Append(current_thread_id_);
    } else if (msg.HasPrefix("qThreadExtraInfo")) {
      while ((msg.Size() > 1) && (',' != msg.Back())) 
        msg.PopBack();
      msg.Append(current_thread_id_);
    }
  }
  *msg_out = msg;
}

}  // namespace rsp_player
