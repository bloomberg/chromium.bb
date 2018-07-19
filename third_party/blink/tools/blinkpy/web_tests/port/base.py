# Copyright (C) 2010 Google Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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

"""Abstract base class for Port classes.

The Port classes encapsulate Port-specific (platform-specific) behavior
in the layout test infrastructure.
"""

import collections
import errno
import json
import logging
import optparse
import os
import re
import sys
import tempfile

from blinkpy.common import exit_codes
from blinkpy.common import find_files
from blinkpy.common import read_checksum_from_png
from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.path import abspath_to_uri
from blinkpy.w3c.wpt_manifest import WPTManifest
from blinkpy.web_tests.layout_package.bot_test_expectations import BotTestExpectationsFactory
from blinkpy.web_tests.models.test_configuration import TestConfiguration
from blinkpy.web_tests.models.test_expectations import TestExpectationParser
from blinkpy.web_tests.models.test_run_results import TestRunException
from blinkpy.web_tests.port import driver
from blinkpy.web_tests.port import server_process
from blinkpy.web_tests.port.factory import PortFactory
from blinkpy.web_tests.servers import apache_http
from blinkpy.web_tests.servers import pywebsocket
from blinkpy.web_tests.servers import wptserve

_log = logging.getLogger(__name__)

# Path relative to the build directory.
CONTENT_SHELL_FONTS_DIR = "test_fonts"

FONT_FILES = [
    [[CONTENT_SHELL_FONTS_DIR], 'Ahem.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Arimo-Bold.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Arimo-BoldItalic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Arimo-Italic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Arimo-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Cousine-Bold.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Cousine-BoldItalic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Cousine-Italic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Cousine-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'DejaVuSans.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'GardinerModBug.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'GardinerModCat.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Garuda.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Gelasio-Bold.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Gelasio-BoldItalic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Gelasio-Italic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Gelasio-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Lohit-Devanagari.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Lohit-Gurmukhi.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Lohit-Tamil.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'MuktiNarrow.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'NotoSansCJKjp-Regular.otf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'NotoSansKhmer-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Tinos-Bold.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Tinos-BoldItalic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Tinos-Italic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Tinos-Regular.ttf', None],
]

# This is the fingerprint of wpt's certificate found in
# blinkpy/third_party/wpt/certs.  The following line is updated by
# update_cert.py.
WPT_FINGERPRINT = 'Nxvaj3+bY3oVrTc+Jp7m3E3sB1n3lXtnMDCyBsqEXiY='
# One for 127.0.0.1.sxg.pem
SXG_FINGERPRINT = '55qC1nKu2A88ESbFmk5sTPQS/ScG+8DD7P+2bgFA9iM='


class Port(object):
    """Abstract class for Port-specific hooks for the layout_test package."""

    # Subclasses override this. This should indicate the basic implementation
    # part of the port name, e.g., 'mac', 'win', 'gtk'; there is one unique
    # value per class.
    # FIXME: Rename this to avoid confusion with the "full port name".
    port_name = None

    # Test paths use forward slash as separator on all platforms.
    TEST_PATH_SEPARATOR = '/'

    ALL_BUILD_TYPES = ('debug', 'release')

    CONTENT_SHELL_NAME = 'content_shell'

    ALL_SYSTEMS = (
        # FIXME: We treat Retina (High-DPI) devices as if they are running a different
        # a different operating system version. This isn't accurate, but will
        # work until we need to test and support baselines across multiple OS versions.
        ('retina', 'x86'),

        ('mac10.10', 'x86'),
        ('mac10.11', 'x86'),
        ('mac10.12', 'x86'),
        ('mac10.13', 'x86'),
        ('win7', 'x86'),
        ('win10', 'x86'),
        ('trusty', 'x86_64'),

        ('fuchsia', 'x86_64'),

        # FIXME: Technically this should be 'arm', but adding a third
        # architecture type breaks TestConfigurationConverter.
        # If we need this to be 'arm' in the future, then we first have to
        # fix TestConfigurationConverter.
        ('kitkat', 'x86'),
    )

    CONFIGURATION_SPECIFIER_MACROS = {
        'mac': ['retina', 'mac10.10', 'mac10.11', 'mac10.12', 'mac10.13'],
        'win': ['win7', 'win10'],
        'linux': ['trusty'],
        'android': ['kitkat'],
    }

    # List of ports open on the host that the tests will connect to. When tests
    # run on a separate machine (Android and Fuchsia) these ports need to be
    # forwarded back to the host.
    # 8000, 8080 and 8443 are for http/https tests;
    # 8880 is for websocket tests (see apache_http.py and pywebsocket.py).
    # 8001, 8081 and 8444 are for http/https WPT;
    # 9001 and 9444 are for websocket WPT (see wptserve.py).
    SERVER_PORTS = [8000, 8001, 8080, 8081, 8443, 8444, 8880, 9001, 9444]

    FALLBACK_PATHS = {}

    SUPPORTED_VERSIONS = []

    # URL to the build requirements page.
    BUILD_REQUIREMENTS_URL = ''

    # The suffixes of baseline files (not extensions).
    BASELINE_SUFFIX = '-expected'
    BASELINE_MISMATCH_SUFFIX = '-expected-mismatch'

    # All of the non-reftest baseline extensions we use.
    BASELINE_EXTENSIONS = ('.wav', '.txt', '.png')

    FLAG_EXPECTATIONS_PREFIX = 'FlagExpectations'

    # Because this is an abstract base class, arguments to functions may be
    # unused in this class - pylint: disable=unused-argument

    @classmethod
    def latest_platform_fallback_path(cls):
        return cls.FALLBACK_PATHS[cls.SUPPORTED_VERSIONS[-1]]

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        """Return a fully-specified port name that can be used to construct objects."""
        # Subclasses will usually override this.
        assert port_name.startswith(cls.port_name)
        return port_name

    def __init__(self, host, port_name, options=None, **kwargs):

        # This value is the "full port name", and may be different from
        # cls.port_name by having version modifiers appended to it.
        self._name = port_name

        # These are default values that should be overridden in a subclasses.
        self._version = ''
        self._architecture = 'x86'

        # FIXME: Ideally we'd have a package-wide way to get a well-formed
        # options object that had all of the necessary options defined on it.
        self._options = options or optparse.Values()

        self.host = host
        self._executive = host.executive
        self._filesystem = host.filesystem
        self._path_finder = PathFinder(host.filesystem)

        self._http_server = None
        self._websocket_server = None
        self._wpt_server = None
        self._image_differ = None
        self.server_process_constructor = server_process.ServerProcess  # This can be overridden for testing.
        self._http_lock = None  # FIXME: Why does this live on the port object?
        self._dump_reader = None

        if not hasattr(options, 'configuration') or not options.configuration:
            self.set_option_default('configuration', self.default_configuration())
        if not hasattr(options, 'target') or not options.target:
            self.set_option_default('target', self._options.configuration)
        self._test_configuration = None
        self._reftest_list = {}
        self._results_directory = None
        self._virtual_test_suites = None

    def __str__(self):
        return 'Port{name=%s, version=%s, architecture=%s, test_configuration=%s}' % (
            self._name, self._version, self._architecture, self._test_configuration)

    def primary_driver_flag(self):
        """Returns the driver flag that is used for flag-specific expectations and baselines. This
           is the flag in LayoutTests/additional-driver-flag.setting, if present, otherwise the
           first flag passed by --additional-driver-flag.
        """
        flag_file = self._filesystem.join(self.layout_tests_dir(), 'additional-driver-flag.setting')
        if self._filesystem.exists(flag_file):
            flag = self._filesystem.read_text_file(flag_file).strip()
            if flag:
                return flag
        flags = self.get_option('additional_driver_flag', [])
        if flags:
            return flags[0]

    def additional_driver_flags(self):
        # Clone list to avoid mutating option state.
        flags = list(self.get_option('additional_driver_flag', []))
        if flags and flags[0] == self.primary_driver_flag():
            flags = flags[1:]
        if self.driver_name() == self.CONTENT_SHELL_NAME:
            flags += [
                '--run-web-tests',
                '--ignore-certificate-errors-spki-list=' + WPT_FINGERPRINT +
                ',' + SXG_FINGERPRINT,
                '--user-data-dir']
        return flags

    def supports_per_test_timeout(self):
        return False

    def default_smoke_test_only(self):
        return False

    def default_timeout_ms(self):
        timeout_ms = 6 * 1000
        if self.get_option('configuration') == 'Debug':
            # Debug is usually 2x-3x slower than Release.
            return 3 * timeout_ms
        return timeout_ms

    def driver_stop_timeout(self):
        """Returns the amount of time in seconds to wait before killing the process in driver.stop()."""
        # We want to wait for at least 3 seconds, but if we are really slow, we
        # want to be slow on cleanup as well (for things like ASAN, Valgrind, etc.)
        return 3.0 * float(self.get_option('time_out_ms', '0')) / self.default_timeout_ms()

    def default_batch_size(self):
        """Returns the default batch size to use for this port."""
        if self.get_option('enable_sanitizer'):
            # ASAN/MSAN/TSAN use more memory than regular content_shell. Their
            # memory usage may also grow over time, up to a certain point.
            # Relaunching the driver periodically helps keep it under control.
            return 40
        # The default is infinite batch size.
        return None

    def default_child_processes(self):
        """Returns the number of child processes to use for this port."""
        return self._executive.cpu_count()

    def max_drivers_per_process(self):
        """Returns the maximum number of drivers a child process can use for this port."""
        return 2

    def default_max_locked_shards(self):
        """Returns the number of "locked" shards to run in parallel (like the http tests)."""
        max_locked_shards = int(self.default_child_processes()) / 4
        if not max_locked_shards:
            return 1
        return max_locked_shards

    def baseline_version_dir(self):
        """Returns the absolute path to the platform-and-version-specific results."""
        baseline_search_paths = self.baseline_search_path()
        return baseline_search_paths[0]

    def baseline_flag_specific_dir(self):
        """If --additional-driver-flag is specified, returns the absolute path to the flag-specific
           platform-independent results. Otherwise returns None."""
        flag_specific_path = self._flag_specific_baseline_search_path()
        return flag_specific_path[-1] if flag_specific_path else None

    def baseline_search_path(self):
        return (self.get_option('additional_platform_directory', []) +
                self._flag_specific_baseline_search_path() +
                self._compare_baseline() +
                self.default_baseline_search_path())

    def default_baseline_search_path(self):
        """Returns a list of absolute paths to directories to search under for baselines.

        The directories are searched in order.
        """
        return map(self._absolute_baseline_path, self.FALLBACK_PATHS[self.version()])

    @memoized
    def _compare_baseline(self):
        factory = PortFactory(self.host)
        target_port = self.get_option('compare_port')
        if target_port:
            return factory.get(target_port).default_baseline_search_path()
        return []

    def _check_file_exists(self, path_to_file, file_description,
                           override_step=None, more_logging=True):
        """Verifies that the file is present where expected, or logs an error.

        Args:
            file_name: The (human friendly) name or description of the file
                you're looking for (e.g., "HTTP Server"). Used for error logging.
            override_step: An optional string to be logged if the check fails.
            more_logging: Whether or not to log the error messages.

        Returns:
            True if the file exists, else False.
        """
        if not self._filesystem.exists(path_to_file):
            if more_logging:
                _log.error('Unable to find %s', file_description)
                _log.error('    at %s', path_to_file)
                if override_step:
                    _log.error('    %s', override_step)
                    _log.error('')
            return False
        return True

    def check_build(self, needs_http, printer):
        if not self._check_file_exists(self._path_to_driver(), 'test driver'):
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        if not self._check_driver_build_up_to_date(self.get_option('configuration')):
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        if self.get_option('pixel_tests') and not self.check_image_diff():
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        if self._dump_reader and not self._dump_reader.check_is_functional():
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        if needs_http and not self.check_httpd():
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        return exit_codes.OK_EXIT_STATUS

    def _check_driver(self):
        driver_path = self._path_to_driver()
        if not self._filesystem.exists(driver_path):
            _log.error('%s was not found at %s', self.driver_name(), driver_path)
            return False
        return True

    def check_sys_deps(self, needs_http):
        """Checks whether the system is properly configured.

        If the port needs to do some runtime checks to ensure that the
        tests can be run successfully, it should override this routine.
        This step can be skipped with --nocheck-sys-deps.

        Returns:
            An exit status code.
        """
        cmd = [self._path_to_driver(), '--check-layout-test-sys-deps']

        additional_flags = self.get_option('additional_driver_flag', [])
        if additional_flags:
            cmd.append(additional_flags[0])

        local_error = ScriptError()

        def error_handler(script_error):
            local_error.exit_code = script_error.exit_code

        if self.host.platform.is_linux():
            _log.debug('DISPLAY = %s', self.host.environ.get('DISPLAY', ''))
        output = self._executive.run_command(cmd, error_handler=error_handler)
        if local_error.exit_code:
            _log.error('System dependencies check failed.')
            _log.error('To override, invoke with --nocheck-sys-deps')
            _log.error('')
            _log.error(output)
            if self.BUILD_REQUIREMENTS_URL is not '':
                _log.error('')
                _log.error('For complete build requirements, please see:')
                _log.error(self.BUILD_REQUIREMENTS_URL)
            return exit_codes.SYS_DEPS_EXIT_STATUS
        return exit_codes.OK_EXIT_STATUS

    def check_image_diff(self):
        """Checks whether image_diff binary exists."""
        image_diff_path = self._path_to_image_diff()
        if not self._filesystem.exists(image_diff_path):
            _log.error('image_diff was not found at %s', image_diff_path)
            return False
        return True

    def check_httpd(self):
        httpd_path = self.path_to_apache()
        if httpd_path:
            try:
                env = self.setup_environ_for_server()
                if self._executive.run_command([httpd_path, '-v'], env=env, return_exit_code=True) != 0:
                    _log.error('httpd seems broken. Cannot run http tests.')
                    return False
                return True
            except OSError:
                pass
        _log.error('No httpd found. Cannot run http tests.')
        return False

    def do_text_results_differ(self, expected_text, actual_text):
        return expected_text != actual_text

    def do_audio_results_differ(self, expected_audio, actual_audio):
        return expected_audio != actual_audio

    def diff_image(self, expected_contents, actual_contents):
        """Compares two images and returns an (image diff, error string) pair.

        If an error occurs (like image_diff isn't found, or crashes), we log an
        error and return True (for a diff).
        """
        # If only one of them exists, return that one.
        if not actual_contents and not expected_contents:
            return (None, None)
        if not actual_contents:
            return (expected_contents, None)
        if not expected_contents:
            return (actual_contents, None)

        tempdir = self._filesystem.mkdtemp()

        expected_filename = self._filesystem.join(str(tempdir), 'expected.png')
        self._filesystem.write_binary_file(expected_filename, expected_contents)

        actual_filename = self._filesystem.join(str(tempdir), 'actual.png')
        self._filesystem.write_binary_file(actual_filename, actual_contents)

        diff_filename = self._filesystem.join(str(tempdir), 'diff.png')

        executable = self._path_to_image_diff()
        # Although we are handed 'old', 'new', image_diff wants 'new', 'old'.
        command = [executable, '--diff', actual_filename, expected_filename, diff_filename]

        result = None
        err_str = None
        try:
            exit_code = self._executive.run_command(command, return_exit_code=True)
            if exit_code == 0:
                # The images are the same.
                result = None
            elif exit_code == 1:
                result = self._filesystem.read_binary_file(diff_filename)
            else:
                err_str = 'Image diff returned an exit code of %s. See http://crbug.com/278596' % exit_code
        except OSError as error:
            err_str = 'error running image diff: %s' % error
        finally:
            self._filesystem.rmtree(str(tempdir))

        return (result, err_str or None)

    def driver_name(self):
        if self.get_option('driver_name'):
            return self.get_option('driver_name')
        return self.CONTENT_SHELL_NAME

    def expected_baselines_by_extension(self, test_name):
        """Returns a dict mapping baseline suffix to relative path for each baseline in a test.

        For reftests, it returns ".==" or ".!=" instead of the suffix.
        """
        # FIXME: The name similarity between this and expected_baselines()
        # below, is unfortunate. We should probably rename them both.
        baseline_dict = {}
        reference_files = self.reference_files(test_name)
        if reference_files:
            # FIXME: How should this handle more than one type of reftest?
            baseline_dict['.' + reference_files[0][0]] = self.relative_test_filename(reference_files[0][1])

        for extension in self.BASELINE_EXTENSIONS:
            path = self.expected_filename(test_name, extension, return_default=False)
            baseline_dict[extension] = self.relative_test_filename(path) if path else path

        return baseline_dict

    def output_filename(self, test_name, suffix, extension):
        """Generates the output filename for a test.

        This method gives a proper filename for various outputs of a test,
        including baselines and actual results. Usually, the output filename
        follows the pattern: test_name_without_ext+suffix+extension, but when
        the test name contains query strings, e.g. external/wpt/foo.html?wss,
        test_name_without_ext is mangled to be external/wpt/foo_wss.

        It is encouraged to use this method instead of writing another mangling.

        Args:
            test_name: The name of a test.
            suffix: A suffix string to add before the extension
                (e.g. "-expected").
            extension: The extension of the output file (starting with .).

        Returns:
            A string, the output filename.
        """
        # WPT names might contain query strings, e.g. external/wpt/foo.html?wss,
        # in which case we mangle test_name_root (the part of a path before the
        # last extension point) to external/wpt/foo_wss, and the output filename
        # becomes external/wpt/foo_wss-expected.txt.
        index = test_name.find('?')
        if index != -1:
            test_name_root, _ = self._filesystem.splitext(test_name[:index])
            query_part = test_name[index:]
            test_name_root += self._filesystem.sanitize_filename(query_part)
        else:
            test_name_root, _ = self._filesystem.splitext(test_name)
        return test_name_root + suffix + extension

    def expected_baselines(self, test_name, extension, all_baselines=False, match=True):
        """Given a test name, finds where the baseline results are located.

        Return values will be in the format appropriate for the current
        platform (e.g., "\\" for path separators on Windows). If the results
        file is not found, then None will be returned for the directory,
        but the expected relative pathname will still be returned.

        This routine is generic but lives here since it is used in
        conjunction with the other baseline and filename routines that are
        platform specific.

        Args:
            test_name: Name of test file (usually a relative path under LayoutTests/)
            extension: File extension of the expected results, including dot;
                e.g. '.txt' or '.png'.  This should not be None, but may be an
                empty string.
            all_baselines: If True, return an ordered list of all baseline paths
                for the given platform. If False, return only the first one.
            match: Whether the baseline is a match or a mismatch.

        Returns:
            A list of (platform_dir, results_filename) pairs, where
                platform_dir - abs path to the top of the results tree (or test
                    tree)
                results_filename - relative path from top of tree to the results
                    file
                (port.join() of the two gives you the full path to the file,
                    unless None was returned.)
        """
        baseline_filename = self.output_filename(
            test_name, self.BASELINE_SUFFIX if match else self.BASELINE_MISMATCH_SUFFIX, extension)
        baseline_search_path = self.baseline_search_path()

        baselines = []
        for platform_dir in baseline_search_path:
            if self._filesystem.exists(self._filesystem.join(platform_dir, baseline_filename)):
                baselines.append((platform_dir, baseline_filename))

            if not all_baselines and baselines:
                return baselines

        # If it wasn't found in a platform directory, return the expected
        # result in the test directory, even if no such file actually exists.
        platform_dir = self.layout_tests_dir()
        if self._filesystem.exists(self._filesystem.join(platform_dir, baseline_filename)):
            baselines.append((platform_dir, baseline_filename))

        if baselines:
            return baselines

        return [(None, baseline_filename)]

    def expected_filename(self, test_name, extension,
                          return_default=True, fallback_base_for_virtual=True, match=True):
        """Given a test name, returns an absolute path to its expected results.

        If no expected results are found in any of the searched directories,
        the directory in which the test itself is located will be returned.
        The return value is in the format appropriate for the platform
        (e.g., "\\" for path separators on windows).

        This routine is generic but is implemented here to live alongside
        the other baseline and filename manipulation routines.

        Args:
            test_name: Name of test file (usually a relative path under LayoutTests/)
            extension: File extension of the expected results, including dot;
                e.g. '.txt' or '.png'.  This should not be None, but may be an
                empty string.
            return_default: If True, returns the path to the generic expectation
                if nothing else is found; if False, returns None.
            fallback_base_for_virtual: For virtual test only. When no virtual
                specific baseline is found, if this parameter is True, fallback
                to find baselines of the base test; if False, depending on
                |return_default|, returns the generic virtual baseline or None.
            match: Whether the baseline is a match or a mismatch.

        Returns:
            An absolute path to its expected results, or None if not found.
        """
        # FIXME: The [0] here is very mysterious, as is the destructured return.
        platform_dir, baseline_filename = self.expected_baselines(test_name, extension, match=match)[0]
        if platform_dir:
            return self._filesystem.join(platform_dir, baseline_filename)

        if fallback_base_for_virtual:
            actual_test_name = self.lookup_virtual_test_base(test_name)
            if actual_test_name:
                return self.expected_filename(actual_test_name, extension, return_default, match=match)

        if return_default:
            return self._filesystem.join(self.layout_tests_dir(), baseline_filename)
        return None

    def expected_checksum(self, test_name):
        """Returns the checksum of the image we expect the test to produce,
        or None if it is a text-only test.
        """
        png_path = self.expected_filename(test_name, '.png')

        if self._filesystem.exists(png_path):
            with self._filesystem.open_binary_file_for_reading(png_path) as filehandle:
                return read_checksum_from_png.read_checksum(filehandle)

        return None

    def expected_image(self, test_name):
        """Returns the image we expect the test to produce."""
        baseline_path = self.expected_filename(test_name, '.png')
        if not self._filesystem.exists(baseline_path):
            return None
        return self._filesystem.read_binary_file(baseline_path)

    def expected_audio(self, test_name):
        baseline_path = self.expected_filename(test_name, '.wav')
        if not self._filesystem.exists(baseline_path):
            return None
        return self._filesystem.read_binary_file(baseline_path)

    def expected_text(self, test_name):
        """Returns the text output we expect the test to produce, or None
        if we don't expect there to be any text output.

        End-of-line characters are normalized to '\n'.
        """
        # FIXME: DRT output is actually utf-8, but since we don't decode the
        # output from DRT (instead treating it as a binary string), we read the
        # baselines as a binary string, too.
        baseline_path = self.expected_filename(test_name, '.txt')
        if not self._filesystem.exists(baseline_path):
            return None
        text = self._filesystem.read_binary_file(baseline_path)
        return text.replace('\r\n', '\n')

    def _get_reftest_list(self, test_name):
        dirname = self._filesystem.join(self.layout_tests_dir(), self._filesystem.dirname(test_name))
        if dirname not in self._reftest_list:
            self._reftest_list[dirname] = Port._parse_reftest_list(self._filesystem, dirname)
        return self._reftest_list[dirname]

    @staticmethod
    def _parse_reftest_list(filesystem, test_dirpath):
        reftest_list_path = filesystem.join(test_dirpath, 'reftest.list')
        if not filesystem.isfile(reftest_list_path):
            return None
        reftest_list_file = filesystem.read_text_file(reftest_list_path)

        parsed_list = {}
        for line in reftest_list_file.split('\n'):
            line = re.sub('#.+$', '', line)
            split_line = line.split()
            if len(split_line) == 4:
                # FIXME: Probably one of mozilla's extensions in the
                # reftest.list format. Do we need to support this?
                _log.warning("unsupported reftest.list line '%s' in %s", line, reftest_list_path)
                continue
            if len(split_line) < 3:
                continue
            expectation_type, test_file, ref_file = split_line
            parsed_list.setdefault(filesystem.join(test_dirpath, test_file), []).append(
                (expectation_type, filesystem.join(test_dirpath, ref_file)))
        return parsed_list

    def reference_files(self, test_name):
        """Returns a list of expectation (== or !=) and filename pairs"""

        # Try to extract information from reftest.list.
        reftest_list = self._get_reftest_list(test_name)
        if reftest_list:
            return reftest_list.get(self._filesystem.join(self.layout_tests_dir(), test_name), [])

        # Try to find -expected.* or -expected-mismatch.* in the same directory.
        reftest_list = []
        for expectation in ('==', '!='):
            for extension in Port.supported_file_extensions:
                path = self.expected_filename(test_name, extension, match=(expectation == '=='))
                if self._filesystem.exists(path):
                    reftest_list.append((expectation, path))
        if reftest_list:
            return reftest_list

        # Try to extract information from MANIFEST.json.
        match = re.match(r'virtual/[^/]+/', test_name)
        if match:
            test_name = test_name[match.end(0):]
        match = re.match(r'external/wpt/(.*)', test_name)
        if not match:
            return []
        path_in_wpt = match.group(1)
        for expectation, ref_path_in_wpt in self._wpt_manifest().extract_reference_list(path_in_wpt):
            ref_absolute_path = self._filesystem.join(self.layout_tests_dir(), 'external/wpt' + ref_path_in_wpt)
            reftest_list.append((expectation, ref_absolute_path))
        return reftest_list

    def tests(self, paths):
        """Returns all tests or tests matching supplied paths.

        Args:
            paths: Array of paths to match. If supplied, this function will only
                return tests matching at least one path in paths.

        Returns:
            An array of test paths and test names. The latter are web platform
            tests that don't correspond to file paths but are valid tests,
            for instance a file path test.any.js could correspond to two test
            names: test.any.html and test.any.worker.html.
        """
        tests = self.real_tests(paths)

        suites = self.virtual_test_suites()
        if paths:
            tests.extend(self._virtual_tests_matching_paths(paths, suites))
            if any('external' in path for path in paths):
                tests.extend(self._wpt_test_urls_matching_paths(paths))
        else:
            tests.extend(self._all_virtual_tests(suites))
            tests.extend(['external/wpt' + test for test in self._wpt_manifest().all_urls()])

        return tests

    def real_tests(self, paths):
        # When collecting test cases, skip these directories.
        skipped_directories = set([
            'platform', 'resources', 'support', 'script-tests',
            'reference', 'reftest', 'external'
        ])
        is_non_wpt_real_test_file = lambda fs, dirname, filename: (
            self.is_test_file(fs, dirname, filename)
            and not re.search(r'[/\\]external[/\\]wpt([/\\].*)?$', dirname)
        )
        files = find_files.find(self._filesystem, self.layout_tests_dir(), paths, skipped_directories,
                                is_non_wpt_real_test_file, self.test_key)
        return [self.relative_test_filename(f) for f in files]

    @staticmethod
    def is_reference_html_file(filesystem, dirname, filename):
        # TODO(robertma): We probably do not need prefixes/suffixes other than
        # -expected{-mismatch} any more. Or worse, there might be actual tests
        # with these prefixes/suffixes.
        if filename.startswith('ref-') or filename.startswith('notref-'):
            return True
        filename_without_ext, _ = filesystem.splitext(filename)
        for suffix in ['-expected', '-expected-mismatch', '-ref', '-notref']:
            if filename_without_ext.endswith(suffix):
                return True
        return False

    # When collecting test cases, we include any file with these extensions.
    supported_file_extensions = set([
        '.html', '.xml', '.xhtml', '.xht', '.pl',
        '.htm', '.php', '.svg', '.mht', '.pdf',
    ])

    @staticmethod
    def _has_supported_extension(filesystem, filename):
        """Returns True if filename is one of the file extensions we want to run a test on."""
        extension = filesystem.splitext(filename)[1]
        return extension in Port.supported_file_extensions

    def is_test_file(self, filesystem, dirname, filename):
        match = re.search(r'[/\\]external[/\\]wpt([/\\].*)?$', dirname)
        if match:
            if match.group(1):
                path_in_wpt = match.group(1)[1:].replace('\\', '/') + '/' + filename
            else:
                path_in_wpt = filename
            return self._wpt_manifest().is_test_file(path_in_wpt)
        extension = filesystem.splitext(filename)[1]
        if 'inspector-protocol' in dirname and extension == '.js':
            return True
        if 'devtools' in dirname and extension == '.js':
            return True
        return Port._has_supported_extension(
            filesystem, filename) and not Port.is_reference_html_file(filesystem, dirname, filename)

    def _convert_wpt_file_path_to_url_paths(self, file_path):
        tests = []
        # Path separators are normalized by relative_test_filename().
        match = re.search(r'external/wpt/(.*)$', file_path)
        if not match:
            return [file_path]
        urls = self._wpt_manifest().file_path_to_url_paths(match.group(1))
        for url in urls:
            tests.append(file_path[0:match.start(1)] + url)
        return tests

    @memoized
    def _wpt_manifest(self):
        manifest_path = self._filesystem.join(self.layout_tests_dir(), 'external', 'wpt', 'MANIFEST.json')
        if not self._filesystem.exists(manifest_path):
            _log.error('Manifest not found at %s. See http://crbug.com/698294', manifest_path)
            return WPTManifest('{}')
        return WPTManifest(self._filesystem.read_text_file(manifest_path))

    def is_slow_wpt_test(self, test_file):
        match = re.match(r'virtual/[^/]+/', test_file)
        if match:
            test_file = test_file[match.end(0):]
        # WPTManifest.is_slow_test() takes a WPT URL with the leading slash.
        match = re.match(r'external/wpt(.*)', test_file)
        if not match:
            return False
        return self._wpt_manifest().is_slow_test(match.group(1))

    def test_key(self, test_name):
        """Turns a test name into a pair of sublists: the natural sort key of the
        dirname, and the natural sort key of the basename.

        This can be used when sorting paths so that files in a directory.
        directory are kept together rather than being mixed in with files in
        subdirectories.
        """
        dirname, basename = self.split_test(test_name)
        return (
            self._natural_sort_key(dirname + self.TEST_PATH_SEPARATOR),
            self._natural_sort_key(basename)
        )

    def _natural_sort_key(self, string_to_split):
        """Turns a string into a list of string and number chunks.

        For example: "z23a" -> ["z", 23, "a"]
        This can be used to implement "natural sort" order. See:
        http://www.codinghorror.com/blog/2007/12/sorting-for-humans-natural-sort-order.html
        http://nedbatchelder.com/blog/200712.html#e20071211T054956
        """
        def tryint(val):
            try:
                return int(val)
            except ValueError:
                return val

        return [tryint(chunk) for chunk in re.split(r'(\d+)', string_to_split)]

    def test_dirs(self):
        """Returns the list of top-level test directories."""
        layout_tests_dir = self.layout_tests_dir()
        fs = self._filesystem
        return [d for d in fs.listdir(layout_tests_dir) if fs.isdir(fs.join(layout_tests_dir, d))]

    @memoized
    def test_isfile(self, test_name):
        """Returns True if the test name refers to a directory of tests."""
        # Used by test_expectations.py to apply rules to whole directories.
        if self._filesystem.isfile(self.abspath_for_test(test_name)):
            return True
        base = self.lookup_virtual_test_base(test_name)
        return base and self._filesystem.isfile(self.abspath_for_test(base))

    @memoized
    def test_isdir(self, test_name):
        """Returns True if the test name refers to a directory of tests."""
        # Used by test_expectations.py to apply rules to whole directories.
        if self._filesystem.isdir(self.abspath_for_test(test_name)):
            return True
        base = self.lookup_virtual_test_base(test_name)
        return base and self._filesystem.isdir(self.abspath_for_test(base))

    @memoized
    def test_exists(self, test_name):
        """Returns True if the test name refers to an existing test or baseline."""
        # Used by test_expectations.py to determine if an entry refers to a
        # valid test and by printing.py to determine if baselines exist.
        return self.is_wpt_test(test_name) or self.test_isfile(test_name) or self.test_isdir(test_name)

    def split_test(self, test_name):
        """Splits a test name into the 'directory' part and the 'basename' part."""
        index = test_name.rfind(self.TEST_PATH_SEPARATOR)
        if index < 1:
            return ('', test_name)
        return (test_name[0:index], test_name[index:])

    def normalize_test_name(self, test_name):
        """Returns a normalized version of the test name or test directory."""
        if test_name.endswith('/'):
            return test_name
        if self.test_isdir(test_name):
            return test_name + '/'
        return test_name

    def driver_cmd_line(self):
        """Prints the DRT (DumpRenderTree) command that will be used."""
        return self.create_driver(0).cmd_line(self.get_option('pixel_tests'), [])

    def update_baseline(self, baseline_path, data):
        """Updates the baseline for a test.

        Args:
            baseline_path: the actual path to use for baseline, not the path to
              the test. This function is used to update either generic or
              platform-specific baselines, but we can't infer which here.
            data: contents of the baseline.
        """
        self._filesystem.write_binary_file(baseline_path, data)

    def _path_from_chromium_base(self, *comps):
        return self._path_finder.path_from_chromium_base(*comps)

    def _perf_tests_dir(self):
        return self._path_finder.perf_tests_dir()

    def layout_tests_dir(self):
        custom_layout_tests_dir = self.get_option('layout_tests_directory')
        if custom_layout_tests_dir:
            return custom_layout_tests_dir
        return self._path_finder.layout_tests_dir()

    def skips_test(self, test):
        """Checks whether the given test is skipped for this port.

        Returns True if the test is skipped because the port runs smoke tests
        only or because the test is marked as WontFix, but *not* if the test
        is only marked as Skip indicating a temporary skip.
        """
        return self.skipped_due_to_smoke_tests(test) or self.skipped_in_never_fix_tests(test)

    @memoized
    def _tests_from_file(self, filename):
        tests = set()
        file_contents = self._filesystem.read_text_file(filename)
        for line in file_contents.splitlines():
            line = line.strip()
            if line.startswith('#') or not line:
                continue
            tests.add(line)
        return tests

    def skipped_due_to_smoke_tests(self, test):
        """Checks if the test is skipped based on the set of Smoke tests.

        Returns True if this port runs only smoke tests, and the test is not
        in the smoke tests file; returns False otherwise.
        """
        if not self.default_smoke_test_only():
            return False
        smoke_test_filename = self.path_to_smoke_tests_file()
        if not self._filesystem.exists(smoke_test_filename):
            return False
        smoke_tests = self._tests_from_file(smoke_test_filename)
        return test not in smoke_tests

    def path_to_smoke_tests_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'SmokeTests')

    def skipped_in_never_fix_tests(self, test):
        """Checks if the test is marked as WontFix for this port.

        In general, WontFix expectations are allowed in NeverFixTests but
        not in other files, and only WontFix lines are allowed in NeverFixTests.
        Some lines in NeverFixTests are platform-specific.

        Note: this will not work with skipped directories. See also the same
        issue with update_all_test_expectations_files in test_importer.py.
        """
        # TODO(qyearsley): Extract parsing logic (reading the file,
        # constructing a parser, etc.) from here and test_copier.py.
        path = self.path_to_never_fix_tests_file()
        contents = self._filesystem.read_text_file(path)
        parser = TestExpectationParser(self, all_tests=(), is_lint_mode=False)
        expectation_lines = parser.parse(path, contents)
        for line in expectation_lines:
            if line.name == test and self.test_configuration() in line.matching_configurations:
                return True
        return False

    def path_to_never_fix_tests_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'NeverFixTests')

    def name(self):
        """Returns a name that uniquely identifies this particular type of port.

        This is the full port name including both base port name and version,
        and can be passed to PortFactory.get() to instantiate a port.
        """
        return self._name

    def operating_system(self):
        # Subclasses should override this default implementation.
        return 'mac'

    def version(self):
        """Returns a string indicating the version of a given platform

        For example, "win10" or "trusty". This is used to help identify the
        exact port when parsing test expectations, determining search paths,
        and logging information.
        """
        return self._version

    def architecture(self):
        return self._architecture

    def get_option(self, name, default_value=None):
        return getattr(self._options, name, default_value)

    def set_option_default(self, name, default_value):
        return self._options.ensure_value(name, default_value)

    def relative_test_filename(self, filename):
        """Returns a Unix-style path for a filename relative to LayoutTests.

        Ports may legitimately return absolute paths here if no relative path
        makes sense.
        """
        # Ports that run on windows need to override this method to deal with
        # filenames with backslashes in them.
        if filename.startswith(self.layout_tests_dir()):
            return self.host.filesystem.relpath(filename, self.layout_tests_dir())
        else:
            return self.host.filesystem.abspath(filename)

    @memoized
    def abspath_for_test(self, test_name):
        """Returns the full path to the file for a given test name.

        This is the inverse of relative_test_filename().
        """
        return self._filesystem.join(self.layout_tests_dir(), test_name)

    def results_directory(self):
        """Returns the absolute path to the place to store the test results."""
        if not self._results_directory:
            option_val = self.get_option('results_directory') or self.default_results_directory()
            self._results_directory = self._filesystem.abspath(option_val)
        return self._results_directory

    def bot_test_times_path(self):
        return self._build_path('webkit_test_times', 'bot_times_ms.json')

    def perf_results_directory(self):
        return self._build_path()

    def inspector_build_directory(self):
        return self._build_path('resources', 'inspector')

    def generated_sources_directory(self):
        return self._build_path('gen')

    def apache_config_directory(self):
        return self._path_finder.path_from_blink_tools('apache_config')

    def default_results_directory(self):
        """Returns the absolute path to the default place to store the test results."""
        return self._build_path('layout-test-results')

    def setup_test_run(self):
        """Performs port-specific work at the beginning of a test run."""
        # Delete the disk cache if any to ensure a clean test run.
        dump_render_tree_binary_path = self._path_to_driver()
        cachedir = self._filesystem.dirname(dump_render_tree_binary_path)
        cachedir = self._filesystem.join(cachedir, 'cache')
        if self._filesystem.exists(cachedir):
            self._filesystem.rmtree(cachedir)

        if self._dump_reader:
            self._filesystem.maybe_make_directory(self._dump_reader.crash_dumps_directory())

    def num_workers(self, requested_num_workers):
        """Returns the number of available workers (possibly less than the number requested)."""
        return requested_num_workers

    def clean_up_test_run(self):
        """Performs port-specific work at the end of a test run."""
        if self._image_differ:
            self._image_differ.stop()
            self._image_differ = None

    def setup_environ_for_server(self):
        # We intentionally copy only a subset of the environment when
        # launching subprocesses to ensure consistent test results.
        clean_env = {}
        variables_to_copy = [
            'CHROME_DEVEL_SANDBOX',
            'CHROME_IPC_LOGGING',
            'ASAN_OPTIONS',
            'TSAN_OPTIONS',
            'MSAN_OPTIONS',
            'LSAN_OPTIONS',
            'UBSAN_OPTIONS',
            'VALGRIND_LIB',
            'VALGRIND_LIB_INNER',
            'TMPDIR',
        ]
        if 'TMPDIR' not in self.host.environ:
            self.host.environ['TMPDIR'] = tempfile.gettempdir()
        # CGIs are run directory-relative so they need an absolute TMPDIR
        self.host.environ['TMPDIR'] = self._filesystem.abspath(self.host.environ['TMPDIR'])
        if self.host.platform.is_linux() or self.host.platform.is_freebsd():
            variables_to_copy += [
                'XAUTHORITY',
                'HOME',
                'LANG',
                'LD_LIBRARY_PATH',
                'DBUS_SESSION_BUS_ADDRESS',
                'XDG_DATA_DIRS',
                'XDG_RUNTIME_DIR'
            ]
            clean_env['DISPLAY'] = self.host.environ.get('DISPLAY', ':1')
        if self.host.platform.is_mac():
            clean_env['DYLD_LIBRARY_PATH'] = self._build_path()
            variables_to_copy += [
                'HOME',
            ]
        if self.host.platform.is_win():
            variables_to_copy += [
                'PATH',
                'GYP_DEFINES',  # Required to locate win sdk.
            ]

        for variable in variables_to_copy:
            if variable in self.host.environ:
                clean_env[variable] = self.host.environ[variable]

        for string_variable in self.get_option('additional_env_var', []):
            [name, value] = string_variable.split('=', 1)
            clean_env[name] = value

        return clean_env

    def show_results_html_file(self, results_filename):
        """Displays the given HTML file in a user's browser."""
        return self.host.user.open_url(abspath_to_uri(self.host.platform, results_filename))

    def create_driver(self, worker_number, no_timeout=False):
        """Returns a newly created Driver subclass for starting/stopping the
        test driver.
        """
        return self._driver_class()(self, worker_number, pixel_tests=self.get_option('pixel_tests'), no_timeout=no_timeout)

    def requires_http_server(self):
        # Does the port require an HTTP server for running tests? This could
        # be the case when the tests aren't run on the host platform.
        return False

    def start_http_server(self, additional_dirs, number_of_drivers):
        """Start a web server. Raise an error if it can't start or is already running.

        Ports can stub this out if they don't need a web server to be running.
        """
        assert not self._http_server, 'Already running an http server.'

        server = apache_http.ApacheHTTP(self, self.results_directory(),
                                        additional_dirs=additional_dirs,
                                        number_of_servers=(number_of_drivers * 4))
        server.start()
        self._http_server = server

    def start_websocket_server(self):
        """Start a web server. Raise an error if it can't start or is already running.

        Ports can stub this out if they don't need a websocket server to be running.
        """
        assert not self._websocket_server, 'Already running a websocket server.'

        server = pywebsocket.PyWebSocket(self, self.results_directory())
        server.start()
        self._websocket_server = server

    @staticmethod
    def is_wpt_test(test):
        """Whether a test is considered a web-platform-tests test."""
        return re.match(r'(virtual/[^/]+/)?external/wpt/', test)

    @staticmethod
    def should_use_wptserve(test):
        return Port.is_wpt_test(test)

    @staticmethod
    def should_run_in_wpt_mode(test):
        """Whether content_shell should run a test in the WPT mode.

        Some tests outside external/wpt should also be run in the WPT mode in
        content_shell, namely: harness-tests/wpt/ (tests for console log
        filtering).
        """
        # Note: match rules in TestInterfaces::ConfigureForTestWithURL in
        # //src/content/shell/test_runner/test_interfaces.cc.
        return (Port.is_wpt_test(test) or
                re.match(r'harness-tests/wpt/', test))

    def start_wptserve(self):
        """Starts a WPT web server.

        Raises an error if it can't start or is already running.
        """
        assert not self._wpt_server, 'Already running a WPT server.'

        # We currently don't support any output mechanism for the WPT server.
        server = wptserve.WPTServe(self, self.results_directory())
        server.start()
        self._wpt_server = server

    def stop_wptserve(self):
        """Shuts down the WPT server if it is running."""
        if self._wpt_server:
            self._wpt_server.stop()
            self._wpt_server = None

    def http_server_requires_http_protocol_options_unsafe(self):
        httpd_path = self.path_to_apache()
        intentional_syntax_error = 'INTENTIONAL_SYNTAX_ERROR'
        cmd = [httpd_path,
               '-C', 'HttpProtocolOptions Unsafe',
               '-C', intentional_syntax_error]
        env = self.setup_environ_for_server()

        def error_handler(err):
            pass
        output = self._executive.run_command(cmd, env=env,
                                             error_handler=error_handler)
        # If apache complains about the intentional error, it apparently
        # accepted the HttpProtocolOptions directive, and we should add it.
        return intentional_syntax_error in output

    def http_server_supports_ipv6(self):
        # Apache < 2.4 on win32 does not support IPv6.
        return not self.host.platform.is_win()

    def stop_http_server(self):
        """Shuts down the http server if it is running."""
        if self._http_server:
            self._http_server.stop()
            self._http_server = None

    def stop_websocket_server(self):
        """Shuts down the websocket server if it is running."""
        if self._websocket_server:
            self._websocket_server.stop()
            self._websocket_server = None

    #
    # TEST EXPECTATION-RELATED METHODS
    #

    def test_configuration(self):
        """Returns the current TestConfiguration for the port."""
        if not self._test_configuration:
            self._test_configuration = TestConfiguration(self._version, self._architecture, self._options.configuration.lower())
        return self._test_configuration

    # FIXME: Belongs on a Platform object.
    @memoized
    def all_test_configurations(self):
        """Returns a list of TestConfiguration instances, representing all available
        test configurations for this port.
        """
        return self._generate_all_test_configurations()

    # FIXME: Belongs on a Platform object.
    def configuration_specifier_macros(self):
        """Ports may provide a way to abbreviate configuration specifiers to conveniently
        refer to them as one term or alias specific values to more generic ones. For example:

        (vista, win7) -> win # Abbreviate all Windows versions into one namesake.
        (precise, trusty) -> linux  # Change specific name of Linux distro to a more generic term.

        Returns a dictionary, each key representing a macro term ('win', for example),
        and value being a list of valid configuration specifiers (such as ['vista', 'win7']).
        """
        return self.CONFIGURATION_SPECIFIER_MACROS

    def _generate_all_test_configurations(self):
        """Returns a sequence of the TestConfigurations the port supports."""
        # By default, we assume we want to test every graphics type in
        # every configuration on every system.
        test_configurations = []
        for version, architecture in self.ALL_SYSTEMS:
            for build_type in self.ALL_BUILD_TYPES:
                test_configurations.append(TestConfiguration(version, architecture, build_type))
        return test_configurations

    def _flag_specific_expectations_path(self):
        flag = self.primary_driver_flag()
        if flag:
            return self._filesystem.join(
                self.layout_tests_dir(), self.FLAG_EXPECTATIONS_PREFIX, flag.lstrip('-'))

    def _flag_specific_baseline_search_path(self):
        flag = self.primary_driver_flag()
        if not flag:
            return []
        flag_dir = self._filesystem.join(
            self.layout_tests_dir(), 'flag-specific', flag.lstrip('-'))
        platform_dirs = [
            self._filesystem.join(flag_dir, 'platform', platform_dir)
            for platform_dir in self.FALLBACK_PATHS[self.version()]]
        return platform_dirs + [flag_dir]

    def expectations_dict(self):
        """Returns an OrderedDict of name -> expectations strings.

        The names are expected to be (but not required to be) paths in the
        filesystem. If the name is a path, the file can be considered updatable
        for things like rebaselining, so don't use names that are paths if
        they're not paths.

        Generally speaking the ordering should be files in the filesystem in
        cascade order (TestExpectations followed by Skipped, if the port honors
        both formats), then any built-in expectations (e.g., from compile-time
        exclusions), then --additional-expectations options.
        """
        # FIXME: rename this to test_expectations() once all the callers are
        # updated to know about the ordered dict.
        expectations = collections.OrderedDict()

        for path in self.expectations_files():
            if self._filesystem.exists(path):
                expectations[path] = self._filesystem.read_text_file(path)

        for path in self.get_option('additional_expectations', []):
            expanded_path = self._filesystem.expanduser(path)
            if self._filesystem.exists(expanded_path):
                _log.debug("reading additional_expectations from path '%s'", path)
                expectations[path] = self._filesystem.read_text_file(expanded_path)
            else:
                _log.warning("additional_expectations path '%s' does not exist", path)
        return expectations

    def all_expectations_dict(self):
        """Returns an OrderedDict of name -> expectations strings."""
        expectations = self.expectations_dict()

        flag_path = self._filesystem.join(self.layout_tests_dir(), 'FlagExpectations')
        if not self._filesystem.exists(flag_path):
            return expectations

        for (_, _, filenames) in self._filesystem.walk(flag_path):
            if 'README.txt' in filenames:
                filenames.remove('README.txt')
            if 'PRESUBMIT.py' in filenames:
                filenames.remove('PRESUBMIT.py')
            for filename in filenames:
                path = self._filesystem.join(flag_path, filename)
                try:
                    expectations[path] = self._filesystem.read_text_file(path)
                except UnicodeDecodeError:
                    _log.error('Failed to read expectations file: \'%s\'', path)
                    raise

        return expectations

    def bot_expectations(self):
        if not self.get_option('ignore_flaky_tests'):
            return {}

        full_port_name = self.determine_full_port_name(self.host, self._options, self.port_name)
        builder_category = self.get_option('ignore_builder_category', 'layout')
        factory = BotTestExpectationsFactory(self.host.builders)
        # FIXME: This only grabs release builder's flakiness data. If we're running debug,
        # when we should grab the debug builder's data.
        expectations = factory.expectations_for_port(full_port_name, builder_category)

        if not expectations:
            return {}

        ignore_mode = self.get_option('ignore_flaky_tests')
        if ignore_mode == 'very-flaky' or ignore_mode == 'maybe-flaky':
            return expectations.flakes_by_path(ignore_mode == 'very-flaky')
        if ignore_mode == 'unexpected':
            return expectations.unexpected_results_by_path()
        _log.warning("Unexpected ignore mode: '%s'.", ignore_mode)
        return {}

    def expectations_files(self):
        """Returns a list of paths to expectations files that apply by default.

        There are other "test expectations" files that may be applied if
        the --additional-expectations flag is passed; those aren't included
        here.
        """
        return filter(None, [
            self.path_to_generic_test_expectations_file(),
            self._filesystem.join(self.layout_tests_dir(), 'NeverFixTests'),
            self._filesystem.join(self.layout_tests_dir(), 'StaleTestExpectations'),
            self._filesystem.join(self.layout_tests_dir(), 'SlowTests'),
            self._flag_specific_expectations_path()
        ])

    def extra_expectations_files(self):
        """Returns a list of paths to test expectations not loaded by default.

        These paths are passed via --additional-expectations on some builders.
        """
        return [
            self._filesystem.join(self.layout_tests_dir(), 'ASANExpectations'),
            self._filesystem.join(self.layout_tests_dir(), 'LeakExpectations'),
            self._filesystem.join(self.layout_tests_dir(), 'MSANExpectations'),
        ]

    @memoized
    def path_to_generic_test_expectations_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'TestExpectations')

    def repository_path(self):
        """Returns the repository path for the chromium code base."""
        return self._path_from_chromium_base('build')

    def default_configuration(self):
        return 'Release'

    def clobber_old_port_specific_results(self):
        pass

    # FIXME: This does not belong on the port object.
    @memoized
    def path_to_apache(self):
        """Returns the full path to the apache binary.

        This is needed only by ports that use the apache_http_server module.
        """
        raise NotImplementedError('Port.path_to_apache')

    def path_to_apache_config_file(self):
        """Returns the full path to the apache configuration file.

        If the WEBKIT_HTTP_SERVER_CONF_PATH environment variable is set, its
        contents will be used instead.

        This is needed only by ports that use the apache_http_server module.
        """
        config_file_from_env = self.host.environ.get('WEBKIT_HTTP_SERVER_CONF_PATH')
        if config_file_from_env:
            if not self._filesystem.exists(config_file_from_env):
                raise IOError('%s was not found on the system' % config_file_from_env)
            return config_file_from_env

        config_file_name = self._apache_config_file_name_for_platform()
        return self._filesystem.join(self.apache_config_directory(), config_file_name)

    def _apache_version(self):
        config = self._executive.run_command([self.path_to_apache(), '-v'])
        # Log version including patch level.
        _log.debug('Found apache version %s', re.sub(
            r'(?:.|\n)*Server version: Apache/(\d+\.\d+(?:\.\d+)?)(?:.|\n)*',
            r'\1', config))
        return re.sub(
            r'(?:.|\n)*Server version: Apache/(\d+\.\d+)(?:.|\n)*',
            r'\1', config)

    def _apache_config_file_name_for_platform(self):
        if self.host.platform.is_linux():
            distribution = self.host.platform.linux_distribution()

            custom_configurations = ['arch', 'debian', 'fedora', 'redhat']
            if distribution in custom_configurations:
                return '%s-httpd-%s.conf' % (distribution, self._apache_version())

        return 'apache2-httpd-' + self._apache_version() + '.conf'

    def _path_to_driver(self, target=None):
        """Returns the full path to the test driver."""
        return self._build_path(target, self.driver_name())

    def _path_to_image_diff(self):
        """Returns the full path to the image_diff binary, or None if it is not available.

        This is likely used only by diff_image()
        """
        return self._build_path('image_diff')

    def _absolute_baseline_path(self, platform_dir):
        """Return the absolute path to the top of the baseline tree for a
        given platform directory.
        """
        return self._filesystem.join(self.layout_tests_dir(), 'platform', platform_dir)

    def _driver_class(self):
        """Returns the port's driver implementation."""
        return driver.Driver

    def output_contains_sanitizer_messages(self, output):
        if not output:
            return None
        if 'AddressSanitizer' in output:
            return 'AddressSanitizer'
        if 'MemorySanitizer' in output:
            return 'MemorySanitizer'
        return None

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than):
        if self.output_contains_sanitizer_messages(stderr):
            # Running the symbolizer script can take a lot of memory, so we need to
            # serialize access to it across all the concurrently running drivers.

            llvm_symbolizer_path = self._path_from_chromium_base(
                'third_party', 'llvm-build', 'Release+Asserts', 'bin', 'llvm-symbolizer')
            if self._filesystem.exists(llvm_symbolizer_path):
                env = self.host.environ.copy()
                env['LLVM_SYMBOLIZER_PATH'] = llvm_symbolizer_path
            else:
                env = None
            sanitizer_filter_path = self._path_from_chromium_base(
                'tools', 'valgrind', 'asan', 'asan_symbolize.py')
            sanitizer_strip_path_prefix = 'Release/../../'
            if self._filesystem.exists(sanitizer_filter_path):
                stderr = self._executive.run_command(
                    ['flock', sys.executable, sanitizer_filter_path, sanitizer_strip_path_prefix],
                    input=stderr, decode_output=False, env=env)

        name_str = name or '<unknown process name>'
        pid_str = str(pid or '<unknown>')

        # We require stdout and stderr to be bytestrings, not character strings.
        if stdout:
            assert isinstance(stdout, basestring)
            stdout_lines = stdout.decode('utf8', 'replace').splitlines()
        else:
            stdout_lines = [u'<empty>']
        if stderr:
            assert isinstance(stderr, basestring)
            stderr_lines = stderr.decode('utf8', 'replace').splitlines()
        else:
            stderr_lines = [u'<empty>']

        return (stderr,
                'crash log for %s (pid %s):\n%s\n%s\n' % (name_str, pid_str,
                                                          '\n'.join(('STDOUT: ' + l) for l in stdout_lines),
                                                          '\n'.join(('STDERR: ' + l) for l in stderr_lines)),
                self._get_crash_site(stderr_lines))

    def _get_crash_site(self, stderr_lines):
        # [blah:blah:blah:FATAL:
        prefix_re = r'\[[\w:/.]*FATAL:'
        # crash_file.ext(line)
        site_re = r'(?P<site>[\w_]*\.[\w_]*\(\d*\))'
        # ] blah failed
        suffix_re = r'\]\s*(Check failed|Security DCHECK failed)'
        pattern = re.compile(prefix_re + site_re + suffix_re)
        for line in stderr_lines:
            match = pattern.search(line)
            if match:
                return match.group('site')
        return None

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        pass

    def look_for_new_samples(self, unresponsive_processes, start_time):
        pass

    def sample_process(self, name, pid):
        pass

    def physical_test_suites(self):
        return [
            # For example, to turn on force-compositing-mode in the svg/ directory:
            # PhysicalTestSuite('svg', ['--force-compositing-mode']),
            PhysicalTestSuite('fast/text', ['--enable-direct-write', '--enable-font-antialiasing']),
            PhysicalTestSuite('hdr', ['--force-color-profile=scrgb-linear']),
        ]

    def virtual_test_suites(self):
        if self._virtual_test_suites is None:
            path_to_virtual_test_suites = self._filesystem.join(self.layout_tests_dir(), 'VirtualTestSuites')
            assert self._filesystem.exists(path_to_virtual_test_suites), 'LayoutTests/VirtualTestSuites not found'
            try:
                test_suite_json = json.loads(self._filesystem.read_text_file(path_to_virtual_test_suites))
                self._virtual_test_suites = []
                for json_config in test_suite_json:
                    vts = VirtualTestSuite(**json_config)
                    if vts in self._virtual_test_suites:
                        raise ValueError('LayoutTests/VirtualTestSuites contains duplicate definition: %r' % json_config)
                    self._virtual_test_suites.append(vts)
            except ValueError as error:
                raise ValueError('LayoutTests/VirtualTestSuites is not a valid JSON file: %s' % error)
        return self._virtual_test_suites

    def _all_virtual_tests(self, suites):
        tests = []
        for suite in suites:
            self._populate_virtual_suite(suite)
            tests.extend(suite.tests.keys())
        return tests

    def _virtual_tests_matching_paths(self, paths, suites):
        tests = []
        for suite in suites:
            if any(p.startswith(suite.name) for p in paths) or any(suite.name.startswith(os.path.join(p, '')) for p in paths):
                self._populate_virtual_suite(suite)
            for test in suite.tests:
                if any(test.startswith(p) for p in paths):
                    tests.append(test)

        if any(self._path_has_wildcard(path) for path in paths):
            _log.warning('WARNING: Wildcards in paths are not supported for virtual test suites.')

        return tests

    def _path_has_wildcard(self, path):
        return '*' in path

    def _wpt_test_urls_matching_paths(self, paths):
        tests = []
        # '/' is used throughout this function instead of filesystem.sep as the WPT manifest always
        # uses '/' for paths (it is not OS dependent).
        if self._filesystem.sep != '/':
            paths = [path.replace(self._filesystem.sep, '/') for path in paths]

        for test_url_path in self._wpt_manifest().all_urls():
            if test_url_path[0] == '/':
                test_url_path = test_url_path[1:]

            full_test_url_path = 'external/wpt/' + test_url_path

            for path in paths:
                if 'external' not in path:
                    continue

                wpt_path = path.replace('external/wpt/', '')

                # When `test_url_path` is test.any.html or test.any.worker.html and path is test.any.js
                matches_any_js_test = (
                    self._wpt_manifest().is_test_file(wpt_path)
                    and test_url_path.startswith(re.sub(r'\.js$', '', wpt_path))
                )

                # Get a list of directories for both paths, filter empty strings
                full_test_url_directories = filter(None, full_test_url_path.split('/'))
                path_directories = filter(None, path.split('/'))

                # For all other path matches within WPT
                if matches_any_js_test or path_directories == full_test_url_directories[0:len(path_directories)]:
                    wpt_file_paths = self._convert_wpt_file_path_to_url_paths(test_url_path)
                    tests.extend('external/wpt/' + wpt_file_path for wpt_file_path in wpt_file_paths)

        return tests

    def _populate_virtual_suite(self, suite):
        if not suite.tests:
            base_tests = self.real_tests([suite.base])
            base_tests.extend(self._wpt_test_urls_matching_paths([suite.base]))
            suite.tests = {}
            for test in base_tests:
                suite.tests[test.replace(suite.base, suite.name, 1)] = test

    def is_virtual_test(self, test_name):
        return bool(self.lookup_virtual_suite(test_name))

    def lookup_virtual_suite(self, test_name):
        for suite in self.virtual_test_suites():
            if test_name.startswith(suite.name):
                return suite
        return None

    def lookup_virtual_test_base(self, test_name):
        suite = self.lookup_virtual_suite(test_name)
        if not suite:
            return None
        return test_name.replace(suite.name, suite.base, 1)

    def lookup_virtual_test_args(self, test_name):
        for suite in self.virtual_test_suites():
            if test_name.startswith(suite.name):
                return suite.args
        return []

    def lookup_virtual_reference_args(self, test_name):
        for suite in self.virtual_test_suites():
            if test_name.startswith(suite.name):
                return suite.reference_args
        return []

    def lookup_physical_test_args(self, test_name):
        for suite in self.physical_test_suites():
            if test_name.startswith(suite.name):
                return suite.args
        return []

    def lookup_physical_reference_args(self, test_name):
        for suite in self.physical_test_suites():
            if test_name.startswith(suite.name):
                return suite.reference_args
        return []

    def should_run_as_pixel_test(self, test_name):
        """Whether a test should run as pixel test (when there is no reference).

        This provides the *default* value for whether a test should run as
        pixel test. When reference files exist (checked by layout_test_runner
        before calling this method), the test always runs as pixel test.
        """
        if not self._options.pixel_tests:
            return False
        # WPT should not run as pixel test by default, except reftests
        # (for which reference files would exist).
        return not self.should_run_in_wpt_mode(test_name)

    def should_run_pixel_test_first(self, test_name):
        """Returns true if the directory of the test (or the base test if the
           test is virtual) is listed in _options.image_first_tests (which comes
           from LayoutTests/ImageFirstTests or the command line).
        """
        if any(test_name.startswith(directory) for directory in self._options.image_first_tests):
            return True
        base = self.lookup_virtual_test_base(test_name)
        return base and self.should_run_pixel_test_first(base)

    def _build_path(self, *comps):
        """Returns a path from the build directory."""
        return self._build_path_with_target(self._options.target, *comps)

    def _build_path_with_target(self, target, *comps):
        target = target or self.get_option('target')
        return self._filesystem.join(
            self._path_from_chromium_base(),
            self.get_option('build_directory') or 'out',
            target, *comps)

    def _check_driver_build_up_to_date(self, target):
        # FIXME: We should probably get rid of this check altogether as it has
        # outlived its usefulness in a GN-based world, but for the moment we
        # will just check things if they are using the standard Debug or Release
        # target directories.
        if target not in ('Debug', 'Release'):
            return True

        try:
            debug_path = self._path_to_driver('Debug')
            release_path = self._path_to_driver('Release')

            debug_mtime = self._filesystem.mtime(debug_path)
            release_mtime = self._filesystem.mtime(release_path)

            if (debug_mtime > release_mtime and target == 'Release' or
                    release_mtime > debug_mtime and target == 'Debug'):
                most_recent_binary = 'Release' if target == 'Debug' else 'Debug'
                _log.warning('You are running the %s binary. However the %s binary appears to be more recent. '
                             'Please pass --%s.', target, most_recent_binary, most_recent_binary.lower())
                _log.warning('')
        # This will fail if we don't have both a debug and release binary.
        # That's fine because, in this case, we must already be running the
        # most up-to-date one.
        except OSError:
            pass
        return True

    def _get_font_files(self):
        """Returns list of font files that should be used by the test."""
        # TODO(sergeyu): Currently FONT_FILES is valid only on Linux. Make it
        # usable on other platforms if necessary.
        result = []
        for (font_dirs, font_file, package) in FONT_FILES:
            exists = False
            for font_dir in font_dirs:
                font_path = os.path.join(font_dir, font_file)
                if not os.path.isabs(font_path):
                    font_path = self._build_path(font_path)
                if self._check_file_exists(font_path, '', more_logging=False):
                    result.append(font_path)
                    exists = True
                    break
            if not exists:
                message = 'You are missing %s under %s.' % (font_file, font_dirs)
                if package:
                    message += ' Try installing %s. See build instructions.' % package

                _log.error(message)
                raise TestRunException(exit_codes.SYS_DEPS_EXIT_STATUS, message)
        return result



class VirtualTestSuite(object):

    def __init__(self, prefix=None, base=None, args=None, references_use_default_args=False):
        assert base
        assert args
        assert '/' not in prefix, "Virtual test suites prefixes cannot contain /'s: %s" % prefix
        self.name = 'virtual/' + prefix + '/' + base
        self.base = base
        self.args = args
        self.reference_args = [] if references_use_default_args else args
        self.tests = {}

    def __repr__(self):
        return "VirtualTestSuite('%s', '%s', %s, %s)" % (self.name, self.base, self.args, self.reference_args)

    def __eq__(self, other):
        return (
            self.name == other.name and
            self.base == other.base and
            self.args == other.args and
            self.reference_args == other.reference_args)


class PhysicalTestSuite(object):

    def __init__(self, base, args, reference_args=None):
        self.name = base
        self.base = base
        self.args = args
        self.reference_args = args if reference_args is None else reference_args
        self.tests = set()

    def __repr__(self):
        return "PhysicalTestSuite('%s', '%s', %s, %s)" % (self.name, self.base, self.args, self.reference_args)
