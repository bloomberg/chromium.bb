#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Flattens a HTML file by inlining its external resources.

This is a small script that takes a HTML file, looks for src attributes
and inlines the specified file, producing one HTML file with no external
dependencies.

This does not inline anything referenced from an inlined file.
"""

import os
import re
import sys
import base64
import mimetypes

from grit.node import base

DIST_DEFAULT = 'chromium'
DIST_ENV_VAR = 'CHROMIUM_BUILD'
DIST_SUBSTR = '%DISTRIBUTION%'

# Matches beginning of an "if" block with trailing spaces.
_BEGIN_IF_BLOCK = re.compile('<if [^>]*?expr="(?P<expression>[^"]*)"[^>]*?>\s*')

# Matches ending of an "if" block with preceding spaces.
_END_IF_BLOCK = re.compile('\s*</if>')

def ReadFile(input_filename):
  """Helper function that returns input_filename as a string.

  Args:
    input_filename: name of file to be read

  Returns:
    string
  """
  f = open(input_filename, 'rb')
  file_contents = f.read()
  f.close()
  return file_contents

def SrcInlineAsDataURL(
    src_match, base_path, distribution, inlined_files, names_only=False):
  """regex replace function.

  Takes a regex match for src="filename", attempts to read the file
  at 'filename' and returns the src attribute with the file inlined
  as a data URI. If it finds DIST_SUBSTR string in file name, replaces
  it with distribution.

  Args:
    src_match: regex match object with 'filename' named capturing group
    base_path: path that to look for files in
    distribution: string that should replace DIST_SUBSTR
    inlined_files: The name of the opened file is appended to this list.
    names_only: If true, the function will not read the file but just return "".
                It will still add the filename to |inlined_files|.

  Returns:
    string
  """
  filename = src_match.group('filename')

  if filename.find(':') != -1:
    # filename is probably a URL, which we don't want to bother inlining
    return src_match.group(0)

  filename = filename.replace('%DISTRIBUTION%', distribution)
  filepath = os.path.join(base_path, filename)
  inlined_files.add(filepath)

  if names_only:
    return ""

  mimetype = mimetypes.guess_type(filename)[0] or 'text/plain'
  inline_data = base64.standard_b64encode(ReadFile(filepath))

  prefix = src_match.string[src_match.start():src_match.start('filename')-1]
  return "%s\"data:%s;base64,%s\"" % (prefix, mimetype, inline_data)


class InlinedData:
  """Helper class holding the results from DoInline().

  Holds the inlined data and the set of filenames of all the inlined
  files.
  """
  def __init__(self, inlined_data, inlined_files):
    self.inlined_data = inlined_data
    self.inlined_files = inlined_files

def DoInline(
    input_filename, grd_node, allow_external_script=False, names_only=False):
  """Helper function that inlines the resources in a specified file.

  Reads input_filename, finds all the src attributes and attempts to
  inline the files they are referring to, then returns the result and
  the set of inlined files.

  Args:
    input_filename: name of file to read in
    grd_node: html node from the grd file for this include tag
    names_only: |nil| will be returned for the inlined contents (faster).
  Returns:
    a tuple of the inlined data as a string and the set of filenames
    of all the inlined files
  """
  input_filepath = os.path.dirname(input_filename)

  distribution = DIST_DEFAULT
  if DIST_ENV_VAR in os.environ.keys():
    distribution = os.environ[DIST_ENV_VAR]
    if len(distribution) > 1 and distribution[0] == '_':
      distribution = distribution[1:].lower()

  # Keep track of all the files we inline.
  inlined_files = set()

  def SrcReplace(src_match, filepath=input_filepath,
                 inlined_files=inlined_files):
    """Helper function to provide SrcInlineAsDataURL with the base file path"""
    return SrcInlineAsDataURL(
        src_match, filepath, distribution, inlined_files, names_only=names_only)

  def GetFilepath(src_match):
    filename = src_match.group('filename')

    if filename.find(':') != -1:
      # filename is probably a URL, which we don't want to bother inlining
      return None

    filename = filename.replace('%DISTRIBUTION%', distribution)
    return os.path.join(input_filepath, filename)

  def IsConditionSatisfied(src_match):
    expression = src_match.group('expression')
    return grd_node is None or grd_node.EvaluateCondition(expression)

  def CheckConditionalElements(str):
    """Helper function to conditionally inline inner elements"""
    while True:
      begin_if = _BEGIN_IF_BLOCK.search(str)
      if begin_if is None:
        return str

      condition_satisfied = IsConditionSatisfied(begin_if)
      leading = str[0:begin_if.start()]
      content_start = begin_if.end()

      # Find matching "if" block end.
      count = 1
      pos = begin_if.end()
      while True:
        end_if = _END_IF_BLOCK.search(str, pos)
        if end_if is None:
          raise Exception('Unmatched <if>')

        next_if = _BEGIN_IF_BLOCK.search(str, pos)
        if next_if is None or next_if.start() >= end_if.end():
          count = count - 1
          if count == 0:
            break
          pos = end_if.end()
        else:
          count = count + 1
          pos = next_if.end()

      content = str[content_start:end_if.start()]
      trailing = str[end_if.end():]

      if condition_satisfied:
        str = leading + CheckConditionalElements(content) + trailing
      else:
        str = leading + trailing

  def InlineFileContents(src_match, pattern, inlined_files=inlined_files):
    """Helper function to inline external script and css files"""
    filepath = GetFilepath(src_match)
    if filepath is None:
      return src_match.group(0)
    inlined_files.add(filepath)

    # Even if names_only is set, html files needs to be opened, because it
    # can link to images that need to be added to the file set.
    if names_only and not filepath.endswith('.html'):
      return ""

    return pattern % ReadFile(filepath)

  def InlineIncludeFiles(src_match):
    """Helper function to inline external script files"""
    return InlineFileContents(src_match, '%s')

  def InlineScript(src_match):
    """Helper function to inline external script files"""
    return InlineFileContents(src_match, '<script>%s</script>')

  def InlineCSSText(text, css_filepath):
    """Helper function that inlines external resources in CSS text"""
    filepath = os.path.dirname(css_filepath)
    return InlineCSSImages(text, filepath)

  def InlineCSSFile(src_match, inlined_files=inlined_files):
    """Helper function to inline external css files.

    Args:
      src_match: A regular expression match with a named group named "filename".

    Returns:
      The text that should replace the reference to the CSS file.
    """
    filepath = GetFilepath(src_match)
    if filepath is None:
      return src_match.group(0)

    # Even if names_only is set, the CSS file needs to be opened, because it
    # can link to images that need to be added to the file set.
    inlined_files.add(filepath)
    # When resolving CSS files we need to pass in the path so that relative URLs
    # can be resolved.
    return '<style>%s</style>' % InlineCSSText(ReadFile(filepath), filepath)

  def InlineCSSImages(text, filepath=input_filepath):
    """Helper function that inlines external images in CSS backgrounds."""
    # Replace contents of url() for css attributes: content, background,
    # or *-image.
    return re.sub('(?:content|background|[\w-]*-image):[ ]*' +
                  'url\((?:\'|\")(?P<filename>[^"\'\)\(]*)(?:\'|\")',
                  lambda m: SrcReplace(m, filepath),
                  text)

  flat_text = ReadFile(input_filename)

  if not allow_external_script:
    # We need to inline css and js before we inline images so that image
    # references gets inlined in the css and js
    flat_text = re.sub('<script .*?src="(?P<filename>[^"\']*)".*?></script>',
                       InlineScript,
                       flat_text)

  flat_text = re.sub(
      '<link rel="stylesheet".+?href="(?P<filename>[^"]*)".*?>',
      InlineCSSFile,
      flat_text)

  flat_text = re.sub(
      '<include\s+src="(?P<filename>[^"\']*)".*>',
      InlineIncludeFiles,
      flat_text)

  # Check conditional elements, remove unsatisfied ones from the file.
  flat_text = CheckConditionalElements(flat_text)

  flat_text = re.sub('<(?!script)[^>]+?src="(?P<filename>[^"\']*)"',
                     SrcReplace,
                     flat_text)

  # TODO(arv): Only do this inside <style> tags.
  flat_text = InlineCSSImages(flat_text)

  flat_text = re.sub('<link rel="icon".+?href="(?P<filename>[^"\']*)"',
                     SrcReplace,
                     flat_text)

  if names_only:
    flat_text = None  # Will contains garbage if the flag is set anyway.
  return InlinedData(flat_text, inlined_files)


def InlineToString(input_filename, grd_node, allow_external_script=False):
  """Inlines the resources in a specified file and returns it as a string.

  Args:
    input_filename: name of file to read in
    grd_node: html node from the grd file for this include tag
  Returns:
    the inlined data as a string
  """
  try:
    return DoInline(input_filename,
                    grd_node,
                    allow_external_script=allow_external_script).inlined_data
  except IOError, e:
    raise Exception("Failed to open %s while trying to flatten %s. (%s)" %
                    (e.filename, input_filename, e.strerror))


def InlineToFile(input_filename, output_filename, grd_node):
  """Inlines the resources in a specified file and writes it.

  Reads input_filename, finds all the src attributes and attempts to
  inline the files they are referring to, then writes the result
  to output_filename.

  Args:
    input_filename: name of file to read in
    output_filename: name of file to be written to
    grd_node: html node from the grd file for this include tag
  Returns:
    a set of filenames of all the inlined files
  """
  inlined_data = InlineToString(input_filename, grd_node)
  out_file = open(output_filename, 'wb')
  out_file.writelines(inlined_data)
  out_file.close()


def GetResourceFilenames(filename):
  """For a grd file, returns a set of all the files that would be inline."""
  try:
    return DoInline(filename, None, names_only=True).inlined_files
  except IOError, e:
    raise Exception("Failed to open %s while trying to flatten %s. (%s)" %
                    (e.filename, filename, e.strerror))


def main():
  if len(sys.argv) <= 2:
    print "Flattens a HTML file by inlining its external resources.\n"
    print "html_inline.py inputfile outputfile"
  else:
    InlineToFile(sys.argv[1], sys.argv[2], None)

if __name__ == '__main__':
  main()
