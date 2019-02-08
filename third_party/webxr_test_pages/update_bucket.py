#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import logging
import os
import re
import subprocess
import sys
import tempfile

# Add third_party directory to the Python import path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import jinja2

# Oldest version of this directory that works for serving. Used to limit git
# history searches.
FIRST_REVISION = '08a37e09f110ab9cb2af3180f054f26a2fd274d6'
TEST_SUBDIR = 'webxr-samples'
INDEX_TEMPLATE = 'bucket_index.html'

# Google Cloud storage bucket destination.
BUCKET = 'gs://chromium-webxr-test'

# URL templates used for creating the index page.
LINK_OMAHAPROXY = ('https://storage.googleapis.com/'
                   'chromium-find-releases-static/index.html#%s')
LINK_CRREV = 'https://crrev.com/%s'

CR_POSITION_RE = re.compile(r'^Cr-Commit-Position:.*\#(\d+)')
BUCKET_COPY_RE = re.compile(r'^r(\d+)')

g_flags = None

def run_command(*args):
  """Runs a shell command and returns output."""
  logging.debug('Executing: %s', args)
  return subprocess.check_output(args)

def run_readonly(*args):
  """Runs command expected to have no side effects, safe for dry runs."""
  return run_command(*args)

def run_modify(*args):
  """Runs command with side effects, skipped for dry runs."""
  if g_flags.dry_run:
    print('Dry-Run:', *args)
    return
  return run_command(*args)

def get_cr_positions():
  """Retrieves list of Cr-Commit-Position entries for local commits"""
  revs = run_readonly('git', 'log', '--format=%H', FIRST_REVISION+'^..', '--',
                      TEST_SUBDIR)
  cr_positions = []
  for rev in revs.splitlines():
    cr_position = None
    msg = run_readonly('git', 'show', '-s', '--format=%B', rev)
    for line in msg.splitlines():
      m = CR_POSITION_RE.match(line)
      if m:
        cr_position = m.group(1)
    cr_positions.append(cr_position)
  return cr_positions

def get_bucket_copies():
  """Retrieves list of test subdirectories from Cloud Storage"""
  copies = []
  dirs = run_readonly('gsutil', 'ls', '-d', BUCKET)
  strip_len = len(BUCKET) + 1
  for rev in dirs.splitlines():
    pos = rev[strip_len:]
    m = BUCKET_COPY_RE.search(pos)
    if m:
      copies.append(m.group(1))
  return copies

def is_working_dir_clean():
  """Checks if the git working directory has unsaved changes"""
  status = run_readonly('git', 'status', '--porcelain', '--untracked-files=no')
  return status.strip() == ''

def write_to_bucket(cr_position):
  """Copies the test directory to Cloud Storage"""
  destination = BUCKET + '/r' + cr_position
  run_modify('gsutil', '-m', 'rsync', '-x', 'media', '-r', './' + TEST_SUBDIR,
          destination)

def write_index():
  """Updates Cloud Storage index.html based on available test copies"""
  cr_positions = get_bucket_copies()
  cr_positions.sort(key=int, reverse=True)
  logging.debug('Index: %s', cr_positions)

  items = []

  for pos in cr_positions:
    rev = 'r' + pos
    links = []
    links.append({'href': '%s/index.html' % rev,
                  'anchor': 'index.html'})
    links.append({'href': LINK_CRREV % pos,
                  'anchor': '[crrev]'})
    links.append({'href': LINK_OMAHAPROXY % rev,
                  'anchor': '[find in releases]'})
    items.append({'text': rev, 'links': links})

  template = jinja2.Template(open(INDEX_TEMPLATE).read())
  content = template.render({'items': items})
  logging.debug('index.html content:\n%s', content)

  with tempfile.NamedTemporaryFile(suffix='.html') as temp:
    temp.write(content)
    temp.seek(0)
    run_modify('gsutil', 'cp', temp.name, BUCKET + '/index.html')

def update_test_copies():
  """Uploads a new test copy if available"""

  if not is_working_dir_clean() and not g_flags.ignore_unclean_status:
    raise Exception('Working directory is not clean, check "git status"')

  cr_positions = get_cr_positions()
  logging.debug('Found git commit positions: %s', cr_positions)
  if not cr_positions:
    raise Exception('No commit positions found')

  latest_cr_position = cr_positions[0]
  if latest_cr_position is None and not g_flags.force_destination_cr_position:
    raise Exception('Top commit has no Cr-Commit-Position. Sync to master?')

  existing_copies = get_bucket_copies()
  logging.debug('Found bucket copies: %s', existing_copies)

  out_cr_position = g_flags.force_destination_cr_position or latest_cr_position

  need_index_update = False
  if out_cr_position in existing_copies:
    logging.info('Destination "r%s" already exists, skipping write',
                 out_cr_position)
  else:
    write_to_bucket(out_cr_position)
    need_index_update = True

  return need_index_update

def main():
  parser = argparse.ArgumentParser(description="""
Copies the current '%s' content to a Google Cloud Storage bucket
subdirectory named after the Cr-Commit-Position, and writes an index.html file
linking to the known test directories.""" % TEST_SUBDIR)

  parser.add_argument('-v', '--verbose', action="store_true",
                      help="Print debugging info")
  parser.add_argument('-n', '--dry-run', action="store_true",
                      help="Don't run state-changing commands")
  parser.add_argument('--update-index-only', action="store_true",
                      help=("Generate a new index.html file based on already "
                            "existing directories, ignoring local changes"))
  parser.add_argument('--ignore-unclean-status', action="store_true",
                      help=("Proceed with copy even if there are uncommitted "
                            "local changes in the git working directory"))
  parser.add_argument('--force-destination-cr-position',
                      help=("Force writing current content to the specified "
                            "destination CR position instead of determining "
                            "the name based on local git history, bypassing "
                            "history sanity checks"))
  parser.add_argument('--bucket', default=BUCKET,
                      help=("Destination Cloud Storage location, including "
                            "'gs://' prefix"))

  global g_flags
  g_flags = parser.parse_args()

  if g_flags.verbose:
    logging.basicConfig(level=logging.DEBUG)

  if not os.path.isdir(TEST_SUBDIR):
    raise Exception('Must be run from webxr_test_pages directory')

  need_index_update = False
  if g_flags.update_index_only:
    need_index_update = True
  else:
    need_index_update = update_test_copies()

  # Create an index.html file covering all found test copies.
  if need_index_update:
    write_index()
  else:
    logging.info('No changes, skipping index update.')


if __name__ == '__main__':
  main()
