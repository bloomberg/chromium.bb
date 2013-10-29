#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Memoize the data produced by slow operations into Google storage.

Caches computations described in terms of command lines and inputs directories
or files, which yield a set of output file.
"""

# Done first to setup python module path.
import toolchain_env

import hashlib
import logging
import os
import platform
import shutil
import subprocess
import sys

import command
import directory_storage
import file_tools
import gsd_storage
import hashing_tools
import log_tools
import working_directory


class Once(object):
  """Class to memoize slow operations."""

  def __init__(self, storage, use_cached_results=True, cache_results=True,
               print_url=None, check_call=None, system_summary=None):
    """Constructor.

    Args:
      storage: An storage layer to read/write from (GSDStorage).
      use_cached_results: Flag indicating that cached computation results
                          should be used when possible.
      cache_results: Flag that indicates if successful computations should be
                     written to the cache.
      print_url: Function that accepts an URL for printing the build result,
                 or None.
      check_call: A testing hook for allowing build commands to be intercepted.
                  Same interface as subprocess.check_call.
    """
    if check_call is None:
      check_call = log_tools.CheckCall
    self._storage = storage
    self._directory_storage = directory_storage.DirectoryStorageAdapter(storage)
    self._use_cached_results = use_cached_results
    self._cache_results = cache_results
    self._print_url = print_url
    self._check_call = check_call
    self._system_summary = system_summary

  def KeyForOutput(self, package, output_hash):
    """Compute the key to store a give output in the data-store.

    Args:
      package: Package name.
      output_hash: Stable hash of the package output.
    Returns:
      Key that this instance of the package output should be stored/retrieved.
    """
    return 'object/%s_%s.tgz' % (package, output_hash)

  def KeyForBuildSignature(self, build_signature):
    """Compute the key to store a computation result in the data-store.

    Args:
      build_signature: Stable hash of the computation.
    Returns:
      Key that this instance of the computation result should be
      stored/retrieved.
    """
    return 'computed/%s.txt' % build_signature

  def WriteOutputFromHash(self, package, out_hash, output):
    """Write output from the cache.

    Args:
      package: Package name (for tgz name).
      out_hash: Hash of desired output.
      output: Output path.
    Returns:
      URL from which output was obtained if successful, or None if not.
    """
    key = self.KeyForOutput(package, out_hash)
    url = self._directory_storage.GetDirectory(key, output)
    if not url:
      logging.debug('Failed to retrieve %s' % key)
      return None
    if hashing_tools.StableHashPath(output) != out_hash:
      logging.warning('Object does not match expected hash, '
                      'has hashing method changed?')
      return None
    return url

  def PrintDownloadURL(self, url):
    """Print download URL if function was provided in the constructor.

    Args:
      urls: A list of urls to print.
    """
    if self._print_url is not None:
      self._print_url(url)

  def WriteResultToCache(self, package, build_signature, output):
    """Cache a computed result by key.

    Also prints URLs when appropriate.
    Args:
      package: Package name (for tgz name).
      build_signature: The input hash of the computation.
      output: A path containing the output of the computation.
    """
    if not self._cache_results:
      return
    out_hash = hashing_tools.StableHashPath(output)
    try:
      output_key = self.KeyForOutput(package, out_hash)
      # Try to get an existing copy in a temporary directory.
      wd = working_directory.TemporaryWorkingDirectory()
      with wd as work_dir:
        temp_output = os.path.join(work_dir, 'out')
        url = self._directory_storage.GetDirectory(output_key, temp_output)
        if url is None:
          # Isn't present. Cache the computed result instead.
          url = self._directory_storage.PutDirectory(output, output_key)
          logging.info('Computed fresh result and cached it.')
        else:
          # Cached version is present. Replace the current output with that.
          file_tools.RemoveDirectoryIfPresent(output)
          shutil.move(temp_output, output)
          logging.info(
              'Recomputed result matches cached value, '
              'using cached value instead.')
      # Upload an entry mapping from computation input to output hash.
      self._storage.PutData(
          out_hash, self.KeyForBuildSignature(build_signature))
      self.PrintDownloadURL(url)
    except gsd_storage.GSDStorageError:
      logging.info('Failed to cache result.')
      raise

  def ReadMemoizedResultFromCache(self, package, build_signature, output):
    """Read a cached result (if it exists) from the cache.

    Also prints URLs when appropriate.
    Args:
      package: Package name (for tgz name).
      build_signature: Build signature of the computation.
      output: Output path.
    Returns:
      Boolean indicating successful retrieval.
    """
    # Check if its in the cache.
    if self._use_cached_results:
      out_hash = self._storage.GetData(
          self.KeyForBuildSignature(build_signature))
      if out_hash is not None:
        url = self.WriteOutputFromHash(package, out_hash, output)
        if url is not None:
          logging.info('Retrieved cached result.')
          self.PrintDownloadURL(url)
          return True
    return False

  def Run(self, package, inputs, output, commands, unpack_commands=None,
          hashed_inputs=None, working_dir=None):
    """Run an operation once, possibly hitting cache.

    Args:
      package: Name of the computation/module.
      inputs: A dict of names mapped to files that are inputs.
      output: An output directory.
      commands: A list of command.Command objects to run.
      unpack_commands: A list of command.Command object to run before computing
                       the build hash. Or None.
      hashed_inputs: An alternate dict of inputs to use for hashing and after
                     the packing stage (or None).
      working_dir: Working directory to use, or None for a temp dir.
    """
    if working_dir is None:
      wdm = working_directory.TemporaryWorkingDirectory()
    else:
      wdm = working_directory.FixedWorkingDirectory(working_dir)

    # Cleanup destination.
    file_tools.RemoveDirectoryIfPresent(output)
    os.mkdir(output)

    with wdm as work_dir:
      # Optionally unpack before hashing.
      if unpack_commands is not None:
        for command in unpack_commands:
          command.Invoke(check_call=self._check_call, package=package,
                         cwd=work_dir, inputs=inputs, output=output)

      # Use an alternate input set from here on.
      if hashed_inputs is not None:
        inputs = hashed_inputs

      # Compute the build signature with modified inputs.
      build_signature = self.BuildSignature(
          package, inputs=inputs, commands=commands)

      # We're done if it's in the cache.
      if self.ReadMemoizedResultFromCache(package, build_signature, output):
        return
      for command in commands:
        command.Invoke(check_call=self._check_call, package=package,
                       cwd=work_dir, inputs=inputs, output=output,
                       build_signature=build_signature)

    self.WriteResultToCache(package, build_signature, output)

  def SystemSummary(self):
    """Gather a string describing intrinsic properties of the current machine.

    Ideally this would capture anything relevant about the current machine that
    would cause build output to vary (other than build recipe + inputs).
    """
    if self._system_summary is None:
      # Note there is no attempt to canonicalize these values.  If two
      # machines that would in fact produce identical builds differ in
      # these values, it just means that a superfluous build will be
      # done once to get the mapping from new input hash to preexisting
      # output hash into the cache.
      assert len(sys.platform) != 0, len(platform.machine()) != 0
      # Use environment from command so we can access MinGW on windows.
      env = command.PlatformEnvironment([])
      gcc = file_tools.Which('gcc', paths=env['PATH'].split(os.pathsep))
      p = subprocess.Popen(
          [gcc, '-v'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
      _, gcc_version = p.communicate()
      assert p.returncode == 0
      items = [
          ('platform', sys.platform),
          ('machine', platform.machine()),
          ('gcc-v', gcc_version),
          ]
      self._system_summary = str(items)
    return self._system_summary

  def BuildSignature(self, package, inputs, commands):
    """Compute a total checksum for a computation.

    The computed hash includes system properties, inputs, and the commands run.
    Args:
      package: The name of the package computed.
      inputs: A dict of names -> files/directories to be included in the
              inputs set.
      commands: A list of command.Command objects describing the commands run
                for this computation.
    Returns:
      A hex formatted sha1 to use as a computation key.
    """
    h = hashlib.sha1()
    h.update('package:' + package)
    h.update('summary:' + self.SystemSummary())
    for command in commands:
      h.update('command:')
      h.update(str(command))
    for key in sorted(inputs.keys()):
      h.update('item_name:' + key + '\x00')
      h.update('item:' + hashing_tools.StableHashPath(inputs[key]))
    return h.hexdigest()
