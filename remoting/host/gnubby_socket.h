// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_GNUBBY_SOCKET_H_
#define REMOTING_HOST_GNUBBY_SOCKET_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"

namespace base {
class Timer;
}  // namespace base

namespace net {
class StreamListenSocket;
}  // namespace net

namespace remoting {

// Class that manages a socket used for gnubby requests.
class GnubbySocket : public base::NonThreadSafe {
 public:
  GnubbySocket(scoped_ptr<net::StreamListenSocket> socket,
               const base::Closure& timeout_callback);
  ~GnubbySocket();

  // Adds data to the current request.
  void AddRequestData(const char* data, int data_len);

  // Gets the current request data and clears it.
  void GetAndClearRequestData(std::string* data_out);

  // Returns true if the current request is complete.
  bool IsRequestComplete() const;

  // Returns true if the stated request size is larger than the allowed maximum.
  bool IsRequestTooLarge() const;

  // Sends response data to the socket.
  void SendResponse(const std::string& data);

  // Sends an SSH error code to the socket.
  void SendSshError();

  // Returns true if |socket| is the same one owned by this object.
  bool IsSocket(net::StreamListenSocket* socket) const;

  // Sets a timer for testing.
  void SetTimerForTesting(scoped_ptr<base::Timer> timer);

 private:
  // Returns the stated request length.
  size_t GetRequestLength() const;

  // Returns the response length bytes.
  std::string GetResponseLengthAsBytes(const std::string& response) const;

  // Resets the socket activity timer.
  void ResetTimer();

  // The socket.
  scoped_ptr<net::StreamListenSocket> socket_;

  // Request data.
  std::vector<char> request_data_;

  // The activity timer.
  scoped_ptr<base::Timer> timer_;

  DISALLOW_COPY_AND_ASSIGN(GnubbySocket);
};

}  // namespace remoting

#endif  // REMOTING_HOST_GNUBBY_SOCKET_H_
