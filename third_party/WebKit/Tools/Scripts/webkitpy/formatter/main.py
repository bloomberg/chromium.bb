# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import lib2to3.refactor

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.thirdparty import autopep8


def parse_args(args=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('--chromium', action='store_const', dest='style', const='chromium', default='blink',
                        help="format according to Chromium's Python coding styles instead of Blink's")
    parser.add_argument('--no-backups', action='store_false', default=True, dest='backup',
                        help='do not back up files before overwriting them')
    parser.add_argument('-j', '--jobs', metavar='n', type=int, default=0,
                        help='number of parallel jobs; match CPU count if less than 1')
    parser.add_argument('files', nargs='*', default=['-'],
                        help="files to format or '-' for standard in")
    return parser.parse_args(args=args)


def main(host=None, args=None):
    options = parse_args(args)

    if options.files == ['-']:
        host = host or SystemHost()
        host.print_(reformat_source(host.stdin.read(), options.style, '<stdin>'), end='')
        return

    # We create the arglist before checking if we need to create a Host, because a
    # real host is non-picklable and can't be passed to host.executive.map().
    arglist = [(host, name, options.style, options.backup) for name in options.files]
    host = host or SystemHost()

    host.executive.map(_reformat_thunk, arglist, processes=options.jobs)


def _reformat_thunk(args):
    reformat_file(*args)


def reformat_file(host, name, style, should_backup_file):
    host = host or SystemHost()
    source = host.filesystem.read_text_file(name)
    dest = reformat_source(source, style, name)
    if dest != source:
        if should_backup_file:
            host.filesystem.write_text_file(name + '.bak', source)
        host.filesystem.write_text_file(name, dest)


def reformat_source(source, style, name):
    options = _autopep8_options_for_style(style)
    tmp_str = autopep8.fix_code(source, options)

    fixers = _fixers_for_style(style)
    tool = lib2to3.refactor.RefactoringTool(fixer_names=fixers,
                                            explicit=fixers)
    return unicode(tool.refactor_string(tmp_str, name=name))


def _autopep8_options_for_style(style):
    if style == 'chromium':
        max_line_length = 80
        indent_size = 2
    else:
        max_line_length = 132
        indent_size = 4

    return autopep8.parse_args(['--aggressive',
                                '--max-line-length', str(max_line_length),
                                '--indent-size', str(indent_size),
                                ''])


def _fixers_for_style(style):
    if style == 'chromium':
        return ['webkitpy.formatter.fix_double_quote_strings']
    else:
        return ['webkitpy.formatter.fix_single_quote_strings']
