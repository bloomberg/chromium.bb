# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import subprocess


def RunCmdAndCheck(cmd, err_string, output_api, cwd=None):
  results = []
  p = subprocess.Popen(cmd, cwd=cwd,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
  (p_stdout, p_stderr) = p.communicate()
  if p.returncode:
    results.append(
        output_api.PresubmitError(err_string,
                                  long_text=p_stderr))
  return results


def RunUnittests(input_api, output_api):
  # Run some Generator unittests if the generator source was changed.
  results = []
  files = input_api.LocalPaths()
  generator_files = []
  for filename in files:
    name_parts = filename.split(os.sep)
    if name_parts[0:2] == ['ppapi', 'generators']:
      generator_files.append(filename)
  if generator_files != []:
    cmd = [ sys.executable, 'idl_tests.py']
    ppapi_dir = input_api.PresubmitLocalPath()
    results.extend(RunCmdAndCheck(cmd,
                                  'PPAPI IDL unittests failed.',
                                  output_api,
                                  os.path.join(ppapi_dir, 'generators')))
  return results


# Verify that the files do not contain a 'TODO' in them.
RE_TODO = re.compile(r'\WTODO\W', flags=re.I)
def CheckTODO(input_api, output_api):
  files = input_api.LocalPaths()
  todo = []

  for filename in files:
    name, ext = os.path.splitext(filename)
    name_parts = name.split(os.sep)

    # Only check normal build sources.
    if ext not in ['.h', '.idl']:
      continue

    # Only examine the ppapi directory.
    if name_parts[0] != 'ppapi':
      continue

    # Only examine public plugin facing directories.
    if name_parts[1] not in ['api', 'c', 'cpp', 'utility']:
      continue

    # Only examine public stable interfaces.
    if name_parts[2] in ['dev', 'private', 'trusted']:
      continue

    filepath = os.path.join('..', filename)
    if RE_TODO.search(open(filepath, 'rb').read()):
      todo.append(filename)

  if todo:
    return [output_api.PresubmitError(
        'TODOs found in stable public PPAPI files:',
        long_text='\n'.join(todo))]
  return []

# Verify that no CPP wrappers use un-versioned PPB interface name macros.
RE_UNVERSIONED_PPB = re.compile(r'\bPPB_\w+_INTERFACE\b')
def CheckUnversionedPPB(input_api, output_api):
  files = input_api.LocalPaths()
  todo = []

  for filename in files:
    name, ext = os.path.splitext(filename)
    name_parts = name.split(os.sep)

    # Only check C++ sources.
    if ext not in ['.cc']:
      continue

    # Only examine the public plugin facing ppapi/cpp directory.
    if name_parts[0:2] != ['ppapi', 'cpp']:
      continue

    # Only examine public stable and trusted interfaces.
    if name_parts[2] in ['dev', 'private']:
      continue

    filepath = os.path.join('..', filename)
    if RE_UNVERSIONED_PPB.search(open(filepath, 'rb').read()):
      todo.append(filename)

  if todo:
    return [output_api.PresubmitError(
        'Unversioned PPB interface references found in PPAPI C++ wrappers:',
        long_text='\n'.join(todo))]
  return []

def CheckChange(input_api, output_api):
  results = []

  results.extend(RunUnittests(input_api, output_api))

  results.extend(CheckTODO(input_api, output_api))

  results.extend(CheckUnversionedPPB(input_api, output_api))

  # Verify all modified *.idl have a matching *.h
  files = input_api.LocalPaths()
  h_files = []
  idl_files = []

  # Find all relevant .h and .idl files.
  for filename in files:
    name, ext = os.path.splitext(filename)
    name_parts = name.split(os.sep)
    if name_parts[0:2] == ['ppapi', 'c'] and ext == '.h':
      h_files.append('/'.join(name_parts[2:]))
    if name_parts[0:2] == ['ppapi', 'api'] and ext == '.idl':
      idl_files.append('/'.join(name_parts[2:]))

  # Generate a list of all appropriate *.h and *.idl changes in this CL.
  both = h_files + idl_files

  # If there aren't any, we are done checking.
  if not both: return results

  missing = []
  for filename in idl_files:
    if filename not in set(h_files):
      missing.append('ppapi/api/%s.idl' % filename)

  # An IDL change that includes [generate_thunk] doesn't need to have
  # an update to the corresponding .h file.
  new_thunk_files = []
  for filename in missing:
    lines = input_api.RightHandSideLines(lambda f: f.LocalPath() == filename)
    for line in lines:
      if line[2].strip() == '[generate_thunk]':
        new_thunk_files.append(filename)
  for filename in new_thunk_files:
    missing.remove(filename)

  if missing:
    results.append(
        output_api.PresubmitPromptWarning(
            'Missing PPAPI header, no change or skipped generation?',
            long_text='\n  '.join(missing)))

  missing_dev = []
  missing_stable = []
  missing_priv = []
  for filename in h_files:
    if filename not in set(idl_files):
      name_parts = filename.split(os.sep)

      if name_parts[-1] == 'pp_macros':
        # The C header generator adds a PPAPI_RELEASE macro based on all the
        # IDL files, so pp_macros.h may change while its IDL does not.
        lines = input_api.RightHandSideLines(
            lambda f: f.LocalPath() == 'ppapi/c/%s.h' % filename)
        releaseChanged = False
        for line in lines:
          if line[2].split()[:2] == ['#define', 'PPAPI_RELEASE']:
            results.append(
                output_api.PresubmitPromptOrNotify(
                    'PPAPI_RELEASE has changed', long_text=line[2]))
            releaseChanged = True
            break
        if releaseChanged:
          continue

      if 'trusted' in name_parts:
        missing_priv.append('  ppapi/c/%s.h' % filename)
        continue

      if 'private' in name_parts:
        missing_priv.append('  ppapi/c/%s.h' % filename)
        continue

      if 'dev' in name_parts:
        missing_dev.append('  ppapi/c/%s.h' % filename)
        continue

      missing_stable.append('  ppapi/c/%s.h' % filename)

  if missing_priv:
    results.append(
        output_api.PresubmitPromptWarning(
            'Missing PPAPI IDL for private interface, please generate IDL:',
            long_text='\n'.join(missing_priv)))

  if missing_dev:
    results.append(
        output_api.PresubmitPromptWarning(
            'Missing PPAPI IDL for DEV, required before moving to stable:',
            long_text='\n'.join(missing_dev)))

  if missing_stable:
    results.append(
        output_api.PresubmitError(
            'Missing PPAPI IDL for stable interface:',
            long_text='\n'.join(missing_stable)))

  # Verify all *.h files match *.idl definitions, use:
  #   --test to prevent output to disk
  #   --diff to generate a unified diff
  #   --out to pick which files to examine (only the ones in the CL)
  ppapi_dir = input_api.PresubmitLocalPath()
  cmd = [sys.executable, 'generator.py',
         '--wnone', '--diff', '--test','--cgen', '--range=start,end']

  # Only generate output for IDL files references (as *.h or *.idl) in this CL
  cmd.append('--out=' + ','.join([name + '.idl' for name in both]))
  cmd_results = RunCmdAndCheck(cmd,
                               'PPAPI IDL Diff detected: Run the generator.',
                               output_api,
                               os.path.join(ppapi_dir, 'generators'))
  if cmd_results:
    results.extend(cmd_results)

  return results


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
