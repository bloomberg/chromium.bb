# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generator script for Web Bluetooth LayoutTests.

For each script-tests/X.js creates the following test files depending on the
contents of X.js
- getPrimaryService/X.html
- getPrimaryServices/X.html
- getPrimaryServices/X-with-uuid.html

script-tests/X.js files should contain "CALLS([variation1 | variation2 | ...])"
tokens that indicate what files to generate. Each variation in CALLS([...])
should corresponds to a js function call and its arguments. Additionally a
variation can end in [UUID] to indicate that the generated file's name should
have the -with-uuid suffix.

The PREVIOUS_CALL token will be replaced with the function that replaced CALLS.

The FUNCTION_NAME token will be replaced with the name of the function that
replaced CALLS.

For example, for the following template file:

// script-tests/example.js
promise_test(() => {
    return navigator.bluetooth.requestDevice(...)
        .then(device => device.gatt.CALLS([
            getPrimaryService('heart_rate')|
            getPrimaryServices('heart_rate')[UUID]]))
        .then(device => device.gatt.PREVIOUS_CALL);
}, 'example test for FUNCTION_NAME');

this script will generate:

// getPrimaryService/example.html
promise_test(() => {
    return navigator.bluetooth.requestDevice(...)
        .then(device => device.gatt.getPrimaryService('heart_rate'))
        .then(device => device.gatt.getPrimaryService('heart_rate'));
}, 'example test for getPrimaryService');

// getPrimaryServices/example-with-uuid.html
promise_test(() => {
    return navigator.bluetooth.requestDevice(...)
        .then(device => device.gatt.getPrimaryServices('heart_rate'))
        .then(device => device.gatt.getPrimaryServices('heart_rate'));
}, 'example test for getPrimaryServices');

Run
$ python //third_party/WebKit/LayoutTests/bluetooth/generate.py
and commit the generated files.
"""
import os
import re
import sys

TEMPLATES_DIR = 'script-tests'


class GeneratedTest:

    def __init__(self, data, path, template):
        self.data = data
        self.path = path
        self.template = template


def GetGeneratedTests():
    """Yields a GeneratedTest for each call in templates in script-tests."""
    bluetooth_tests_dir = os.path.dirname(os.path.realpath(__file__))

    # Read Base Test Template.
    base_template_file_handle = open(
        os.path.join(bluetooth_tests_dir, TEMPLATES_DIR, 'base_test_template.html'))
    base_template_file_data = base_template_file_handle.read().decode('utf-8')
    base_template_file_handle.close()

    # Get Templates.

    template_path = os.path.join(bluetooth_tests_dir, TEMPLATES_DIR)

    available_templates = []
    for root, _, files in os.walk(template_path):
        for template in files:
            if template.endswith('.js'):
                available_templates.append(os.path.join(root, template))

    # Generate Test Files
    for template in available_templates:
        # Read template
        template_file_handle = open(template)
        template_file_data = template_file_handle.read().decode('utf-8')
        template_file_handle.close()

        template_name = os.path.splitext(os.path.basename(template))[0]

        # Find function names in multiline pattern: CALLS( [ function_name,function_name2[UUID] ])
        result = re.search(
            r'CALLS\(' + # CALLS(
            r'[^\[]*' +  # Any characters not [, allowing for new lines.
            r'\[' +      # [
            r'(.*?)' +   # group matching: function_name(), function_name2[UUID]
            r'\]\)',     # adjacent closing characters: ])
            template_file_data, re.MULTILINE | re.DOTALL)

        if result is None:
            raise Exception('Template must contain \'CALLS\' tokens')

        new_test_file_data = base_template_file_data.replace('TEST',
            template_file_data)
        # Replace CALLS([...]) with CALLS so that we don't have to replace the
        # CALLS([...]) for every new test file.
        new_test_file_data = new_test_file_data.replace(result.group(), 'CALLS')

        # Replace 'PREVIOUS_CALL' with 'CALLS' so that we can replace it while
        # replacing CALLS.
        new_test_file_data = new_test_file_data.replace('PREVIOUS_CALL', 'CALLS')

        calls = result.group(1)
        calls = ''.join(calls.split())    # Removes whitespace.
        calls = calls.split('|')

        for call in calls:
            # Parse call
            function_name, args, uuid_suffix = re.search(r'(.*?)\((.*?)\)(\[UUID\])?', call).groups()

            # Replace template tokens
            call_test_file_data = new_test_file_data
            call_test_file_data = call_test_file_data.replace('CALLS', '{}({})'.format(function_name, args))
            call_test_file_data = call_test_file_data.replace('FUNCTION_NAME', function_name)

            # Get test file name
            group_dir = os.path.basename(os.path.abspath(os.path.join(template, os.pardir)))

            call_test_file_name = 'gen-{}{}.html'.format(template_name, '-with-uuid' if uuid_suffix else '')
            call_test_file_path = os.path.join(bluetooth_tests_dir, group_dir, function_name, call_test_file_name)

            yield GeneratedTest(call_test_file_data, call_test_file_path, template)


def main():

    for generated_test in GetGeneratedTests():
        # Create or open test file
        test_file_handle = open(generated_test.path, 'wb')

        # Write contents
        test_file_handle.write(generated_test.data.encode('utf-8'))
        test_file_handle.close()


if __name__ == '__main__':
    sys.exit(main())
