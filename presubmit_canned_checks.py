#!/usr/bin/env python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generic presubmit checks that can be reused by other presubmit checks."""


### Description checks

def CheckChangeHasTestField(input_api, output_api):
  """Requires that the changelist have a TEST= field."""
  if input_api.change.TEST:
    return []
  else:
    return [output_api.PresubmitNotifyResult(
        "Changelist should have a TEST= field. TEST=none is allowed.")]


def CheckChangeHasBugField(input_api, output_api):
  """Requires that the changelist have a BUG= field."""
  if input_api.change.BUG:
    return []
  else:
    return [output_api.PresubmitNotifyResult(
        "Changelist should have a BUG= field. BUG=none is allowed.")]


def CheckChangeHasTestedField(input_api, output_api):
  """Requires that the changelist have a TESTED= field."""
  if input_api.change.TESTED:
    return []
  else:
    return [output_api.PresubmitError("Changelist must have a TESTED= field.")]


def CheckChangeHasQaField(input_api, output_api):
  """Requires that the changelist have a QA= field."""
  if input_api.change.QA:
    return []
  else:
    return [output_api.PresubmitError("Changelist must have a QA= field.")]


def CheckDoNotSubmitInDescription(input_api, output_api):
  """Checks that the user didn't add 'DO NOT ' + 'SUBMIT' to the CL description.
  """
  keyword = 'DO NOT ' + 'SUBMIT'
  if keyword in input_api.change.DescriptionText():
    return [output_api.PresubmitError(
        keyword + " is present in the changelist description.")]
  else:
    return []


def CheckChangeHasDescription(input_api, output_api):
  """Checks the CL description is not empty."""
  text = input_api.change.DescriptionText()
  if text.strip() == '':
    if input_api.is_committing:
      return [output_api.PresubmitError("Add a description.")]
    else:
      return [output_api.PresubmitNotifyResult("Add a description.")]
  return []

### Content checks

def CheckDoNotSubmitInFiles(input_api, output_api):
  """Checks that the user didn't add 'DO NOT ' + 'SUBMIT' to any files."""
  keyword = 'DO NOT ' + 'SUBMIT'
  # We want to check every text files, not just source files.
  for f, line_num, line in input_api.RightHandSideLines(lambda x: x):
    if keyword in line:
      text = 'Found ' + keyword + ' in %s, line %s' % (f.LocalPath(), line_num)
      return [output_api.PresubmitError(text)]
  return []


def CheckChangeHasNoCR(input_api, output_api, source_file_filter=None):
  """Checks no '\r' (CR) character is in any source files."""
  cr_files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    if '\r' in input_api.ReadFile(f, 'rb'):
      cr_files.append(f.LocalPath())
  if cr_files:
    return [output_api.PresubmitPromptWarning(
        "Found a CR character in these files:", items=cr_files)]
  return []


def CheckSvnModifiedDirectories(input_api, output_api, source_file_filter=None):
  """Checks for files in svn modified directories.

  They will get submitted on accident because svn commits recursively by
  default, and that's very dangerous.
  """
  if input_api.change.scm != 'svn':
    return []

  errors = []
  current_cl_files = input_api.change.GetModifiedFiles()
  all_modified_files = input_api.change.GetAllModifiedFiles()
  # Filter out files in the current CL.
  modified_files = [f for f in all_modified_files if f not in current_cl_files]
  modified_abspaths = [input_api.os_path.abspath(f) for f in modified_files]

  for f in input_api.AffectedFiles(source_file_filter):
    if f.Action() == 'M' and f.IsDirectory():
      curpath = f.AbsoluteLocalPath()
      bad_files = []
      # Check if any of the modified files in other CLs are under curpath.
      for i in xrange(len(modified_files)):
        abspath = modified_abspaths[i]
        if input_api.os_path.commonprefix([curpath, abspath]) == curpath:
          bad_files.append(modified_files[i])
      if bad_files:
        if input_api.is_committing:
          error_type = output_api.PresubmitPromptWarning
        else:
          error_type = output_api.PresubmitNotifyResult
        errors.append(error_type(
            "Potential accidental commits in changelist %s:" % f.LocalPath(),
            items=bad_files))
  return errors


def CheckChangeHasOnlyOneEol(input_api, output_api, source_file_filter=None):
  """Checks the files ends with one and only one \n (LF)."""
  eof_files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    # Check that the file ends in one and only one newline character.
    if len(contents) > 1 and (contents[-1:] != "\n" or contents[-2:-1] == "\n"):
      eof_files.append(f.LocalPath())

  if eof_files:
    return [output_api.PresubmitPromptWarning(
      'These files should end in one (and only one) newline character:',
      items=eof_files)]
  return []


def CheckChangeHasNoCrAndHasOnlyOneEol(input_api, output_api,
                                       source_file_filter=None):
  """Runs both CheckChangeHasNoCR and CheckChangeHasOnlyOneEOL in one pass.

  It is faster because it is reading the file only once.
  """
  cr_files = []
  eof_files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if '\r' in contents:
      cr_files.append(f.LocalPath())
    # Check that the file ends in one and only one newline character.
    if len(contents) > 1 and (contents[-1:] != "\n" or contents[-2:-1] == "\n"):
      eof_files.append(f.LocalPath())
  outputs = []
  if cr_files:
    outputs.append(output_api.PresubmitPromptWarning(
        "Found a CR character in these files:", items=cr_files))
  if eof_files:
    outputs.append(output_api.PresubmitPromptWarning(
      'These files should end in one (and only one) newline character:',
      items=eof_files))
  return outputs


def CheckChangeHasNoTabs(input_api, output_api, source_file_filter=None):
  """Checks that there are no tab characters in any of the text files to be
  submitted.
  """
  tabs = []
  for f, line_num, line in input_api.RightHandSideLines(source_file_filter):
    if '\t' in line:
      tabs.append("%s, line %s" % (f.LocalPath(), line_num))
  if tabs:
    return [output_api.PresubmitPromptWarning("Found a tab character in:",
                                              long_text="\n".join(tabs))]
  return []


def CheckChangeHasNoStrayWhitespace(input_api, output_api,
                                    source_file_filter=None):
  """Checks that there is no stray whitespace at source lines end."""
  errors = []
  for f, line_num, line in input_api.RightHandSideLines(source_file_filter):
    if line.rstrip() != line:
      errors.append("%s, line %s" % (f.LocalPath(), line_num))
  if errors:
    return [output_api.PresubmitPromptWarning(
        "Found line ending with white spaces in:",
        long_text="\n".join(errors))]
  return []


def CheckLongLines(input_api, output_api, maxlen=80, source_file_filter=None):
  """Checks that there aren't any lines longer than maxlen characters in any of
  the text files to be submitted.
  """
  bad = []
  for f, line_num, line in input_api.RightHandSideLines(source_file_filter):
    # Allow lines with http://, https:// and #define/#pragma/#include/#if/#endif
    # to exceed the maxlen rule.
    if (len(line) > maxlen and
        not 'http://' in line and
        not 'https://' in line and
        not line.startswith('#define') and
        not line.startswith('#include') and
        not line.startswith('#pragma') and
        not line.startswith('#if') and
        not line.startswith('#endif')):
      bad.append(
          '%s, line %s, %s chars' %
          (f.LocalPath(), line_num, len(line)))
      if len(bad) == 5:  # Just show the first 5 errors.
        break

  if bad:
    msg = "Found lines longer than %s characters (first 5 shown)." % maxlen
    return [output_api.PresubmitPromptWarning(msg, items=bad)]
  else:
    return []


def CheckChangeSvnEolStyle(input_api, output_api, source_file_filter=None):
  """Checks that the source files have svn:eol-style=LF."""
  return CheckSvnProperty(input_api, output_api,
                          'svn:eol-style', 'LF',
                          input_api.AffectedSourceFiles(source_file_filter))


def CheckSvnForCommonMimeTypes(input_api, output_api):
  """Checks that common binary file types have the correct svn:mime-type."""
  output = []
  files = input_api.AffectedFiles(include_deletes=False)
  def IsExts(x, exts):
    path = x.LocalPath()
    for extension in exts:
      if path.endswith(extension):
        return True
    return False
  def FilterFiles(extension):
    return filter(lambda x: IsExts(x, extension), files)
  def RunCheck(mime_type, files):
    output.extend(CheckSvnProperty(input_api, output_api, 'svn:mime-type',
                                   mime_type, files))
  RunCheck('application/pdf', FilterFiles(['.pdf']))
  RunCheck('image/bmp', FilterFiles(['.bmp']))
  RunCheck('image/gif', FilterFiles(['.gif']))
  RunCheck('image/png', FilterFiles(['.png']))
  RunCheck('image/jpeg', FilterFiles(['.jpg', '.jpeg', '.jpe']))
  RunCheck('image/vnd.microsoft.icon', FilterFiles(['.ico']))
  return output


def CheckSvnProperty(input_api, output_api, prop, expected, affected_files):
  """Checks that affected_files files have prop=expected."""
  if input_api.change.scm != 'svn':
    return []

  bad = filter(lambda f: f.Property(prop) != expected, affected_files)
  if bad:
    if input_api.is_committing:
      res_type = output_api.PresubmitError
    else:
      res_type = output_api.PresubmitNotifyResult
    message = "Run `svn pset %s %s <item>` on these files:" % (prop, expected)
    return [res_type(message, items=bad)]
  return []


### Other checks

def CheckDoNotSubmit(input_api, output_api):
  return (
      CheckDoNotSubmitInDescription(input_api, output_api) +
      CheckDoNotSubmitInFiles(input_api, output_api)
      )


def CheckTreeIsOpen(input_api, output_api, url, closed):
  """Checks that an url's content doesn't match a regexp that would mean that
  the tree is closed."""
  assert(input_api.is_committing)
  try:
    connection = input_api.urllib2.urlopen(url)
    status = connection.read()
    connection.close()
    if input_api.re.match(closed, status):
      long_text = status + '\n' + url
      return [output_api.PresubmitPromptWarning("The tree is closed.",
                                                long_text=long_text)]
  except IOError:
    pass
  return []


def RunPythonUnitTests(input_api, output_api, unit_tests):
  """Run the unit tests out of process, capture the output and use the result
  code to determine success.
  """
  # We don't want to hinder users from uploading incomplete patches.
  if input_api.is_committing:
    message_type = output_api.PresubmitError
  else:
    message_type = output_api.PresubmitNotifyResult
  outputs = []
  for unit_test in unit_tests:
    # Run the unit tests out of process. This is because some unit tests
    # stub out base libraries and don't clean up their mess. It's too easy to
    # get subtle bugs.
    cwd = None
    env = None
    unit_test_name = unit_test
    # "python -m test.unit_test" doesn't work. We need to change to the right
    # directory instead.
    if '.' in unit_test:
      # Tests imported in submodules (subdirectories) assume that the current
      # directory is in the PYTHONPATH. Manually fix that.
      unit_test = unit_test.replace('.', '/')
      cwd = input_api.os_path.dirname(unit_test)
      unit_test = input_api.os_path.basename(unit_test)
      env = input_api.environ.copy()
      # At least on Windows, it seems '.' must explicitly be in PYTHONPATH
      backpath = [
          '.', input_api.os_path.pathsep.join(['..'] * (cwd.count('/') + 1))
        ]
      if env.get('PYTHONPATH'):
        backpath.append(env.get('PYTHONPATH'))
      env['PYTHONPATH'] = input_api.os_path.pathsep.join((backpath))
    subproc = input_api.subprocess.Popen(
        [
          input_api.python_executable,
          "-m",
          "%s" % unit_test
        ],
        cwd=cwd,
        env=env,
        stdin=input_api.subprocess.PIPE,
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE)
    stdoutdata, stderrdata = subproc.communicate()
    # Discard the output if returncode == 0
    if subproc.returncode:
      outputs.append("Test '%s' failed with code %d\n%s\n%s\n" % (
          unit_test_name, subproc.returncode, stdoutdata, stderrdata))
  if outputs:
    return [message_type("%d unit tests failed." % len(outputs),
                                long_text='\n'.join(outputs))]
  return []
