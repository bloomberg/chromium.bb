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
import sys

import directory_storage
import file_tools
import gsd_storage
import hashing_tools
import log_tools
import working_directory


class Once(object):
  """Class to memoize slow operations."""

  def __init__(self, storage, use_cached_results=True, cache_results=True,
               print_urls=None, check_call=None):
    """Constructor.

    Args:
      storage: An storage layer to read/write from (GSDStorage).
      use_cached_results: Flag indicating that cached computation results
                          should be used when possible.
      cache_results: Flag that indicates if successful computations should be
                     written to the cache.
      print_urls: Function that accepts a list of build artifact URLs to print
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
    self._print_urls = print_urls
    self._check_call = check_call

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

  def WriteOutputsFromHashes(self, package, out_hashes, outputs):
    """Write output from the cache.

    Args:
      package: Package name (for tgz name).
      out_hashes: List of desired hashes of outputs.
      outputs: List of output filenames.
    Returns:
      List of urls from which outputs were obtained if successful,
      or None if not.
    """
    if len(out_hashes) != len(outputs):
      logging.warning(
          ('Cached output count (%d) does not '
           'match current output count (%d)') % (
              len(outputs), len(out_hashes)))
      return None
    urls = []
    for output, out_hash in zip(outputs, out_hashes):
      key = self.KeyForOutput(package, out_hash)
      url = self._directory_storage.GetDirectory(key, output)
      if not url:
        logging.debug('Failed to retrieve %s' % key)
        return None
      if hashing_tools.StableHashPath(output) != out_hash:
        logging.warning('Object does not match expected hash, '
                        'has hashing method changed?')
        return None
      urls.append(url)
    return urls

  def PrintDownloadURLs(self, urls):
    """Print download URLs if function was provided in the constructor.

    Args:
      urls: A list of urls to print.
    """
    if self._print_urls is not None:
      self._print_urls(urls)

  def WriteResultToCache(self, package, build_signature, outputs):
    """Cache a computed result by key.

    Also prints URLs when appropriate.
    Args:
      package: Package name (for tgz name).
      build_signature: The input hash of the computation.
      outputs: A list of filenames containing the output of the computation.
    """
    if self._cache_results:
      out_hashes = [hashing_tools.StableHashPath(o) for o in outputs]
      urls = []
      try:
        for output, out_hash in zip(outputs, out_hashes):
          urls.append(self._directory_storage.PutDirectory(
            output, self.KeyForOutput(package, out_hash)))
        self._storage.PutData('\n'.join(out_hashes),
            self.KeyForBuildSignature(build_signature))
        logging.info('Computed fresh result and cached.')
        self.PrintDownloadURLs(urls)
      except gsd_storage.GSDStorageError:
        logging.info('Failed to cache result.')
        raise

  def ReadMemoizedResultFromCache(self, package, build_signature, outputs):
    """Read a cached result (if it exists) from the cache.

    Also prints URLs when appropriate.
    Args:
      package: Package name (for tgz name).
      build_signature: Build signature of the computation.
      outputs: List of output filenames.
    Returns:
      Boolean indicating successful retrieval.
    """
    # Check if its in the cache.
    if self._use_cached_results:
      data = self._storage.GetData(self.KeyForBuildSignature(build_signature))
      if data is not None:
        out_hashes = data.split('\n')
        urls = self.WriteOutputsFromHashes(package, out_hashes, outputs)
        if urls is not None:
          logging.info('Retrieved cached result.')
          self.PrintDownloadURLs(urls)
          return True
    return False

  def Run(self, package, inputs, outputs, commands, unpack_commands=None,
          hashed_inputs=None, working_dir=None):
    """Run an operation once, possibly hitting cache.

    Args:
      package: Name of the computation/module.
      inputs: A list of files or directories that are inputs.
      outputs: A list of files that are outputs.
      commands: A list of command.Command objects to run.
      unpack_commands: A list of command.Command object to run before computing
                       the build hash. Or None.
      hashed_inputs: An alternate list of inputs to use for hashing and after
                     the packing stage (or None).
      working_dir: Working directory to use, or None for a temp dir.
    """
    if working_dir is None:
      wdm = working_directory.TemporaryWorkingDirectory()
    else:
      wdm = working_directory.FixedWorkingDirectory(working_dir)

    # Cleanup destination.
    for output in outputs:
      file_tools.RemoveDirectoryIfPresent(output)
      os.mkdir(output)

    with wdm as work_dir:
      # Optionally unpack before hashing.
      if unpack_commands is not None:
        for command in unpack_commands:
          command.Invoke(check_call=self._check_call, package=package,
                         cwd=work_dir, inputs=inputs, outputs=outputs)

      # Use an alternate input set from here on.
      if hashed_inputs is not None:
        inputs = hashed_inputs

      # Compute the build signature with modified inputs.
      build_signature = self.BuildSignature(
          package, inputs=inputs, commands=commands)

      # We're done if it's in the cache.
      if self.ReadMemoizedResultFromCache(package, build_signature, outputs):
        return
      for command in commands:
        command.Invoke(check_call=self._check_call, package=package,
                       cwd=work_dir, inputs=inputs, outputs=outputs,
                       build_signature=build_signature)

    self.WriteResultToCache(package, build_signature, outputs)

  def SystemSummary(self):
    """Gather a string describing intrinsic properties of the current machine.

    Ideally this would capture anything relevant about the current machine that
    would cause build output to vary (other than build recipe + inputs).
    """
    return sys.platform

  def BuildSignature(self, package, inputs, commands):
    """Compute a total checksum for a computation.

    The computed hash includes system properties, inputs, and the commands run.
    Args:
      package: The name of the package computed.
      inputs: A list of files/directories to be included in the inputs set.
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
    for path in inputs:
      h.update('item:' + hashing_tools.StableHashPath(path))
    return h.hexdigest()
