#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import argparse
import functools
import logging
import os
import re
import shlex
import subprocess
import sys
import tempfile


@functools.lru_cache(1)
def get_chromium_root():
    path = os.path.realpath('../../../../')
    assert os.path.basename(path) == 'src'
    return path


def shell_join(cmd):
    return ' '.join(shlex.quote(c) for c in cmd)


def run(args, cwd=None):
    logging.debug(f'$ {shell_join(args)}')
    subprocess.check_call(args, cwd=cwd)


def run_node(args):
    root = get_chromium_root()
    node = os.path.join(root, 'third_party/node/linux/node-linux-x64/bin/node')
    binary = os.path.join(root, 'third_party/node/node_modules', args[0])
    run([node, binary] + args[1:])


def build_preload_images_js(outdir):
    with open('images/images.gni') as f:
        in_app_images = ast.literal_eval(
            re.search(r'in_app_images\s*=\s*(\[.*?\])', f.read(),
                      re.DOTALL).group(1))
    with tempfile.NamedTemporaryFile('w') as f:
        f.writelines(asset + '\n' for asset in in_app_images)
        f.flush()
        cmd = [
            'utils/gen_preload_images_js.py',
            '--images_list_file',
            f.name,
            '--output_file',
            os.path.join(outdir, 'preload_images.js'),
        ]
        subprocess.check_call(cmd)


def deploy(args):
    cca_root = os.getcwd()
    target_dir = os.path.join(get_chromium_root(), f'out_{args.board}/Release')

    build_preload_images_js(
        os.path.join(target_dir,
                     'gen/chromeos/components/camera_app_ui/resources/js'))

    build_pak_cmd = [
        'tools/grit/grit.py',
        '-i',
        os.path.join(
            target_dir, 'gen/chromeos/components/camera_app_ui/' +
            'chromeos_camera_app_resources.grd'),
        'build',
        '-o',
        os.path.join(target_dir, 'gen/chromeos'),
        '-f',
        os.path.join(target_dir,
                     'gen/tools/gritsettings/default_resource_ids'),
        '-D',
        f'SHARED_INTERMEDIATE_DIR={os.path.join(target_dir, "gen")}',
        '-E',
        f'root_src_dir={get_chromium_root()}',
        '-E',
        f'root_gen_dir={os.path.join(target_dir, "gen")}',
    ]
    # Since there is a constraint in grit.py which will replace ${root_gen_dir}
    # in .grd file only if the script is executed in the parent directory of
    # ${root_gen_dir}, execute the script in Chromium root as a workaround.
    run(build_pak_cmd, get_chromium_root())

    with tempfile.TemporaryDirectory() as tmp_dir:
        pak_util_script = os.path.join(get_chromium_root(),
                                       'tools/grit/pak_util.py')
        extract_resources_pak_cmd = [
            pak_util_script,
            'extract',
            os.path.join(target_dir, 'resources.pak'),
            '-o',
            tmp_dir,
        ]
        run(extract_resources_pak_cmd)

        extract_camera_pak_cmd = [
            pak_util_script,
            'extract',
            os.path.join(target_dir,
                         'gen/chromeos/chromeos_camera_app_resources.pak'),
            '-o',
            tmp_dir,
        ]
        run(extract_camera_pak_cmd)

        create_new_resources_pak_cmd = [
            pak_util_script,
            'create',
            '-i',
            tmp_dir,
            os.path.join(target_dir, 'resources.pak'),
        ]
        run(create_new_resources_pak_cmd)

    deploy_new_resources_pak_cmd = [
        'rsync',
        '--inplace',
        os.path.join(target_dir, 'resources.pak'),
        f'{args.device}:/opt/google/chrome/',
    ]
    run(deploy_new_resources_pak_cmd)


def test(args):
    assert 'CCAUI' not in args.device, (
        'The first argument should be <device> instead of a test name pattern.'
    )
    cmd = ['cros_run_test', '--device', args.device, '--tast'] + args.pattern
    run(cmd)


def lint(args):
    cmd = [
        'eslint/bin/eslint.js',
        'js',
        '--resolve-plugins-relative-to',
        os.path.join(get_chromium_root(), 'third_party/node'),
    ]
    if args.fix:
        cmd.append('--fix')
    try:
        run_node(cmd)
    except subprocess.CalledProcessError as e:
        print('ESLint check failed, return code =', e.returncode)


def tsc(args):
    try:
        run_node(['typescript/bin/tsc'])
    except subprocess.CalledProcessError as e:
        print('TypeScript check failed, return code =', e.returncode)


def parse_args(args):
    parser = argparse.ArgumentParser(description='CCA developer tools.')
    parser.add_argument('--debug', action='store_true')
    subparsers = parser.add_subparsers()

    deploy_parser = subparsers.add_parser('deploy',
                                          help='deploy to device',
                                          description='''Deploy CCA to device.
            This script only works if there is no file added/deleted.
            And please build Chrome at least once before running the command.'''
                                          )
    deploy_parser.add_argument('board')
    deploy_parser.add_argument('device')
    deploy_parser.set_defaults(func=deploy)

    test_parser = subparsers.add_parser('test',
                                        help='run tests',
                                        description='Run CCA tests on device.')
    test_parser.add_argument('device')
    test_parser.add_argument('pattern',
                             nargs='*',
                             default=['camera.CCAUI*'],
                             help='test patterns. (default: camera.CCAUI*)')
    test_parser.set_defaults(func=test)

    lint_parser = subparsers.add_parser(
        'lint',
        help='check code with eslint',
        description='Check coding styles with eslint.')
    lint_parser.add_argument('--fix', action='store_true')
    lint_parser.set_defaults(func=lint)

    tsc_parser = subparsers.add_parser('tsc',
                                       help='check code with tsc',
                                       description='Check types with tsc.')
    tsc_parser.set_defaults(func=tsc)

    parser.set_defaults(func=lambda _args: parser.print_help())

    return parser.parse_args(args)


def main(args):
    cca_root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    assert os.path.basename(cca_root) == 'resources'
    os.chdir(cca_root)

    args = parse_args(args)

    log_level = logging.DEBUG if args.debug else logging.INFO
    log_format = '%(asctime)s - %(levelname)s - %(funcName)s: %(message)s'
    logging.basicConfig(level=log_level, format=log_format)

    logging.debug(f'args = {args}')
    return args.func(args)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
