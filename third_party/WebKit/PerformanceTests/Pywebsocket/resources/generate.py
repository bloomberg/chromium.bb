# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generator script for PerformanceTests/Pywebsocket/. Run
$ python resources/generate.py
at PerformanceTests/Pywebsocket/, and commit the generated files.'''

import sys
import os


def generate(connection_type,
             benchmark_name,
             data_type,
             is_worker,
             is_async,
             does_verify):
    pywebsocket_server = 'http://localhost:8001'

    test_file_name = ('%s-%s%s-%s-%s-%s.html' %
                      (connection_type,
                       benchmark_name,
                       '' if data_type == '' else '-' + data_type,
                       'worker' if is_worker else 'window',
                       'async' if is_async else 'sync',
                       'verify' if does_verify else 'noverify'))

    test_file_content_template = '''<!DOCTYPE html>
<head>
<script src="../resources/runner.js"></script>
<script src="{pywebsocket_server_root}/util.js"></script>
<script src="resources/util_performance_test.js"></script>
<script src="{pywebsocket_server_root}/{script_name}"></script>
</head>
<body onload="startPerformanceTest(
    '{connection_type}',
    '{benchmark_name}Benchmark',
    '{data_type}',
    {is_worker},
    {is_async},
    {does_verify})">
</body>
</html>
'''

    if connection_type == 'WebSocket':
        script_name = 'benchmark.js'
    elif connection_type == 'XHR':
        script_name = 'xhr_benchmark.js'
    else:
        script_name = 'fetch_benchmark.js'

    test_file_content = test_file_content_template.format(
        pywebsocket_server_root=pywebsocket_server,
        script_name=script_name,
        connection_type=connection_type,
        benchmark_name=benchmark_name,
        data_type=data_type,
        is_worker='true' if is_worker else 'false',
        is_async='true' if is_async else 'false',
        does_verify='true' if does_verify else 'false')

    with open(test_file_name, 'w') as f:
        f.write(test_file_content)


def main():
    for is_worker in [True, False]:
        for benchmark_name in ['send', 'receive']:
            if benchmark_name != 'send':
                # Disable WebSocket-send tests because it is very slow
                # without a native module in pywebsocket.
                generate('WebSocket', benchmark_name, '',
                         is_worker, is_async=True, does_verify=True)

            for data_type in ['arraybuffer', 'blob', 'text']:
                generate('XHR', benchmark_name, data_type,
                         is_worker, is_async=True, does_verify=True)
                generate('fetch', benchmark_name, data_type,
                         is_worker, is_async=True, does_verify=True)

            if benchmark_name == 'receive' and not is_worker:
                # Disable XHR-receive-*-window-sync tests.
                continue

            for data_type in ['arraybuffer', 'blob', 'text']:
                generate('XHR', benchmark_name, data_type,
                         is_worker, is_async=False, does_verify=True)

if __name__ == "__main__":
    sys.exit(main())
