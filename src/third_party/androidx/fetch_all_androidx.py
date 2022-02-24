#!/usr/bin/env python3

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to generate build.gradle from template and run fetch_all.py

More specifically, to generate build.gradle:
  - It downloads the BUILD_INFO file for the latest androidx snapshot from
    https://androidx.dev/snapshots/builds
  - It replaces {{androidx_repository_url}} with the URL for the latest snapshot
  - For each dependency, it looks up the version in the BUILD_INFO file and
    substitutes the version into {{androidx_dependency_version}}.
"""

import argparse
import contextlib
import json
import logging
import os
import re
import shutil
import stat
import subprocess
import tempfile
import urllib
from urllib import request

_ANDROIDX_PATH = os.path.normpath(os.path.join(__file__, '..'))

_FETCH_ALL_PATH = os.path.normpath(
    os.path.join(_ANDROIDX_PATH, '..', 'android_deps', 'fetch_all.py'))

# URL to BUILD_INFO in latest androidx snapshot.
_ANDROIDX_LATEST_SNAPSHOT_BUILD_INFO_URL = 'https://androidx.dev/snapshots/latest/artifacts/BUILD_INFO'

# Snapshot repository URL with {{version}} placeholder.
_SNAPSHOT_REPOSITORY_URL = 'https://androidx.dev/snapshots/builds/{{version}}/artifacts/repository'


def _build_snapshot_repository_url(version):
    return _SNAPSHOT_REPOSITORY_URL.replace('{{version}}', version)


def _delete_readonly_files(paths):
    for path in paths:
        if os.path.exists(path):
            os.chmod(path, stat.S_IRUSR | stat.S_IRGRP | stat.S_IWUSR)
            os.remove(path)


def _parse_dir_list(dir_list):
    """Computes 'library_group:library_name'->library_version mapping.

    Args:
      dir_list: List of androidx library directories.
    """
    dependency_version_map = dict()
    for dir_entry in dir_list:
        stripped_dir = dir_entry.strip()
        if not stripped_dir.startswith('repository/androidx/'):
            continue
        dir_components = stripped_dir.split('/')
        # Expected format:
        # "repository/androidx/library_group/library_name/library_version/pom_or_jar"
        if len(dir_components) < 6:
            continue
        dependency_package = 'androidx.' + '.'.join(dir_components[2:-3])
        dependency_module = '{}:{}'.format(dependency_package,
                                           dir_components[-3])
        if dependency_module not in dependency_version_map:
            dependency_version_map[dependency_module] = dir_components[-2]
    return dependency_version_map


def _compute_replacement(dependency_version_map, androidx_repository_url,
                         line):
    """Computes output line for build.gradle from build.gradle.template line.

    Replaces {{android_repository_url}}, {{androidx_dependency_version}} and
      {{version_overrides}}.

    Args:
      dependency_version_map: An "dependency_group:dependency_name"->dependency_version mapping.
      androidx_repository_url: URL of the maven repository.
      line: Input line from the build.gradle.template.
    """
    line = line.replace('{{androidx_repository_url}}', androidx_repository_url)

    if line.strip() == '{{version_overrides}}':
        lines = ['versionOverrideMap = [:]']
        for dependency, version in dependency_version_map.items():
            lines.append(f"versionOverrideMap['{dependency}'] = '{version}'")
        return '\n'.join(lines)

    match = re.search(r'\'(\S+):{{androidx_dependency_version}}\'', line)
    if not match:
        return line

    dependency = match.group(1)
    version = dependency_version_map.get(dependency)
    if not version:
        raise Exception(f'Version for {dependency} not found.')

    return line.replace('{{androidx_dependency_version}}', version)


@contextlib.contextmanager
def _build_dir():
    dirname = tempfile.mkdtemp()
    try:
        yield dirname
    finally:
        shutil.rmtree(dirname)


def _download_and_parse_build_info():
    """Downloads and parses BUILD_INFO file."""
    with _build_dir() as build_dir:
        androidx_build_info_response = request.urlopen(
            _ANDROIDX_LATEST_SNAPSHOT_BUILD_INFO_URL)
        androidx_build_info_url = androidx_build_info_response.geturl()
        logging.info('URL for the latest build info: %s',
                     androidx_build_info_url)
        androidx_build_info_path = os.path.join(build_dir, 'BUILD_INFO')
        with open(androidx_build_info_path, 'w') as f:
            f.write(androidx_build_info_response.read().decode('utf-8'))

        # Strip '/repository' from pattern.
        resolved_snapshot_repository_url_pattern = (
            _build_snapshot_repository_url('([0-9]*)').rsplit('/', 1)[0])

        version = re.match(resolved_snapshot_repository_url_pattern,
                           androidx_build_info_url).group(1)

        with open(androidx_build_info_path, 'r') as f:
            build_info_dict = json.loads(f.read())
        dir_list = build_info_dict['target']['dir_list']

        dependency_version_map = _parse_dir_list(dir_list)
        return (dependency_version_map, version)


def _process_build_gradle(dependency_version_map, androidx_repository_url):
    """Generates build.gradle from template.

    Args:
      dependency_version_map: An "dependency_group:dependency_name"->dependency_version mapping.
      androidx_repository_url: URL of the maven repository.
    """
    build_gradle_template_path = os.path.join(_ANDROIDX_PATH,
                                              'build.gradle.template')
    build_gradle_out_path = os.path.join(_ANDROIDX_PATH, 'build.gradle')
    # |build_gradle_out_path| is not deleted after script has finished running. The file is in
    # .gitignore and thus will be excluded from uploaded CLs.
    with open(build_gradle_template_path, 'r') as template_f, \
        open(build_gradle_out_path, 'w') as out:
        for template_line in template_f:
            replacement = _compute_replacement(dependency_version_map,
                                               androidx_repository_url,
                                               template_line)
            out.write(replacement)


def _write_cipd_yaml(libs_dir, version, cipd_yaml_path):
    """Writes cipd.yaml file at the passed-in path."""

    lib_dirs = os.listdir(libs_dir)
    if not lib_dirs:
        raise Exception('No generated libraries in {}'.format(libs_dir))

    data_files = [
        'BUILD.gn', 'VERSION.txt', 'additional_readme_paths.json',
        'build.gradle'
    ]
    for lib_dir in lib_dirs:
        abs_lib_dir = os.path.join(libs_dir, lib_dir)
        androidx_rel_lib_dir = os.path.relpath(abs_lib_dir, _ANDROIDX_PATH)
        if not os.path.isdir(abs_lib_dir):
            continue
        lib_files = os.listdir(abs_lib_dir)
        if not 'cipd.yaml' in lib_files:
            continue

        for lib_file in lib_files:
            if lib_file == 'cipd.yaml' or lib_file == 'OWNERS':
                continue
            data_files.append(os.path.join(androidx_rel_lib_dir, lib_file))

    contents = [
        '# Copyright 2021 The Chromium Authors. All rights reserved.',
        '# Use of this source code is governed by a BSD-style license that can be',
        '# found in the LICENSE file.',
        '# version: ' + version,
        'package: chromium/third_party/androidx',
        'description: androidx',
        'data:',
    ]
    contents.extend('- file: ' + f for f in data_files)

    with open(cipd_yaml_path, 'w') as out:
        out.write('\n'.join(contents))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-v',
                        '--verbose',
                        dest='verbose_count',
                        default=0,
                        action='count',
                        help='Verbose level (multiple times for more)')
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.WARNING - 10 * args.verbose_count,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')

    libs_dir = os.path.join(_ANDROIDX_PATH, 'libs')

    # Let recipe delete contents of lib directory because it has API to retry
    # directory deletion if the first deletion attempt does not work.
    if os.path.exists(libs_dir) and os.listdir(libs_dir):
        raise Exception('Recipe did not empty \'libs\' directory.')

    # Files uploaded to cipd are read-only. Delete them because they will be
    # re-generated.
    _delete_readonly_files([
        os.path.join(_ANDROIDX_PATH, 'BUILD.gn'),
        os.path.join(_ANDROIDX_PATH, 'VERSION.txt'),
        os.path.join(_ANDROIDX_PATH, 'additional_readme_paths.json'),
        os.path.join(_ANDROIDX_PATH, 'build.gradle'),
    ])

    dependency_version_map, version = _download_and_parse_build_info()
    androidx_snapshot_repository_url = _build_snapshot_repository_url(version)
    _process_build_gradle(dependency_version_map,
                          androidx_snapshot_repository_url)

    fetch_all_cmd = [
        _FETCH_ALL_PATH, '--android-deps-dir', _ANDROIDX_PATH,
        '--ignore-vulnerabilities'
    ]
    subprocess.run(fetch_all_cmd, check=True)

    # Prepend '0' to version to avoid conflicts with previous version format.
    version = 'cr-0' + version

    version_txt_path = os.path.join(_ANDROIDX_PATH, 'VERSION.txt')
    with open(version_txt_path, 'w') as f:
        f.write(version)

    yaml_path = os.path.join(_ANDROIDX_PATH, 'cipd.yaml')
    _write_cipd_yaml(libs_dir, version, yaml_path)


if __name__ == '__main__':
    main()
