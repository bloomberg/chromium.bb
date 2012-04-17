# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Utility to remove comments from JSON files so that they can be parsed by
json.loads."""

def _ReadString(input, start, output):
  output.append('"')
  in_escape = False
  for pos in xrange(start, len(input)):
    output.append(input[pos])
    if in_escape:
      in_escape = False
    else:
      if input[pos] == '\\':
        in_escape = True
      elif input[pos] == '"':
        return pos + 1
  return pos

def _ReadComment(input, start, output):
  for pos in xrange(start, len(input)):
    if input[pos] in ['\r', '\n']:
      output.append(input[pos])
      return pos + 1
  return pos

def Nom(input):
  output = []
  pos = 0
  while pos < len(input):
    if input[pos] == '"':
      pos = _ReadString(input, pos + 1, output)
    elif input[pos:pos+2] == '//':
      pos = _ReadComment(input, pos + 2, output)
    else:
      output.append(input[pos])
      pos += 1
  return ''.join(output)
