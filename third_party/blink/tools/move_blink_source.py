#!/usr/bin/env vpython
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import re
import sys
from functools import partial

# Without abspath(), PathFinder can't find chromium_base correctly.
sys.path.append(os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..',
                 'third_party', 'WebKit', 'Tools', 'Scripts')))
from blinkpy.common.name_style_converter import NameStyleConverter
from plan_blink_move import plan_blink_move
from webkitpy.common.path_finder import get_chromium_src_dir
from webkitpy.common.system.filesystem import FileSystem

_log = logging.getLogger('move_blink_source')


class FileType(object):
    NONE = 0
    BUILD = 1
    BLINK_BUILD = 2
    OWNERS = 3
    DEPS = 4

    @staticmethod
    def detect(path):
        _, basename = os.path.split(path)
        if basename == 'DEPS':
            return FileType.DEPS
        if basename == 'OWNERS':
            return FileType.OWNERS
        if basename.endswith(('.gn', '.gni')):
            path = path.replace('\\', '/')
            if 'third_party/WebKit' in path or 'third_party/blink' in path:
                return FileType.BLINK_BUILD
            if 'third_party' in path:
                return FileType.NONE
            return FileType.BUILD
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
        self._checked_in_header_re = None

    def main(self):
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
        self._update_cpp_includes_in_directories(dirs)

        # TODO(tkent): Update basenames in generated files; *.tmpl,
        # bindings/scripts/*.py
        # TODO(tkent): Rename and update *.typemap and *.mojom

        # Content update for individual files
        self._update_single_file_content('third_party/WebKit/Source/bindings/scripts/scripts.gni',
                                         [('bindings_generate_snake_case_files = false',
                                           'bindings_generate_snake_case_files = true')])

        self._move_files(file_pairs)

    def _create_basename_maps(self, file_pairs):
        basename_map = {}
        pattern = r'\b('
        idl_headers = set()
        header_pattern = r'\b('
        for source, dest in file_pairs:
            _, source_base = self._fs.split(source)
            _, dest_base = self._fs.split(dest)
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
            elif source_base.endswith('.h'):
                header_pattern += re.escape(source_base) + '|'
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
        return content

    def _update_blink_build(self, content):
        content = self._update_build(content)
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
        _log.info('Find *.gn, DEPS, and OWNERS ...')
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
                content = self._update_deps(content)

            if original_content == content:
                continue
            if self._options.run:
                self._fs.write_text_file(file_path, content)
            if file_type == FileType.DEPS:
                deps_dir = self._fs.dirname(file_path)
                if deps_dir != self._repo_root:
                    self._append_unless_upper_dir_exists(updated_deps_dirs, deps_dir)
            _log.info('Updated %s', self._shorten_path(file_path))
        return updated_deps_dirs

    def _update_cpp_includes_in_directories(self, dirs):
        for dirname in dirs:
            _log.info('Processing #include in %s ...', self._shorten_path(dirname))
            files = self._fs.files_under(
                dirname, file_filter=lambda fs, _, basename: basename.endswith(('.h', '.cc', '.cpp', '.mm')))
            for file_path in files:
                original_content = self._fs.read_text_file(file_path)

                content = self._update_cpp_includes(original_content)
                if file_path.endswith('.h') and '/third_party/WebKit/public/' in file_path:
                    content = self._update_basename_only_includes(content, file_path)

                if original_content == content:
                    continue
                if self._options.run:
                    self._fs.write_text_file(file_path, content)
                _log.info('Updated %s', self._shorten_path(file_path))

    def _replace_include_path(self, match):
        path = match.group(1)

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
            return '#include "%s"' % path

        match = self._checked_in_header_re.search(path)
        if match:
            path = 'third_party/blink/renderer/' + path[:match.start(1)] + self._basename_map[match.group(1)]
        else:
            basename_start = path.rfind('/') + 1
            basename = path[basename_start:]
            if basename in self._idl_generated_impl_headers:
                path = path[:basename_start] + self._basename_map[basename]
            elif basename.startswith('V8'):
                path = path[:basename_start] + NameStyleConverter(basename[:len(basename) - 2]).to_snake_case() + '.h'
        return '#include "%s"' % path

    def _update_cpp_includes(self, content):
        pattern = re.compile(r'#include\s+"((bindings|core|modules|platform|public|' +
                             r'third_party/WebKit/(Source|common|public))/[-_\w/.]+)"')
        return pattern.sub(self._replace_include_path, content)

    def _replace_basename_only_include(self, subdir, source_path, match):
        source_basename = match.group(1)
        if source_basename in self._basename_map:
            return '#include "third_party/blink/renderer/public/%s/%s"' % (subdir, self._basename_map[source_basename])
        _log.warning('Basename-only %s in %s', match.group(0), self._shorten_path(source_path))
        return match.group(0)

    def _update_basename_only_includes(self, content, source_path):
        if not source_path.endswith('.h') or '/third_party/WebKit/public/' not in source_path:
            return
        # In public/ header files, we should replace |#include "WebFoo.h"|
        # with |#include "third_party/blink/renderer/public/platform-or-web/web_foo.h"|
        subdir = self._fs.basename(self._fs.dirname(source_path))
        # subdir is 'web' or 'platform'.
        return re.sub(r'#include\s+"(\w+\.h)"',
                      partial(self._replace_basename_only_include, subdir, source_path), content)

    def _update_single_file_content(self, file_path, replace_list):
        full_path = self._fs.join(self._repo_root, file_path)
        original_content = self._fs.read_text_file(full_path)
        content = original_content
        for src, dest in replace_list:
            content = content.replace(src, dest)
        if content != original_content:
            self._fs.write_text_file(full_path, content)
            _log.info('Updated %s', file_path)
        else:
            _log.warning('%s does not contain specified source strings.', file_path)

    def _move_files(self, file_pairs):
        # TODO(tkent): Implement.
        return file_pairs


def main():
    logging.basicConfig(level=logging.DEBUG,
                        format='[%(asctime)s %(levelname)s %(name)s] %(message)s',
                        datefmt='%H:%M:%S')
    parser = argparse.ArgumentParser(description='Blink source mover')
    parser.add_argument('--run', dest='run', action='store_true')
    options = parser.parse_args()
    MoveBlinkSource(FileSystem(), options, get_chromium_src_dir()).main()


if __name__ == '__main__':
    main()
