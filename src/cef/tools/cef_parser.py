# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from date_util import *
from file_util import *
import os
import re
import shutil
import string
import sys
import textwrap
import time


def notify(msg):
  """ Display a message. """
  sys.stdout.write('  NOTE: ' + msg + '\n')


def wrap_text(text, indent='', maxchars=80):
  """ Wrap the text to the specified number of characters. If
    necessary a line will be broken and wrapped after a word.
    """
  result = ''
  lines = textwrap.wrap(text, maxchars - len(indent))
  for line in lines:
    result += indent + line + '\n'
  return result


def is_base_class(clsname):
  """ Returns true if |clsname| is a known base (root) class in the object
        hierarchy.
    """
  return clsname == 'CefBaseRefCounted' or clsname == 'CefBaseScoped'


def get_capi_file_name(cppname):
  """ Convert a C++ header file name to a C API header file name. """
  return cppname[:-2] + '_capi.h'


def get_capi_name(cppname, isclassname, prefix=None):
  """ Convert a C++ CamelCaps name to a C API underscore name. """
  result = ''
  lastchr = ''
  for chr in cppname:
    # add an underscore if the current character is an upper case letter
    # and the last character was a lower case letter
    if len(result) > 0 and not chr.isdigit() \
        and chr.upper() == chr \
        and not lastchr.upper() == lastchr:
      result += '_'
    result += chr.lower()
    lastchr = chr

  if isclassname:
    result += '_t'

  if not prefix is None:
    if prefix[0:3] == 'cef':
      # if the prefix name is duplicated in the function name
      # remove that portion of the function name
      subprefix = prefix[3:]
      pos = result.find(subprefix)
      if pos >= 0:
        result = result[0:pos] + result[pos + len(subprefix):]
    result = prefix + '_' + result

  return result


def get_wrapper_type_enum(cppname):
  """ Returns the wrapper type enumeration value for the specified C++ class
        name. """
  return 'WT_' + get_capi_name(cppname, False)[4:].upper()


def get_prev_line(body, pos):
  """ Retrieve the start and end positions and value for the line immediately
    before the line containing the specified position.
    """
  end = body.rfind('\n', 0, pos)
  start = body.rfind('\n', 0, end) + 1
  line = body[start:end]
  return {'start': start, 'end': end, 'line': line}


def get_comment(body, name):
  """ Retrieve the comment for a class or function. """
  result = []

  pos = body.find(name)
  in_block_comment = False
  while pos > 0:
    data = get_prev_line(body, pos)
    line = data['line'].strip()
    pos = data['start']
    if len(line) == 0:
      # check if the next previous line is a comment
      prevdata = get_prev_line(body, pos)
      prevline = prevdata['line'].strip()
      if prevline[0:2] == '//' and prevline[0:3] != '///':
        result.append(None)
      else:
        break
    # single line /*--cef()--*/
    elif line[0:2] == '/*' and line[-2:] == '*/':
      continue
    # start of multi line /*--cef()--*/
    elif in_block_comment and line[0:2] == '/*':
      in_block_comment = False
      continue
    # end of multi line /*--cef()--*/
    elif not in_block_comment and line[-2:] == '*/':
      in_block_comment = True
      continue
    elif in_block_comment:
      continue
    elif line[0:2] == '//':
      # keep the comment line including any leading spaces
      result.append(line[2:])
    else:
      break

  result.reverse()
  return result


def validate_comment(file, name, comment):
  """ Validate the comment array returned by get_comment(). """
  # Verify that the comment contains beginning and ending '///' as required by
  # CppDoc (the leading '//' from each line will already have been removed by
  # the get_comment() logic). There may be additional comments proceeding the
  # CppDoc block so we look at the quantity of lines equaling '/' and expect
  # the last line to be '/'.
  docct = 0
  for line in comment:
    if not line is None and len(line) > 0 and line == '/':
      docct = docct + 1
  if docct != 2 or len(comment) < 3 or comment[len(comment) - 1] != '/':
    raise Exception('Missing or incorrect comment in %s for: %s' % \
        (file, name))


def format_comment(comment, indent, translate_map=None, maxchars=80):
  """ Return the comments array as a formatted string. """
  if not translate_map is None:
    # Replace longest keys first in translation.
    translate_keys = sorted(
        translate_map.keys(), key=lambda item: (-len(item), item))

  result = ''
  wrapme = ''
  hasemptyline = False
  for line in comment:
    # if the line starts with a leading space, remove that space
    if not line is None and len(line) > 0 and line[0:1] == ' ':
      line = line[1:]
      didremovespace = True
    else:
      didremovespace = False

    if line is None or len(line) == 0 or line[0:1] == ' ' \
        or line[0:1] == '/':
      # the previous paragraph, if any, has ended
      if len(wrapme) > 0:
        if not translate_map is None:
          # apply the translation
          for key in translate_keys:
            wrapme = wrapme.replace(key, translate_map[key])
        # output the previous paragraph
        result += wrap_text(wrapme, indent + '// ', maxchars)
        wrapme = ''

    if not line is None:
      if len(line) == 0 or line[0:1] == ' ' or line[0:1] == '/':
        # blank lines or anything that's further indented should be
        # output as-is
        result += indent + '//'
        if len(line) > 0:
          if didremovespace:
            result += ' ' + line
          else:
            result += line
        result += '\n'
      else:
        # add to the current paragraph
        wrapme += line + ' '
    else:
      # output an empty line
      hasemptyline = True
      result += '\n'

  if len(wrapme) > 0:
    if not translate_map is None:
      # apply the translation
      for key in translate_map.keys():
        wrapme = wrapme.replace(key, translate_map[key])
    # output the previous paragraph
    result += wrap_text(wrapme, indent + '// ', maxchars)

  if hasemptyline:
    # an empty line means a break between comments, so the comment is
    # probably a section heading and should have an extra line before it
    result = '\n' + result
  return result


def format_translation_changes(old, new):
  """ Return a comment stating what is different between the old and new
    function prototype parts.
    """
  changed = False
  result = ''

  # normalize C API attributes
  oldargs = [x.replace('struct _', '') for x in old['args']]
  oldretval = old['retval'].replace('struct _', '')
  newargs = [x.replace('struct _', '') for x in new['args']]
  newretval = new['retval'].replace('struct _', '')

  # check if the prototype has changed
  oldset = set(oldargs)
  newset = set(newargs)
  if len(oldset.symmetric_difference(newset)) > 0:
    changed = True
    result += '\n  // WARNING - CHANGED ATTRIBUTES'

    # in the implementation set only
    oldonly = oldset.difference(newset)
    for arg in oldonly:
      result += '\n  //   REMOVED: ' + arg

    # in the current set only
    newonly = newset.difference(oldset)
    for arg in newonly:
      result += '\n  //   ADDED:   ' + arg

  # check if the return value has changed
  if oldretval != newretval:
    changed = True
    result += '\n  // WARNING - CHANGED RETURN VALUE'+ \
              '\n  //   WAS: '+old['retval']+ \
              '\n  //   NOW: '+new['retval']

  if changed:
    result += '\n  #pragma message("Warning: "__FILE__": '+new['name']+ \
              ' prototype has changed")\n'

  return result


def format_translation_includes(header, body):
  """ Return the necessary list of includes based on the contents of the
    body.
    """
  result = ''

  # <algorithm> required for VS2013.
  if body.find('std::min') > 0 or body.find('std::max') > 0:
    result += '#include <algorithm>\n'

  if body.find('cef_api_hash(') > 0:
    result += '#include "include/cef_api_hash.h"\n'

  # identify what CppToC classes are being used
  p = re.compile('([A-Za-z0-9_]{1,})CppToC')
  list = sorted(set(p.findall(body)))
  for item in list:
    directory = ''
    if not is_base_class(item):
      cls = header.get_class(item)
      dir = cls.get_file_directory()
      if not dir is None:
        directory = dir + '/'
    result += '#include "libcef_dll/cpptoc/'+directory+ \
              get_capi_name(item[3:], False)+'_cpptoc.h"\n'

  # identify what CToCpp classes are being used
  p = re.compile('([A-Za-z0-9_]{1,})CToCpp')
  list = sorted(set(p.findall(body)))
  for item in list:
    directory = ''
    if not is_base_class(item):
      cls = header.get_class(item)
      dir = cls.get_file_directory()
      if not dir is None:
        directory = dir + '/'
    result += '#include "libcef_dll/ctocpp/'+directory+ \
              get_capi_name(item[3:], False)+'_ctocpp.h"\n'

  if body.find('shutdown_checker') > 0:
    result += '#include "libcef_dll/shutdown_checker.h"\n'

  if body.find('transfer_') > 0:
    result += '#include "libcef_dll/transfer_util.h"\n'

  return result


def str_to_dict(str):
  """ Convert a string to a dictionary. If the same key has multiple values
        the values will be stored in a list. """
  dict = {}
  parts = str.split(',')
  for part in parts:
    part = part.strip()
    if len(part) == 0:
      continue
    sparts = part.split('=')
    if len(sparts) > 2:
      raise Exception('Invalid dictionary pair format: ' + part)
    name = sparts[0].strip()
    if len(sparts) == 2:
      val = sparts[1].strip()
    else:
      val = True
    if name in dict:
      # a value with this name already exists
      curval = dict[name]
      if not isinstance(curval, list):
        # convert the string value to a list
        dict[name] = [curval]
      dict[name].append(val)
    else:
      dict[name] = val
  return dict


def dict_to_str(dict):
  """ Convert a dictionary to a string. """
  str = []
  for name in dict.keys():
    if not isinstance(dict[name], list):
      if dict[name] is True:
        # currently a bool value
        str.append(name)
      else:
        # currently a string value
        str.append(name + '=' + dict[name])
    else:
      # currently a list value
      for val in dict[name]:
        str.append(name + '=' + val)
  return ','.join(str)


# regex for matching comment-formatted attributes
_cre_attrib = '/\*--cef\(([A-Za-z0-9_ ,=:\n]{0,})\)--\*/'
# regex for matching class and function names
_cre_cfname = '([A-Za-z0-9_]{1,})'
# regex for matching class and function names including path separators
_cre_cfnameorpath = '([A-Za-z0-9_\/]{1,})'
# regex for matching function return values
_cre_retval = '([A-Za-z0-9_<>:,\*\&]{1,})'
# regex for matching typedef value and name combination
_cre_typedef = '([A-Za-z0-9_<>:,\*\&\s]{1,})'
# regex for matching function return value and name combination
_cre_func = '([A-Za-z][A-Za-z0-9_<>:,\*\&\s]{1,})'
# regex for matching virtual function modifiers + arbitrary whitespace
_cre_vfmod = '([\sA-Za-z0-9_]{0,})'
# regex for matching arbitrary whitespace
_cre_space = '[\s]{1,}'
# regex for matching optional virtual keyword
_cre_virtual = '(?:[\s]{1,}virtual){0,1}'

# Simple translation types. Format is:
#   'cpp_type' : ['capi_type', 'capi_default_value']
_simpletypes = {
    'void': ['void', ''],
    'void*': ['void*', 'NULL'],
    'int': ['int', '0'],
    'int16': ['int16', '0'],
    'uint16': ['uint16', '0'],
    'int32': ['int32', '0'],
    'uint32': ['uint32', '0'],
    'int64': ['int64', '0'],
    'uint64': ['uint64', '0'],
    'double': ['double', '0'],
    'float': ['float', '0'],
    'float*': ['float*', 'NULL'],
    'long': ['long', '0'],
    'unsigned long': ['unsigned long', '0'],
    'long long': ['long long', '0'],
    'size_t': ['size_t', '0'],
    'bool': ['int', '0'],
    'char': ['char', '0'],
    'char* const': ['char* const', 'NULL'],
    'cef_color_t': ['cef_color_t', '0'],
    'cef_json_parser_error_t': ['cef_json_parser_error_t', 'JSON_NO_ERROR'],
    'cef_plugin_policy_t': ['cef_plugin_policy_t', 'PLUGIN_POLICY_ALLOW'],
    'CefCursorHandle': ['cef_cursor_handle_t', 'kNullCursorHandle'],
    'CefCompositionUnderline': [
        'cef_composition_underline_t', 'CefCompositionUnderline()'
    ],
    'CefEventHandle': ['cef_event_handle_t', 'kNullEventHandle'],
    'CefWindowHandle': ['cef_window_handle_t', 'kNullWindowHandle'],
    'CefPoint': ['cef_point_t', 'CefPoint()'],
    'CefRect': ['cef_rect_t', 'CefRect()'],
    'CefSize': ['cef_size_t', 'CefSize()'],
    'CefRange': ['cef_range_t', 'CefRange()'],
    'CefDraggableRegion': ['cef_draggable_region_t', 'CefDraggableRegion()'],
    'CefThreadId': ['cef_thread_id_t', 'TID_UI'],
    'CefTime': ['cef_time_t', 'CefTime()'],
    'CefAudioParameters': ['cef_audio_parameters_t', 'CefAudioParameters()']
}


def get_function_impls(content, ident, has_impl=True):
  """ Retrieve the function parts from the specified contents as a set of
    return value, name, arguments and body. Ident must occur somewhere in
    the value.
    """
  # extract the functions
  find_regex = '\n' + _cre_func + '\((.*?)\)([A-Za-z0-9_\s]{0,})'
  if has_impl:
    find_regex += '\{(.*?)\n\}'
  else:
    find_regex += '(;)'
  p = re.compile(find_regex, re.MULTILINE | re.DOTALL)
  list = p.findall(content)

  # build the function map with the function name as the key
  result = []
  for retval, argval, vfmod, body in list:
    if retval.find(ident) < 0:
      # the identifier was not found
      continue

    # remove the identifier
    retval = retval.replace(ident, '')
    retval = retval.strip()

    # Normalize the delimiter.
    retval = retval.replace('\n', ' ')

    # retrieve the function name
    parts = retval.split(' ')
    name = parts[-1]
    del parts[-1]
    retval = ' '.join(parts)

    # parse the arguments
    args = []
    for v in argval.split(','):
      v = v.strip()
      if len(v) > 0:
        args.append(v)

    result.append({
        'retval': retval.strip(),
        'name': name,
        'args': args,
        'vfmod': vfmod.strip(),
        'body': body if has_impl else '',
    })

  return result


def get_next_function_impl(existing, name):
  result = None
  for item in existing:
    if item['name'] == name:
      result = item
      existing.remove(item)
      break
  return result


def get_copyright(full=False, translator=True):
  if full:
    result = \
"""// Copyright (c) $YEAR$ Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
  else:
    result = \
"""// Copyright (c) $YEAR$ The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
"""

  if translator:
    result += \
"""//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool. If making changes by
// hand only do so within the body of existing method and function
// implementations. See the translator.README.txt file in the tools directory
// for more information.
//
// $hash=$$HASH$$$
//

"""

  # add the copyright year
  return result.replace('$YEAR$', get_year())


class obj_header:
  """ Class representing a C++ header file. """

  def __init__(self):
    self.filenames = []
    self.typedefs = []
    self.funcs = []
    self.classes = []
    self.root_directory = None

  def set_root_directory(self, root_directory):
    """ Set the root directory. """
    self.root_directory = root_directory

  def get_root_directory(self):
    """ Get the root directory. """
    return self.root_directory

  def add_directory(self, directory, excluded_files=[]):
    """ Add all header files from the specified directory. """
    files = get_files(os.path.join(directory, '*.h'))
    for file in files:
      if len(excluded_files) == 0 or \
          not os.path.split(file)[1] in excluded_files:
        self.add_file(file)

  def add_file(self, filepath):
    """ Add a header file. """

    if self.root_directory is None:
      filename = os.path.split(filepath)[1]
    else:
      filename = os.path.relpath(filepath, self.root_directory)
      filename = filename.replace('\\', '/')

    # read the input file into memory
    self.add_data(filename, read_file(filepath))

  def add_data(self, filename, data):
    """ Add header file contents. """

    added = False

    # remove space from between template definition end brackets
    data = data.replace("> >", ">>")

    # extract global typedefs
    p = re.compile('\ntypedef' + _cre_space + _cre_typedef + ';',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(data)
    if len(list) > 0:
      # build the global typedef objects
      for value in list:
        pos = value.rfind(' ')
        if pos < 0:
          raise Exception('Invalid typedef: ' + value)
        alias = value[pos + 1:].strip()
        value = value[:pos].strip()
        self.typedefs.append(obj_typedef(self, filename, value, alias))

    # extract global functions
    p = re.compile('\n' + _cre_attrib + '\n' + _cre_func + '\((.*?)\)',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(data)
    if len(list) > 0:
      added = True

      # build the global function objects
      for attrib, retval, argval in list:
        comment = get_comment(data, retval + '(' + argval + ');')
        validate_comment(filename, retval, comment)
        self.funcs.append(
            obj_function(self, filename, attrib, retval, argval, comment))

    # extract includes
    p = re.compile('\n#include \"include/' + _cre_cfnameorpath + '.h')
    includes = p.findall(data)

    # extract forward declarations
    p = re.compile('\nclass' + _cre_space + _cre_cfname + ';')
    forward_declares = p.findall(data)

    # extract empty classes
    p = re.compile('\n' + _cre_attrib + '\nclass' + _cre_space + _cre_cfname +
                   _cre_space + ':' + _cre_space + 'public' + _cre_virtual +
                   _cre_space + _cre_cfname + _cre_space + '{};',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(data)
    if len(list) > 0:
      added = True

      # build the class objects
      for attrib, name, parent_name in list:
        # Style may place the ':' on the next line.
        comment = get_comment(data, name + ' :')
        if len(comment) == 0:
          comment = get_comment(data, name + "\n")
        validate_comment(filename, name, comment)
        self.classes.append(
            obj_class(self, filename, attrib, name, parent_name, "", comment,
                      includes, forward_declares))

      # Remove empty classes from |data| so we don't mess up the non-empty
      # class search that follows.
      data = p.sub('', data)

    # extract classes
    p = re.compile('\n' + _cre_attrib + '\nclass' + _cre_space + _cre_cfname +
                   _cre_space + ':' + _cre_space + 'public' + _cre_virtual +
                   _cre_space + _cre_cfname + _cre_space + '{(.*?)\n};',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(data)
    if len(list) > 0:
      added = True

      # build the class objects
      for attrib, name, parent_name, body in list:
        # Style may place the ':' on the next line.
        comment = get_comment(data, name + ' :')
        if len(comment) == 0:
          comment = get_comment(data, name + "\n")
        validate_comment(filename, name, comment)
        self.classes.append(
            obj_class(self, filename, attrib, name, parent_name, body, comment,
                      includes, forward_declares))

    if added:
      # a global function or class was read from the header file
      self.filenames.append(filename)

  def __repr__(self):
    result = ''

    if len(self.typedefs) > 0:
      strlist = []
      for cls in self.typedefs:
        strlist.append(str(cls))
      result += "\n".join(strlist) + "\n\n"

    if len(self.funcs) > 0:
      strlist = []
      for cls in self.funcs:
        strlist.append(str(cls))
      result += "\n".join(strlist) + "\n\n"

    if len(self.classes) > 0:
      strlist = []
      for cls in self.classes:
        strlist.append(str(cls))
      result += "\n".join(strlist)

    return result

  def get_file_names(self):
    """ Return the array of header file names. """
    return self.filenames

  def get_typedefs(self):
    """ Return the array of typedef objects. """
    return self.typedefs

  def get_funcs(self, filename=None):
    """ Return the array of function objects. """
    if filename is None:
      return self.funcs
    else:
      # only return the functions in the specified file
      res = []
      for func in self.funcs:
        if func.get_file_name() == filename:
          res.append(func)
      return res

  def get_classes(self, filename=None):
    """ Return the array of class objects. """
    if filename is None:
      return self.classes
    else:
      # only return the classes in the specified file
      res = []
      for cls in self.classes:
        if cls.get_file_name() == filename:
          res.append(cls)
      return res

  def get_class(self, classname, defined_structs=None):
    """ Return the specified class or None if not found. """
    for cls in self.classes:
      if cls.get_name() == classname:
        return cls
      elif not defined_structs is None:
        defined_structs.append(cls.get_capi_name())
    return None

  def get_class_names(self):
    """ Returns the names of all classes in this object. """
    result = []
    for cls in self.classes:
      result.append(cls.get_name())
    return result

  def get_base_class_name(self, classname):
    """ Returns the base (root) class name for |classname|. """
    cur_cls = self.get_class(classname)
    while True:
      parent_name = cur_cls.get_parent_name()
      if is_base_class(parent_name):
        return parent_name
      else:
        parent_cls = self.get_class(parent_name)
        if parent_cls is None:
          break
      cur_cls = self.get_class(parent_name)
    return None

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    for cls in self.typedefs:
      cls.get_types(list)

    for cls in self.classes:
      cls.get_types(list)

  def get_alias_translation(self, alias):
    """ Return a translation of alias to value based on typedef
            statements. """
    for cls in self.typedefs:
      if cls.alias == alias:
        return cls.value
    return None

  def get_analysis(self, value, named=True):
    """ Return an analysis of the value based the header file context. """
    return obj_analysis([self], value, named)

  def get_defined_structs(self):
    """ Return a list of already defined structure names. """
    return [
        'cef_print_info_t', 'cef_window_info_t', 'cef_base_ref_counted_t',
        'cef_base_scoped_t'
    ]

  def get_capi_translations(self):
    """ Return a dictionary that maps C++ terminology to C API terminology.
        """
    # strings that will be changed in C++ comments
    map = {
        'class': 'structure',
        'Class': 'Structure',
        'interface': 'structure',
        'Interface': 'Structure',
        'true': 'true (1)',
        'false': 'false (0)',
        'empty': 'NULL',
        'method': 'function'
    }

    # add mappings for all classes and functions
    funcs = self.get_funcs()
    for func in funcs:
      map[func.get_name() + '()'] = func.get_capi_name() + '()'

    classes = self.get_classes()
    for cls in classes:
      map[cls.get_name()] = cls.get_capi_name()

      funcs = cls.get_virtual_funcs()
      for func in funcs:
        map[func.get_name() + '()'] = func.get_capi_name() + '()'

      funcs = cls.get_static_funcs()
      for func in funcs:
        map[func.get_name() + '()'] = func.get_capi_name() + '()'

    return map


class obj_class:
  """ Class representing a C++ class. """

  def __init__(self, parent, filename, attrib, name, parent_name, body, comment,
               includes, forward_declares):
    if not isinstance(parent, obj_header):
      raise Exception('Invalid parent object type')

    self.parent = parent
    self.filename = filename
    self.attribs = str_to_dict(attrib)
    self.name = name
    self.parent_name = parent_name
    self.comment = comment
    self.includes = includes
    self.forward_declares = forward_declares

    # extract typedefs
    p = re.compile(
        '\n' + _cre_space + 'typedef' + _cre_space + _cre_typedef + ';',
        re.MULTILINE | re.DOTALL)
    list = p.findall(body)

    # build the typedef objects
    self.typedefs = []
    for value in list:
      pos = value.rfind(' ')
      if pos < 0:
        raise Exception('Invalid typedef: ' + value)
      alias = value[pos + 1:].strip()
      value = value[:pos].strip()
      self.typedefs.append(obj_typedef(self, filename, value, alias))

    # extract static functions
    p = re.compile('\n' + _cre_space + _cre_attrib + '\n' + _cre_space +
                   'static' + _cre_space + _cre_func + '\((.*?)\)',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(body)

    # build the static function objects
    self.staticfuncs = []
    for attrib, retval, argval in list:
      comment = get_comment(body, retval + '(' + argval + ')')
      validate_comment(filename, retval, comment)
      self.staticfuncs.append(
          obj_function_static(self, attrib, retval, argval, comment))

    # extract virtual functions
    p = re.compile(
        '\n' + _cre_space + _cre_attrib + '\n' + _cre_space + 'virtual' +
        _cre_space + _cre_func + '\((.*?)\)' + _cre_vfmod,
        re.MULTILINE | re.DOTALL)
    list = p.findall(body)

    # build the virtual function objects
    self.virtualfuncs = []
    for attrib, retval, argval, vfmod in list:
      comment = get_comment(body, retval + '(' + argval + ')')
      validate_comment(filename, retval, comment)
      self.virtualfuncs.append(
          obj_function_virtual(self, attrib, retval, argval, comment,
                               vfmod.strip()))

  def __repr__(self):
    result = '/* ' + dict_to_str(
        self.attribs) + ' */ class ' + self.name + "\n{"

    if len(self.typedefs) > 0:
      result += "\n\t"
      strlist = []
      for cls in self.typedefs:
        strlist.append(str(cls))
      result += "\n\t".join(strlist)

    if len(self.staticfuncs) > 0:
      result += "\n\t"
      strlist = []
      for cls in self.staticfuncs:
        strlist.append(str(cls))
      result += "\n\t".join(strlist)

    if len(self.virtualfuncs) > 0:
      result += "\n\t"
      strlist = []
      for cls in self.virtualfuncs:
        strlist.append(str(cls))
      result += "\n\t".join(strlist)

    result += "\n};\n"
    return result

  def get_file_name(self):
    """ Return the C++ header file name. Includes the directory component,
            if any. """
    return self.filename

  def get_capi_file_name(self):
    """ Return the CAPI header file name. Includes the directory component,
            if any. """
    return get_capi_file_name(self.filename)

  def get_file_directory(self):
    """ Return the file directory component, if any. """
    pos = self.filename.rfind('/')
    if pos >= 0:
      return self.filename[:pos]
    return None

  def get_name(self):
    """ Return the class name. """
    return self.name

  def get_capi_name(self):
    """ Return the CAPI structure name for this class. """
    return get_capi_name(self.name, True)

  def get_parent_name(self):
    """ Return the parent class name. """
    return self.parent_name

  def get_parent_capi_name(self):
    """ Return the CAPI structure name for the parent class. """
    return get_capi_name(self.parent_name, True)

  def has_parent(self, parent_name):
    """ Returns true if this class has the specified class anywhere in its
            inheritance hierarchy. """
    # Every class has a known base class as the top-most parent.
    if is_base_class(parent_name) or parent_name == self.parent_name:
      return True
    if is_base_class(self.parent_name):
      return False

    cur_cls = self.parent.get_class(self.parent_name)
    while True:
      cur_parent_name = cur_cls.get_parent_name()
      if is_base_class(cur_parent_name):
        break
      elif cur_parent_name == parent_name:
        return True
      cur_cls = self.parent.get_class(cur_parent_name)

    return False

  def get_comment(self):
    """ Return the class comment as an array of lines. """
    return self.comment

  def get_includes(self):
    """ Return the list of classes that are included from this class'
            header file. """
    return self.includes

  def get_forward_declares(self):
    """ Return the list of classes that are forward declared for this
            class. """
    return self.forward_declares

  def get_attribs(self):
    """ Return all attributes as a dictionary. """
    return self.attribs

  def has_attrib(self, name):
    """ Return true if the specified attribute exists. """
    return name in self.attribs

  def get_attrib(self, name):
    """ Return the first or only value for specified attribute. """
    if name in self.attribs:
      if isinstance(self.attribs[name], list):
        # the value is a list
        return self.attribs[name][0]
      else:
        # the value is a string
        return self.attribs[name]
    return None

  def get_attrib_list(self, name):
    """ Return all values for specified attribute as a list. """
    if name in self.attribs:
      if isinstance(self.attribs[name], list):
        # the value is already a list
        return self.attribs[name]
      else:
        # convert the value to a list
        return [self.attribs[name]]
    return None

  def get_typedefs(self):
    """ Return the array of typedef objects. """
    return self.typedefs

  def has_typedef_alias(self, alias):
    """ Returns true if the specified typedef alias is defined in the scope
            of this class declaration. """
    for typedef in self.typedefs:
      if typedef.get_alias() == alias:
        return True
    return False

  def get_static_funcs(self):
    """ Return the array of static function objects. """
    return self.staticfuncs

  def get_virtual_funcs(self):
    """ Return the array of virtual function objects. """
    return self.virtualfuncs

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    for cls in self.typedefs:
      cls.get_types(list)

    for cls in self.staticfuncs:
      cls.get_types(list)

    for cls in self.virtualfuncs:
      cls.get_types(list)

  def get_alias_translation(self, alias):
    for cls in self.typedefs:
      if cls.alias == alias:
        return cls.value
    return None

  def get_analysis(self, value, named=True):
    """ Return an analysis of the value based on the class definition
        context.
        """
    return obj_analysis([self, self.parent], value, named)

  def is_library_side(self):
    """ Returns true if the class is implemented by the library. """
    return self.attribs['source'] == 'library'

  def is_client_side(self):
    """ Returns true if the class is implemented by the client. """
    return self.attribs['source'] == 'client'


class obj_typedef:
  """ Class representing a typedef statement. """

  def __init__(self, parent, filename, value, alias):
    if not isinstance(parent, obj_header) \
        and not isinstance(parent, obj_class):
      raise Exception('Invalid parent object type')

    self.parent = parent
    self.filename = filename
    self.alias = alias
    self.value = self.parent.get_analysis(value, False)

  def __repr__(self):
    return 'typedef ' + self.value.get_type() + ' ' + self.alias + ';'

  def get_file_name(self):
    """ Return the C++ header file name. """
    return self.filename

  def get_capi_file_name(self):
    """ Return the CAPI header file name. """
    return get_capi_file_name(self.filename)

  def get_alias(self):
    """ Return the alias. """
    return self.alias

  def get_value(self):
    """ Return an analysis of the value based on the class or header file
        definition context.
        """
    return self.value

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    name = self.value.get_type()
    if not name in list:
      list[name] = self.value


class obj_function:
  """ Class representing a function. """

  def __init__(self, parent, filename, attrib, retval, argval, comment):
    self.parent = parent
    self.filename = filename
    self.attribs = str_to_dict(attrib)
    self.retval = obj_argument(self, retval)
    self.name = self.retval.remove_name()
    self.comment = comment

    # build the argument objects
    self.arguments = []
    arglist = argval.split(',')
    argindex = 0
    while argindex < len(arglist):
      arg = arglist[argindex]
      if arg.find('<') >= 0 and arg.find('>') == -1:
        # We've split inside of a template type declaration. Join the
        # next argument with this argument.
        argindex += 1
        arg += ',' + arglist[argindex]

      arg = arg.strip()
      if len(arg) > 0:
        argument = obj_argument(self, arg)
        if argument.needs_attrib_count_func() and \
            argument.get_attrib_count_func() is None:
          raise Exception("A 'count_func' attribute is required "+ \
                          "for the '"+argument.get_name()+ \
                          "' parameter to "+self.get_qualified_name())
        self.arguments.append(argument)

      argindex += 1

    if self.retval.needs_attrib_default_retval() and \
        self.retval.get_attrib_default_retval() is None:
      raise Exception("A 'default_retval' attribute is required for "+ \
                      self.get_qualified_name())

  def __repr__(self):
    return '/* ' + dict_to_str(self.attribs) + ' */ ' + self.get_cpp_proto()

  def get_file_name(self):
    """ Return the C++ header file name. """
    return self.filename

  def get_capi_file_name(self):
    """ Return the CAPI header file name. """
    return get_capi_file_name(self.filename)

  def get_name(self):
    """ Return the function name. """
    return self.name

  def get_qualified_name(self):
    """ Return the fully qualified function name. """
    if isinstance(self.parent, obj_header):
      # global function
      return self.name
    else:
      # member function
      return self.parent.get_name() + '::' + self.name

  def get_capi_name(self, prefix=None):
    """ Return the CAPI function name. """
    if 'capi_name' in self.attribs:
      return self.attribs['capi_name']
    return get_capi_name(self.name, False, prefix)

  def get_comment(self):
    """ Return the function comment as an array of lines. """
    return self.comment

  def get_attribs(self):
    """ Return all attributes as a dictionary. """
    return self.attribs

  def has_attrib(self, name):
    """ Return true if the specified attribute exists. """
    return name in self.attribs

  def get_attrib(self, name):
    """ Return the first or only value for specified attribute. """
    if name in self.attribs:
      if isinstance(self.attribs[name], list):
        # the value is a list
        return self.attribs[name][0]
      else:
        # the value is a string
        return self.attribs[name]
    return None

  def get_attrib_list(self, name):
    """ Return all values for specified attribute as a list. """
    if name in self.attribs:
      if isinstance(self.attribs[name], list):
        # the value is already a list
        return self.attribs[name]
      else:
        # convert the value to a list
        return [self.attribs[name]]
    return None

  def get_retval(self):
    """ Return the return value object. """
    return self.retval

  def get_arguments(self):
    """ Return the argument array. """
    return self.arguments

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    for cls in self.arguments:
      cls.get_types(list)

  def get_capi_parts(self, defined_structs=[], prefix=None):
    """ Return the parts of the C API function definition. """
    retval = ''
    dict = self.retval.get_type().get_capi(defined_structs)
    if dict['format'] == 'single':
      retval = dict['value']

    name = self.get_capi_name(prefix)
    args = []

    if isinstance(self, obj_function_virtual):
      # virtual functions get themselves as the first argument
      str = 'struct _' + self.parent.get_capi_name() + '* self'
      if isinstance(self, obj_function_virtual) and self.is_const():
        # const virtual functions get const self pointers
        str = 'const ' + str
      args.append(str)

    if len(self.arguments) > 0:
      for cls in self.arguments:
        type = cls.get_type()
        dict = type.get_capi(defined_structs)
        if dict['format'] == 'single':
          args.append(dict['value'])
        elif dict['format'] == 'multi-arg':
          # add an additional argument for the size of the array
          type_name = type.get_name()
          if type.is_const():
            # for const arrays pass the size argument by value
            args.append('size_t ' + type_name + 'Count')
          else:
            # for non-const arrays pass the size argument by address
            args.append('size_t* ' + type_name + 'Count')
          args.append(dict['value'])

    return {'retval': retval, 'name': name, 'args': args}

  def get_capi_proto(self, defined_structs=[], prefix=None):
    """ Return the prototype of the C API function. """
    parts = self.get_capi_parts(defined_structs, prefix)
    result = parts['retval']+' '+parts['name']+ \
             '('+', '.join(parts['args'])+')'
    return result

  def get_cpp_parts(self, isimpl=False):
    """ Return the parts of the C++ function definition. """
    retval = str(self.retval)
    name = self.name

    args = []
    if len(self.arguments) > 0:
      for cls in self.arguments:
        args.append(str(cls))

    if isimpl and isinstance(self, obj_function_virtual):
      # enumeration return values must be qualified with the class name
      # if the type is defined in the class declaration scope.
      type = self.get_retval().get_type()
      if type.is_result_struct() and type.is_result_struct_enum() and \
          self.parent.has_typedef_alias(retval):
        retval = self.parent.get_name() + '::' + retval

    return {'retval': retval, 'name': name, 'args': args}

  def get_cpp_proto(self, classname=None):
    """ Return the prototype of the C++ function. """
    parts = self.get_cpp_parts()
    result = parts['retval'] + ' '
    if not classname is None:
      result += classname + '::'
    result += parts['name'] + '(' + ', '.join(parts['args']) + ')'
    if isinstance(self, obj_function_virtual) and self.is_const():
      result += ' const'
    return result

  def is_same_side(self, other_class_name):
    """ Returns true if this function is on the same side (library or
            client) and the specified class. """
    if isinstance(self.parent, obj_class):
      # this function is part of a class
      this_is_library_side = self.parent.is_library_side()
      header = self.parent.parent
    else:
      # this function is global
      this_is_library_side = True
      header = self.parent

    if is_base_class(other_class_name):
      other_is_library_side = False
    else:
      other_class = header.get_class(other_class_name)
      if other_class is None:
        raise Exception('Unknown class: ' + other_class_name)
      other_is_library_side = other_class.is_library_side()

    return other_is_library_side == this_is_library_side


class obj_function_static(obj_function):
  """ Class representing a static function. """

  def __init__(self, parent, attrib, retval, argval, comment):
    if not isinstance(parent, obj_class):
      raise Exception('Invalid parent object type')
    obj_function.__init__(self, parent, parent.filename, attrib, retval, argval,
                          comment)

  def __repr__(self):
    return 'static ' + obj_function.__repr__(self) + ';'

  def get_capi_name(self, prefix=None):
    """ Return the CAPI function name. """
    if prefix is None:
      # by default static functions are prefixed with the class name
      prefix = get_capi_name(self.parent.get_name(), False)
    return obj_function.get_capi_name(self, prefix)


class obj_function_virtual(obj_function):
  """ Class representing a virtual function. """

  def __init__(self, parent, attrib, retval, argval, comment, vfmod):
    if not isinstance(parent, obj_class):
      raise Exception('Invalid parent object type')
    obj_function.__init__(self, parent, parent.filename, attrib, retval, argval,
                          comment)
    if vfmod == 'const':
      self.isconst = True
    else:
      self.isconst = False

  def __repr__(self):
    return 'virtual ' + obj_function.__repr__(self) + ';'

  def is_const(self):
    """ Returns true if the method declaration is const. """
    return self.isconst


class obj_argument:
  """ Class representing a function argument. """

  def __init__(self, parent, argval):
    if not isinstance(parent, obj_function):
      raise Exception('Invalid parent object type')

    self.parent = parent
    self.type = self.parent.parent.get_analysis(argval)

  def __repr__(self):
    result = ''
    if self.type.is_const():
      result += 'const '
    result += self.type.get_type()
    if self.type.is_byref():
      result += '&'
    elif self.type.is_byaddr():
      result += '*'
    if self.type.has_name():
      result += ' ' + self.type.get_name()
    return result

  def get_name(self):
    """ Return the name for this argument. """
    return self.type.get_name()

  def remove_name(self):
    """ Remove and return the name value. """
    name = self.type.get_name()
    self.type.name = None
    return name

  def get_type(self):
    """ Return an analysis of the argument type based on the class
        definition context.
        """
    return self.type

  def get_types(self, list):
    """ Return a dictionary mapping data types to analyzed values. """
    name = self.type.get_type()
    if not name in list:
      list[name] = self.type

  def needs_attrib_count_func(self):
    """ Returns true if this argument requires a 'count_func' attribute. """
    # A 'count_func' attribute is required for non-const non-string vector
    # attribute types
    return self.type.has_name() and \
        self.type.is_result_vector() and \
        not self.type.is_result_vector_string() and \
        not self.type.is_const()

  def get_attrib_count_func(self):
    """ Returns the count function for this argument. """
    # The 'count_func' attribute value format is name:function
    if not self.parent.has_attrib('count_func'):
      return None
    name = self.type.get_name()
    vals = self.parent.get_attrib_list('count_func')
    for val in vals:
      parts = val.split(':')
      if len(parts) != 2:
        raise Exception("Invalid 'count_func' attribute value for "+ \
                        self.parent.get_qualified_name()+': '+val)
      if parts[0].strip() == name:
        return parts[1].strip()
    return None

  def needs_attrib_default_retval(self):
    """ Returns true if this argument requires a 'default_retval' attribute.
        """
    # A 'default_retval' attribute is required for enumeration return value
    # types.
    return not self.type.has_name() and \
        self.type.is_result_struct() and \
        self.type.is_result_struct_enum()

  def get_attrib_default_retval(self):
    """ Returns the defualt return value for this argument. """
    return self.parent.get_attrib('default_retval')

  def get_arg_type(self):
    """ Returns the argument type as defined in translator.README.txt. """
    if not self.type.has_name():
      raise Exception('Cannot be called for retval types')

    # simple or enumeration type
    if (self.type.is_result_simple() and \
            self.type.get_type() != 'bool') or \
       (self.type.is_result_struct() and \
            self.type.is_result_struct_enum()):
      if self.type.is_byref():
        if self.type.is_const():
          return 'simple_byref_const'
        return 'simple_byref'
      elif self.type.is_byaddr():
        return 'simple_byaddr'
      return 'simple_byval'

    # boolean type
    if self.type.get_type() == 'bool':
      if self.type.is_byref():
        return 'bool_byref'
      elif self.type.is_byaddr():
        return 'bool_byaddr'
      return 'bool_byval'

    # structure type
    if self.type.is_result_struct() and self.type.is_byref():
      if self.type.is_const():
        return 'struct_byref_const'
      return 'struct_byref'

    # string type
    if self.type.is_result_string() and self.type.is_byref():
      if self.type.is_const():
        return 'string_byref_const'
      return 'string_byref'

    # *ptr type
    if self.type.is_result_ptr():
      prefix = self.type.get_result_ptr_type_prefix()
      same_side = self.parent.is_same_side(self.type.get_ptr_type())
      if self.type.is_byref():
        if same_side:
          return prefix + 'ptr_same_byref'
        return prefix + 'ptr_diff_byref'
      if same_side:
        return prefix + 'ptr_same'
      return prefix + 'ptr_diff'

    if self.type.is_result_vector():
      # all vector types must be passed by reference
      if not self.type.is_byref():
        return 'invalid'

      if self.type.is_result_vector_string():
        # string vector type
        if self.type.is_const():
          return 'string_vec_byref_const'
        return 'string_vec_byref'

      if self.type.is_result_vector_simple():
        if self.type.get_vector_type() != 'bool':
          # simple/enumeration vector types
          if self.type.is_const():
            return 'simple_vec_byref_const'
          return 'simple_vec_byref'

        # boolean vector types
        if self.type.is_const():
          return 'bool_vec_byref_const'
        return 'bool_vec_byref'

      if self.type.is_result_vector_ptr():
        # *ptr vector types
        prefix = self.type.get_result_vector_ptr_type_prefix()
        same_side = self.parent.is_same_side(self.type.get_ptr_type())
        if self.type.is_const():
          if same_side:
            return prefix + 'ptr_vec_same_byref_const'
          return prefix + 'ptr_vec_diff_byref_const'
        if same_side:
          return prefix + 'ptr_vec_same_byref'
        return prefix + 'ptr_vec_diff_byref'

    # string single map type
    if self.type.is_result_map_single():
      if not self.type.is_byref():
        return 'invalid'
      if self.type.is_const():
        return 'string_map_single_byref_const'
      return 'string_map_single_byref'

    # string multi map type
    if self.type.is_result_map_multi():
      if not self.type.is_byref():
        return 'invalid'
      if self.type.is_const():
        return 'string_map_multi_byref_const'
      return 'string_map_multi_byref'

    return 'invalid'

  def get_retval_type(self):
    """ Returns the retval type as defined in translator.README.txt. """
    if self.type.has_name():
      raise Exception('Cannot be called for argument types')

    # unsupported modifiers
    if self.type.is_const() or self.type.is_byref() or \
        self.type.is_byaddr():
      return 'invalid'

    # void types don't have a return value
    if self.type.get_type() == 'void':
      return 'none'

    if (self.type.is_result_simple() and \
            self.type.get_type() != 'bool') or \
       (self.type.is_result_struct() and self.type.is_result_struct_enum()):
      return 'simple'

    if self.type.get_type() == 'bool':
      return 'bool'

    if self.type.is_result_string():
      return 'string'

    if self.type.is_result_ptr():
      prefix = self.type.get_result_ptr_type_prefix()
      if self.parent.is_same_side(self.type.get_ptr_type()):
        return prefix + 'ptr_same'
      else:
        return prefix + 'ptr_diff'

    return 'invalid'

  def get_retval_default(self, for_capi):
    """ Returns the default return value based on the retval type. """
    # start with the default retval attribute, if any.
    retval = self.get_attrib_default_retval()
    if not retval is None:
      if for_capi:
        # apply any appropriate C API translations.
        if retval == 'true':
          return '1'
        if retval == 'false':
          return '0'
      return retval

    # next look at the retval type value.
    type = self.get_retval_type()
    if type == 'simple':
      return self.get_type().get_result_simple_default()
    elif type == 'bool':
      if for_capi:
        return '0'
      return 'false'
    elif type == 'string':
      if for_capi:
        return 'NULL'
      return 'CefString()'
    elif type == 'refptr_same' or type == 'refptr_diff' or \
         type == 'rawptr_same' or type == 'rawptr_diff':
      if for_capi:
        return 'NULL'
      return 'nullptr'
    elif type == 'ownptr_same' or type == 'ownptr_diff':
      if for_capi:
        return 'NULL'
      return 'CefOwnPtr<' + self.type.get_ptr_type() + '>()'

    return ''


class obj_analysis:
  """ Class representing an analysis of a data type value. """

  def __init__(self, scopelist, value, named):
    self.value = value
    self.result_type = 'unknown'
    self.result_value = None
    self.result_default = None
    self.ptr_type = None

    # parse the argument string
    partlist = value.strip().split()

    if named == True:
      # extract the name value
      self.name = partlist[-1]
      del partlist[-1]
    else:
      self.name = None

    if len(partlist) == 0:
      raise Exception('Invalid argument value: ' + value)

    # check const status
    if partlist[0] == 'const':
      self.isconst = True
      del partlist[0]
    else:
      self.isconst = False

    if len(partlist) == 0:
      raise Exception('Invalid argument value: ' + value)

    # combine the data type
    self.type = ' '.join(partlist)

    # extract the last character of the data type
    endchar = self.type[-1]

    # check if the value is passed by reference
    if endchar == '&':
      self.isbyref = True
      self.type = self.type[:-1]
    else:
      self.isbyref = False

    # check if the value is passed by address
    if endchar == '*':
      self.isbyaddr = True
      self.type = self.type[:-1]
    else:
      self.isbyaddr = False

    # see if the value is directly identifiable
    if self._check_advanced(self.type) == True:
      return

    # not identifiable, so look it up
    translation = None
    for scope in scopelist:
      if not isinstance(scope, obj_header) \
          and not isinstance(scope, obj_class):
        raise Exception('Invalid scope object type')
      translation = scope.get_alias_translation(self.type)
      if not translation is None:
        break

    if translation is None:
      raise Exception('Failed to translate type: ' + self.type)

    # the translation succeeded so keep the result
    self.result_type = translation.result_type
    self.result_value = translation.result_value

  def _check_advanced(self, value):
    # check for vectors
    if value.find('std::vector') == 0:
      self.result_type = 'vector'
      val = value[12:-1].strip()
      self.result_value = [self._get_basic(val)]
      self.result_value[0]['vector_type'] = val
      return True

    # check for maps
    if value.find('std::map') == 0:
      self.result_type = 'map'
      vals = value[9:-1].split(',')
      if len(vals) == 2:
        self.result_value = [
            self._get_basic(vals[0].strip()),
            self._get_basic(vals[1].strip())
        ]
        return True

    # check for multimaps
    if value.find('std::multimap') == 0:
      self.result_type = 'multimap'
      vals = value[14:-1].split(',')
      if len(vals) == 2:
        self.result_value = [
            self._get_basic(vals[0].strip()),
            self._get_basic(vals[1].strip())
        ]
        return True

    # check for basic types
    basic = self._get_basic(value)
    if not basic is None:
      self.result_type = basic['result_type']
      self.result_value = basic['result_value']
      if 'ptr_type' in basic:
        self.ptr_type = basic['ptr_type']
      if 'result_default' in basic:
        self.result_default = basic['result_default']
      return True

    return False

  def _get_basic(self, value):
    # check for string values
    if value == "CefString":
      return {'result_type': 'string', 'result_value': None}

    # check for simple direct translations
    if value in _simpletypes.keys():
      return {
          'result_type': 'simple',
          'result_value': _simpletypes[value][0],
          'result_default': _simpletypes[value][1],
      }

    # check if already a C API structure
    if value[-2:] == '_t':
      return {'result_type': 'structure', 'result_value': value}

    # check for CEF reference pointers
    p = re.compile('^CefRefPtr<(.*?)>$', re.DOTALL)
    list = p.findall(value)
    if len(list) == 1:
      return {
          'result_type': 'refptr',
          'result_value': get_capi_name(list[0], True) + '*',
          'ptr_type': list[0]
      }

    # check for CEF owned pointers
    p = re.compile('^CefOwnPtr<(.*?)>$', re.DOTALL)
    list = p.findall(value)
    if len(list) == 1:
      return {
          'result_type': 'ownptr',
          'result_value': get_capi_name(list[0], True) + '*',
          'ptr_type': list[0]
      }

    # check for CEF raw pointers
    p = re.compile('^CefRawPtr<(.*?)>$', re.DOTALL)
    list = p.findall(value)
    if len(list) == 1:
      return {
          'result_type': 'rawptr',
          'result_value': get_capi_name(list[0], True) + '*',
          'ptr_type': list[0]
      }

    # check for CEF structure types
    if value[0:3] == 'Cef' and value[-4:] != 'List':
      return {
          'result_type': 'structure',
          'result_value': get_capi_name(value, True)
      }

    return None

  def __repr__(self):
    return '(' + self.result_type + ') ' + str(self.result_value)

  def has_name(self):
    """ Returns true if a name value exists. """
    return (not self.name is None)

  def get_name(self):
    """ Return the name. """
    return self.name

  def get_value(self):
    """ Return the C++ value (type + name). """
    return self.value

  def get_type(self):
    """ Return the C++ type. """
    return self.type

  def get_ptr_type(self):
    """ Return the C++ class type referenced by a CefRefPtr. """
    if self.is_result_vector() and self.is_result_vector_ptr():
      # return the vector RefPtr type
      return self.result_value[0]['ptr_type']
    # return the basic RefPtr type
    return self.ptr_type

  def get_vector_type(self):
    """ Return the C++ class type referenced by a std::vector. """
    if self.is_result_vector():
      return self.result_value[0]['vector_type']
    return None

  def is_const(self):
    """ Returns true if the argument value is constant. """
    return self.isconst

  def is_byref(self):
    """ Returns true if the argument is passed by reference. """
    return self.isbyref

  def is_byaddr(self):
    """ Returns true if the argument is passed by address. """
    return self.isbyaddr

  def is_result_simple(self):
    """ Returns true if this is a simple argument type. """
    return (self.result_type == 'simple')

  def get_result_simple_type_root(self):
    """ Return the simple structure or basic type name. """
    return self.result_value

  def get_result_simple_type(self):
    """ Return the simple type. """
    result = ''
    if self.is_const():
      result += 'const '
    result += self.result_value
    if self.is_byaddr() or self.is_byref():
      result += '*'
    return result

  def get_result_simple_default(self):
    """ Return the default value fo the basic type. """
    return self.result_default

  def is_result_ptr(self):
    """ Returns true if this is a *Ptr type. """
    return self.is_result_refptr() or self.is_result_ownptr() or \
           self.is_result_rawptr()

  def get_result_ptr_type_root(self):
    """ Return the *Ptr type structure name. """
    return self.result_value[:-1]

  def get_result_ptr_type(self, defined_structs=[]):
    """ Return the *Ptr type. """
    result = ''
    if not self.result_value[:-1] in defined_structs:
      result += 'struct _'
    result += self.result_value
    if self.is_byref() or self.is_byaddr():
      result += '*'
    return result

  def get_result_ptr_type_prefix(self):
    """ Returns the *Ptr type prefix. """
    if self.is_result_refptr():
      return 'ref'
    if self.is_result_ownptr():
      return 'own'
    if self.is_result_rawptr():
      return 'raw'
    raise Exception('Not a pointer type')

  def is_result_refptr(self):
    """ Returns true if this is a RefPtr type. """
    return (self.result_type == 'refptr')

  def is_result_ownptr(self):
    """ Returns true if this is a OwnPtr type. """
    return (self.result_type == 'ownptr')

  def is_result_rawptr(self):
    """ Returns true if this is a RawPtr type. """
    return (self.result_type == 'rawptr')

  def is_result_struct(self):
    """ Returns true if this is a structure type. """
    return (self.result_type == 'structure')

  def is_result_struct_enum(self):
    """ Returns true if this struct type is likely an enumeration. """
    # structure values that are passed by reference or address must be
    # structures and not enumerations
    if not self.is_byref() and not self.is_byaddr():
      return True
    return False

  def get_result_struct_type(self, defined_structs=[]):
    """ Return the structure or enumeration type. """
    result = ''
    is_enum = self.is_result_struct_enum()
    if not is_enum:
      if self.is_const():
        result += 'const '
      if not self.result_value in defined_structs:
        result += 'struct _'
    result += self.result_value
    if not is_enum:
      result += '*'
    return result

  def is_result_string(self):
    """ Returns true if this is a string type. """
    return (self.result_type == 'string')

  def get_result_string_type(self):
    """ Return the string type. """
    if not self.has_name():
      # Return values are string structs that the user must free. Use
      # the name of the structure as a hint.
      return 'cef_string_userfree_t'
    elif not self.is_const() and (self.is_byref() or self.is_byaddr()):
      # Parameters passed by reference or address. Use the normal
      # non-const string struct.
      return 'cef_string_t*'
    # Const parameters use the const string struct.
    return 'const cef_string_t*'

  def is_result_vector(self):
    """ Returns true if this is a vector type. """
    return (self.result_type == 'vector')

  def is_result_vector_string(self):
    """ Returns true if this is a string vector. """
    return self.result_value[0]['result_type'] == 'string'

  def is_result_vector_simple(self):
    """ Returns true if this is a string vector. """
    return self.result_value[0]['result_type'] == 'simple'

  def is_result_vector_ptr(self):
    """ Returns true if this is a *Ptr vector. """
    return self.is_result_vector_refptr() or \
           self.is_result_vector_ownptr() or \
           self.is_result_vector_rawptr()

  def get_result_vector_ptr_type_prefix(self):
    """ Returns the *Ptr type prefix. """
    if self.is_result_vector_refptr():
      return 'ref'
    if self.is_result_vector_ownptr():
      return 'own'
    if self.is_result_vector_rawptr():
      return 'raw'
    raise Exception('Not a pointer type')

  def is_result_vector_refptr(self):
    """ Returns true if this is a RefPtr vector. """
    return self.result_value[0]['result_type'] == 'refptr'

  def is_result_vector_ownptr(self):
    """ Returns true if this is a OwnPtr vector. """
    return self.result_value[0]['result_type'] == 'ownptr'

  def is_result_vector_rawptr(self):
    """ Returns true if this is a RawPtr vector. """
    return self.result_value[0]['result_type'] == 'rawptr'

  def get_result_vector_type_root(self):
    """ Return the vector structure or basic type name. """
    return self.result_value[0]['result_value']

  def get_result_vector_type(self, defined_structs=[]):
    """ Return the vector type. """
    if not self.has_name():
      raise Exception('Cannot use vector as a return type')

    type = self.result_value[0]['result_type']
    value = self.result_value[0]['result_value']

    result = {}
    if type == 'string':
      result['value'] = 'cef_string_list_t'
      result['format'] = 'single'
      return result

    if type == 'simple':
      str = value
      if self.is_const():
        str += ' const'
      str += '*'
      result['value'] = str
    elif type == 'refptr' or type == 'ownptr' or type == 'rawptr':
      str = ''
      if not value[:-1] in defined_structs:
        str += 'struct _'
      str += value
      if self.is_const():
        str += ' const'
      str += '*'
      result['value'] = str
    else:
      raise Exception('Unsupported vector type: ' + type)

    # vector values must be passed as a value array parameter
    # and a size parameter
    result['format'] = 'multi-arg'
    return result

  def is_result_map(self):
    """ Returns true if this is a map type. """
    return (self.result_type == 'map' or self.result_type == 'multimap')

  def is_result_map_single(self):
    """ Returns true if this is a single map type. """
    return (self.result_type == 'map')

  def is_result_map_multi(self):
    """ Returns true if this is a multi map type. """
    return (self.result_type == 'multimap')

  def get_result_map_type(self, defined_structs=[]):
    """ Return the map type. """
    if not self.has_name():
      raise Exception('Cannot use map as a return type')
    if self.result_value[0]['result_type'] == 'string' \
        and self.result_value[1]['result_type'] == 'string':
      if self.result_type == 'map':
        return {'value': 'cef_string_map_t', 'format': 'single'}
      elif self.result_type == 'multimap':
        return {'value': 'cef_string_multimap_t', 'format': 'multi'}
    raise Exception('Only mappings of strings to strings are supported')

  def get_capi(self, defined_structs=[]):
    """ Format the value for the C API. """
    result = ''
    format = 'single'
    if self.is_result_simple():
      result += self.get_result_simple_type()
    elif self.is_result_ptr():
      result += self.get_result_ptr_type(defined_structs)
    elif self.is_result_struct():
      result += self.get_result_struct_type(defined_structs)
    elif self.is_result_string():
      result += self.get_result_string_type()
    elif self.is_result_map():
      resdict = self.get_result_map_type(defined_structs)
      if resdict['format'] == 'single' or resdict['format'] == 'multi':
        result += resdict['value']
      else:
        raise Exception('Unsupported map type')
    elif self.is_result_vector():
      resdict = self.get_result_vector_type(defined_structs)
      if resdict['format'] != 'single':
        format = resdict['format']
      result += resdict['value']

    if self.has_name():
      result += ' ' + self.get_name()

    return {'format': format, 'value': result}


# test the module
if __name__ == "__main__":
  import pprint
  import sys

  # verify that the correct number of command-line arguments are provided
  if len(sys.argv) != 2:
    sys.stderr.write('Usage: ' + sys.argv[0] + ' <directory>')
    sys.exit()

  pp = pprint.PrettyPrinter(indent=4)

  # create the header object
  header = obj_header()
  header.add_directory(sys.argv[1])

  # output the type mapping
  types = {}
  header.get_types(types)
  pp.pprint(types)
  sys.stdout.write('\n')

  # output the parsed C++ data
  sys.stdout.write(str(header))

  # output the C API formatted data
  defined_names = header.get_defined_structs()
  result = ''

  # global functions
  funcs = header.get_funcs()
  if len(funcs) > 0:
    for func in funcs:
      result += func.get_capi_proto(defined_names) + ';\n'
    result += '\n'

  classes = header.get_classes()
  for cls in classes:
    # virtual functions are inside a structure
    result += 'struct ' + cls.get_capi_name() + '\n{\n'
    funcs = cls.get_virtual_funcs()
    if len(funcs) > 0:
      for func in funcs:
        result += '\t' + func.get_capi_proto(defined_names) + ';\n'
    result += '}\n\n'

    defined_names.append(cls.get_capi_name())

    # static functions become global
    funcs = cls.get_static_funcs()
    if len(funcs) > 0:
      for func in funcs:
        result += func.get_capi_proto(defined_names) + ';\n'
      result += '\n'
  sys.stdout.write(result)
