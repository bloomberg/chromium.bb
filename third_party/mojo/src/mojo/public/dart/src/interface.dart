// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of bindings;

abstract class Interface extends core.MojoEventStreamListener {
  int _outstandingResponseFutures = 0;
  bool _isClosing = false;

  Interface(core.MojoMessagePipeEndpoint endpoint) : super(endpoint);

  Interface.fromHandle(core.MojoHandle handle) : super.fromHandle(handle);

  Interface.unbound() : super.unbound();

  Future<Message> handleMessage(ServiceMessage message);

  void handleRead() {
    // Query how many bytes are available.
    var result = endpoint.query();
    assert(result.status.isOk || result.status.isResourceExhausted);

    // Read the data and view as a message.
    var bytes = new ByteData(result.bytesRead);
    var handles = new List<core.MojoHandle>(result.handlesRead);
    result = endpoint.read(bytes, result.bytesRead, handles);
    assert(result.status.isOk || result.status.isResourceExhausted);

    // Prepare the response.
    var message = new ServiceMessage.fromMessage(new Message(bytes, handles));
    var responseFuture = _isClosing ? null : handleMessage(message);

    // If there's a response, send it.
    if (responseFuture != null) {
      _outstandingResponseFutures++;
      responseFuture.then((response) {
        _outstandingResponseFutures--;
        if (isOpen) {
          endpoint.write(response.buffer,
                         response.buffer.lengthInBytes,
                         response.handles);
          if (!endpoint.status.isOk) {
            throw "message pipe write failed: ${endpoint.status}";
          }
          if (_isClosing && (_outstandingResponseFutures == 0)) {
            // This was the final response future for which we needed to send
            // a response. It is safe to close.
            super.close();
            _isClosing = false;
          }
        }
      });
    } else if (_isClosing && (_outstandingResponseFutures == 0)) {
      // We are closing, there is no response to send for this message, and
      // there are no outstanding response futures. Do the close now.
      super.close();
      _isClosing = false;
    }
  }

  void handleWrite() {
    throw 'Unexpected write signal in client.';
  }

  void close() {
    if (!isOpen) return;
    if (isInHandler || (_outstandingResponseFutures > 0)) {
      // Either close() is being called from within handleRead() or
      // handleWrite(), or close() is being called while there are outstanding
      // response futures. Defer the actual close until all response futures
      // have been resolved.
      _isClosing = true;
    } else {
      super.close();
    }
  }

  Message buildResponse(Struct response, int name) {
    var header = new MessageHeader(name);
    return response.serializeWithHeader(header);
  }

  Message buildResponseWithId(Struct response, int name, int id, int flags) {
    var header = new MessageHeader.withRequestId(name, flags, id);
    return response.serializeWithHeader(header);
  }

  void sendMessage(Struct message, int name) {
    var header = new MessageHeader(name);
    var serviceMessage = message.serializeWithHeader(header);
    endpoint.write(serviceMessage.buffer,
                    serviceMessage.buffer.lengthInBytes,
                    serviceMessage.handles);
    if (!endpoint.status.isOk) {
      throw "message pipe write failed: ${endpoint.status}";
    }
  }

  Future sendMessageWithRequestId(Struct response, int name, int id) {
    throw "The client interface should not expect a response";
  }
}
