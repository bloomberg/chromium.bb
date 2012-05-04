" Copyright (c) 2012 The Chromium Authors. All rights reserved.
" Use of this source code is governed by a BSD-style license that can be
" found in the LICENSE file.
"
" Adds a "Compile this file" function, using ninja. On Mac, binds Cmd-k to
" this command. On Windows, Ctrl-F7 (which is the same as the VS default).
"
" Requires that gyp has already generated build.ninja files, and that ninja is
" in your path (which it is automatically if depot_tools is in your path).
"
" Add the following to your .vimrc file:
"     so /path/to/src/tools/vim/ninja-build.vim

python << endpython
import os
import vim


def path_to_current_buffer():
  """Returns the absolute path of the current buffer."""
  return vim.current.buffer.name


def path_to_source_root():
  """Returns the absolute path to the chromium source root."""
  candidate = os.path.dirname(path_to_current_buffer())
  # This is a list of files that need to identify the src directory. The shorter
  # it is, the more likely it's wrong (checking for just "build/common.gypi"
  # would find "src/v8" for files below "src/v8", as "src/v8/build/common.gypi"
  # exists). The longer it is, the more likely it is to break when we rename
  # directories.
  fingerprints = ['chrome', 'net', 'v8', 'build', 'skia']
  while candidate and not all(
      [os.path.isdir(os.path.join(candidate, fp)) for fp in fingerprints]):
    candidate = os.path.dirname(candidate)
  return candidate


def guess_configuration():
  """Default to the configuration with either a newer build.ninja or a newer
  protoc."""
  root = os.path.join(path_to_source_root(), 'out')
  def is_release_15s_newer(test_path):
    try:
      debug_mtime = os.path.getmtime(os.path.join(root, 'Debug', test_path))
    except os.error:
      debug_mtime = 0
    try:
      rel_mtime = os.path.getmtime(os.path.join(root, 'Release',  test_path))
    except os.error:
      rel_mtime = 0
    return rel_mtime - debug_mtime >= 15
  configuration = 'Debug'
  if is_release_15s_newer('build.ninja') or is_release_15s_newer('protoc'):
    configuration = 'Release'
  return configuration


def compute_ninja_command(configuration=None):
  """Returns the shell command to compile the file in the current buffer."""
  if not configuration: configuration = guess_configuration()
  build_dir = os.path.join(path_to_source_root(), 'out', configuration)

  # ninja needs filepaths for the ^ syntax to be relative to the
  # build directory.
  file_to_build = path_to_current_buffer()
  file_to_build = os.path.relpath(file_to_build, build_dir)

  return ' '.join(['ninja', '-C', build_dir, file_to_build + '^'])


def set_makepgr_to_single_file_ninja():
  build_cmd = compute_ninja_command()
  if sys.platform == 'win32':
    # Escape \ for Vim, and ^ for both Vim and shell.
    build_cmd = build_cmd.replace('\\', '\\\\').replace('^', '^^^^')
  vim.command('let &makeprg=\'%s\'' % build_cmd)
endpython

fun! CrCompileFile()
  let l:oldmakepgr = &makeprg
  python set_makepgr_to_single_file_ninja()
  silent make | cwindow
  if !has('gui')
    redraw!
  endif
  let &makeprg = l:oldmakepgr
endfun

command! CrCompileFile call CrCompileFile()

if has('mac')
  map <D-k> :CrCompileFile<cr>
  imap <D-k> <esc>:CrCompileFile<cr>
elseif has('win32')
  map <C-F7> :CrCompileFile<cr>
  imap <C-F7> <esc>:CrCompileFile<cr>
endif
" TODO(linuxuser): Suggest a keyboard shortcut and send review to thakis@.
