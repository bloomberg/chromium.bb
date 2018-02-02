# Copyright (C) 2018 Google Inc. All rights reserved.
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
#     * Neither the name of Google Inc. nor the names of its
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

import logging
import os
import select
import socket
import subprocess
import sys
import threading

from webkitpy.common import exit_codes
from webkitpy.layout_tests.port import base
from webkitpy.layout_tests.port import driver
from webkitpy.layout_tests.port import factory
from webkitpy.layout_tests.port import linux
from webkitpy.layout_tests.port import server_process


# Modules loaded dynamically in _import_fuchsia_runner().
# pylint: disable=invalid-name
fuchsia_target = None
qemu_target = None
# pylint: enable=invalid-name


# Imports Fuchsia runner modules. This is done dynamically only when FuchsiaPort
# is instantiated to avoid dependency on Fuchsia runner on other platforms.
def _import_fuchsia_runner():
    sys.path.append(os.path.abspath(os.path.join(
        os.path.dirname(__file__), *(['..'] * 7 + ['build/fuchsia']))))

    # pylint: disable=import-error
    # pylint: disable=invalid-name
    # pylint: disable=redefined-outer-name
    global fuchsia_target
    import runner_v2.target as fuchsia_target
    global qemu_target
    import runner_v2.qemu_target as qemu_target
    # pylint: enable=import-error
    # pylint: enable=invalid-name
    # pylint: disable=redefined-outer-name


# HTTP path prefix for the HTTP server.
PERF_TEST_PATH_PREFIX = '/all-perf-tests'
LAYOUT_TEST_PATH_PREFIX = '/all-tests'

# Paths to the directory where the fonts are copied to. Must match the path in
# content/shell/app/blink_test_platform_support_fuchsia.cc .
FONTS_DEVICE_PATH = '/system/fonts'

# Number of content_shell instances to run in parallel.
MAX_WORKERS = 8

PROCESS_START_TIMEOUT = 20


_log = logging.getLogger(__name__)


def _subprocess_log_thread(pipe, prefix):
    try:
        while True:
            line = pipe.readline()
            if not line:
                return
            _log.error('%s: %s', prefix, line)
    finally:
        pipe.close()


class SubprocessOutputLogger(object):
    def __init__(self, process, prefix):
        self._process = process
        self._thread = threading.Thread(
            target=_subprocess_log_thread,
            args=(process.stdout, prefix))
        self._thread.daemon = True
        self._thread.start()

    def __del__(self):
        self.close()

    def close(self):
        self._process.kill()


class _TargetHost(object):
    def __init__(self, build_path, ports_to_forward, fonts):
        try:
            self._target = None
            self._target = qemu_target.QemuTarget(
                build_path, 'x64', ram_size_mb=8192)
            self._target.Start()
            self._setup_target(build_path, ports_to_forward, fonts)
        except:
            self.cleanup()
            raise

    def _setup_target(self, build_path, ports_to_forward, fonts):
        # Run a proxy to forward all server ports from the Fuchsia device to
        # the host.
        # TODO(sergeyu): Potentially this can be implemented using port
        # forwarding in SSH, but that feature is currently broken on Fuchsia,
        # see ZX-1555. Remove layout_test_proxy once that SSH bug is fixed.
        self._target.PutFile(
            os.path.join(build_path, 'package/layout_test_proxy.far'),
            '/tmp')
        command = ['run', '/tmp/layout_test_proxy.far',
                   '--remote-address=' + qemu_target.HOST_IP_ADDRESS,
                   '--ports=' + ','.join([str(p) for p in ports_to_forward])]
        self._proxy = self._target.RunCommandPiped(command,
                                                   stdin=subprocess.PIPE,
                                                   stdout=subprocess.PIPE)

        # Copy content_shell package to the device.
        self._target.PutFile(
            os.path.join(build_path, 'package/content_shell.far'), '/tmp')

        # Currently dynamic library loading is not implemented for packaged
        # apps. Copy libosmesa.so to /system/lib as a workaround.
        self._target.PutFile(
            os.path.join(build_path, 'libosmesa.so'), '/system/lib')

        # Copy fonts.
        self._target.RunCommand(['mkdir', '/system/fonts'])

        self._target.PutFile(
            os.path.join(build_path,
                         '../../content/shell/test_runner/resources/fonts',
                         'android_main_fonts.xml'),
            FONTS_DEVICE_PATH + '/fonts.xml')

        self._target.PutFile(
            os.path.join(build_path,
                         '../../content/shell/test_runner/resources/fonts',
                         'android_fallback_fonts.xml'),
            FONTS_DEVICE_PATH + '/fonts_fallback.xml')

        self._target.PutFiles(fonts, FONTS_DEVICE_PATH)

    def run_command(self, *args, **kvargs):
        return self._target.RunCommandPiped(*args,
                                            stdin=subprocess.PIPE,
                                            stdout=subprocess.PIPE,
                                            stderr=subprocess.PIPE,
                                            **kvargs)
    def cleanup(self):
        if self._target:
            # TODO(sergeyu): Currently __init__() always starts Qemu, so we can
            # just shutdown it. Update this logic when reusing target devices
            # for multiple test runs.
            self._target.Shutdown()
            self._target = None


class FuchsiaPort(base.Port):
    port_name = 'fuchsia'

    SUPPORTED_VERSIONS = ('sdk',)

    FALLBACK_PATHS = {'sdk': ['fuchsia'] + linux.LinuxPort.latest_platform_fallback_path()}

    def __init__(self, host, port_name, **kwargs):
        _import_fuchsia_runner()
        super(FuchsiaPort, self).__init__(host, port_name, **kwargs)

        self._operating_system = 'fuchsia'
        self._version = 'sdk'

        # TODO(sergeyu): Add support for arm64.
        self._architecture = 'x86_64'

        self.server_process_constructor = FuchsiaServerProcess

        # Used to implement methods that depend on the host platform.
        self._host_port = factory.PortFactory(host).get(**kwargs)

        self._target_host = self.get_option('fuchsia_target')
        self._zircon_logger = None

    def _driver_class(self):
        return ChromiumFuchsiaDriver

    def _path_to_driver(self, target=None):
        return self._build_path_with_target(target, 'package/content_shell.far')

    def __del__(self):
        if self._zircon_logger:
            self._zircon_logger.close()

    def setup_test_run(self):
        super(FuchsiaPort, self).setup_test_run()
        try:
            self._target_host = _TargetHost(
                self._build_path(), self.SERVER_PORTS, self._get_font_files())

            if self.get_option('zircon_logging'):
                self._zircon_logger = SubprocessOutputLogger(
                    self._target_host.run_command(['dlog', '-f']),
                    'Zircon')

            # Save fuchsia_target in _options, so it can be shared with other
            # workers.
            self._options.fuchsia_target = self._target_host

        except fuchsia_target.FuchsiaTargetException as e:
            _log.error('Failed to start qemu: %s.', str(e))
            return exit_codes.NO_DEVICES_EXIT_STATUS

    def cleanup_test_run(self):
        if self._target_host:
            self._target_host.cleanup()
            self._target_host = None

    def num_workers(self, requested_num_workers):
        # Run a single qemu instance.
        return min(MAX_WORKERS, requested_num_workers)

    def check_sys_deps(self, needs_http):
        # _get_font_files() will throw if any of the required fonts is missing.
        self._get_font_files()
        # TODO(sergeyu): Implement this.
        return exit_codes.OK_EXIT_STATUS

    def requires_http_server(self):
        """HTTP server is always required to avoid copying the tests to the VM.
        """
        return True

    def start_http_server(self, additional_dirs, number_of_drivers):
        additional_dirs[PERF_TEST_PATH_PREFIX] = self._perf_tests_dir()
        additional_dirs[LAYOUT_TEST_PATH_PREFIX] = self.layout_tests_dir()
        super(FuchsiaPort, self).start_http_server(
            additional_dirs, number_of_drivers)

    def path_to_apache(self):
        return self._host_port.path_to_apache()

    def path_to_apache_config_file(self):
        return self._host_port.path_to_apache_config_file()

    def default_smoke_test_only(self):
        return True

    def get_target_host(self):
        return self._target_host


class ChromiumFuchsiaDriver(driver.Driver):
    def __init__(self, port, worker_number, pixel_tests, no_timeout=False):
        super(ChromiumFuchsiaDriver, self).__init__(
            port, worker_number, pixel_tests, no_timeout)

    def _base_cmd_line(self):
        return ['run', '/tmp/content_shell.far']

    def _command_from_driver_input(self, driver_input):
        command = super(ChromiumFuchsiaDriver, self)._command_from_driver_input(
            driver_input)
        if command.startswith('/'):
            relative_test_filename = \
                os.path.relpath(command, self._port.layout_tests_dir())
            command = 'http://127.0.0.1:8000/all-tests/' + \
                relative_test_filename
        return command


# Custom version of ServerProcess that runs processes on a remote device.
class FuchsiaServerProcess(server_process.ServerProcess):
    def __init__(self, port_obj, name, cmd, env=None,
                 treat_no_data_as_crash=False, more_logging=False):
        super(FuchsiaServerProcess, self).__init__(
            port_obj, name, cmd, env, treat_no_data_as_crash, more_logging)

    def _start(self):
        if self._proc:
            raise ValueError('%s already running' % self._name)
        self._reset()

        # Fuchsia doesn't support stdin stream for packaged applications, so the
        # stdin stream for content_shell is routed through a separate TCP
        # socket. Open a local socket and then pass the address with the port as
        # --stdin-redirect parameter. content_shell will connect to this address
        # and will use that connection as its stdin stream.
        listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_socket.bind(('127.0.0.1', 0))
        listen_socket.listen(1)
        stdin_port = listen_socket.getsockname()[1]

        command = ['%s=%s' % (k, v) for k, v in self._env.items()] + \
            self._cmd + \
            ['--stdin-redirect=%s:%s' %
             (qemu_target.HOST_IP_ADDRESS, stdin_port)]
        proc = self._port.get_target_host().run_command(command)
        # Wait for incoming connection from content_shell.
        fd = listen_socket.fileno()
        read_fds, _, _ = select.select([fd], [], [], PROCESS_START_TIMEOUT)
        if fd not in read_fds:
            listen_socket.close()
            proc.kill()
            raise driver.DeviceFailure(
                'Timed out waiting connection from content_shell.')

        # Python's interfaces for sockets and pipes are different. To masquerade
        # the socket as a pipe dup() the file descriptor and pass it to
        # os.fdopen().
        stdin_socket, _ = listen_socket.accept()
        fd = stdin_socket.fileno()  # pylint: disable=no-member
        stdin_pipe = os.fdopen(os.dup(fd), "w", 0)
        stdin_socket.close()

        proc.stdin.close()
        proc.stdin = stdin_pipe

        self._set_proc(proc)
