#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Closure compiler on a JavaScript file to check for errors."""

import argparse
import os
import re
import subprocess
import sys
import tempfile

import build.inputs
import processor
import error_filter


class Checker(object):
  """Runs the Closure compiler on a given source file and returns the
  success/errors."""

  _COMMON_CLOSURE_ARGS = [
    "--accept_const_keyword",
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
    "--jscomp_error=missingProperties",
    "--jscomp_error=missingReturn",
    "--jscomp_error=nonStandardJsDocs",
    "--jscomp_error=suspiciousCode",
    "--jscomp_error=undefinedNames",
    "--jscomp_error=undefinedVars",
    "--jscomp_error=unknownDefines",
    "--jscomp_error=uselessCode",
    "--jscomp_error=visibility",
    "--language_in=ECMASCRIPT5_STRICT",
    "--summary_detail_level=3",
    "--compilation_level=SIMPLE_OPTIMIZATIONS",
    "--source_map_format=V3",
  ]

  # These are the extra flags used when compiling in 'strict' mode.
  # Flags that are normally disabled are turned on for strict mode.
  _STRICT_CLOSURE_ARGS = [
    "--jscomp_error=reportUnknownTypes",
    "--jscomp_error=duplicate",
    "--jscomp_error=misplacedTypeAnnotation",
  ]

  _DISABLED_CLOSURE_ARGS = [
    # TODO(dbeam): happens when the same file is <include>d multiple times.
    "--jscomp_off=duplicate",
    # TODO(fukino): happens when cr.defineProperty() has a type annotation.
    # Avoiding parse-time warnings needs 2 pass compiling. crbug.com/421562.
    "--jscomp_off=misplacedTypeAnnotation",
  ]

  _JAR_COMMAND = [
    "java",
    "-jar",
    "-Xms1024m",
    "-client",
    "-XX:+TieredCompilation"
  ]

  _found_java = False

  def __init__(self, verbose=False, strict=False):
    current_dir = os.path.join(os.path.dirname(__file__))
    self._runner_jar = os.path.join(current_dir, "runner", "runner.jar")
    self._temp_files = []
    self._verbose = verbose
    self._strict = strict
    self._error_filter = error_filter.PromiseErrorFilter()

  def _clean_up(self):
    if not self._temp_files:
      return

    self._debug("Deleting temporary files: %s" % ", ".join(self._temp_files))
    for f in self._temp_files:
      os.remove(f)
    self._temp_files = []

  def _debug(self, msg, error=False):
    if self._verbose:
      print "(INFO) %s" % msg

  def _error(self, msg):
    print >> sys.stderr, "(ERROR) %s" % msg
    self._clean_up()

  def _common_args(self):
    """Returns an array of the common closure compiler args."""
    if self._strict:
      return self._COMMON_CLOSURE_ARGS + self._STRICT_CLOSURE_ARGS
    return self._COMMON_CLOSURE_ARGS + self._DISABLED_CLOSURE_ARGS

  def _run_command(self, cmd):
    """Runs a shell command.

    Args:
        cmd: A list of tokens to be joined into a shell command.

    Return:
        True if the exit code was 0, else False.
    """
    cmd_str = " ".join(cmd)
    self._debug("Running command: %s" % cmd_str)

    devnull = open(os.devnull, "w")
    return subprocess.Popen(
        cmd_str, stdout=devnull, stderr=subprocess.PIPE, shell=True)

  def _check_java_path(self):
    """Checks that `java` is on the system path."""
    if not self._found_java:
      proc = self._run_command(["which", "java"])
      proc.communicate()
      if proc.returncode == 0:
        self._found_java = True
      else:
        self._error("Cannot find java (`which java` => %s)" % proc.returncode)

    return self._found_java

  def _run_jar(self, jar, args=None):
    args = args or []
    self._check_java_path()
    return self._run_command(self._JAR_COMMAND + [jar] + args)

  def _fix_line_number(self, match):
    """Changes a line number from /tmp/file:300 to /orig/file:100.

    Args:
        match: A re.MatchObject from matching against a line number regex.

    Returns:
        The fixed up /file and :line number.
    """
    real_file = self._processor.get_file_from_line(match.group(1))
    return "%s:%d" % (os.path.abspath(real_file.file), real_file.line_number)

  def _fix_up_error(self, error):
    """Filter out irrelevant errors or fix line numbers.

    Args:
        error: A Closure compiler error (2 line string with error and source).

    Return:
        The fixed up error string (blank if it should be ignored).
    """
    if " first declared in " in error:
      # Ignore "Variable x first declared in /same/file".
      return ""

    expanded_file = self._expanded_file
    fixed = re.sub("%s:(\d+)" % expanded_file, self._fix_line_number, error)
    return fixed.replace(expanded_file, os.path.abspath(self._file_arg))

  def _format_errors(self, errors):
    """Formats Closure compiler errors to easily spot compiler output."""
    errors = filter(None, errors)
    contents = "\n## ".join("\n\n".join(errors).splitlines())
    return "## %s" % contents if contents else ""

  def _create_temp_file(self, contents):
    with tempfile.NamedTemporaryFile(mode="wt", delete=False) as tmp_file:
      self._temp_files.append(tmp_file.name)
      tmp_file.write(contents)
    return tmp_file.name

  def _run_js_check(self, sources, out_file=None, externs=None):
    if not self._check_java_path():
      return 1, ""

    args = ["--js=%s" % s for s in sources]

    if out_file:
      args += ["--js_output_file=%s" % out_file]
      args += ["--create_source_map=%s.map" % out_file]

    if externs:
      args += ["--externs=%s" % e for e in externs]
    args_file_content = " %s" % " ".join(self._common_args() + args)
    self._debug("Args: %s" % args_file_content.strip())

    args_file = self._create_temp_file(args_file_content)
    self._debug("Args file: %s" % args_file)

    runner_args = ["--compiler-args-file=%s" % args_file]
    runner_cmd = self._run_jar(self._runner_jar, args=runner_args)
    _, stderr = runner_cmd.communicate()

    errors = stderr.strip().split("\n\n")
    self._debug("Summary: %s" % errors.pop())

    self._clean_up()

    return errors, stderr

  def check(self, source_file, out_file=None, depends=None, externs=None):
    """Closure compile a file and check for errors.

    Args:
        source_file: A file to check.
        out_file: A file where the compiled output is written to.
        depends: Other files that would be included with a <script> earlier in
            the page.
        externs: @extern files that inform the compiler about custom globals.

    Returns:
        (has_errors, output) A boolean indicating if there were errors and the
            Closure compiler output (as a string).
    """
    depends = depends or []
    externs = externs or set()

    if not self._check_java_path():
      return 1, ""

    self._debug("FILE: %s" % source_file)

    if source_file.endswith("_externs.js"):
      self._debug("Skipping externs: %s" % source_file)
      return

    self._file_arg = source_file

    tmp_dir = tempfile.gettempdir()
    rel_path = lambda f: os.path.join(os.path.relpath(os.getcwd(), tmp_dir), f)

    includes = [rel_path(f) for f in depends + [source_file]]
    contents = ['<include src="%s">' % i for i in includes]
    meta_file = self._create_temp_file("\n".join(contents))
    self._debug("Meta file: %s" % meta_file)

    self._processor = processor.Processor(meta_file)
    self._expanded_file = self._create_temp_file(self._processor.contents)
    self._debug("Expanded file: %s" % self._expanded_file)

    errors, stderr = self._run_js_check([self._expanded_file],
                                        out_file=out_file, externs=externs)

    # Filter out false-positive promise chain errors.
    # See https://github.com/google/closure-compiler/issues/715 for details.
    errors = self._error_filter.filter(errors);

    output = self._format_errors(map(self._fix_up_error, errors))
    if errors:
      prefix = "\n" if output else ""
      self._error("Error in: %s%s%s" % (source_file, prefix, output))
    elif output:
      self._debug("Output: %s" % output)

    return bool(errors), output

  def check_multiple(self, sources):
    """Closure compile a set of files and check for errors.

    Args:
        sources: An array of files to check.

    Returns:
        (has_errors, output) A boolean indicating if there were errors and the
            Closure compiler output (as a string).
    """

    errors, stderr = self._run_js_check(sources)
    return bool(errors), stderr

if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Typecheck JavaScript using Closure compiler")
  parser.add_argument("sources", nargs=argparse.ONE_OR_MORE,
                      help="Path to a source file to typecheck")
  single_file_group = parser.add_mutually_exclusive_group()
  single_file_group.add_argument("--single-file", dest="single_file",
                                 action="store_true",
                                 help="Process each source file individually")
  single_file_group.add_argument("--no-single-file", dest="single_file",
                                 action="store_false",
                                 help="Process all source files as a group")
  parser.add_argument("-d", "--depends", nargs=argparse.ZERO_OR_MORE)
  parser.add_argument("-e", "--externs", nargs=argparse.ZERO_OR_MORE)
  parser.add_argument("-o", "--out_file",
                      help="A file where the compiled output is written to")
  parser.add_argument("-v", "--verbose", action="store_true",
                      help="Show more information as this script runs")
  parser.add_argument("--strict", action="store_true",
                      help="Enable strict type checking")
  parser.add_argument("--success-stamp",
                      help="Timestamp file to update upon success")

  parser.set_defaults(single_file=True, strict=False)
  opts = parser.parse_args()

  depends = opts.depends or []
  externs = opts.externs or set()

  if opts.out_file:
    out_dir = os.path.dirname(opts.out_file)
    if not os.path.exists(out_dir):
      os.makedirs(out_dir)

  checker = Checker(verbose=opts.verbose, strict=opts.strict)
  if opts.single_file:
    for source in opts.sources:
      depends, externs = build.inputs.resolve_recursive_dependencies(
          source,
          depends,
          externs)
      has_errors, _ = checker.check(source, out_file=opts.out_file,
                                    depends=depends, externs=externs)
      if has_errors:
        sys.exit(1)

  else:
    has_errors, errors = checker.check_multiple(opts.sources)
    if has_errors:
      print errors
      sys.exit(1)

  if opts.success_stamp:
    with open(opts.success_stamp, 'w'):
      os.utime(opts.success_stamp, None)
