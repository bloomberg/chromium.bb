#!/usr/bin/env python3
# Copyright 2019 The PDFium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Encodes binary data using one or more PDF stream filters.

This tool helps with the common task of converting binary data into ASCII PDF
streams. In test PDFs (and the corresponding .in files), we often want the
contents to be plain (or mostly plain) ASCII.

Requires Python 3 (mainly for Ascii85 support). This should be fine for a
manually-run script.
"""

import argparse
import base64
import collections
import collections.abc
import io
import sys
import zlib


class _PdfStream:
  _unique_filter_classes = []
  _filter_classes = {}

  @staticmethod
  def GetFilterByName(name):
    # Tolerate any case-insensitive match for "/Name" or "Name", or an alias.
    key_name = name.lower()
    if key_name and key_name[0] == '/':
      key_name = key_name[:1]

    filter_class = _PdfStream._filter_classes.get(key_name)
    if not filter_class:
      raise KeyError(name)

    return filter_class

  @classmethod
  def Register(cls):
    assert cls not in _PdfStream._unique_filter_classes
    _PdfStream._unique_filter_classes.append(cls)
    cls.RegisterByName()
    cls.RegisterByAliases()

  @classmethod
  def RegisterByName(cls):
    assert cls.name[0] == '/'
    lower_name = cls.name.lower()
    _PdfStream._filter_classes[lower_name] = cls
    _PdfStream._filter_classes[lower_name[1:]] = cls

  @classmethod
  def RegisterByAliases(cls):
    for alias in cls.aliases:
      _PdfStream._filter_classes[alias.lower()] = cls

  @staticmethod
  def GetHelp():
    text = 'Available filters:\n'
    for filter_class in _PdfStream._unique_filter_classes:
      text += '  {} (aliases: {})\n'.format(filter_class.name,
                                            ', '.join(filter_class.aliases))
    return text

  @classmethod
  def AddEntries(cls, entries):
    _PdfStream.AddListEntry(entries, 'Filter', cls.name)

  @staticmethod
  def AddListEntry(entries, key, value):
    old_value = entries.get(key)
    if old_value is None:
      entries[key] = value
    else:
      if not isinstance(old_value, collections.abc.MutableSequence):
        old_value = [old_value]
        entries[key] = old_value
      old_value.append(value)

  def __init__(self, out_buffer, **kwargs):
    del kwargs
    self.buffer = out_buffer

  def write(self, data):
    self.buffer.write(data)

  def flush(self):
    self.buffer.flush()

  def close(self):
    self.buffer.close()


class _SinkPdfStream(_PdfStream):

  def __init__(self):
    super().__init__(io.BytesIO())

  def close(self):
    # Don't call io.BytesIO.close(); this deallocates the written data.
    self.flush()

  def getbuffer(self):
    return self.buffer.getbuffer()


class _AsciiPdfStream(_PdfStream):

  def __init__(self, out_buffer, wrapcol=0, **kwargs):
    super().__init__(out_buffer, **kwargs)
    self.wrapcol = wrapcol
    self.column = 0

  def write(self, data):
    if not self.wrapcol:
      self.buffer.write(data)
      return

    tail = self.wrapcol - self.column
    self.buffer.write(data[:tail])
    if tail >= len(data):
      self.column += len(data)
      return

    for start in range(tail, len(data), self.wrapcol):
      self.buffer.write(b'\n')
      self.buffer.write(data[start:start + self.wrapcol])

    tail = len(data) - tail
    self.column = self.wrapcol - -tail % self.wrapcol


class _Ascii85DecodePdfStream(_AsciiPdfStream):
  name = '/ASCII85Decode'
  aliases = ('ascii85', 'base85')

  def __init__(self, out_buffer, **kwargs):
    super().__init__(out_buffer, **kwargs)
    self.trailer = b''

  def write(self, data):
    # Need to write ASCII85 in units of 4.
    data = self.trailer + data
    trailer_length = len(data) % 4
    super().write(base64.a85encode(data[:-trailer_length]))
    self.trailer = data[-trailer_length:]

  def close(self):
    super().write(base64.a85encode(self.trailer))
    # Avoid breaking the end-of-data marker (but still try to wrap).
    if self.wrapcol and self.column > self.wrapcol - 2:
      self.buffer.write(b'\n')
    self.buffer.write(b'~>')
    self.buffer.close()


class _AsciiHexDecodePdfStream(_AsciiPdfStream):
  name = '/ASCIIHexDecode'
  aliases = ('base16', 'hex', 'hexadecimal')

  def __init__(self, out_buffer, **kwargs):
    super().__init__(out_buffer, **kwargs)

  def write(self, data):
    super().write(base64.b16encode(data))


class _FlateDecodePdfStream(_PdfStream):
  name = '/FlateDecode'
  aliases = ('deflate', 'flate', 'zlib')

  def __init__(self, out_buffer, **kwargs):
    super().__init__(out_buffer, **kwargs)
    self.deflate = zlib.compressobj(level=9, memLevel=9)

  def write(self, data):
    self.buffer.write(self.deflate.compress(data))

  def flush(self):
    self.buffer.write(self.deflate.flush(zlib.Z_NO_FLUSH))

  def close(self):
    self.buffer.write(self.deflate.flush())
    self.buffer.close()


class _VirtualPdfStream(_PdfStream):

  @classmethod
  def RegisterByName(cls):
    pass

  @classmethod
  def AddEntries(cls, entries):
    pass


class _PassthroughPdfStream(_VirtualPdfStream):
  name = '(virtual) passthrough'
  aliases = ('noop', 'passthrough')


class _PngIdatPdfStream(_VirtualPdfStream):
  name = '(virtual) PNG IDAT'
  aliases = ('png',)

  _EXPECT_HEADER = -1
  _EXPECT_LENGTH = -2
  _EXPECT_CHUNK_TYPE = -3
  _EXPECT_CRC = -4

  _PNG_HEADER = 0x89504E470D0A1A0A
  _PNG_CHUNK_IDAT = 0x49444154

  @classmethod
  def AddEntries(cls, entries):
    # Technically only true for compression method 0 (zlib), but no other
    # methods have been standardized.
    _PdfStream.AddListEntry(entries, 'Filter', '/FlateDecode')

  def __init__(self, out_buffer, **kwargs):
    super().__init__(out_buffer, **kwargs)
    self.chunk = _PngIdatPdfStream._EXPECT_HEADER
    self.remaining = 8
    self.accumulator = 0
    self.length = 0

  def write(self, data):
    position = 0
    while position < len(data):
      if self.chunk >= 0:
        # Only pass through IDAT chunk data.
        read_size = min(self.remaining, len(data) - position)
        if self.chunk == _PngIdatPdfStream._PNG_CHUNK_IDAT:
          self.buffer.write(data[position:position + read_size])
        self.remaining -= read_size
        if self.remaining == 0:
          self.ResetAccumulator(_PngIdatPdfStream._EXPECT_CRC, 4)
        position += read_size
      else:
        # As far as we're concerned, PNG files are just a header followed by a
        # series of (length, chunk type, data[length], CRC) chunks.
        if self.AccumulateByte(data[position]):
          if self.chunk == _PngIdatPdfStream._EXPECT_HEADER:
            if self.accumulator != _PngIdatPdfStream._PNG_HEADER:
              raise ValueError('Invalid PNG header', self.accumulator)
            self.ResetAccumulator(_PngIdatPdfStream._EXPECT_LENGTH, 4)
          elif self.chunk == _PngIdatPdfStream._EXPECT_LENGTH:
            self.length = self.accumulator
            self.ResetAccumulator(_PngIdatPdfStream._EXPECT_CHUNK_TYPE, 4)
          elif self.chunk == _PngIdatPdfStream._EXPECT_CHUNK_TYPE:
            self.ResetAccumulator(self.accumulator, self.length)
          elif self.chunk == _PngIdatPdfStream._EXPECT_CRC:
            # Don't care if the CRC is correct.
            self.ResetAccumulator(_PngIdatPdfStream._EXPECT_LENGTH, 4)
        position += 1

  def ResetAccumulator(self, chunk, remaining):
    self.chunk = chunk
    self.remaining = remaining
    self.accumulator = 0

  def AccumulateByte(self, byte):
    assert self.remaining > 0
    self.accumulator = self.accumulator << 8 | byte
    self.remaining -= 1
    return self.remaining == 0


_Ascii85DecodePdfStream.Register()
_AsciiHexDecodePdfStream.Register()
_FlateDecodePdfStream.Register()
_PassthroughPdfStream.Register()
_PngIdatPdfStream.Register()

_DEFAULT_FILTERS = (_Ascii85DecodePdfStream, _FlateDecodePdfStream)


def _ParseCommandLine(argv):
  arg_parser = argparse.ArgumentParser(
      description='Encodes binary data using one or more PDF stream filters.',
      epilog=_PdfStream.GetHelp(),
      formatter_class=argparse.RawDescriptionHelpFormatter)
  arg_parser.add_argument(
      '-r',
      '--raw',
      action='store_true',
      help='output raw bytes (no PDF stream header or trailer)')
  arg_parser.add_argument(
      '-l',
      '--length',
      action='store_true',
      help='output actual /Length, instead of {{streamlen}}')
  arg_parser.add_argument(
      '-w',
      '--wrap',
      default=80,
      type=int,
      help='wrap ASCII lines at COLUMN; defaults to 80 (0 = off)',
      metavar='COLUMN')
  arg_parser.add_argument(
      '-f',
      '--filter',
      action='append',
      type=_PdfStream.GetFilterByName,
      help=('one or more filters, in decoding order; defaults to ' + ' '.join(
          [f.name for f in _DEFAULT_FILTERS])),
      metavar='NAME')
  arg_parser.add_argument(
      'infile',
      nargs='?',
      default=sys.stdin,
      type=argparse.FileType('r'),
      help='input file; use - for standard input (default)')
  arg_parser.add_argument(
      'outfile',
      nargs='?',
      default=sys.stdout,
      type=argparse.FileType('w'),
      help='output file; use - for standard output (default)')
  args = arg_parser.parse_intermixed_args(argv)
  args.filter = args.filter or _DEFAULT_FILTERS
  assert args.wrap >= 0, '--wrap COLUMN must be non-negative'
  return args


def _WrapWithFilters(out_buffer, filter_classes, **kwargs):
  for filter_class in filter_classes:
    out_buffer = filter_class(out_buffer, **kwargs)
  return out_buffer


def _CopyBytes(in_buffer, out_buffer):
  data = bytearray(io.DEFAULT_BUFFER_SIZE)
  while True:
    data_length = in_buffer.readinto(data)
    if not data_length:
      return
    out_buffer.write(data[:data_length])


def _WritePdfStreamObject(out_buffer,
                          data,
                          entries,
                          raw=False,
                          use_streamlen=False):
  if not raw:
    out_buffer.write(b'<<\n')
    entries['Length'] = len(data)
    for k, v in entries.items():
      v = _EncodePdfValue(v)
      if k == 'Length' and use_streamlen:
        out_buffer.write(b'  {{streamlen}}\n')
      else:
        out_buffer.write('  /{} {}\n'.format(k, v).encode('ascii'))
    out_buffer.write(b'>>\nstream\n')

  out_buffer.write(data)

  if not raw:
    if data and data[-1] != '\n':
      out_buffer.write(b'\n')
    out_buffer.write(b'endstream\n')


def _EncodePdfValue(value):
  if isinstance(value, collections.abc.MutableSequence):
    value = '[' + ' '.join(value) + ']'
  return value


def main(argv):
  args = _ParseCommandLine(argv)

  encoded_sink = _SinkPdfStream()
  with args.infile:
    out_buffer = _WrapWithFilters(encoded_sink, args.filter, wrapcol=args.wrap)
    _CopyBytes(args.infile.buffer, out_buffer)
    out_buffer.close()

  entries = collections.OrderedDict()
  for f in args.filter:
    f.AddEntries(entries)
  _WritePdfStreamObject(
      args.outfile.buffer,
      data=encoded_sink.getbuffer(),
      entries=entries,
      raw=args.raw,
      use_streamlen=not args.length)
  return args.outfile.close()


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
