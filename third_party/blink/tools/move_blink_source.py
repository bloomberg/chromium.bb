#!/usr/bin/env vpython
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool to move Blink source from third_party/WebKit to third_party/blink.

See https://docs.google.com/document/d/1l3aPv1Wx__SpRkdOhvJz8ciEGigNT3wFKv78XiuW0Tw/edit?usp=sharing#heading=h.o225wrxp242h
for the details.
"""

import argparse
import logging
import os
import re
import subprocess
import sys
from functools import partial

# Without abspath(), PathFinder can't find chromium_base correctly.
sys.path.append(os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..',
                 'third_party', 'WebKit', 'Tools', 'Scripts')))
from blinkpy.common.name_style_converter import NameStyleConverter
from plan_blink_move import plan_blink_move
from webkitpy.common.checkout.git import Git
from webkitpy.common.path_finder import get_chromium_src_dir
from webkitpy.common.system.filesystem import FileSystem

_log = logging.getLogger('move_blink_source')


class FileType(object):
    NONE = 0
    BUILD = 1
    BLINK_BUILD = 2
    OWNERS = 3
    DEPS = 4
    MOJOM = 5
    TYPEMAP = 6
    BLINK_BUILD_PY = 7
    LAYOUT_TESTS_WITH_MOJOM = 8

    @staticmethod
    def detect(path):
        slash_dir, basename = os.path.split(path)
        slash_dir = slash_dir.replace(os.path.sep, '/')
        if basename == 'DEPS':
            return FileType.DEPS
        if basename == 'OWNERS':
            return FileType.OWNERS
        if basename.endswith('.mojom'):
            return FileType.MOJOM
        if basename.endswith('.typemap'):
            return FileType.TYPEMAP
        if basename.endswith('.py') and 'third_party/WebKit/Source/build' in slash_dir:
            return FileType.BLINK_BUILD_PY
        if basename.endswith(('.gn', '.gni')):
            if 'third_party/WebKit' in path or 'third_party/blink' in slash_dir:
                return FileType.BLINK_BUILD
            if 'third_party' in slash_dir:
                return FileType.NONE
            return FileType.BUILD
        if basename.endswith('.html') and re.search(
                r'third_party/WebKit/LayoutTests/(geolocation-api|installedapp|' +
                r'media/mediasession|payments|presentation|webshare)', slash_dir):
            return FileType.LAYOUT_TESTS_WITH_MOJOM
        return FileType.NONE


class MoveBlinkSource(object):

    def __init__(self, fs, options, repo_root):
        self._fs = fs
        self._options = options
        _log.debug(options)
        self._repo_root = repo_root

        # The following fields are initialized in _create_basename_maps.
        self._basename_map = None
        self._basename_re = None
        self._idl_generated_impl_headers = None
        # _checked_in_header_re is used to distinguish checked-in header files
        # and generated header files.
        self._checked_in_header_re = None

    def update(self):
        _log.info('Planning renaming ...')
        file_pairs = plan_blink_move(self._fs, [])
        _log.info('Will move %d files', len(file_pairs))

        self._create_basename_maps(file_pairs)
        dirs = self._update_file_content()

        # Updates #includes in files in directories with updated DEPS +
        # third_party/WebKit/{Source,common,public}.
        self._append_unless_upper_dir_exists(dirs, self._fs.join(self._repo_root, 'third_party', 'WebKit', 'Source'))
        self._append_unless_upper_dir_exists(dirs, self._fs.join(self._repo_root, 'third_party', 'WebKit', 'common'))
        self._append_unless_upper_dir_exists(dirs, self._fs.join(self._repo_root, 'third_party', 'WebKit', 'public'))
        self._append_unless_upper_dir_exists(dirs, self._fs.join(self._repo_root, 'mojo', 'public', 'tools',
                                                                 'bindings', 'generators', 'cpp_templates'))
        self._update_cpp_includes_in_directories(dirs)

        # Content update for individual files.
        # The following is a list of tuples.
        #  Tuple: (<file path relative to repo root>, [replacement commands])
        #  Command: a callable object, or
        #           a tuple of (<original string>, <new string>).
        file_replacement_list = [
            ('DEPS',
             [('src/third_party/WebKit/Source/devtools',
               'src/third_party/blink/renderer/devtools')]),
            ('third_party/WebKit/Source/BUILD.gn',
             [('$root_gen_dir/third_party/WebKit',
               '$root_gen_dir/third_party/blink/renderer')]),
            ('third_party/WebKit/Source/config.gni',
             [('snake_case_source_files = false',
               'snake_case_source_files = true')]),
            ('third_party/WebKit/Source/core/css/CSSProperties.json5',
             [self._update_basename]),
            ('third_party/WebKit/Source/core/css/ComputedStyleExtraFields.json5',
             [self._update_basename]),
            ('third_party/WebKit/Source/core/css/ComputedStyleFieldAliases.json5',
             [self._update_basename]),
            ('third_party/WebKit/Source/core/html/parser/create-html-entity-table',
             [self._update_basename]),
            ('third_party/WebKit/Source/core/inspector/inspector_protocol_config.json',
             [self._update_basename]),
            ('third_party/WebKit/Source/core/probe/CoreProbes.json5',
             [self._update_basename]),
            ('third_party/WebKit/Source/core/testing/InternalSettings.h',
             [('InternalSettingsGenerated.h', 'internal_settings_generated.h')]),
            ('third_party/WebKit/Source/core/testing/Internals.cpp',
             [('InternalRuntimeFlags.h', 'internal_runtime_flags.h')]),
            ('third_party/WebKit/Source/platform/probe/PlatformProbes.json5',
             [self._update_basename]),
            ('third_party/WebKit/public/BUILD.gn',
             [('$root_gen_dir/third_party/WebKit',
               '$root_gen_dir/third_party/blink/renderer')]),
            ('third_party/WebKit/public/blink_resources.grd',
             [('../Source/', '../')]),
            ('tools/gritsettings/resource_ids',
             [('third_party/WebKit/public', 'third_party/blink/renderer/public'),
              ('third_party/WebKit/Source', 'third_party/blink/renderer')]),
        ]
        for file_path, replacement_list in file_replacement_list:
            self._update_single_file_content(file_path, replacement_list, should_write=self._options.run)

    def move(self):
        _log.info('Planning renaming ...')
        file_pairs = plan_blink_move(self._fs, [])
        _log.info('Will move %d files', len(file_pairs))

        git = Git(cwd=self._repo_root)
        for i, (src, dest) in enumerate(file_pairs):
            src_from_repo = self._fs.join('third_party', 'WebKit', src)
            dest_from_repo = self._fs.join('third_party', 'blink', dest)
            self._fs.maybe_make_directory(self._repo_root, 'third_party', 'blink', self._fs.dirname(dest))
            if self._options.run_git:
                if git.exists(src_from_repo):
                    git.move(src_from_repo, dest_from_repo)
                    _log.info('[%d/%d] Git moved %s', i + 1, len(file_pairs), src)
                else:
                    _log.info('%s is not in the repository', src)
            else:
                self._fs.move(self._fs.join(self._repo_root, src_from_repo),
                              self._fs.join(self._repo_root, dest_from_repo))
                _log.info('[%d/%d] Moved %s', i + 1, len(file_pairs), src)
        self._update_single_file_content(
            'build/get_landmines.py',
            [('\ndef main', '  print \'The Great Blink mv for source files (crbug.com/768828)\'\n\ndef main')])


    def _create_basename_maps(self, file_pairs):
        basename_map = {}
        # Generated inspector/protocol/* contains a lot of names duplicated with
        # checked-in core files. We don't want to rename them, and don't want to
        # replace them in BUILD.gn and #include accidentally.
        pattern = r'(?<!inspector/protocol/)\b('
        idl_headers = set()
        header_pattern = r'(?<!inspector/protocol/)\b('
        for source, dest in file_pairs:
            _, source_base = self._fs.split(source)
            _, dest_base = self._fs.split(dest)
            # ConditionalFeaturesForCore.h in bindings/tests/results/modules/
            # confuses generated/checked-in detection in _replace_include_path().
            if 'bindings/tests' in source.replace('\\', '/'):
                continue
            if source_base.endswith('.h'):
                header_pattern += re.escape(source_base) + '|'
            if source_base == dest_base:
                continue
            basename_map[source_base] = dest_base
            pattern += re.escape(source_base) + '|'
            # IDL sometimes generates implementation files as well as
            # binding files. We'd like to update #includes for such files.
            if source_base.endswith('.idl'):
                source_header = source_base.replace('.idl', '.h')
                basename_map[source_header] = dest_base.replace('.idl', '.h')
                pattern += re.escape(source_header) + '|'
                idl_headers.add(source_header)
        _log.info('Rename %d files for snake_case', len(basename_map))
        self._basename_map = basename_map
        self._basename_re = re.compile(pattern[0:len(pattern) - 1] + ')(?=["\']|$)')
        self._idl_generated_impl_headers = idl_headers
        self._checked_in_header_re = re.compile(header_pattern[0:len(header_pattern) - 1] + ')$')

    def _shorten_path(self, path):
        if path.startswith(self._repo_root):
            return path[len(self._repo_root) + 1:]
        return path

    @staticmethod
    def _filter_file(fs, dirname, basename):
        return FileType.detect(fs.join(dirname, basename)) != FileType.NONE

    def _update_build(self, content):
        content = content.replace('//third_party/WebKit/Source', '//third_party/blink/renderer')
        content = content.replace('//third_party/WebKit/common', '//third_party/blink/common')
        content = content.replace('//third_party/WebKit/public', '//third_party/blink/renderer/public')
        # export_header_blink exists outside of Blink too.
        content = content.replace('export_header_blink = "third_party/WebKit/public/platform/WebCommon.h"',
                                  'export_header_blink = "third_party/blink/renderer/public/platform/web_common.h"')
        return content

    def _update_blink_build(self, content):
        content = self._update_build(content)

        # Update visibility=[...]
        content = content.replace('//third_party/WebKit/*', '//third_party/blink/*')
        content = content.replace('//third_party/WebKit/Source/*', '//third_party/blink/renderer/*')
        content = content.replace('//third_party/WebKit/public/*', '//third_party/blink/renderer/public/*')

        return self._update_basename(content)

    def _update_owners(self, content):
        content = content.replace('//third_party/WebKit/Source', '//third_party/blink/renderer')
        content = content.replace('//third_party/WebKit/common', '//third_party/blink/common')
        content = content.replace('//third_party/WebKit/public', '//third_party/blink/renderer/public')
        return content

    def _update_deps(self, content):
        original_content = content
        content = content.replace('third_party/WebKit/Source', 'third_party/blink/renderer')
        content = content.replace('third_party/WebKit/common', 'third_party/blink/common')
        content = content.replace('third_party/WebKit/public', 'third_party/blink/renderer/public')
        content = content.replace('third_party/WebKit', 'third_party/blink')
        if original_content == content:
            return content
        return self._update_basename(content)

    def _update_mojom(self, content):
        content = content.replace('third_party/WebKit/public', 'third_party/blink/renderer/public')
        return content

    def _update_typemap(self, content):
        content = content.replace('//third_party/WebKit/Source', '//third_party/blink/renderer')
        content = content.replace('//third_party/WebKit/common', '//third_party/blink/common')
        content = content.replace('//third_party/WebKit/public', '//third_party/blink/renderer/public')
        return self._update_basename(content)

    def _update_blink_build_py(self, content):
        # We don't prepend 'third_party/blink/renderer/' to matched basenames
        # because it won't affect build and manual update after the great mv is
        # enough.
        return self._update_basename(content)

    def _update_layout_tests(self, content):
        return content.replace('file:///gen/third_party/WebKit/', 'file:///gen/third_party/blink/renderer/')

    def _update_basename(self, content):
        return self._basename_re.sub(lambda match: self._basename_map[match.group(1)], content)

    @staticmethod
    def _append_unless_upper_dir_exists(dirs, new_dir):
        for i in range(0, len(dirs)):
            if new_dir.startswith(dirs[i]):
                return
            if dirs[i].startswith(new_dir):
                dirs[i] = new_dir
                return
        dirs.append(new_dir)

    def _update_file_content(self):
        _log.info('Find *.gn, *.mojom, *.py, *.typemap, DEPS, and OWNERS ...')
        files = self._fs.files_under(
            self._repo_root, dirs_to_skip=['.git', 'out'], file_filter=self._filter_file)
        _log.info('Scan contents of %d files ...', len(files))
        updated_deps_dirs = []
        for file_path in files:
            file_type = FileType.detect(file_path)
            original_content = self._fs.read_text_file(file_path)
            content = original_content
            if file_type == FileType.BUILD:
                content = self._update_build(content)
            elif file_type == FileType.BLINK_BUILD:
                content = self._update_blink_build(content)
            elif file_type == FileType.OWNERS:
                content = self._update_owners(content)
            elif file_type == FileType.DEPS:
                if self._fs.dirname(file_path) == self._repo_root:
                    _log.info("Skip //DEPS")
                    continue
                content = self._update_deps(content)
            elif file_type == FileType.MOJOM:
                content = self._update_mojom(content)
            elif file_type == FileType.TYPEMAP:
                content = self._update_typemap(content)
            elif file_type == FileType.BLINK_BUILD_PY:
                content = self._update_blink_build_py(content)
            elif file_type == FileType.LAYOUT_TESTS_WITH_MOJOM:
                content = self._update_layout_tests(content)

            if original_content == content:
                continue
            if self._options.run:
                self._fs.write_text_file(file_path, content)
            if file_type == FileType.DEPS:
                self._append_unless_upper_dir_exists(updated_deps_dirs, self._fs.dirname(file_path))
            _log.info('Updated %s', self._shorten_path(file_path))
        return updated_deps_dirs

    def _update_cpp_includes_in_directories(self, dirs):
        for dirname in dirs:
            _log.info('Processing #include in %s ...', self._shorten_path(dirname))
            files = self._fs.files_under(
                dirname, file_filter=lambda fs, _, basename: basename.endswith(
                    ('.h', '.cc', '.cpp', '.mm', '.cc.tmpl', '.cpp.tmpl',
                     '.h.tmpl', 'XPathGrammar.y', '.gperf')))
            for file_path in files:
                original_content = self._fs.read_text_file(file_path)

                content = self._update_cpp_includes(original_content)
                if file_path.endswith('.h') and '/third_party/WebKit/public/' in file_path.replace('\\', '/'):
                    content = self._update_basename_only_includes(content, file_path)

                if original_content == content:
                    continue
                if self._options.run:
                    self._fs.write_text_file(file_path, content)
                _log.info('Updated %s', self._shorten_path(file_path))

    def _replace_include_path(self, match):
        include_or_import = match.group(1)
        path = match.group(2)

        # If |path| starts with 'third_party/WebKit', we should adjust the
        # directory name for third_party/blink, and replace its basename by
        # self._basename_map.
        #
        # If |path| starts with a Blink-internal directory such as bindings,
        # core, modules, platform, public, it refers to a checked-in file, or a
        # generated file. For the former, we should add
        # 'third_party/blink/renderer/' and replace the basename.  For the
        # latter, we should update the basename for a name mapped from an IDL
        # renaming, and should not add 'third_party/blink/renderer'.

        if path.startswith('third_party/WebKit'):
            path = path.replace('third_party/WebKit/Source', 'third_party/blink/renderer')
            path = path.replace('third_party/WebKit/common', 'third_party/blink/common')
            path = path.replace('third_party/WebKit/public', 'third_party/blink/renderer/public')
            path = self._update_basename(path)
            return '#%s "%s"' % (include_or_import, path)

        match = self._checked_in_header_re.search(path)
        if match:
            if match.group(1) in self._basename_map:
                path = 'third_party/blink/renderer/' + path[:match.start(1)] + self._basename_map[match.group(1)]
            else:
                path = 'third_party/blink/renderer/' + path
        elif 'core/inspector/protocol/' not in path:
            basename_start = path.rfind('/') + 1
            basename = path[basename_start:]
            if basename in self._idl_generated_impl_headers:
                path = path[:basename_start] + self._basename_map[basename]
            elif basename.startswith('V8'):
                path = path[:basename_start] + NameStyleConverter(basename[:len(basename) - 2]).to_snake_case() + '.h'
        return '#%s "%s"' % (include_or_import, path)

    def _update_cpp_includes(self, content):
        pattern = re.compile(r'#(include|import)\s+"((bindings|core|modules|platform|public|' +
                             r'third_party/WebKit/(Source|common|public))/[-_\w/.]+)"')
        return pattern.sub(self._replace_include_path, content)

    def _replace_basename_only_include(self, subdir, source_path, match):
        source_basename = match.group(1)
        if source_basename in self._basename_map:
            return '#include "third_party/blink/renderer/public/%s/%s"' % (subdir, self._basename_map[source_basename])
        _log.warning('Basename-only %s in %s', match.group(0), self._shorten_path(source_path))
        return match.group(0)

    def _update_basename_only_includes(self, content, source_path):
        if not source_path.endswith('.h') or '/third_party/WebKit/public/' not in source_path.replace('\\', '/'):
            return
        # In public/ header files, we should replace |#include "WebFoo.h"|
        # with |#include "third_party/blink/renderer/public/platform-or-web/web_foo.h"|
        subdir = self._fs.basename(self._fs.dirname(source_path))
        # subdir is 'web' or 'platform'.
        return re.sub(r'#include\s+"(\w+\.h)"',
                      partial(self._replace_basename_only_include, subdir, source_path), content)

    def _update_single_file_content(self, file_path, replace_list, should_write=True):
        full_path = self._fs.join(self._repo_root, file_path)
        original_content = self._fs.read_text_file(full_path)
        content = original_content
        for command in replace_list:
            if isinstance(command, tuple):
                src, dest = command
                content = content.replace(src, dest)
            elif callable(command):
                content = command(content)
            else:
                raise TypeError('A tuple or a function is expected.')
        if content != original_content:
            if should_write:
                self._fs.write_text_file(full_path, content)
            _log.info('Updated %s', file_path)
        else:
            _log.warning('%s does not contain specified source strings.', file_path)


def main():
    logging.basicConfig(level=logging.DEBUG,
                        format='[%(asctime)s %(levelname)s %(name)s] %(message)s',
                        datefmt='%H:%M:%S')
    parser = argparse.ArgumentParser(description='Blink source mover')
    sub_parsers = parser.add_subparsers()

    update_parser = sub_parsers.add_parser('update')
    update_parser.set_defaults(command='update')
    update_parser.add_argument('--run', dest='run', action='store_true',
                               help='Update file contents')

    move_parser = sub_parsers.add_parser('move')
    move_parser.set_defaults(command='move')
    move_parser.add_argument('--git', dest='run_git', action='store_true',
                             help='Run |git mv| command instead of |mv|.')

    options = parser.parse_args()
    mover = MoveBlinkSource(FileSystem(), options, get_chromium_src_dir())
    if options.command == 'update':
        mover.update()
    elif options.command == 'move':
        mover.move()


if __name__ == '__main__':
    main()
