// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationConnectionProxy_h
#define WebPresentationConnectionProxy_h

namespace blink {

// The implementation the embedder has to provide for the Presentation API to
// work. This class is used to send messages to a PresentationConnection hosted
// in a different frame.
class WebPresentationConnectionProxy {
 public:
  virtual ~WebPresentationConnectionProxy() = default;

  // Close target connection.
  virtual void close() const = 0;
};

}  // namespace blink

#endif  // WebPresentationConnectionProxy_h
