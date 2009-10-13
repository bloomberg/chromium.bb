#!/usr/bin/python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
#  This script extracts  "nacl syscall" prototypes from a c file
#  given on stdin and then produces wrapped versions of the syscalls.
#
import getopt
import re
import string
import StringIO
import sys

AUTOGEN_COMMENT = """\
/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * Automatically generated code.  See nacl_syscall_handlers_gen.py
 *
 * NaCl Server Runtime Service Call abstractions
 */
"""


TABLE_INITIALIZER = """\

/* auto generated */

void NaClSyscallTableInit() {
  int i;
  for (i = 0; i < NACL_MAX_SYSCALLS; ++i) {
     nacl_syscall[i].handler = &NotImplementedDecoder;
  }

%s
}
"""


IMPLEMENTATION_SKELETON = """\
/* this function was automagically generated */
static int32_t %(name)sDecoder(struct NaClAppThread *natp) {
%(members)s\
  return %(name)s(natp%(arglist)s);
}
"""

def FunctionNameToSyscallDefine(name):
  assert name.startswith("NaClSys")
  name = name[7:]
  return "NACL_sys_" + name.lower()

def ParamList(alist):
  if not alist:
    return ''
  return ', ' + ', '.join(alist)


def ExtractVariable(decl):
  type_or_idents = re.findall(r'[a-zA-Z][-_a-zA-Z]*', decl)
  return type_or_idents[len(type_or_idents)-1]


def ArgList(alist):
  if not alist:
    return ''
  # Note: although this return value might be computed more
  # concisely with a list comprehension, we instead use a
  # for statement to maintain python 2.3 compatibility.
  extractedargs = []
  for arg in alist:
    extractedargs += ['p.' + ExtractVariable(arg)]
  return ', ' + ', '.join(extractedargs)


def MemberList(name, alist):
  if not alist:
    return ''

  # Note: although this return value might be computed more
  # concisely with a list comprehension, we instead use a
  # for statement to maintain python 2.3 compatibility.
  margs = []
  for arg in alist:
    margs += ['    ' + arg]
  values = {
      'name': name,
      'members' : ';\n'.join(margs) + ';'
      }

  return """\
  struct %(name)sArgs {
%(members)s
  } p = *(struct %(name)sArgs *) natp->x_sp;

""" % values


def PrintSyscallTableIntializer(protos, ostr):
  assign = []
  for name, _ in protos:
    syscall = FunctionNameToSyscallDefine(name)
    assign.append("  NaClAddSyscall(%s, &%sDecoder);" % (syscall, name))
  print >>ostr, TABLE_INITIALIZER % "\n".join(assign)


def PrintImplSkel(protos, ostr):
  print >>ostr, AUTOGEN_COMMENT;
  for name, alist in protos:
    values = { 'name' : name,
               'paramlist' : ParamList(alist[1:]),
               'arglist' : ArgList(alist[1:]),
               'members' : MemberList(name, alist[1:]),
               }
    print >>ostr, IMPLEMENTATION_SKELETON % values


def GetProtoArgs(s, fin):
  if "{" not in s:
    for line in fin:
      s += line
      if "{" in line:
        break
    else:
      # broken input
      assert 0
  pos = s.rfind(")")
  assert pos >= 0
  # prune stuff after )
  s = s[0:pos]
  args = [a.strip() for a in s.split(",")]
  return args


def ParseFileToBeWrapped(fin, filter_regex):
  protos = []
  for line in fin:
    match = re.search(r"^int32_t (NaClSys[_a-zA-Z0-9]+)[(](.*)$", line)
    if not match:
      continue
    name = match.group(1)
    args = GetProtoArgs(match.group(2), fin)
    if filter_regex and re.search(filter_regex, name):
      continue
    protos.append( (name, args) )
  return protos


def main(argv):
  usage='Usage: nacl_syscall_handlers_gen2.py [-f regex] [-c] [-d]'
  filter_regex = []
  mode = "dump"
  input = sys.stdin
  output = sys.stdout
  try:
    opts, pargs = getopt.getopt(argv[1:], 'hcdf:i:o:')
  except getopt.error, e:
    print >>sys.stderr, 'Illegal option:', str(e)
    print >>sys.stderr, usage
    return 1

  for opt, val in opts:
    if opt == '-d':
      mode = "dump"
    elif opt == '-c':
      mode = "codegen"
    elif opt == '-f':
      filter_regex.append(val)
    elif opt == '-i':
      input = open(val, 'r')
    elif opt == '-o':
      output = open(val, 'w')
    else:
      assert 0

  data = input.read()
  protos = ParseFileToBeWrapped(StringIO.StringIO(data),
                                "|".join(filter_regex))
  if mode == "dump":
    for f, a in  protos:
      print >>output, f
      print >>output, "\t", a
  elif mode == "header":
    PrintHeaderFile(protos, output)
  elif mode == "codegen":
    print >>output, data
    PrintImplSkel(protos, output)
    PrintSyscallTableIntializer(protos, output)


  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
