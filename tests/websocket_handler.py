# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple WebSocket server implementation for use of the test runner."""

import array
import base64
import hashlib
import socket
import SocketServer
import struct

STATUS_FAILURE = -1
STATUS_NO_RESPONSE = -2
STATUS_INTERNAL_ERROR = -3
STATUS_SUCCESS = 0


class Server(SocketServer.ThreadingTCPServer):
  """Simple threaded Websocket test server.

  Extends the server with additional fields and callbacks to communicate with
  the test runner.
  """

  def __init__(self, server_address, RequestHandlerClass, callback,
               command_callback, test_case):
    self.allow_reuse_address = True
    SocketServer.ThreadingTCPServer.__init__(self, server_address,
                                             RequestHandlerClass)
    self.callback = callback
    self.command_callback = command_callback
    self.test_case = test_case

  # Terminates the thread.
  def Terminate(self):
    # TODO(mtomasz): This is far from a good design. This is called from the
    # UI thread, but the Server lives on a separate thread.
    self.shutdown()


class Handler(SocketServer.StreamRequestHandler):
  """Handler of WebSocket incoming connections.

  Establishes the connection by
  exchanging headers, and then parses the frames and passes the messages to the
  test runner via callbacks.
  """

  def handle(self):
    """Handles an incoming connection."""
    status = STATUS_INTERNAL_ERROR

    # Accept incoming messages.
    if self.HandleHeaders():
      status = self.HandleFrames()

    # Return the result
    self.server.callback(status)

  def HandleHeaders(self):
    """Handles request headers.

    Parses the request headers and responses the response headers plus the test
    case name to be executed.

    Returns:
      True for success, false for a failure.
    """
    headers = {}

    while True:
      try:
        data = self.rfile.readline()
        if not data:
          return False
        line = data.strip()
        if line.find(':') != -1:
          pair = [word.strip() for word in line.split(':', 1)]
          headers[pair[0]] = pair[1]
        elif line:
          pass
        else:
          # Send the response headers.
          sha1 = hashlib.sha1()
          sha1.update(headers['Sec-WebSocket-Key'] +
                      '258EAFA5-E914-47DA-95CA-C5AB0DC85B11')
          secret = base64.b64encode(sha1.digest())
          self.wfile.write('HTTP/1.1 101 Switching Protocols\r\n')
          self.wfile.write('Upgrade: websocket\r\n')
          self.wfile.write('Connection: Upgrade\r\n')
          self.wfile.write('Sec-WebSocket-Accept: %s\r\n\r\n' % secret)

          # Send the test case name.
          print '\n{TEST CASE} %s' % self.server.test_case
          data = self.server.test_case.encode('utf-8')
          self.request.send('\x81%s%s' % (chr(len(data)), data))

          return True
      except socket.error:
        return False

  def HandleFrames(self):
    """Handles incoming frames from the WebSocket client.

    This parser is a limited and supports only features used by the test case
    runner.

    Returns:
      The error code. Zero for success, and STATUS_* constant for an error.
    """

    status = STATUS_NO_RESPONSE
    while True:
      try:
        data = self.rfile.read(2)
        if not data:
          break

        # Parse the header.
        b1, b2 = struct.unpack_from('>BB', data)
        frame_fin = (b1 & 0x80) >> 7
        frame_opcode = b1 & 0x0f
        frame_masked = (b2 & 0x80) >> 7
        frame_length = b2 & 0x7f
        frame_header_length = 2

        if frame_length == 126:
          frame_header_length += 2
          frame_length = struct.unpack_from('>xxH', data)
        elif frame_length == 127:
          frame_header_length += 8
          frame_length = struct.unpack_from('>xxQ', data)
        frame_total_length = (frame_header_length + frame_masked * 4 +
                              frame_length)

        data = self.rfile.read(frame_total_length - 2)

        # Partitioned messages are not supported.
        if not frame_fin:
          print 'Error: Partitioned messages are not implemented.'
          status = STATUS_INTERNAL_ERROR
          break

        # Extract the payload.
        frame_payload = ''
        if frame_masked:
          frame_mask = data[0:4]
          frame_mask_a = [ord(char) for char in frame_mask]
          data_array = array.array('B')
          data_array.fromstring(data[4:4 + frame_length])
          for i in range(len(data_array)):
            data_array[i] ^= frame_mask_a[i % 4]
          frame_payload = data_array.tostring()
        else:
          frame_payload = data

        # Close the connection.
        if frame_opcode == 8:
          return status

        status = self.HandleFrameMessage(frame_payload, status)
      except socket.error:
        status = STATUS_INTERNAL_ERROR

    return status

  def HandleFrameMessage(self, line, status):
    """Handles a decoded frame message.

    Handles a frame and calls the test case runner's callbacks if necessary.
    TODO(mtomasz): Move this logic to run_test.py.

    Args:
      line: (string) Line to be parsed.
      status: (number) Previous status code.

    Returns:
      The new return code.
    """

    command = ''
    message = ''
    if line.find(' ') != -1:
      tokens = line.split(' ', 1)
      command = tokens[0]
      message = tokens[1]
    else:
      command = line

    if message:
      print '[%s] %s' % (command, message)
    else:
      print '[%s]' % command

    if command == 'SUCCESS':
      if status == STATUS_NO_RESPONSE:
        return STATUS_SUCCESS
    if command == 'FAILURE':
      return STATUS_FAILURE
    if command == 'COMMAND':
      command_array = line.split(' ', 2)
      self.server.command_callback(command_array[1])

    return status

