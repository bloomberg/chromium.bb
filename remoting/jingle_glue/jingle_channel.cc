// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_channel.h"

#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/waitable_event.h"
#include "media/base/data_buffer.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "third_party/libjingle/source/talk/base/stream.h"

using media::DataBuffer;

namespace remoting {

// Size of a read buffer chunk in bytes.
const size_t kReadBufferSize = 4096;

JingleChannel::JingleChannel(Callback* callback)
    : state_(INITIALIZING),
      callback_(callback),
      closed_(false),
      event_handler_(this),
      write_buffer_size_(0),
      current_write_buf_pos_(0) {
  DCHECK(callback_ != NULL);
}

// This constructor is only used in unit test.
JingleChannel::JingleChannel()
    : state_(CLOSED),
      closed_(false),
      write_buffer_size_(0),
      current_write_buf_pos_(0) {
}

JingleChannel::~JingleChannel() {
  DCHECK(closed_ || stream_ == NULL);
}

void JingleChannel::Init(JingleThread* thread,
                         talk_base::StreamInterface* stream,
                         const std::string& jid) {
  thread_ = thread;
  stream_.reset(stream);
  stream_->SignalEvent.connect(&event_handler_, &EventHandler::OnStreamEvent);
  jid_ = jid;

  // Initialize |state_|.
  switch (stream->GetState()) {
    case talk_base::SS_CLOSED:
      SetState(CLOSED);
      break;
    case talk_base::SS_OPENING:
      SetState(CONNECTING);
      break;
    case talk_base::SS_OPEN:
      SetState(OPEN);
      // Try to read in case there is something in the stream.
      thread_->message_loop()->PostTask(
          FROM_HERE, NewRunnableMethod(this, &JingleChannel::DoRead));
      break;
    default:
      NOTREACHED();
  }
}

void JingleChannel::Write(scoped_refptr<DataBuffer> data) {
  // Discard empty packets.
  if (data->GetDataSize() != 0) {
    AutoLock auto_lock(write_lock_);
    write_queue_.push_back(data);
    write_buffer_size_ += data->GetDataSize();
    // Post event so that the data gets written in the tunnel thread.
    thread_->message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JingleChannel::DoWrite));
  }
}

void JingleChannel::DoRead() {
  while (true) {
    size_t bytes_to_read;
    if (stream_->GetAvailable(&bytes_to_read)) {
      // Return immediately if we know there is nothing to read.
      if (bytes_to_read == 0)
        return;
    } else {
      // Try to read kReadBufferSize if the stream doesn't support
      // GetAvailable().
      bytes_to_read = kReadBufferSize;
    }

    scoped_refptr<DataBuffer> buffer(
        new DataBuffer(new uint8[bytes_to_read], kReadBufferSize));
    size_t bytes_read;
    int error;
    talk_base::StreamResult result = stream_->Read(
        buffer->GetWritableData(), bytes_to_read, &bytes_read, &error);
    switch (result) {
      case talk_base::SR_SUCCESS: {
        DCHECK_GT(bytes_read, 0U);
        buffer->SetDataSize(bytes_read);
        {
          AutoLock auto_lock(state_lock_);
          // Drop received data if the channel is already closed.
          if (!closed_)
            callback_->OnPacketReceived(this, buffer);
        }
        break;
      }
      case talk_base::SR_BLOCK: {
        return;
      }
      case talk_base::SR_EOS: {
        SetState(CLOSED);
        return;
      }
      case talk_base::SR_ERROR: {
        SetState(FAILED);
        return;
      }
    }
  }
}

void JingleChannel::DoWrite() {
  while (true) {
    if (!current_write_buf_) {
      AutoLock auto_lock(write_lock_);
      if (write_queue_.empty())
        break;
      current_write_buf_ = write_queue_.front();
      current_write_buf_pos_ = 0;
      write_queue_.pop_front();
    }

    size_t bytes_written;
    int error;
    talk_base::StreamResult result = stream_->Write(
        current_write_buf_->GetData() + current_write_buf_pos_,
        current_write_buf_->GetDataSize() - current_write_buf_pos_,
        &bytes_written, &error);
    switch (result) {
      case talk_base::SR_SUCCESS: {
        current_write_buf_pos_ += bytes_written;
        if (current_write_buf_pos_ >= current_write_buf_->GetDataSize())
          current_write_buf_ = NULL;
        {
          AutoLock auto_lock(write_lock_);
          write_buffer_size_ -= bytes_written;
        }
        break;
      }
      case talk_base::SR_BLOCK: {
        return;
      }
      case talk_base::SR_EOS: {
        SetState(CLOSED);
        return;
      }
      case talk_base::SR_ERROR: {
        SetState(FAILED);
        return;
      }
    }
  }
}

void JingleChannel::OnStreamEvent(talk_base::StreamInterface* stream,
                                  int events, int error) {
  if (events & talk_base::SE_OPEN)
    SetState(OPEN);

  if (state_ == OPEN && (events & talk_base::SE_WRITE))
    DoWrite();

  if (state_ == OPEN && (events & talk_base::SE_READ))
    DoRead();

  if (events & talk_base::SE_CLOSE)
    SetState(CLOSED);
}

void JingleChannel::SetState(State new_state) {
  if (new_state != state_) {
    state_ = new_state;
    {
      AutoLock auto_lock(state_lock_);
      if (!closed_)
        callback_->OnStateChange(this, new_state);
    }
  }
}

void JingleChannel::Close() {
  Close(NULL);
}

void JingleChannel::Close(Task* closed_task) {
  {
    AutoLock auto_lock(state_lock_);
    if (closed_) {
      // We are already closed.
      if (closed_task)
        thread_->message_loop()->PostTask(FROM_HERE, closed_task);
      return;
    }
    closed_ = true;
    if (closed_task)
      closed_task_.reset(closed_task);
  }

  thread_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleChannel::DoClose));
}


void JingleChannel::DoClose() {
  DCHECK(closed_);
  stream_->Close();
  stream_.reset();

  // TODO(sergeyu): Even though we have called Close() for the stream, it
  // doesn't mean that the p2p sessions has been closed. I.e. |closed_task_|
  // is called too early. If the client is closed right after that the other
  // side will not receive notification that the channel was closed.
  if (closed_task_.get()) {
    closed_task_->Run();
    closed_task_.reset();
  }
}

size_t JingleChannel::write_buffer_size() {
  AutoLock auto_lock(write_lock_);
  return write_buffer_size_;
}

}  // namespace remoting
