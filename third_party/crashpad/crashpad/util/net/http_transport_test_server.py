#!/usr/bin/env python
# coding: utf-8

# Copyright 2014 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""A one-shot testing webserver.

When invoked, this server will write a short integer to stdout, indiciating on
which port the server is listening. It will then read one integer from stdin,
indiciating the response code to be sent in response to a request. It also reads
16 characters from stdin, which, after having "\r\n" appended, will form the
response body in a successful response (one with code 200). The server will
process one HTTP request, deliver the prearranged response to the client, and
write the entire request to stdout. It will then terminate.

This server is written in Python since it provides a simple HTTP stack, and
because parsing chunked encoding is safer and easier in a memory-safe language.
This could easily have been written in C++ instead.
"""

import os
import struct
import sys
import zlib

if sys.platform == 'win32':
  import msvcrt

if sys.version_info[0] < 3:
  import BaseHTTPServer as http_server
else:
  import http.server as http_server


class BufferedReadFile(object):
  """A File-like object that stores all read contents into a buffer."""

  def __init__(self, real_file):
    self.file = real_file
    self.buffer = b''

  def read(self, size=-1):
    buf = self.file.read(size)
    self.buffer += buf
    return buf

  def readline(self, size=-1):
    buf = self.file.readline(size)
    self.buffer += buf
    return buf

  def flush(self):
    self.file.flush()

  def close(self):
    self.file.close()


class RequestHandler(http_server.BaseHTTPRequestHandler):
  # Everything to be written to stdout is collected into this string. It can’t
  # be written to stdout until after the HTTP transaction is complete, because
  # stdout is a pipe being read by a test program that’s also the HTTP client.
  # The test program expects to complete the entire HTTP transaction before it
  # even starts reading this script’s stdout. If the stdout pipe buffer fills up
  # during an HTTP transaction, deadlock would result.
  raw_request = b''

  response_code = 500
  response_body = b''

  def handle_one_request(self):
    # Wrap the rfile in the buffering file object so that the raw header block
    # can be written to stdout after it is parsed.
    self.rfile = BufferedReadFile(self.rfile)
    http_server.BaseHTTPRequestHandler.handle_one_request(self)

  def do_POST(self):
    RequestHandler.raw_request = self.rfile.buffer
    self.rfile.buffer = b''

    if self.headers.get('Transfer-Encoding', '').lower() == 'chunked':
      if 'Content-Length' in self.headers:
        raise AssertionError
      body = self.handle_chunked_encoding()
    else:
      length = int(self.headers.get('Content-Length', -1))
      body = self.rfile.read(length)

    if self.headers.get('Content-Encoding', '').lower() == 'gzip':
      # 15 is the value of |wbits|, which should be at the maximum possible
      # value to ensure that any gzip stream can be decoded. The offset of 16
      # specifies that the stream to decompress will be formatted with a gzip
      # wrapper.
      body = zlib.decompress(body, 16 + 15)

    RequestHandler.raw_request += body

    self.send_response(self.response_code)
    self.end_headers()
    if self.response_code == 200:
      self.wfile.write(self.response_body)
      self.wfile.write(b'\r\n')

  def handle_chunked_encoding(self):
    """This parses a "Transfer-Encoding: Chunked" body in accordance with RFC
    7230 §4.1. This returns the result as a string.
    """
    body = b''
    chunk_size = self.read_chunk_size()
    while chunk_size > 0:
      # Read the body.
      data = self.rfile.read(chunk_size)
      chunk_size -= len(data)
      body += data

      # Finished reading this chunk.
      if chunk_size == 0:
        # Read through any trailer fields.
        trailer_line = self.rfile.readline()
        while trailer_line.strip() != b'':
          trailer_line = self.rfile.readline()

        # Read the chunk size.
        chunk_size = self.read_chunk_size()
    return body

  def read_chunk_size(self):
    # Read the whole line, including the \r\n.
    chunk_size_and_ext_line = self.rfile.readline()
    # Look for a chunk extension.
    chunk_size_end = chunk_size_and_ext_line.find(b';')
    if chunk_size_end == -1:
      # No chunk extensions; just encounter the end of line.
      chunk_size_end = chunk_size_and_ext_line.find(b'\r')
    if chunk_size_end == -1:
      self.send_response(400)  # Bad request.
      return -1
    return int(chunk_size_and_ext_line[:chunk_size_end], base=16)

  def log_request(self, code='-', size='-'):
    # The default implementation logs these to sys.stderr, which is just noise.
    pass


def StdioBinaryEquivalent(file):
  """Return a file object equivalent to sys.stdin or sys.stdout capable of
  reading or writing binary “bytes”.

  struct.unpack consumes “bytes”, and struct.pack produces “bytes”. These are
  distinct from “str” in Python 3 (but not 2). In order to read and write these
  from stdin and stdout, the underlying binary buffer must be used in place of
  the upper-layer text wrapper. This function returns a suitable file.

  There is no underlying buffer in Python 2, but on Windows, the file mode must
  still be set to binary in order to cleanly pass binary data. Note that in this
  case, the mode of |file| itself is changed, as it’s not distinct from the
  returned file.
  """
  if hasattr(file, 'buffer'):
    file = file.buffer
  elif sys.platform == 'win32':
    msvcrt.setmode(file.fileno(), os.O_BINARY)
  return file


def Main():
  in_file = StdioBinaryEquivalent(sys.stdin)
  out_file = StdioBinaryEquivalent(sys.stdout)

  # Start the server.
  server = http_server.HTTPServer(('127.0.0.1', 0), RequestHandler)

  # Write the port as an unsigned short to the parent process.
  out_file.write(struct.pack('=H', server.server_address[1]))
  out_file.flush()

  # Read the desired test response code as an unsigned short and the desired
  # response body as a 16-byte string from the parent process.
  RequestHandler.response_code, RequestHandler.response_body = \
      struct.unpack('=H16s', in_file.read(struct.calcsize('=H16s')))

  # Handle the request.
  server.handle_request()

  # Share the entire request with the test program, which will validate it.
  out_file.write(RequestHandler.raw_request)
  out_file.flush()

if __name__ == '__main__':
  Main()
