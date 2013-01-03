#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Generator for C++ style thunks """

import glob
import os
import re
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLAttribute, IDLNode
from idl_ast import IDLAst
from idl_option import GetOption, Option, ParseOptions
from idl_outfile import IDLOutFile
from idl_parser import ParseFiles
from idl_c_proto import CGen, GetNodeComments, CommentLines, Comment
from idl_generator import Generator, GeneratorByFile

Option('thunkroot', 'Base directory of output',
       default=os.path.join('..', 'thunk'))


class TGenError(Exception):
  def __init__(self, msg):
    self.value = msg

  def __str__(self):
    return repr(self.value)


class ThunkBodyMetadata(object):
  """Metadata about thunk body. Used for selecting which headers to emit."""
  def __init__(self):
    self._apis = set()
    self._includes = set()

  def AddApi(self, api):
    self._apis.add(api)

  def Apis(self):
    return self._apis

  def AddInclude(self, include):
    self._includes.add(include)

  def Includes(self):
    return self._includes


def _GetBaseFileName(filenode):
  """Returns the base name for output files, given the filenode.

  Examples:
    'dev/ppb_find_dev.h' -> 'ppb_find'
    'trusted/ppb_buffer_trusted.h' -> 'ppb_buffer_trusted'
  """
  path, name = os.path.split(filenode.GetProperty('NAME'))
  name = os.path.splitext(name)[0]
  if name.endswith('_dev'):
    # Clip off _dev suffix.
    name = name[:-len('_dev')]
  return name


def _GetHeaderFileName(filenode):
  """Returns the name for the header for this file."""
  path, name = os.path.split(filenode.GetProperty('NAME'))
  name = os.path.splitext(name)[0]
  if path:
    header = "ppapi/c/%s/%s.h" % (path, name)
  else:
    header = "ppapi/c/%s.h" % name
  return header


def _GetThunkFileName(filenode, relpath):
  """Returns the thunk file name."""
  path = os.path.split(filenode.GetProperty('NAME'))[0]
  name = _GetBaseFileName(filenode)
  # We don't reattach the path for thunk.
  if relpath: name = os.path.join(relpath, name)
  name = '%s%s' % (name, '_thunk.cc')
  return name


def _MakeEnterLine(filenode, interface, arg, handle_errors, callback, meta):
  """Returns an EnterInstance/EnterResource string for a function."""
  if arg[0] == 'PP_Instance':
    if callback is None:
      return 'EnterInstance enter(%s);' % arg[1]
    else:
      return 'EnterInstance enter(%s, %s);' % (arg[1], callback)
  elif arg[0] == 'PP_Resource':
    api_name = interface.GetName()
    if api_name.endswith('_Dev'):
      api_name = api_name[:-len('_Dev')]
    api_name += '_API'

    enter_type = 'EnterResource<%s>' % api_name
    # The API header matches the file name, not the interface name.
    meta.AddApi(_GetBaseFileName(filenode) + '_api')

    if callback is None:
      return '%s enter(%s, %s);' % (enter_type, arg[1],
                                    str(handle_errors).lower())
    else:
      return '%s enter(%s, %s, %s);' % (enter_type, arg[1],
                                        callback,
                                        str(handle_errors).lower())
  else:
    raise TGenError("Unknown type for _MakeEnterLine: %s" % arg[0])


def _GetShortName(interface, filter_suffixes):
  """Return a shorter interface name that matches Is* and Create* functions."""
  parts = interface.GetName().split('_')[1:]
  tail = parts[len(parts) - 1]
  if tail in filter_suffixes:
    parts = parts[:-1]
  return ''.join(parts)


def _IsTypeCheck(interface, node):
  """Returns true if node represents a type-checking function."""
  return node.GetName() == 'Is%s' % _GetShortName(interface, ['Dev', 'Private'])


def _GetCreateFuncName(interface):
  """Returns the creation function name for an interface."""
  return 'Create%s' % _GetShortName(interface, ['Dev'])


def _GetDefaultFailureValue(t):
  """Returns the default failure value for a given type.

  Returns None if no default failure value exists for the type.
  """
  values = {
      'PP_Bool': 'PP_FALSE',
      'PP_Resource': '0',
      'struct PP_Var': 'PP_MakeUndefined()',
      'int32_t': 'enter.retval()',
      'uint16_t': '0',
      'uint32_t': '0',
      'uint64_t': '0',
  }
  if t in values:
    return values[t]
  return None


def _MakeCreateMemberBody(interface, member, args):
  """Returns the body of a Create() function.

  Args:
    interface - IDLNode for the interface
    member - IDLNode for member function
    args - List of arguments for the Create() function
  """
  if args[0][0] == 'PP_Resource':
    body = '  Resource* object =\n'
    body += '      PpapiGlobals::Get()->GetResourceTracker()->'
    body += 'GetResource(%s);\n' % args[0][1]
    body += '  if (!object)\n'
    body += '    return 0;\n'
    body += '  EnterResourceCreation enter(object->pp_instance());\n'
  elif args[0][0] == 'PP_Instance':
    body = '  EnterResourceCreation enter(%s);\n' % args[0][1]
  else:
    raise TGenError('Unknown arg type for Create(): %s' % args[0][0])

  body += '  if (enter.failed())\n'
  body += '    return 0;\n'
  arg_list = ', '.join([a[1] for a in args])
  if member.GetProperty('create_func'):
    create_func = member.GetProperty('create_func')
  else:
    create_func = _GetCreateFuncName(interface)
  body += '  return enter.functions()->%s(%s);' % (create_func,
                                                   arg_list)
  return body


def _MakeNormalMemberBody(filenode, node, member, rtype, args, meta):
  """Returns the body of a typical function.

  Args:
    filenode - IDLNode for the file
    node - IDLNode for the interface
    member - IDLNode for the member function
    rtype - Return type for the member function
    args - List of 4-tuple arguments for the member function
    meta - ThunkBodyMetadata for header hints
  """
  is_callback_func = args[len(args) - 1][0] == 'struct PP_CompletionCallback'

  if is_callback_func:
    call_args = args[:-1] + [('', 'enter.callback()', '', '')]
    meta.AddInclude('ppapi/c/pp_completion_callback.h')
  else:
    call_args = args

  if args[0][0] == 'PP_Instance':
    call_arglist = ', '.join(a[1] for a in call_args)
    function_container = 'functions'
  else:
    call_arglist = ', '.join(a[1] for a in call_args[1:])
    function_container = 'object'

  invocation = 'enter.%s()->%s(%s)' % (function_container,
                                       member.GetName(),
                                       call_arglist)

  handle_errors = not (member.GetProperty('report_errors') == 'False')
  if is_callback_func:
    body = '  %s\n' % _MakeEnterLine(filenode, node, args[0], handle_errors,
                                     args[len(args) - 1][1], meta)
    body += '  if (enter.failed())\n'
    value = member.GetProperty('on_failure')
    if value is None:
      value = 'enter.retval()'
    body += '    return %s;\n' % value
    body += '  return enter.SetResult(%s);' % invocation
  elif rtype == 'void':
    body = '  %s\n' % _MakeEnterLine(filenode, node, args[0], handle_errors,
                                     None, meta)
    body += '  if (enter.succeeded())\n'
    body += '    %s;' % invocation
  else:
    value = member.GetProperty('on_failure')
    if value is None:
      value = _GetDefaultFailureValue(rtype)
    if value is None:
      raise TGenError('No default value for rtype %s' % rtype)

    body = '  %s\n' % _MakeEnterLine(filenode, node, args[0], handle_errors,
                                     None, meta)
    body += '  if (enter.failed())\n'
    body += '    return %s;\n' % value
    body += '  return %s;' % invocation
  return body


def DefineMember(filenode, node, member, release, include_version, meta):
  """Returns a definition for a member function of an interface.

  Args:
    filenode - IDLNode for the file
    node - IDLNode for the interface
    member - IDLNode for the member function
    release - release to generate
    include_version - include the version in emitted function name.
    meta - ThunkMetadata for header hints
  Returns:
    A string with the member definition.
  """
  cgen = CGen()
  rtype, name, arrays, args = cgen.GetComponents(member, release, 'return')

  if _IsTypeCheck(node, member):
    body = '  %s\n' % _MakeEnterLine(filenode, node, args[0], False, None, meta)
    body += '  return PP_FromBool(enter.succeeded());'
  elif member.GetName() == 'Create':
    body = _MakeCreateMemberBody(node, member, args)
  else:
    body = _MakeNormalMemberBody(filenode, node, member, rtype, args, meta)

  signature = cgen.GetSignature(member, release, 'return', func_as_ptr=False,
                                include_version=include_version)
  member_code = '%s {\n%s\n}' % (signature, body)
  return cgen.Indent(member_code, tabs=0)


class TGen(GeneratorByFile):
  def __init__(self):
    Generator.__init__(self, 'Thunk', 'tgen', 'Generate the C++ thunk.')

  def GenerateFile(self, filenode, releases, options):
    savename = _GetThunkFileName(filenode, GetOption('thunkroot'))
    my_min, my_max = filenode.GetMinMax(releases)
    if my_min > releases[-1] or my_max < releases[0]:
      if os.path.isfile(savename):
        print "Removing stale %s for this range." % filenode.GetName()
        os.remove(os.path.realpath(savename))
      return False
    do_generate = filenode.GetProperty('generate_thunk')
    if not do_generate:
      return False

    thunk_out = IDLOutFile(savename)
    body, meta = self.GenerateBody(thunk_out, filenode, releases, options)
    self.WriteHead(thunk_out, filenode, releases, options, meta)
    thunk_out.Write('\n\n'.join(body))
    self.WriteTail(thunk_out, filenode, releases, options)
    return thunk_out.Close()

  def WriteHead(self, out, filenode, releases, options, meta):
    __pychecker__ = 'unusednames=options'
    cgen = CGen()

    cright_node = filenode.GetChildren()[0]
    assert(cright_node.IsA('Copyright'))
    out.Write('%s\n' % cgen.Copyright(cright_node, cpp_style=True))

    # Wrap the From ... modified ... comment if it would be >80 characters.
    from_text = 'From %s' % (
        filenode.GetProperty('NAME').replace(os.sep,'/'))
    modified_text = 'modified %s.' % (
        filenode.GetProperty('DATETIME'))
    if len(from_text) + len(modified_text) < 74:
      out.Write('// %s %s\n\n' % (from_text, modified_text))
    else:
      out.Write('// %s,\n//   %s\n\n' % (from_text, modified_text))


    # TODO(teravest): Don't emit includes we don't need.
    includes = ['ppapi/c/pp_errors.h',
                'ppapi/shared_impl/tracked_callback.h',
                'ppapi/thunk/enter.h',
                'ppapi/thunk/ppb_instance_api.h',
                'ppapi/thunk/resource_creation_api.h',
                'ppapi/thunk/thunk.h']
    includes.append(_GetHeaderFileName(filenode))
    for api in meta.Apis():
      includes.append('ppapi/thunk/%s.h' % api.lower())
    for i in meta.Includes():
      includes.append(i)
    for include in sorted(includes):
      out.Write('#include "%s"\n' % include)
    out.Write('\n')
    out.Write('namespace ppapi {\n')
    out.Write('namespace thunk {\n')
    out.Write('\n')
    out.Write('namespace {\n')
    out.Write('\n')

  def GenerateBody(self, out, filenode, releases, options):
    """Generates a member function lines to be written and metadata.

    Returns a tuple of (body, meta) where:
      body - a list of lines with member function bodies
      meta - a ThunkMetadata instance for hinting which headers are needed.
    """
    __pychecker__ = 'unusednames=options'
    members = []
    meta = ThunkBodyMetadata()
    for node in filenode.GetListOf('Interface'):
      # Skip if this node is not in this release
      if not node.InReleases(releases):
        print "Skipping %s" % node
        continue

      # Generate Member functions
      if node.IsA('Interface'):
        for child in node.GetListOf('Member'):
          build_list = child.GetUniqueReleases(releases)
          # We have to filter out releases this node isn't in.
          build_list = filter(lambda r: child.InReleases([r]), build_list)
          if len(build_list) == 0:
            continue
          release = build_list[-1]  # Pick the newest release.
          member = DefineMember(filenode, node, child, release, False, meta)
          if not member:
            continue
          members.append(member)
          for build in build_list[:-1]:
            member = DefineMember(filenode, node, child, build, True, meta)
            if not member:
              continue
            members.append(member)
    return (members, meta)

  def WriteTail(self, out, filenode, releases, options):
    __pychecker__ = 'unusednames=options'
    cgen = CGen()

    version_list = []
    out.Write('\n\n')
    for node in filenode.GetListOf('Interface'):
      build_list = node.GetUniqueReleases(releases)
      for build in build_list:
        version = node.GetVersion(build).replace('.', '_')
        thunk_name = 'g_' + node.GetName().lower() + '_thunk_' + \
                      version
        thunk_type = '_'.join((node.GetName(), version))
        version_list.append((thunk_type, thunk_name))

        out.Write('const %s %s = {\n' % (thunk_type, thunk_name))
        generated_functions = []
        for child in node.GetListOf('Member'):
          rtype, name, arrays, args = cgen.GetComponents(
              child, build, 'return')
          if child.InReleases([build]):
            generated_functions.append(name)
        out.Write(',\n'.join(['  &%s' % f for f in generated_functions]))
        out.Write('\n};\n\n')

    out.Write('}  // namespace\n')
    out.Write('\n')
    for thunk_type, thunk_name in version_list:
      thunk_decl = 'const %s* Get%s_Thunk() {\n' % (thunk_type, thunk_type)
      if len(thunk_decl) > 80:
        thunk_decl = 'const %s*\n    Get%s_Thunk() {\n' % (thunk_type,
                                                           thunk_type)
      out.Write(thunk_decl)
      out.Write('  return &%s;\n' % thunk_name)
      out.Write('}\n')
      out.Write('\n')
    out.Write('}  // namespace thunk\n')
    out.Write('}  // namespace ppapi\n')


tgen = TGen()


def Main(args):
  # Default invocation will verify the golden files are unchanged.
  failed = 0
  if not args:
    args = ['--wnone', '--diff', '--test', '--thunkroot=.']

  ParseOptions(args)

  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_thunk', '*.idl')
  filenames = glob.glob(idldir)
  ast = ParseFiles(filenames)
  if tgen.GenerateRange(ast, ['M13', 'M14'], {}):
    print "Golden file for M13-M14 failed."
    failed = 1
  else:
    print "Golden file for M13-M14 passed."

  return failed


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
