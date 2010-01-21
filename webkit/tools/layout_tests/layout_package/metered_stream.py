#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Package that implements a stream wrapper that has 'meters' as well as
regular output. A 'meter' is a single line of text that can be erased
and rewritten repeatedly, without producing multiple lines of output. It
can be used to produce effects like progress bars.
"""


class MeteredStream:
    """This class is a wrapper around a stream that allows you to implement
    meters.

    It can be used like a stream, but calling update() will print
    the string followed by only a carriage return (instead of a carriage
    return and a line feed). This can be used to implement progress bars and
    other sorts of meters. Note that anything written by update() will be
    erased by a subsequent update(), write(), or flush()."""

    def __init__(self, verbose, stream):
        """
        Args:
          verbose: whether update is a no-op
          stream: output stream to write to
        """
        self._dirty = False
        self._verbose = verbose
        self._stream = stream
        self._last_update = ""

    def write(self, txt):
        """Write text directly to the stream, overwriting and resetting the
        meter."""
        if self._dirty:
            self.update("")
            self._dirty = False
        self._stream.write(txt)

    def flush(self):
        """Flush any buffered output."""
        self._stream.flush()

    def update(self, str):
        """Write an update to the stream that will get overwritten by the next
        update() or by a write().

        This is used for progress updates that don't need to be preserved in
        the log. Note that verbose disables this routine; we have this in
        case we are logging lots of output and the update()s will get lost
        or won't work properly (typically because verbose streams are
        redirected to files.

        TODO(dpranke): figure out if there is a way to detect if we're writing
        to a stream that handles CRs correctly (e.g., terminals). That might
        be a cleaner way of handling this.
        """
        if self._verbose:
            return

        # Print the necessary number of backspaces to erase the previous
        # message.
        self._stream.write("\b" * len(self._last_update))
        self._stream.write(str)
        num_remaining = len(self._last_update) - len(str)
        if num_remaining > 0:
            self._stream.write(" " * num_remaining + "\b" * num_remaining)
        self._last_update = str
        self._dirty = True
