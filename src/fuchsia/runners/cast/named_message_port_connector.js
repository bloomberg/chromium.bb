// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!cast)
  var cast = new Object;

if (!cast.__platform__)
  cast.__platform__ = new Object;

// Defines a late-bound port for performing bidirectional communication with
// platform code. Ports are passed in from outside the browser process after
// every pages load. Constructed ports are not initially connected, but they are
// immediately available for writing via a buffer. When the Channel is
// eventually connected, the buffer will be flushed and all subsequent writes
// will be sent directly to the peer.
cast.__platform__.Port = class {
  constructor(id, messageHandler) {
    this.messageHandler_ = messageHandler;
    this.id_ = id;
  }

  // Sends a message and optionally a list of Transferables to the Port,
  // or buffers the message for later delivery if |this| is unconnected.
  sendMessage(data, transferables) {
    if (this.port_) {
      this.port_.postMessage(data, transferables);
    } else {
      this.buffer_.push({'data': data, 'transferables': transferables});
    }
  }

  // Receives a MessagePort from cast.__platform__.connector.
  receivePort(port) {
    this.port_ = port;
    this.port_.onmessage = this.onMessage.bind(this);
    this.port_.onmessageerror = this.onMessageError.bind(this);

    // Signal the peer that the port was received.
    this.port_.postMessage('connected');

    // Flush and destroy the pre-connect buffer.
    for (var i = 0; i < this.buffer_.length; ++i) {
      this.port_.postMessage(this.buffer_[i].data,
          this.buffer_[i].transferables);
    }
    this.buffer = null;
  }

  onMessage(message) {
    this.messageHandler_(message.data);
  }

  onMessageError() {
    // The underlying MessagePort should be long-lived and reliable. Any
    // connection drop is likely due to a bug on the other side of the
    // FIDL IPC connection.
    console.error('Unexpected connection loss for channel ' + this.id);
    this.port_ = null;
  }

  // Called when a message arrives.
  messageHandler_ = null;

  // Buffer of messages to be sent prior to a port being received.
  // Once a port is bound, the buffer is flushed and then destroyed.
  buffer_ = [];

  // Underlying message transport.
  port_ = null;
};

// Creates new string-identified Ports and asynchronously binds them to
// message transport.
cast.__platform__.connector = new class {
  // Sets up a Port which will be asynchronously bound to a MessagePort.
  // The port is identified by a string |id|, which must not have spaces.
  bind(id, messageHandler) {
    var binding = new cast.__platform__.Port(id, messageHandler);
    this.pending_ports_[id] = binding;
    return binding;
  }

  // Services incoming port connection events via the window.onmessage handler.
  onMessageEvent(e) {
    // Only process events identified by a magic 'connect' prefix.
    // Let all other events pass through to other handlers.
    var tokenized = e.data.split(' ');
    if (tokenized.length != 2 || tokenized[0] != 'connect')
      return;

    if (e.ports.length != 1) {
      console.error('Expected only one MessagePort, got ' + e.ports.length +
                    ' instead.');
      return;
    }

    var portName = tokenized[1];
    if (!(portName in this.pending_ports_)) {
      // Bindings must be registered in advance of MessagePorts.
      console.error('No handler for port: ' + portName);
      return;
    }

    this.pending_ports_[portName].receivePort(e.ports[0]);
    delete this.pending_ports_[portName];
    e.stopPropagation();
  }

  // A map of unconnected ports waiting to be connected with MessagePorts,
  // keyed string IDs.
  pending_ports_ = {};
}();

window.addEventListener('message',
    cast.__platform__.connector.onMessageEvent.bind(
        cast.__platform__.connector), true);
