#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests of a network memoizer."""

# Done first to setup python module path.
import toolchain_env

import os
import subprocess
import sys
import unittest

import command
import directory_storage
import fake_storage
import file_tools
import gsd_storage
import once
import working_directory


class TestOnce(unittest.TestCase):

  def GenerateTestData(self, noise, work_dir):
    self._input_dirs = {}
    self._input_files = []
    for i in range(2):
      dir_name = os.path.join(work_dir, noise + 'input%d_dir' % i)
      os.mkdir(dir_name)
      filename = os.path.join(dir_name, 'in%d' % i)
      file_tools.WriteFile(noise + 'data%d' % i, filename)
      self._input_dirs['input%d' % i] = dir_name
      self._input_files.append(filename)
    self._output_dirs = []
    self._output_files = []
    for i in range(2):
      dir_name = os.path.join(work_dir, noise + 'output%d_dir' % i)
      os.mkdir(dir_name)
      filename = os.path.join(dir_name, 'out')
      self._output_dirs.append(dir_name)
      self._output_files.append(filename)

  def test_FirstTime(self):
    # Test that the computation is always performed if the cache is empty.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      self.GenerateTestData('FirstTime', work_dir)
      o = once.Once(storage=fake_storage.FakeStorage(), system_summary='test')
      o.Run('test', self._input_dirs, self._output_dirs[0],
            [command.Copy('%(input0)s/in0', '%(output)s/out', cwd=work_dir)]),
      self.assertEquals('FirstTimedata0',
                        file_tools.ReadFile(self._output_files[0]))

  def test_HitsCacheSecondTime(self):
    # Test that the computation is not performed on a second instance.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      self.GenerateTestData('HitsCacheSecondTime', work_dir)
      self._tally = 0
      def check_call(cmd, **kwargs):
        self._tally += 1
        subprocess.check_call(cmd, **kwargs)
      self._url = None
      def stash_url(urls):
        self._url = urls
      o = once.Once(storage=fake_storage.FakeStorage(), check_call=check_call,
                    print_url=stash_url, system_summary='test')
      o.Run('test', self._input_dirs, self._output_dirs[0],
            [command.Copy('%(input0)s/in0', '%(output)s/out', cwd=work_dir)])
      initial_url = self._url
      self._url = None
      o.Run('test', self._input_dirs, self._output_dirs[1],
            [command.Copy('%(input0)s/in0', '%(output)s/out', cwd=work_dir)])
      self.assertEquals(file_tools.ReadFile(self._input_files[0]),
                        file_tools.ReadFile(self._output_files[0]))
      self.assertEquals(file_tools.ReadFile(self._input_files[0]),
                        file_tools.ReadFile(self._output_files[1]))
      self.assertEquals(1, self._tally)
      self.assertEquals(initial_url, self._url)

  def FileLength(self, src, dst, **kwargs):
    """Command object to write the length of one file into another."""
    return command.Command([
        sys.executable, '-c',
          'import sys; open(sys.argv[2], "wb").write('
          'str(len(open(sys.argv[1], "rb").read())))', src, dst
        ], **kwargs)

  def test_RecomputeHashMatches(self):
    # Test that things don't get stored to the output cache if they exist
    # already.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      # Setup test data in input0, input1 using memory storage.
      self.GenerateTestData('RecomputeHashMatches', work_dir)
      fs = fake_storage.FakeStorage()
      ds = directory_storage.DirectoryStorageAdapter(storage=fs)
      o = once.Once(storage=fs, system_summary='test')

      # Run the computation (compute the length of a file) from input0 to
      # output0.
      o.Run('test', self._input_dirs, self._output_dirs[0],
            [self.FileLength(
                '%(input0)s/in0', '%(output)s/out', cwd=work_dir)])

      # Check that 2 writes have occurred. One to write a mapping from in->out,
      # and one for the output data.
      self.assertEquals(2, fs.WriteCount())

      # Run the computation again from input1 to output1.
      # (These should have the same length.)
      o.Run('test', self._input_dirs, self._output_dirs[1],
            [self.FileLength(
                '%(input1)s/in1', '%(output)s/out', cwd=work_dir)])

      # Write count goes up by one as an in->out hash is added,
      # but no new output is stored (as it is the same).
      self.assertEquals(3, fs.WriteCount())

      # Check that the test is still valid:
      #   - in0 and in1 have equal length.
      #   - out0 and out1 have that length in them.
      #   - out0 and out1 agree.
      self.assertEquals(str(len(file_tools.ReadFile(self._input_files[0]))),
                        file_tools.ReadFile(self._output_files[0]))
      self.assertEquals(str(len(file_tools.ReadFile(self._input_files[1]))),
                        file_tools.ReadFile(self._output_files[1]))
      self.assertEquals(file_tools.ReadFile(self._output_files[0]),
                        file_tools.ReadFile(self._output_files[1]))

  def test_FailsWhenWritingFails(self):
    # Check that once doesn't eat the storage layer failures for writes.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      self.GenerateTestData('FailsWhenWritingFails', work_dir)
      def call(cmd, **kwargs):
        # Cause gsutil commands to fail.
        return 1
      bad_storage = gsd_storage.GSDStorage(
          gsutil=['mygsutil'],
          write_bucket='mybucket',
          read_buckets=[],
          call=call)
      o = once.Once(storage=bad_storage, system_summary='test')
      self.assertRaises(gsd_storage.GSDStorageError, o.Run, 'test',
          self._input_dirs, self._output_dirs[0],
          [command.Copy('%(input0)s/in0', '%(output)s/out')])

  def test_UseCachedResultsFalse(self):
    # Check that the use_cached_results=False does indeed cause computations
    # to be redone, even when present in the cache.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      self.GenerateTestData('UseCachedResultsFalse', work_dir)
      self._tally = 0
      def check_call(cmd, **kwargs):
        subprocess.check_call(cmd, **kwargs)
        self._tally += 1
      o = once.Once(storage=fake_storage.FakeStorage(),
                    use_cached_results=False, check_call=check_call,
                    system_summary='test')
      o.Run('test', self._input_dirs, self._output_dirs[0],
            [command.Copy('%(input0)s/in0', '%(output)s/out', cwd=work_dir)])
      o.Run('test', self._input_dirs, self._output_dirs[1],
            [command.Copy('%(input0)s/in0', '%(output)s/out', cwd=work_dir)])
      self.assertEquals(2, self._tally)
      self.assertEquals(file_tools.ReadFile(self._input_files[0]),
                        file_tools.ReadFile(self._output_files[0]))
      self.assertEquals(file_tools.ReadFile(self._input_files[0]),
                        file_tools.ReadFile(self._output_files[1]))

  def test_CacheResultsFalse(self):
    # Check that setting cache_results=False prevents results from being written
    # to the cache.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      self.GenerateTestData('CacheResultsFalse', work_dir)
      storage = fake_storage.FakeStorage()
      o = once.Once(storage=storage, cache_results=False, system_summary='test')
      o.Run('test', self._input_dirs, self._output_dirs[0],
            [command.Copy('%(input0)s/in0', '%(output)s/out', cwd=work_dir)])
      self.assertEquals(0, storage.ItemCount())
      self.assertEquals(file_tools.ReadFile(self._input_files[0]),
                        file_tools.ReadFile(self._output_files[0]))

  def test_Mkdir(self):
    # Test the Mkdir convenience wrapper works.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      self.GenerateTestData('Mkdir', work_dir)
      foo = os.path.join(work_dir, 'foo')
      o = once.Once(storage=fake_storage.FakeStorage(),
                    cache_results=False, system_summary='test')
      o.Run('test', self._input_dirs, foo,
            [command.Mkdir('hi')],
            working_dir=foo)
      self.assertTrue(os.path.isdir(os.path.join(foo, 'hi')))

  def test_Command(self):
    # Test a plain command.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      self.GenerateTestData('Command', work_dir)
      o = once.Once(storage=fake_storage.FakeStorage(), system_summary='test')
      o.Run('test', self._input_dirs, self._output_dirs[0],
            [command.Command([
                sys.executable, '-c',
                'import sys; open(sys.argv[1], "wb").write("hello")',
                '%(output)s/out'], cwd=work_dir)])
      self.assertEquals('hello', file_tools.ReadFile(self._output_files[0]))

  def test_UnpackCommands(self):
    # Test that unpack commnds get run first and hashed_inputs get
    # used when present.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      self.GenerateTestData('UnpackCommands', work_dir)
      self._tally = 0
      def check_call(cmd, **kwargs):
        self._tally += 1
        subprocess.check_call(cmd, **kwargs)
      o = once.Once(
          storage=fake_storage.FakeStorage(), check_call=check_call,
          system_summary='test')
      alt_inputs = {'input0': os.path.join(work_dir, 'alt_input')}
      unpack_commands = [command.Copy('%(input0)s/in0', alt_inputs['input0'])]
      commands = [command.Copy('%(input0)s', '%(output)s/out', cwd=work_dir)]
      o.Run('test', self._input_dirs, self._output_dirs[0],
            commands=commands,
            unpack_commands=unpack_commands,
            hashed_inputs=alt_inputs)
      o.Run('test', self._input_dirs, self._output_dirs[1], commands=commands,
            unpack_commands=unpack_commands,
            hashed_inputs=alt_inputs)
      self.assertEquals(file_tools.ReadFile(self._input_files[0]),
                        file_tools.ReadFile(self._output_files[0]))
      self.assertEquals(file_tools.ReadFile(self._input_files[0]),
                        file_tools.ReadFile(self._output_files[1]))
      self.assertEquals(3, self._tally)


if __name__ == '__main__':
  unittest.main()
