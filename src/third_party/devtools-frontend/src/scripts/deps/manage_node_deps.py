#!/usr/bin/env vpython
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Helper to manage DEPS. Use this script to update node_modules instead of
running npm install manually. To upgrade a dependency, change the version
number in DEPS below and run this script.
"""

import os
import os.path as path
import json
import shutil
import subprocess
import sys
from collections import OrderedDict

scripts_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(scripts_path)

import devtools_paths

LICENSES = [
    "MIT",
    "Apache-2.0",
    "BSD",
    "BSD-2-Clause",
    "BSD-3-Clause",
    "CC0-1.0",
    "CC-BY-3.0",
    "CC-BY-4.0",
    "ISC",
    "MPL-2.0",
    "Python-2.0",
]

# List all DEPS here.
DEPS = {
    "@rollup/plugin-commonjs": "17.1.0",
    "@types/chai": "4.2.16",
    "@types/codemirror": "0.0.108",
    "@types/karma-chai-sinon": "0.1.15",
    "@types/estree": "0.0.47",
    "@types/filesystem": "0.0.30",
    "@types/istanbul-lib-coverage": "2.0.3",
    "@types/istanbul-lib-instrument": "1.7.4",
    "@types/istanbul-lib-report": "3.0.0",
    "@types/istanbul-lib-source-maps": "4.0.1",
    "@types/istanbul-reports": "3.0.0",
    "@types/node": "14.14.37",
    "@types/marked": "2.0.2",
    "@types/mocha": "8.2.2",
    "@types/rimraf": "3.0.0",
    "@types/sinon": "9.0.11",
    "@typescript-eslint/parser": "4.21.0",
    "@typescript-eslint/eslint-plugin": "4.21.0",
    "bl": "4.1.0",
    "chai": "4.3.4",
    "convert-source-map": "1.7.0",
    "escodegen": "2.0.0",
    "eslint": "7.22.0",
    "eslint-plugin-import": "2.22.1",
    "eslint-plugin-lit-a11y": "1.0.1",
    "eslint-plugin-mocha": "8.1.0",
    "eslint-plugin-rulesdir": "0.2.0",
    "istanbul-lib-instrument": "4.0.3",
    "istanbul-lib-report": "3.0.0",
    "karma": "6.3.0",
    "karma-chai": "0.1.0",
    "karma-chrome-launcher": "3.1.0",
    "karma-coverage": "2.0.3",
    "karma-mocha": "2.0.1",
    "karma-sinon": "1.0.5",
    "karma-sourcemap-loader": "0.3.8",
    "karma-spec-reporter": "0.0.32",
    "license-checker": "25.0.1",
    "mocha": "8.3.2",
    # Purposefully not the latest; we keep this in sync with what stylelint
    # depends on
    "postcss": "7.0.35",
    "puppeteer": "8.0.0",
    "recast": "0.20.4",
    "rimraf": "3.0.2",
    "rollup": "2.42.3",
    "rollup-plugin-terser": "7.0.2",
    "sinon": "10.0.0",
    "source-map-support": "0.5.19",
    "stylelint": "13.13.1",
    "stylelint-config-standard": "21.0.0",
    "typescript": "4.3.1-rc",
    "yargs": "16.2.0",
}

def load_json_file(location):
    # By default, json load uses a standard Python dictionary, which is not ordered.
    # To prevent subsequent invocations of this script to erroneously alter the order
    # of keys defined in package.json files, we should use an `OrderedDict`. This
    # ensures not only that we use a strict ordering, it will also make sure we maintain
    # the order defined by the NPM packages themselves. That in turn is important, since
    # NPM packages can define `exports`, where the order of entrypoints is crucial for
    # how an NPM package is loaded. If you would change the order, it could break loading
    # that package.
    return json.load(location, object_pairs_hook=OrderedDict)

def exec_command(cmd):
    try:
        new_env = os.environ.copy()
        # Prevent large files from being checked in to git.
        new_env["PUPPETEER_SKIP_CHROMIUM_DOWNLOAD"] = "true"
        cmd_proc_result = subprocess.check_call(cmd,
                                                cwd=devtools_paths.root_path(),
                                                env=new_env)
    except Exception as error:
        print(error)
        return True

    return False


def ensure_licenses():
    cmd = [
        devtools_paths.node_path(),
        devtools_paths.license_checker_path(),
        '--onlyAllow',
        ('%s' % (';'.join(LICENSES)))
    ]

    return exec_command(cmd)


def strip_private_fields():
    # npm adds private fields which need to be stripped.
    pattern = path.join(devtools_paths.node_modules_path(), 'package.json')
    packages = []
    for root, dirnames, filenames in os.walk(devtools_paths.node_modules_path()):
        for filename in filter(lambda f: f == 'package.json', filenames):
            packages.append(path.join(root, filename))

    for pkg in packages:
        with open(pkg, 'r+') as pkg_file:
            try:
                pkg_data = load_json_file(pkg_file)

                # Remove anything that begins with an underscore, as these are
                # the private fields in a package.json
                for key in pkg_data.keys():
                    if key.find(u'_') == 0:
                        pkg_data.pop(key)

                pkg_file.truncate(0)
                pkg_file.seek(0)
                json.dump(pkg_data, pkg_file, indent=2, separators=(',', ': '))
                pkg_file.write('\n')
            except:
                print('Unable to fix: %s' % pkg)
                return True

    return False


# Required to keep the package-lock.json in sync with the package.json dependencies
def install_missing_deps():
    with open(devtools_paths.package_lock_json_path(), 'r+') as pkg_lock_file:
        try:
            pkg_lock_data = load_json_file(pkg_lock_file)
            existing_deps = pkg_lock_data[u'dependencies']
            new_deps = []

            # Find any new DEPS and add them in.
            for dep, version in DEPS.items():
                if not dep in existing_deps or not existing_deps[dep]['version'] == version:
                    new_deps.append("%s@%s" % (dep, version))

            # Now install.
            if len(new_deps) > 0:
                cmd = ['npm', 'install', '--save-dev']
                cmd.extend(new_deps)
                return exec_command(cmd)

        except Exception as exception:
            print('Unable to install: %s' % exception)
            return True

    return False


def append_package_json_entries():
    with open(devtools_paths.package_json_path(), 'r+') as pkg_file:
        try:
            pkg_data = load_json_file(pkg_file)

            # Replace the dev deps.
            pkg_data[u'devDependencies'] = DEPS

            pkg_file.truncate(0)
            pkg_file.seek(0)
            json.dump(pkg_data, pkg_file, indent=2, separators=(',', ': '))
            pkg_file.write('\n')

        except:
            print('Unable to fix: %s' % sys.exc_info()[0])
            return True
    return False


def remove_package_json_entries():
    with open(devtools_paths.package_json_path(), 'r+') as pkg_file:
        try:
            pkg_data = load_json_file(pkg_file)

            # Remove the dependencies and devDependencies from the root package.json
            # so that they can't be used to overwrite the node_modules managed by this file.
            for key in pkg_data.keys():
                if key.find(u'dependencies') == 0 or key.find(u'devDependencies') == 0:
                    pkg_data.pop(key)

            pkg_file.truncate(0)
            pkg_file.seek(0)
            json.dump(pkg_data, pkg_file, indent=2, separators=(',', ': '))
            pkg_file.write('\n')
        except:
            print('Unable to fix: %s' % pkg)
            return True
    return False


def addClangFormat():
    with open(path.join(devtools_paths.node_modules_path(), '.clang-format'), 'w+') as clang_format_file:
        try:
            clang_format_file.write('DisableFormat: true\n')
        except:
            print('Unable to write .clang-format file')
            return True
    return False


def addOwnersFile():
    with open(path.join(devtools_paths.node_modules_path(), 'OWNERS'),
              'w+') as owners_file:
        try:
            owners_file.write('file://config/owner/INFRA_OWNERS\n')
        except:
            print('Unable to write OWNERS file')
            return True
    return False

def addChromiumReadme():
    with open(path.join(devtools_paths.node_modules_path(), 'README.chromium'),
              'w+') as readme_file:
        try:
            readme_file.write('This directory hosts all packages downloaded from NPM that are used in either the build system or infrastructure scripts.\n')
            readme_file.write('If you want to make any changes to this directory, please see "scripts/deps/manage_node_deps.py".\n')
        except:
            print('Unable to write README.chromium file')
            return True
    return False


def run_npm_command(npm_command_args=None):
    for (name, version) in DEPS.items():
        if (version.find(u'^') == 0):
            print('Versions must be locked to a specific version; remove ^ from the start of the version.')
            return True

    run_custom_command = npm_command_args is not None

    if append_package_json_entries():
        return True

    if install_missing_deps():
        return True

    runs_analysis_command = False

    if run_custom_command:
        runs_analysis_command = npm_command_args[:1] == [
            'outdated'
        ] or npm_command_args[:1] == ['audit'
                                      ] or npm_command_args[:1] == ['ls']

    # By default, run the CI version of npm, which prevents updates to the versions of modules.
    # However, when we are analyzing the installed NPM dependencies, we don't need to run
    # the installation process again.
    if not runs_analysis_command:
        if exec_command(['npm', 'ci']):
            return True

        # To minimize disk usage for Chrome DevTools node_modules, always try to dedupe dependencies.
        # We need to perform this every time, as the order of dependencies added could lead to a
        # non-optimal dependency tree, resulting in unnecessary disk usage.
        if exec_command(['npm', 'dedupe']):
            return True

    if run_custom_command:
        custom_command_result = exec_command(['npm'] + npm_command_args)

    if remove_package_json_entries():
        return True

    if strip_private_fields():
        return True

    if addClangFormat():
        return True

    if addOwnersFile():
        return True

    if addChromiumReadme():
        return True

    if run_custom_command:
        return custom_command_result

    return ensure_licenses()


npm_args = None

if (len(sys.argv[1:]) > 0):
    npm_args = sys.argv[1:]

npm_errors_found = run_npm_command(npm_args)

if npm_errors_found:
    print('npm command failed')
    exit(1)
