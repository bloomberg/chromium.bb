#!/usr/bin/env python
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

""" Mirrors a Gerrit repo into GitHub, turning CLs into individual branches.

This script does a bit of git black magic. It does mainly two things:
1) Mirrors all the branches (refs/heads/foo) from Gerrit to Github as-is, taking
   care of propagating also deletions.
2) Rewrites Gerrit CLs (refs/changes/NN/cl_number/patchset_number) as
   Github branches (refs/heads/cl_number) recreating a linear chain of commits
   for each patchset in any given CL.

2. Is the trickier part. The problem is that Gerrit stores each patchset of
each CL as an independent ref, e.g.:
  $ git ls-remote origin
  94df12f950462b55a2257b89d1fad6fac24353f9	refs/changes/10/496410/1
  4472fadddf8def74fd76a66ff373ca1245c71bcc	refs/changes/10/496410/2
  90b8535da0653d8f072e86cef9891a664f4e9ed7	refs/changes/10/496410/3
  2149c215fa9969bb454f23ce355459f28604c545	refs/changes/10/496410/meta

  53db7261268802648d7f6125ae6242db17e7a60d	refs/changes/20/494620/1
  d25e56930486363e0637b0a9debe3ae3ec805207	refs/changes/20/494620/2

Where each ref is base on top of the master branch (or whatever the dev choose).
On GitHub, instead, we want to recreate something similar to the pull-request
model, ending up with one branch per CL, and one commit per patchset.
Also we want to make them non-hidden branch heads (i.e. in the refs/heads/)
name space, because Travis CI does not hooks hidden branches.
In conclusion we want to transform the above into:

refs/changes/496410
  * commit: [CL 496410, Patchset 3] (parent: [CL 496410, Patchset 2])
  * commit: [CL 496410, Patchset 2] (parent: [CL 496410, Patchset 1])
  * commit: [CL 496410, Patchset 1] (parent: [master])
refs/changes/496420
  * commit: [CL 496420, Patchset 2] (parent: [CL 496420, Patchset 1])
  * commit: [CL 496420, Patchset 1] (parent: [master])

"""

import argparse
import collections
import logging
import os
import re
import shutil
import subprocess
import sys
import time
import traceback

from multiprocessing.pool import ThreadPool

CUR_DIR = os.path.dirname(os.path.abspath(__file__))
GIT_UPSTREAM = 'https://android.googlesource.com/platform/external/perfetto/'
GIT_MIRROR = 'git@github.com:catapult-project/perfetto.git'
WORKDIR = os.path.join(CUR_DIR, 'repo')

# Ignores CLs that have a cumulative tree size greater than this. GitHub rightly
# refuses to accept commits that have files that are too big, suggesting to use
# LFS instead.
MAX_TREE_SIZE_MB = 50

# Ignores all CL numbers < this. 913796 roughly maps to end of Feb 2019.
MIN_CL_NUM = 913796

# Max number of concurrent git subprocesses that can be run while generating
# per-CL branches.
NUM_JOBS = 10

# Min delay (in seconds) between two consecutive git poll cycles. This is to
# avoid hitting gerrit API quota limits.
POLL_PERIOD_SEC = 60

# The actual deploy_key is stored into the internal team drive, undef /infra/.
ENV = {'GIT_SSH_COMMAND': 'ssh -i ' + os.path.join(CUR_DIR, 'deploy_key')}


def GitCmd(*args, **kwargs):
  cmd = ['git'] + list(args)
  p = subprocess.Popen(
      cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=sys.stderr,
      cwd=WORKDIR, env=ENV)
  out = p.communicate(kwargs.get('stdin'))[0]
  assert p.returncode == 0, 'FAIL: ' + ' '.join(cmd)
  return out


# Create a git repo that mirrors both the upstream and the mirror repos.
def Setup(args):
  if os.path.exists(WORKDIR):
    if args.no_clean:
      return
    shutil.rmtree(WORKDIR)
  os.makedirs(WORKDIR)
  GitCmd('init', '--bare', '--quiet')
  GitCmd('remote', 'add', 'upstream', GIT_UPSTREAM)
  GitCmd('config', 'remote.upstream.fetch', '+refs/*:refs/remotes/upstream/*')
  GitCmd('remote', 'add', 'mirror', GIT_MIRROR, '--mirror=fetch')


# Returns the SUM(file.size) for file in the given git tree.
def GetTreeSize(tree_sha1):
  raw = GitCmd('ls-tree', '-r', '--long', tree_sha1)
  return sum(int(line.split()[3]) for line in raw.splitlines())


def GetCommit(commit_sha1):
  raw = GitCmd('cat-file', 'commit', commit_sha1)
  return {
      'tree': re.search(r'^tree\s(\w+)$', raw, re.M).group(1),
      'parent': re.search(r'^parent\s(\w+)$', raw, re.M).group(1),
      'author': re.search(r'^author\s(.+)$', raw, re.M).group(1),
      'committer': re.search(r'^committer\s(.+)$', raw, re.M).group(1),
      'message': re.search(r'\n\n(.+)', raw, re.M | re.DOTALL).group(1),
  }


def ForgeCommit(tree, parent, author, committer, message):
  raw = 'tree %s\nparent %s\nauthor %s\ncommitter %s\n\n%s' % (
      tree, parent, author, committer, message)
  out = GitCmd('hash-object', '-w', '-t', 'commit', '--stdin', stdin=raw)
  return out.strip()


# Translates a CL, identified by a (Gerrit) CL number and a list of patchsets
# into a git branch, where all patchsets look like subsequent commits.
# This function must be stateless and idempotent, it's invoked by ThreadPool.
def TranslateClIntoBranch(packed_args):
  cl_num, patchsets = packed_args
  if cl_num < MIN_CL_NUM:
    return
  parent_sha1 = None
  dbg = 'Translating CL %d\n' % cl_num
  for patchset_num, commit_sha1 in sorted(patchsets.items(), key=lambda x:x[0]):
    patchset_data = GetCommit(commit_sha1)
    # Skip Cls that are too big as they would be rejected by GitHub.
    tree_size_bytes = GetTreeSize(patchset_data['tree'])
    if tree_size_bytes > MAX_TREE_SIZE_MB * (1 << 20):
      logging.warning('Skipping CL %s because its too big (%d bytes)',
                      cl_num, tree_size_bytes)
      return
    parent_sha1 = parent_sha1 or patchset_data['parent']
    forged_sha1 = ForgeCommit(
        tree=patchset_data['tree'],
        parent=parent_sha1,
        author=patchset_data['author'],
        committer=patchset_data['committer'],
        message='[Patchset %d] %s' % (patchset_num, patchset_data['message']))
    parent_sha1 = forged_sha1
    dbg += '  %s : patchet %d\n' % (forged_sha1[0:8], patchset_num)
  dbg += '  Final SHA1: %s' % parent_sha1
  logging.debug(dbg)
  return 'refs/heads/changes/%d' % cl_num, parent_sha1


def Sync(args):
  logging.info('Fetching git remotes')
  GitCmd('fetch', '--all', '--quiet')
  all_refs = GitCmd('show-ref')
  future_heads = {}
  current_heads = {}
  changes = collections.defaultdict(dict)

  # List all refs from both repos and:
  # 1. Keep track of all branch heads refnames and sha1s from the (github)
  #    mirror into |current_heads|.
  # 2. Keep track of all upstream (AOSP) branch heads into |future_heads|. Note:
  #    this includes only pure branches and NOT CLs. CLs and their patchsets are
  #    stored in a hidden ref (refs/changes) which is NOT under refs/heads.
  # 3. Keep track of all upstream (AOSP) CLs from the refs/changes namespace
  #    into changes[cl_number][patchset_number].
  for line in all_refs.splitlines():
    ref_sha1, ref = line.split()

    PREFIX = 'refs/heads/'
    if ref.startswith(PREFIX):
      branch = ref[len(PREFIX):]
      current_heads['refs/heads/' + branch] = ref_sha1
      continue

    PREFIX = 'refs/remotes/upstream/heads/'
    if ref.startswith(PREFIX):
      branch = ref[len(PREFIX):]
      future_heads['refs/heads/' + branch] = ref_sha1
      continue

    PREFIX = 'refs/remotes/upstream/changes/'
    if ref.startswith(PREFIX):
      (_, cl_num, patchset) = ref[len(PREFIX):].split('/')
      if not cl_num.isdigit() or not patchset.isdigit():
        continue
      cl_num, patchset = int(cl_num), int(patchset)
      changes[cl_num][patchset] = ref_sha1

  # Now iterate over the upstream (AOSP) CLS and forge a chain of commits,
  # creating one branch refs/heads/changes/cl_number for each set of patchsets.
  # Forging commits is mostly fork() + exec() and I/O bound, parallelism helps
  # significantly to hide those latencies.
  logging.info('Forging per-CL branches')
  pool = ThreadPool(processes=args.jobs)
  for res in pool.imap_unordered(TranslateClIntoBranch, changes.iteritems()):
    if res is None:
      continue
    branch_ref, forged_sha1 = res
    future_heads[branch_ref] = forged_sha1
  pool.close()

  deleted_heads = set(current_heads) - set(future_heads)
  logging.info('current_heads: %d, future_heads: %d, deleted_heads: %d',
               len(current_heads), len(future_heads), len(deleted_heads))

  # Now compute:
  # 1. The set of branches in the mirror (github) that have been deleted on the
  #    upstream (AOSP) repo. These will be deleted also from the mirror.
  # 2. The set of rewritten branches to be updated.
  update_ref_cmd = ''
  for ref_to_delete in deleted_heads:
    update_ref_cmd += 'delete %s\n' % ref_to_delete
  for ref_to_update, ref_sha1 in future_heads.iteritems():
    if current_heads.get(ref_to_update) != ref_sha1:
      update_ref_cmd += 'update %s %s\n' % (ref_to_update, ref_sha1)

  GitCmd('update-ref', '--stdin', stdin=update_ref_cmd)

  if args.push:
    logging.info('Pushing updates')
    GitCmd('push', 'mirror', '--all', '--prune', '--force')
    GitCmd('gc', '--prune=all', '--aggressive', '--quiet')
  else:
    logging.info('Dry-run mode, skipping git push. Pass --push for prod mode.')


def Main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--push', default=False, action='store_true')
  parser.add_argument('--no-clean', default=False, action='store_true')
  parser.add_argument('-j', dest='jobs', default=NUM_JOBS, type=int)
  parser.add_argument('-v', dest='verbose', default=False, action='store_true')
  args = parser.parse_args()

  logging.basicConfig(
      format='%(asctime)s %(levelname)-8s %(message)s',
      level=logging.DEBUG if args.verbose else logging.INFO,
      datefmt='%Y-%m-%d %H:%M:%S')

  logging.info('Setting up git repo one-off')
  Setup(args)
  while True:
    logging.info('------- BEGINNING OF SYNC CYCLE -------')
    Sync(args)
    logging.info('------- END OF SYNC CYCLE -------')
    time.sleep(POLL_PERIOD_SEC)


if __name__ == '__main__':
  sys.exit(Main())
