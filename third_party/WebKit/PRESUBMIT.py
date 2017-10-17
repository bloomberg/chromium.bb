# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import os
import re
import sys


_EXCLUDED_PATHS = (
    # This directory is created and updated via a script.
    r'^third_party[\\\/]WebKit[\\\/]Tools[\\\/]Scripts[\\\/]webkitpy[\\\/]thirdparty[\\\/]wpt[\\\/]wpt[\\\/].*',
)


def _CheckForNonBlinkVariantMojomIncludes(input_api, output_api):
    def source_file_filter(path):
        return input_api.FilterSourceFile(path,
                                          black_list=[r'third_party/WebKit/common/'])

    pattern = input_api.re.compile(r'#include\s+.+\.mojom(.*)\.h[>"]')
    errors = []
    for f in input_api.AffectedFiles(file_filter=source_file_filter):
        for line_num, line in f.ChangedContents():
            m = pattern.match(line)
            if m and m.group(1) != '-blink' and m.group(1) != '-shared':
                errors.append('    %s:%d %s' % (
                    f.LocalPath(), line_num, line))

    results = []
    if errors:
        results.append(output_api.PresubmitError(
            'Files that include non-Blink variant mojoms found. '
            'You must include .mojom-blink.h or .mojom-shared.h instead:',
            errors))
    return results


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    # We should figure out what license checks we actually want to use.
    license_header = r'.*'

    results = []
    results.extend(input_api.canned_checks.PanProjectChecks(
        input_api, output_api, excluded_paths=_EXCLUDED_PATHS,
        maxlen=800, license_header=license_header))
    results.extend(_CheckForNonBlinkVariantMojomIncludes(input_api, output_api))
    return results


def _CheckStyle(input_api, output_api):
    # Files that follow Chromium's coding style do not include capital letters.
    re_chromium_style_file = re.compile(r'\b[a-z_]+\.(cc|h)$')
    style_checker_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
        'Tools', 'Scripts', 'check-webkit-style')
    args = [input_api.python_executable, style_checker_path, '--diff-files']
    files = [input_api.os_path.join('..', '..', f.LocalPath())
             for f in input_api.AffectedFiles()
             # Filter out files that follow Chromium's coding style.
             if not re_chromium_style_file.search(f.LocalPath())]
    # Do not call check-webkit-style with empty affected file list if all
    # input_api.AffectedFiles got filtered.
    if not files:
        return []
    args += files
    results = []

    try:
        child = input_api.subprocess.Popen(args,
                                           stderr=input_api.subprocess.PIPE)
        _, stderrdata = child.communicate()
        if child.returncode != 0:
            results.append(output_api.PresubmitError(
                'check-webkit-style failed', [stderrdata]))
    except Exception as e:
        results.append(output_api.PresubmitNotifyResult(
            'Could not run check-webkit-style', [str(e)]))

    return results


def _CheckForPrintfDebugging(input_api, output_api):
    """Generally speaking, we'd prefer not to land patches that printf
    debug output."""
    printf_re = input_api.re.compile(r'^\s*(printf\(|fprintf\(stderr,)')
    errors = input_api.canned_checks._FindNewViolationsOfRule(
        lambda _, x: not printf_re.search(x),
        input_api, None)
    errors = ['  * %s' % violation for violation in errors]
    if errors:
        return [output_api.PresubmitPromptOrNotify(
                    'printf debugging is best debugging! That said, it might '
                    'be a good idea to drop the following occurances from '
                    'your patch before uploading:\n%s' % '\n'.join(errors))]
    return []


def _CheckForForbiddenChromiumCode(input_api, output_api):
    """Checks that Blink uses Chromium classes and namespaces only in permitted code."""
    # This list is not exhaustive, but covers likely ones.
    chromium_namespaces = ["base", "cc", "content", "gfx", "net", "ui"]
    chromium_forbidden_classes = ["GURL"]
    chromium_allowed_classes = [
        "base::make_span",
        "base::span",
        "gfx::ColorSpace",
        "gfx::CubicBezier",
        "gfx::ICCProfile",
        "gfx::ScrollOffset",
    ]

    def source_file_filter(path):
        return input_api.FilterSourceFile(path,
                                          white_list=[r'third_party/WebKit/Source/.*\.(h|cpp)$'],
                                          black_list=[r'third_party/WebKit/Source/(platform|controller)/',
                                                      r'.*Test\.(h|cpp)$'])
    comment_re = input_api.re.compile(r'^\s*//')
    result = []
    for namespace in chromium_namespaces:
        namespace_re = input_api.re.compile(r'\b{0}::([A-Za-z_][A-Za-z0-9_]*)'.format(input_api.re.escape(namespace)))

        def uses_namespace_outside_comments(line):
            if comment_re.search(line):
                return False
            re_result = namespace_re.search(line)
            if not re_result:
                return False
            parsed_class_name = namespace + "::" + re_result.group(1)
            return not (parsed_class_name in chromium_allowed_classes)

        errors = input_api.canned_checks._FindNewViolationsOfRule(lambda _, line: not uses_namespace_outside_comments(line),
                                                                  input_api, source_file_filter)
        if errors:
            result += [output_api.PresubmitError('Do not use Chromium class from namespace {} inside Blink core:\n{}'.format(namespace, '\n'.join(errors)))]
    for namespace in chromium_namespaces:
        namespace_re = input_api.re.compile(r'^\s*using namespace {0};|^\s*namespace {0} \{{'.format(input_api.re.escape(namespace)))
        uses_namespace_outside_comments = lambda line: namespace_re.search(line) and not comment_re.search(line)
        errors = input_api.canned_checks._FindNewViolationsOfRule(lambda _, line: not uses_namespace_outside_comments(line),
                                                                  input_api, source_file_filter)
        if errors:
            result += [output_api.PresubmitError('Do not use Chromium namespace {} inside Blink core:\n{}'.format(namespace, '\n'.join(errors)))]
    for class_name in chromium_forbidden_classes:
        class_re = input_api.re.compile(r'\b{0}\b'.format(input_api.re.escape(class_name)))
        uses_class_outside_comments = lambda line: class_re.search(line) and not comment_re.search(line)
        errors = input_api.canned_checks._FindNewViolationsOfRule(lambda _, line: not uses_class_outside_comments(line),
                                                                  input_api, source_file_filter)
        if errors:
            result += [output_api.PresubmitError('Do not use Chromium class {} inside Blink core:\n{}'.format(class_name, '\n'.join(errors)))]
    return result


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(_CheckStyle(input_api, output_api))
    results.extend(_CheckForPrintfDebugging(input_api, output_api))
    results.extend(_CheckForForbiddenChromiumCode(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(input_api.canned_checks.CheckTreeIsOpen(
        input_api, output_api,
        json_url='http://chromium-status.appspot.com/current?format=json'))
    results.extend(input_api.canned_checks.CheckChangeHasDescription(
        input_api, output_api))
    return results


def _ArePaintOrCompositingDirectoriesModified(change):  # pylint: disable=C0103
    """Checks whether CL has changes to paint or compositing directories."""
    paint_or_compositing_paths = [
        os.path.join('third_party', 'WebKit', 'Source', 'platform', 'graphics',
                     'compositing'),
        os.path.join('third_party', 'WebKit', 'Source', 'platform', 'graphics',
                     'paint'),
        os.path.join('third_party', 'WebKit', 'Source', 'core', 'layout',
                     'compositing'),
        os.path.join('third_party', 'WebKit', 'Source', 'core', 'paint'),
        os.path.join('third_party', 'WebKit', 'LayoutTests', 'FlagExpectations',
                     'enable-slimming-paint-v2'),
        os.path.join('third_party', 'WebKit', 'LayoutTests', 'flag-specific',
                     'enable-slimming-paint-v2'),
        os.path.join('third_party', 'WebKit', 'LayoutTests', 'FlagExpectations',
                     'enable-slimming-paint-v175'),
        os.path.join('third_party', 'WebKit', 'LayoutTests', 'flag-specific',
                     'enable-slimming-paint-v175'),
    ]
    for affected_file in change.AffectedFiles():
        file_path = affected_file.LocalPath()
        if any(x in file_path for x in paint_or_compositing_paths):
            return True
    return False


def _AreLayoutNGDirectoriesModified(change):  # pylint: disable=C0103
    """Checks whether CL has changes to a layout ng directory."""
    layout_ng_paths = [
        os.path.join('third_party', 'WebKit', 'Source', 'core', 'layout',
                     'ng'),
    ]
    for affected_file in change.AffectedFiles():
        file_path = affected_file.LocalPath()
        if any(x in file_path for x in layout_ng_paths):
            return True
    return False


def PostUploadHook(cl, change, output_api):  # pylint: disable=C0103
    """git cl upload will call this hook after the issue is created/modified.

    This hook adds extra try bots to the CL description in order to run slimming
    paint v2 tests or LayoutNG tests in addition to the CQ try bots if the
    change contains changes in a relevant direcotry (see:
    _ArePaintOrCompositingDirectoriesModified and
    _AreLayoutNGDirectoriesModified). For more information about
    slimming-paint-v2 tests see https://crbug.com/601275 and for information
    about the LayoutNG tests see https://crbug.com/706183.
    """
    results = []
    if _ArePaintOrCompositingDirectoriesModified(change):
        results.extend(output_api.EnsureCQIncludeTrybotsAreAdded(
            cl,
            ['master.tryserver.chromium.linux:'
             'linux_layout_tests_slimming_paint_v2'],
            'Automatically added slimming-paint-v2 tests to run on CQ due to '
            'changes in paint or compositing directories.'))
    if _AreLayoutNGDirectoriesModified(change):
        results.extend(output_api.EnsureCQIncludeTrybotsAreAdded(
            cl,
            ['master.tryserver.chromium.linux:'
             'linux_layout_tests_layout_ng'],
            'Automatically added linux_layout_tests_layout_ng to run on CQ due '
            'to changes in LayoutNG directories.'))
    return results
