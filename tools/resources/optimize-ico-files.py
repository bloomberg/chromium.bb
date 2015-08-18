#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Windows ICO file crusher.

Optimizes the PNG images within a Windows ICO icon file. This extracts all of
the sub-images within the file, runs any PNG-formatted images through
optimize-png-files.sh, then packs them back into an ICO file.

NOTE: ICO files can contain both raw uncompressed BMP files and PNG files. This
script does not touch the BMP files, which means if you have a huge uncompressed
image, it will not get smaller. 256x256 icons should be PNG-formatted first.
(Smaller icons should be BMPs for compatibility with Windows XP.)
"""

import argparse
import logging
import os
import StringIO
import struct
import subprocess
import sys
import tempfile

OPTIMIZE_PNG_FILES = 'tools/resources/optimize-png-files.sh'

logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')

class InvalidFile(Exception):
  """Represents an invalid ICO file."""

def IsPng(png_data):
  """Determines whether a sequence of bytes is a PNG."""
  return png_data.startswith('\x89PNG\r\n\x1a\n')

def OptimizePngFile(temp_dir, png_filename, optimization_level=None):
  """Optimize a PNG file.

  Args:
    temp_dir: The directory containing the PNG file. Must be the only file in
      the directory.
    png_filename: The full path to the PNG file to optimize.

  Returns:
    The raw bytes of a PNG file, an optimized version of the input.
  """
  logging.debug('Crushing PNG image...')
  args = [OPTIMIZE_PNG_FILES]
  if optimization_level is not None:
    args.append('-o%d' % optimization_level)
  args.append(temp_dir)
  result = subprocess.call(args, stdout=sys.stderr)
  if result != 0:
    logging.warning('Warning: optimize-png-files failed (%d)', result)
  else:
    logging.debug('optimize-png-files succeeded')

  with open(png_filename, 'rb') as png_file:
    return png_file.read()

def OptimizePng(png_data, optimization_level=None):
  """Optimize a PNG.

  Args:
    png_data: The raw bytes of a PNG file.

  Returns:
    The raw bytes of a PNG file, an optimized version of the input.
  """
  temp_dir = tempfile.mkdtemp()
  try:
    logging.debug('temp_dir = %s', temp_dir)
    png_filename = os.path.join(temp_dir, 'image.png')
    with open(png_filename, 'wb') as png_file:
      png_file.write(png_data)
    return OptimizePngFile(temp_dir, png_filename,
                           optimization_level=optimization_level)

  finally:
    if os.path.exists(png_filename):
      os.unlink(png_filename)
    os.rmdir(temp_dir)

def OptimizeIcoFile(infile, outfile, optimization_level=None):
  """Read an ICO file, optimize its PNGs, and write the output to outfile.

  Args:
    infile: The file to read from. Must be a seekable file-like object
      containing a Microsoft ICO file.
    outfile: The file to write to.
  """
  filename = os.path.basename(infile.name)
  icondir = infile.read(6)
  zero, image_type, num_images = struct.unpack('<HHH', icondir)
  if zero != 0:
    raise InvalidFile('First word must be 0.')
  if image_type not in (1, 2):
    raise InvalidFile('Image type must be 1 or 2.')

  # Read and unpack each ICONDIRENTRY.
  icon_dir_entries = []
  for i in range(num_images):
    icondirentry = infile.read(16)
    icon_dir_entries.append(struct.unpack('<BBBBHHLL', icondirentry))

  # Read each icon's bitmap data, crush PNGs, and update icon dir entries.
  current_offset = infile.tell()
  icon_bitmap_data = []
  for i in range(num_images):
    width, height, num_colors, r1, r2, r3, size, _ = icon_dir_entries[i]
    width = width or 256
    height = height or 256
    offset = current_offset
    icon_data = infile.read(size)
    if len(icon_data) != size:
      raise EOFError()

    entry_is_png = IsPng(icon_data)
    logging.info('%s entry #%d: %dx%d, %d bytes (%s)', filename, i + 1, width,
                 height, size, 'PNG' if entry_is_png else 'BMP')

    if entry_is_png:
      icon_data = OptimizePng(icon_data, optimization_level=optimization_level)
    elif width >= 256 or height >= 256:
      # TODO(mgiuca): Automatically convert large BMP images to PNGs.
      logging.warning('Entry #%d is a large image in uncompressed BMP format. '
                      'Please manually convert to PNG format before running '
                      'this utility.', i + 1)

    new_size = len(icon_data)
    current_offset += new_size
    icon_dir_entries[i] = (width % 256, height % 256, num_colors, r1, r2, r3,
                           new_size, offset)
    icon_bitmap_data.append(icon_data)

  # Write the data back to outfile.
  outfile.write(icondir)
  for icon_dir_entry in icon_dir_entries:
    outfile.write(struct.pack('<BBBBHHLL', *icon_dir_entry))
  for icon_bitmap in icon_bitmap_data:
    outfile.write(icon_bitmap)

def main(args=None):
  if args is None:
    args = sys.argv[1:]

  parser = argparse.ArgumentParser(description='Crush Windows ICO files.')
  parser.add_argument('files', metavar='ICO', type=argparse.FileType('r+b'),
                      nargs='+', help='.ico files to be crushed')
  parser.add_argument('-o', dest='optimization_level', metavar='OPT', type=int,
                      help='optimization level')
  parser.add_argument('-d', '--debug', dest='debug', action='store_true',
                      help='enable debug logging')

  args = parser.parse_args()

  if args.debug:
    logging.getLogger().setLevel(logging.DEBUG)

  for file in args.files:
    buf = StringIO.StringIO()
    file.seek(0, os.SEEK_END)
    old_length = file.tell()
    file.seek(0, os.SEEK_SET)
    OptimizeIcoFile(file, buf, args.optimization_level)

    new_length = len(buf.getvalue())
    if new_length >= old_length:
      logging.info('%s : Could not reduce file size.', file.name)
    else:
      file.truncate(new_length)
      file.seek(0)
      file.write(buf.getvalue())

      saving = old_length - new_length
      saving_percent = float(saving) / old_length
      logging.info('%s : %d => %d (%d bytes : %d %%)', file.name, old_length,
                   new_length, saving, int(saving_percent * 100))

if __name__ == '__main__':
  sys.exit(main())
