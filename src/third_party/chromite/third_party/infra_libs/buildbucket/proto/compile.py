#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Copies .proto files from buildbucket Go.

This ugly script copies files from
../../../go/src/go.chromium.org/luci/buildbucket/proto/
to this dir and modifies them to make them usable from Python.

The reason they are not usable as is is cproto requires protos in Go to use
go-style absolute import paths, e.g.
  import "github.com/user/repo/path/to/file.proto"
which results in Python import
  import github.com.user.repo.path.to.file
"""

import os
import re
import subprocess
import shutil
import tempfile

THIS_DIR = os.path.dirname(os.path.abspath(__file__))

LUCI_GO_DIR = os.path.normpath(
    os.path.join(
        THIS_DIR,
        *('../../../../go/src/go.chromium.org/luci').split('/')
    )
)
RPC_PROTO_DIR = os.path.join(LUCI_GO_DIR, 'grpc', 'proto')
BUILDBUCKET_PROTO_DIR = os.path.join(LUCI_GO_DIR, 'buildbucket', 'proto')


def modify_proto(src, dest):
  with open(src) as f:
    contents = f.read()

  # Rewrite imports.
  contents = re.sub(
      r'import "go\.chromium\.org/luci/buildbucket/proto/([^"]+)";',
      r'import "\1";', contents
  )

  with open(dest, 'w') as f:
    f.write(contents)


def find_files(path, suffix=''):
  return [f for f in os.listdir(path) if f.endswith(suffix)]


def compile_protos(src_dir, dest_dir):
  tmpd = tempfile.mkdtemp(suffix='buildbucket-proto')

  proto_files = find_files(src_dir, suffix='.proto')
  # Copy modified .proto files into temp dir.
  for f in proto_files:
    modify_proto(os.path.join(src_dir, f), os.path.join(tmpd, f))

  # Compile them.
  args = [
      'protoc',
      '-I',
      RPC_PROTO_DIR,
      '-I',
      tmpd,
      '--python_out=.',
      '--prpc-python_out=.',
  ]
  args += [os.path.join(tmpd, f) for f in proto_files]
  subprocess.check_call(args, cwd=tmpd)
  pb2_files = find_files(tmpd, suffix='_pb2.py')

  # YAPF them.
  # TODO(nodir): remove this when
  # https://github.com/google/yapf/issues/357 is fixed.
  shutil.copyfile(
      os.path.join(THIS_DIR, '..', '.style.yapf'),
      os.path.join(tmpd, '.style.yapf')
  )
  args = ['yapf', '-i'] + pb2_files
  subprocess.check_call(args, cwd=tmpd)

  # Copy _pb2.py files to dest dir.
  for f in pb2_files:
    shutil.copyfile(os.path.join(tmpd, f), os.path.join(dest_dir, f))


def main():
  compile_protos(BUILDBUCKET_PROTO_DIR, THIS_DIR)
  compile_protos(
      os.path.join(BUILDBUCKET_PROTO_DIR, 'config'),
      os.path.join(THIS_DIR, 'config')
  )


if __name__ == '__main__':
  main()
