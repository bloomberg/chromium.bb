#!/usr/bin/env python3

# Copyright (C) 2021 Bloomberg L.P. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import json
import os
import queue
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import traceback
import uuid


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
COMPRESSOR = os.path.join(SCRIPT_DIR, '7zr.exe')
ROOT_DIR = os.path.normpath(os.path.join(SCRIPT_DIR,
                                         os.path.pardir,
                                         os.path.pardir,
                                         os.path.pardir))
DEFAULT_MODE = (stat.S_IRUSR | stat.S_IWUSR |
                stat.S_IRGRP | stat.S_IWGRP |
                stat.S_IROTH | stat.S_IWOTH)

nongit_dir = os.path.join(ROOT_DIR, 'nongit')


def find_python27():
  for path in os.environ['PATH'].split(os.pathsep):
    for fname in ['python.exe', 'python2.exe', 'python2.7.exe']:
      candidate = os.path.join(path, fname)
      if not os.path.isfile(candidate):
        continue
      p = subprocess.run(
          [candidate, '--version'], capture_output=True, check=True,
          encoding='utf-8')
      if p.stdout.startswith('Python 2.7') or p.stderr.startswith('Python 2.7'):
        return candidate
  return None


def start_thread(target):
  thread = threading.Thread(target=target)
  thread.start()
  return thread


def ensure_nongit_dir(repo, branch):
  if not os.path.isdir(nongit_dir):
    os.makedirs(nongit_dir)
    subprocess.run(['git', 'init', '--bare'], check=True, cwd=nongit_dir)
  subprocess.run(
      [
        'git',
        'fetch',
        repo,
        '+refs/heads/%s:refs/heads/%s' % (branch, branch),
      ],
      check=True,
      cwd=nongit_dir)


def load_nongit_manifest(commit):
  p = subprocess.run(
      ['git', 'show', '%s:.nongit' % commit],
      capture_output=True,
      encoding='utf-8',
      cwd=ROOT_DIR)
  return json.loads(p.stdout) if 0 == p.returncode else None


def load_file_metadata(manifest):
  files = {}
  for dirname, metadatas in manifest['files'].items():
    for basename, metadata in metadatas.items():
      if isinstance(metadata, str):
        metadata = {'sha': metadata}
      files['%s/%s' % (dirname, basename)] = metadata
  return files


def get_major_version(commit):
  p = subprocess.run(
      ['git', 'show', '%s:src/chrome/VERSION' % commit],
      capture_output=True,
      check=True,
      encoding='utf-8',
      cwd=ROOT_DIR)
  return int(p.stdout.splitlines()[0][6:])  # strip "MAJOR="


def rmtree_force(directory):
  def onerror(unused_func, path, unused_excinfo):
    os.chmod(path, stat.S_IREAD | stat.S_IWRITE)
    if os.path.isdir(path):
      rmtree_force(path, onerror=onerror)
    else:
      os.remove(path)
  shutil.rmtree(directory, onerror=onerror)


def rm_force(path):
  os.chmod(path, stat.S_IREAD | stat.S_IWRITE)
  if os.path.isdir(path):
    rmtree_force(path)
  else:
    os.remove(path)


def rm_clean_dirs(path):
  rm_force(path)
  try:
    os.removedirs(os.path.dirname(path))
  except Exception:
    pass


def catfile_writefile(stdout, path, chunk_size=4096):
  first_line = b''
  while True:
    char = stdout.read(1)
    if char == b'\n':
      break
    first_line += char
  _, _, size = first_line.split(b' ')
  size = int(size.decode('utf-8'))
  remaining = size
  with open(path, 'wb') as f:
    while remaining > 0:
      buf = stdout.read(min(chunk_size, remaining))
      remaining -= len(buf)
      f.write(buf)
  char = stdout.read(1)
  if char != b'\n':
    raise Exception('Expected terminating LF, got: %s' % char)


def handle_catfile_output(stdout, path, metadata):
  root_path = os.path.join(ROOT_DIR, path)
  if os.path.exists(root_path):
    rm_force(root_path)
  os.makedirs(os.path.dirname(root_path), exist_ok=True)
  mode = metadata.get('mode', DEFAULT_MODE)
  filetype = metadata.get('type', None)
  if '7z' == filetype:
    with tempfile.TemporaryDirectory() as tempdir:
      compressed_path = os.path.join(tempdir, uuid.uuid4().hex + '.7z')
      catfile_writefile(stdout, compressed_path)
      p = subprocess.run(
          [
            COMPRESSOR,
            'x',
            '-y',
            os.path.basename(compressed_path),
          ],
          capture_output=True,
          cwd=tempdir)
      if 0 != p.returncode:
        raise Exception('Failed: %s' % p)
      decompressed_path = [
          os.path.join(tempdir, x)
          for x in os.listdir(tempdir)
          if x != os.path.basename(compressed_path)][0]
      shutil.move(decompressed_path, root_path)
    os.utime(root_path)
  else:
    catfile_writefile(stdout, root_path)
  os.chmod(root_path, mode)


def restore_changed_files(nongit_repo, nongit_branch, changed_files):
  ensure_nongit_dir(args.nongit_repo, to_manifest['branch'])
  p_catfile = subprocess.Popen(
      [
        'git',
        'cat-file',
        '--batch',
      ],
      stdin=subprocess.PIPE,
      stdout=subprocess.PIPE,
      cwd=nongit_dir)

  catfile_queue = queue.Queue()
  def read_catfile_output():
    try:
      total = len(changed_files)
      print('Checking out %d files from nongit repo:' % total)
      sys.stdout.flush()
      done = 0
      prev_stars = 0
      sys.stdout.write('  [' + '.' * 40 + ']')
      sys.stdout.flush()
      while True:
        item = catfile_queue.get()
        if not item:
          break
        handle_catfile_output(p_catfile.stdout, *item)
        done += 1
        stars = int((done * 40) / total)
        if stars != prev_stars:
          sys.stdout.write('\r  [' + '*' * stars + '.' * (40-stars) + ']')
          sys.stdout.flush()
      print('')
      sys.stdout.flush()
    except Exception:
      traceback.print_exc()
      os._exit(1)

  read_catfile_thread = start_thread(read_catfile_output)
  for path, metadata in changed_files.items():
    catfile_queue.put((path, metadata))
    line = '%s\n' % metadata['sha']
    p_catfile.stdin.write(line.encode('utf-8'))
  catfile_queue.put(None)
  p_catfile.stdin.close()
  read_catfile_thread.join()


def get_toolchain_if_necessary(from_commit, to_commit):
  if int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1')) == 0:
    return
  bpcdev_path = os.environ.get('BPCDEV_PATH', None)
  if not bpcdev_path:
    return
  toolchain_dir = os.path.join(bpcdev_path, 'wtk2_toolchain')
  if not os.path.isdir(toolchain_dir):
    return
  current_major_version = get_major_version(to_commit)

  if os.path.isdir(os.path.join(ROOT_DIR, 'src', 'third_party', 'depot_tools',
                                'win_toolchain', 'vs_files')):
    # Skip toolchain update if we didn't change major version.  We only skip
    # here if we already have a vs_files directory.  If we do not have a
    # vs_files directory, that means this is an initial clone and we should go
    # ahead and install the sdk.
    if from_commit:
      previous_major_version = get_major_version(from_commit)
      if previous_major_version == current_major_version:
        return

  latest_version_found = None
  latest_version_number_found = 0
  for version in os.listdir(toolchain_dir):
    version_number = int(re.search('^\d+', version).group())
    if (version_number > current_major_version or
        version_number < latest_version_number_found):
      continue
    latest_version_found = version
    latest_version_number_found = version_number
  if latest_version_found is None:
    return
  toolchain_base_url = os.path.normpath(
      os.path.join(toolchain_dir, latest_version_found))
  env = {k:v for k,v in os.environ.items()}
  env['DEPOT_TOOLS_WIN_TOOLCHAIN_BASE_URL'] = toolchain_base_url
  toolchain_hash = [os.path.splitext(x)[0]
                    for x in os.listdir(toolchain_base_url)
                    if x.endswith('.zip')][0]
  json_data_file = os.path.join(ROOT_DIR, 'src', 'build', 'win_toolchain.json')
  subprocess.run(
      [
        find_python27(),
        os.path.join(ROOT_DIR, 'src', 'third_party', 'depot_tools',
                     'win_toolchain', 'get_toolchain_if_necessary.py'),
        '--output-json', json_data_file,
        toolchain_hash,
      ],
      check=True,
      env=env,
      cwd=os.path.join(ROOT_DIR, 'src'))


parser = argparse.ArgumentParser()
parser.add_argument('--from-commit', help='From commit')
parser.add_argument('--to-commit', default='HEAD', help='To commit')
parser.add_argument('--nongit-repo', help='Nongit repo')
args = parser.parse_args(sys.argv[1:])

from_files = {}
if args.from_commit:
  from_manifest = load_nongit_manifest(args.from_commit)
  from_files = load_file_metadata(from_manifest) if from_manifest else {}

to_manifest = load_nongit_manifest(args.to_commit)
if to_manifest is None:
  raise Exception('Failed to load .nongit manifest')
to_files = load_file_metadata(to_manifest)

# Delete files that no longer exist.
for path, metadata in from_files.items():
  if path not in to_files:
    root_path = os.path.join(ROOT_DIR, path)
    if os.path.isfile(root_path):
      rm_clean_dirs(root_path)

changed_files = {
    path: metadata
    for path, metadata in to_files.items()
    if (path not in from_files or
        from_files[path]['sha'] != metadata['sha'] or
        not os.path.exists(os.path.join(ROOT_DIR, path)))
}

if changed_files:
  restore_changed_files(args.nongit_repo, to_manifest['branch'], changed_files)

get_toolchain_if_necessary(args.from_commit, args.to_commit)
