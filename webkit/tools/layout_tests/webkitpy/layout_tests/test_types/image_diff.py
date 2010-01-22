# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compares the image output of a test to the expected image output.

Compares hashes for the generated and expected images. If the output doesn't
match, returns FailureImageHashMismatch and outputs both hashes into the layout
test results directory.
"""

import errno
import logging
import os
import shutil
import subprocess

from layout_package import path_utils
from layout_package import test_failures
from test_types import test_type_base

# Cache whether we have the image_diff executable available.
_compare_available = True
_compare_msg_printed = False


class ImageDiff(test_type_base.TestTypeBase):

    def _CopyOutputPNG(self, test_filename, source_image, extension):
        """Copies result files into the output directory with appropriate
        names.

        Args:
          test_filename: the test filename
          source_file: path to the image file (either actual or expected)
          extension: extension to indicate -actual.png or -expected.png
        """
        self._MakeOutputDirectory(test_filename)
        dest_image = self.OutputFilename(test_filename, extension)

        try:
            shutil.copyfile(source_image, dest_image)
        except IOError, e:
            # A missing expected PNG has already been recorded as an error.
            if errno.ENOENT != e.errno:
                raise

    def _SaveBaselineFiles(self, filename, png_path, checksum):
        """Saves new baselines for the PNG and checksum.

        Args:
          filename: test filename
          png_path: path to the actual PNG result file
          checksum: value of the actual checksum result
        """
        png_file = open(png_path, "rb")
        png_data = png_file.read()
        png_file.close()
        self._SaveBaselineData(filename, png_data, ".png")
        self._SaveBaselineData(filename, checksum, ".checksum")

    def _CreateImageDiff(self, filename, target):
        """Creates the visual diff of the expected/actual PNGs.

        Args:
          filename: the name of the test
          target: Debug or Release
        """
        diff_filename = self.OutputFilename(filename,
          self.FILENAME_SUFFIX_COMPARE)
        actual_filename = self.OutputFilename(filename,
          self.FILENAME_SUFFIX_ACTUAL + '.png')
        expected_filename = self.OutputFilename(filename,
          self.FILENAME_SUFFIX_EXPECTED + '.png')

        global _compare_available
        cmd = ''

        try:
            executable = path_utils.ImageDiffPath(target)
            cmd = [executable, '--diff', actual_filename, expected_filename,
                   diff_filename]
        except Exception, e:
            _compare_available = False

        result = 1
        if _compare_available:
            try:
                result = subprocess.call(cmd)
            except OSError, e:
                if e.errno == errno.ENOENT or e.errno == errno.EACCES:
                    _compare_available = False
                else:
                    raise e
            except ValueError:
                # work around a race condition in Python 2.4's implementation
                # of subprocess.Popen
                pass

        global _compare_msg_printed

        if not _compare_available and not _compare_msg_printed:
            _compare_msg_printed = True
            print('image_diff not found. Make sure you have a ' + target +
                  ' build of the image_diff executable.')

        return result

    def CompareOutput(self, filename, proc, output, test_args, target):
        """Implementation of CompareOutput that checks the output image and
        checksum against the expected files from the LayoutTest directory.
        """
        failures = []

        # If we didn't produce a hash file, this test must be text-only.
        if test_args.hash is None:
            return failures

        # If we're generating a new baseline, we pass.
        if test_args.new_baseline:
            self._SaveBaselineFiles(filename, test_args.png_path,
                                    test_args.hash)
            return failures

        # Compare hashes.
        expected_hash_file = path_utils.ExpectedFilename(filename, '.checksum')

        expected_png_file = path_utils.ExpectedFilename(filename, '.png')

        if test_args.show_sources:
            logging.debug('Using %s' % expected_hash_file)
            logging.debug('Using %s' % expected_png_file)

        try:
            expected_hash = open(expected_hash_file, "r").read()
        except IOError, e:
            if errno.ENOENT != e.errno:
                raise
            expected_hash = ''


        if not os.path.isfile(expected_png_file):
            # Report a missing expected PNG file.
            self.WriteOutputFiles(filename, '', '.checksum', test_args.hash,
                                  expected_hash, diff=False, wdiff=False)
            self._CopyOutputPNG(filename, test_args.png_path, '-actual.png')
            failures.append(test_failures.FailureMissingImage(self))
            return failures
        elif test_args.hash == expected_hash:
            # Hash matched (no diff needed, okay to return).
            return failures


        self.WriteOutputFiles(filename, '', '.checksum', test_args.hash,
                              expected_hash, diff=False, wdiff=False)
        self._CopyOutputPNG(filename, test_args.png_path, '-actual.png')
        self._CopyOutputPNG(filename, expected_png_file, '-expected.png')

        # Even though we only use result in one codepath below but we
        # still need to call CreateImageDiff for other codepaths.
        result = self._CreateImageDiff(filename, target)
        if expected_hash == '':
            failures.append(test_failures.FailureMissingImageHash(self))
        elif test_args.hash != expected_hash:
            # Hashes don't match, so see if the images match. If they do, then
            # the hash is wrong.
            if result == 0:
                failures.append(test_failures.FailureImageHashIncorrect(self))
            else:
                failures.append(test_failures.FailureImageHashMismatch(self))

        return failures

    def DiffFiles(self, file1, file2):
        """Diff two image files.

        Args:
          file1, file2: full paths of the files to compare.

        Returns:
          True if two files are different.
          False otherwise.
        """

        try:
            executable = path_utils.ImageDiffPath('Debug')
        except Exception, e:
            logging.warn('Failed to find image diff executable.')
            return True

        cmd = [executable, file1, file2]
        result = 1
        try:
            result = subprocess.call(cmd)
        except OSError, e:
            logging.warn('Failed to compare image diff: %s', e)
            return True

        return result == 1
