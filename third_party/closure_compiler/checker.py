# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import os
import re
import subprocess
import sys
import tempfile



class LineNumber(object):
  def __init__(self, file, line_number):
    self.file = file
    self.line_number = int(line_number)


class FileCache(object):
  _cache = defaultdict(str)
    
  def _read(self, file):
    file = os.path.abspath(file)
    self._cache[file] = self._cache[file] or open(file, "r").read()
    return self._cache[file]

  @staticmethod
  def read(file):
    return FileCache()._read(file)


class Flattener(object):
  _IF_TAGS_REG = "</?if[^>]*?>"
  _INCLUDE_REG = "<include[^>]+src=['\"]([^>]*)['\"]>"

  def __init__(self, file):
    self.index = 0
    self.lines = self._get_file(file)

    while self.index < len(self.lines):
      current_line = self.lines[self.index]
      match = re.search(self._INCLUDE_REG, current_line[2])
      if match:
        file_dir = os.path.dirname(current_line[0])
        self._inline_file(os.path.join(file_dir, match.group(1)))
      else:
        self.index += 1

    # Replace every occurrence of tags like <if expr="..."> and </if>
    # with an empty string.
    for i, line in enumerate(self.lines):
      self.lines[i] = line[:2] + (re.sub(self._IF_TAGS_REG, "", line[2]),)

    self.contents = "\n".join(l[2] for l in self.lines)

  # Returns a list of tuples in the format: (file, line number, line contents).
  def _get_file(self, file):
    lines = FileCache.read(file).splitlines()
    return [(file, lnum + 1, line) for lnum, line in enumerate(lines)]

  def _inline_file(self, file):
    lines = self._get_file(file)
    self.lines = self.lines[:self.index] + lines + self.lines[self.index + 1:]

  def get_file_from_line(self, line_number):
    line_number = int(line_number) - 1
    return LineNumber(self.lines[line_number][0], self.lines[line_number][1])


class Checker(object):
  _common_closure_args = [
    "--accept_const_keyword",
    "--language_in=ECMASCRIPT5",
    "--summary_detail_level=3",
    "--warning_level=VERBOSE",
    "--jscomp_error=accessControls",
    "--jscomp_error=ambiguousFunctionDecl",
    "--jscomp_error=checkStructDictInheritance",
    "--jscomp_error=checkTypes",
    "--jscomp_error=checkVars",
    "--jscomp_error=constantProperty",
    "--jscomp_error=deprecated",
    "--jscomp_error=externsValidation",
    "--jscomp_error=globalThis",
    "--jscomp_error=invalidCasts",
    "--jscomp_error=misplacedTypeAnnotation",
    "--jscomp_error=missingProperties",
    "--jscomp_error=missingReturn",
    "--jscomp_error=nonStandardJsDocs",
    "--jscomp_error=suspiciousCode",
    "--jscomp_error=undefinedNames",
    "--jscomp_error=undefinedVars",
    "--jscomp_error=unknownDefines",
    "--jscomp_error=uselessCode",
    "--jscomp_error=visibility",
    # TODO(dbeam): happens when the same file is <include>d multiple times.
    "--jscomp_off=duplicate",
  ]

  _found_java = False

  _jar_command = [
    "java",
    "-jar",
    "-Xms1024m",
    "-server",
    "-XX:+TieredCompilation"
  ]

  def __init__(self, verbose=False):
    current_dir = os.path.join(os.path.dirname(__file__))
    self._compiler_jar = os.path.join(current_dir, "lib", "compiler.jar")
    self._runner_jar = os.path.join(current_dir, "runner", "runner.jar")
    self._temp_files = []
    self._verbose = verbose

  def _clean_up(self):
    if not self._temp_files:
      return

    self._debug("Deleting temporary files: " + ", ".join(self._temp_files))
    for f in self._temp_files:
      os.remove(f)
    self._temp_files = []

  def _debug(self, msg, error=False):
    if self._verbose:
      print "(INFO) " + msg

  def _fatal(self, msg):
    print >> sys.stderr, "(FATAL) " + msg
    self._clean_up()
    sys.exit(1)

  def _run_command(self, cmd):
    cmd_str = " ".join(cmd)
    self._debug("Running command: " + cmd_str)

    devnull = open(os.devnull, "w")
    return subprocess.Popen(
        cmd_str, stdout=devnull, stderr=subprocess.PIPE, shell=True)

  def _check_java_path(self):
    if self._found_java:
      return

    proc = self._run_command(["which", "java"])
    proc.communicate()
    if proc.returncode == 0:
      self._found_java = True
    else:
      self._fatal("Cannot find java (`which java` => %s)" % proc.returncode)

  def _run_jar(self, jar, args=[]):
    self._check_java_path()
    return self._run_command(self._jar_command + [jar] + args)

  def _fix_line_number(self, match):
    real_file = self._flattener.get_file_from_line(match.group(1))
    return "%s:%d" % (os.path.abspath(real_file.file), real_file.line_number)

  def _fix_up_error(self, error):
    if " first declared in " in error:
      # Ignore "Variable x first declared in /same/file".
      return ""

    file = self._expanded_file
    fixed = re.sub("%s:(\d+)" % file, self._fix_line_number, error)
    return fixed.replace(file, os.path.abspath(self._file_arg))

  def _format_errors(self, errors):
    errors = filter(None, errors)
    contents = ("\n" + "## ").join("\n\n".join(errors).splitlines())
    return "## " + contents if contents else ""

  def _create_temp_file(self, contents):
    with tempfile.NamedTemporaryFile(mode='wt', delete=False) as tmp_file:
      self._temp_files.append(tmp_file.name)
      tmp_file.write(contents)
    return tmp_file.name

  def check(self, file, depends=[], externs=[]):
    self._debug("FILE: " + file)

    if file.endswith("_externs.js"):
      self._debug("Skipping externs: " + file)
      return

    self._file_arg = file

    tmp_dir = tempfile.gettempdir()
    rel_path = lambda f: os.path.join(os.path.relpath(os.getcwd(), tmp_dir), f)

    contents = ['<include src="%s">' % rel_path(f) for f in depends + [file]]
    meta_file = self._create_temp_file("\n".join(contents))
    self._debug("Meta file: " + meta_file)

    self._flattener = Flattener(meta_file)
    self._expanded_file = self._create_temp_file(self._flattener.contents)
    self._debug("Expanded file: " + self._expanded_file)

    args = ["--js=" + self._expanded_file] + ["--externs=" + e for e in externs]
    args_file_content = " " + " ".join(self._common_closure_args + args)
    self._debug("Args: " + args_file_content.strip())

    args_file = self._create_temp_file(args_file_content)
    self._debug("Args file: " + args_file)

    runner_args = ["--compiler-args-file=" + args_file]
    runner_cmd = self._run_jar(self._runner_jar, args=runner_args)
    (_, stderr) = runner_cmd.communicate()

    errors = stderr.strip().split("\n\n")
    self._debug("Summary: " + errors.pop())

    output = self._format_errors(map(self._fix_up_error, errors))
    if runner_cmd.returncode:
      self._fatal("Error in: " + file + ("\n" + output if output else ""))
    elif output:
      self._debug("Output: " + output)
   
    self._clean_up()

    return runner_cmd.returncode == 0
