#!/usr/bin/env python

# Copyright (C) 2014 Bloomberg L.P. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

# This script implements the bare bones "gclient runhooks" behavior so that we
# don't need to have gclient installed everywhere.  Also, arguments passed to
# this script will be forwarded to gyp.

import os, sys, subprocess, gclient_eval

scriptDir = os.path.dirname(os.path.realpath(__file__))


def execInShell(cmd):
  print "Executing '" + " ".join(cmd) + "'"
  sys.stdout.flush()
  return subprocess.call(cmd, shell=True)


def dummyVar(s):
  return s


def loadDepInfo(solution):
  name = solution['name']
  deps = solution['deps_file']
  scope = {
    'Var': dummyVar,
  }
  execfile(os.path.join(name, deps), scope)
  return {
      'hooks': scope['hooks'],
      'vars': scope['vars']
  }

def get_vars(dep_vars):
  """Returns a dictionary of effective variable values
  (DEPS file contents with applied custom_vars overrides)."""
  # Provide some built-in variables.
  result = {
      'checkout_android': False,
      'checkout_chromeos': False,
      'checkout_fuchsia': False,
      'checkout_ios': False,
      'checkout_linux': False,
      'checkout_mac': False,
      'checkout_win': True,
      'host_os': "win",

      'checkout_arm': False,
      'checkout_arm64': False,
      'checkout_x86': True,
      'checkout_mips': False,
      'checkout_mips64': False,
      'checkout_ppc': False,
      'checkout_s390': False,
      'checkout_x64': True,
  }

  # Variable precedence:
  # - built-in
  # - DEPS vars
  result.update(dep_vars)
  return result

def main(args):
  # Need to be in the root directory
  os.chdir(os.path.join(scriptDir, os.pardir, os.pardir))

  scope = {}
  execfile('.gclient', scope)
  for sln in scope['solutions']:
    dep_info = loadDepInfo(sln)
    hooks = dep_info['hooks']

    for hook in hooks:
      cond = hook.get('condition', 'True')
      if not gclient_eval.EvaluateCondition(cond, get_vars(dep_info['vars'])):
        continue

      cmd = hook['action']
      if hook['name'] == 'gyp':
        cmd.extend(args)
      rc = execInShell(cmd)
      if 0 != rc:
        return rc

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

