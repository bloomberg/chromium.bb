#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import subprocess
import os
from os import path
from datetime import date, timedelta
from collections import namedtuple, defaultdict, Counter

Commit = namedtuple('Commit', ['hash', 'author', 'commit_date', 'dirs'])

# Takes a git command arguments and runs it returning the output (throwing an
# exception on error).
def _RunGitCommand(options, cmd_args):
  repo_path = os.path.join(options.repo_path, '.git')
  cmd = ['git', '--git-dir', repo_path] + cmd_args
  return subprocess.check_output(cmd)


# return true if this author is a chromium dev and is not a bot. Pretty naive,
# looks for roller in the username.
def _IsValidAuthor(author):
  return author.find('@chromium.org') > -1 and author.find('roller') == -1


# Get a list of commits from the repo and return a nested dictionary
# directory -> author -> num_commits
def processAllCommits(options):
  date_limit = date.today() - timedelta(days=options.days_ago)
  format_string = "%h,%ae,%cI"
  cmd_args = [
    'log',
    '--since', date_limit.isoformat(),
    '--name-only',
    '--pretty=format:%s'%format_string,
  ]

  # has to be last arg
  if options.subdirectory:
    cmd_args += ['--', options.subdirectory]

  output = _RunGitCommand(options, cmd_args)
  current_commit = None
  author = None
  directory_authors = defaultdict(Counter)
  for line in output.splitlines():
    if current_commit is None:
      commit_hash, author, commit_date = line.split(",")
      current_commit = Commit(hash=commit_hash, author=author,
                              commit_date=commit_date, dirs=set())
    else:
      if line == '': # all commit details read
        if _IsValidAuthor(current_commit.author):
          for directory in current_commit.dirs:
            if directory == '':
              continue
            directory_authors[directory][author] += 1
        current_commit = None
      else:
        current_commit.dirs.add(os.path.dirname(line))
  return directory_authors


# Return a list of owners for a given directory by reading OWNERS files in its
# ancestors. The parsing of OWNERS files is pretty naive, it does not handle
# file imports.
def _GetOwners(options, repo_subdir):
  directory_path = os.path.join(options.repo_path, repo_subdir)
  owners_path = os.path.join(directory_path, 'OWNERS')
  owners = []
  while directory_path != '':
    if os.path.isfile(owners_path):
      with open(owners_path) as f:
        owners.extend([line.strip() for line in f.readlines() if
                       line.find('@chromium.org') > -1])
    directory_path = path.dirname(directory_path)
    owners_path = os.path.join(directory_path, 'OWNERS')
  return owners


# Return the number of commits for a given directory
def _CountDirectoryCommits(directory_authors, directory):
  return sum(directory_authors[directory].values())


# Given a directory merge all its children's commits into its own, then delete
# each child subdirectory's entry if it has too few commits.
def _GroupToParentDirectory(options, directory_authors, parent):
  global DIRECTORY_AUTHORS
  parent_path = path.join(options.repo_path, parent)

  for entry in os.listdir(parent_path):
    if path.isdir(os.path.join(parent_path, entry)):
      entry_dir = path.join(parent, entry)
      directory_authors[parent].update(directory_authors[entry_dir])
      commit_count = _CountDirectoryCommits(directory_authors, entry_dir)
      if  commit_count < options.dir_commit_limit:
        directory_authors.pop(entry_dir)


# Merge directories with too few commits into their parent directory. This
# method changes the directory_authors dict in-place.
def mergeDirectories(options, directory_authors):
  changed = False
  for directory in directory_authors.keys():
    if not path.exists(path.join(options.repo_path, directory)):
      del directory_authors[directory]
      continue
    num_commits = _CountDirectoryCommits(directory_authors, directory)
    if num_commits == 0:
      continue
    elif num_commits < options.dir_commit_limit:
      parent = os.path.dirname(directory)
      _GroupToParentDirectory(options, directory_authors, parent)
      changed = True
  return changed


# Retrieves a set of authors that should not be suggested for a directory
def _GetIgnoredAuthors(options, repo_subdir):
  if options.ignore_authors:
    ignored_authors = set(map(str.strip, options.ignore_authors.split(',')))
  else:
    ignored_authors = set()
  ignored_authors.update(_GetOwners(options, repo_subdir))
  return ignored_authors


# Prints out a list of suggested new owners for each directory with a high
# enough commit count.
def outputSuggestions(options, directory_authors):
  for directory, authors in sorted(directory_authors.iteritems()):
    commit_count = _CountDirectoryCommits(directory_authors, directory)
    if commit_count < options.dir_commit_limit:
      continue
    ignored_authors = _GetIgnoredAuthors(options, directory)
    suggestions = [(a,c) for a,c in authors.most_common()
                   if a not in ignored_authors and c >= options.author_cl_limit]
    print "%s: %d commits in the last %d days" % \
        (directory, commit_count, options.days_ago)
    for author, commit_count in suggestions[:options.max_suggestions]:
      print author, commit_count
    print


# main 2.0
def do(options):
  directory_authors = processAllCommits(options)
  while mergeDirectories(options, directory_authors):
    pass
  outputSuggestions(options, directory_authors)


def main():
  parser = argparse.ArgumentParser(
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('repo_path')
  parser.add_argument('--days-ago', help='Number of days of history to search'
                      ' through.', default=365)
  parser.add_argument('--subdirectory', help='Limit to this subdirectory')
  parser.add_argument('--ignore-authors', help='Ignore this comma separated'
                      ' list of authors')
  parser.add_argument('--max-suggestions', help='Maximum number of suggested'
                      ' authors per directory.', default=5)
  parser.add_argument('--author-cl-limit', help='Do not suggest authors who'
                      ' have commited less than this to the directory.',
                      default=10)
  parser.add_argument('--dir-commit-limit', help='Merge directories with less'
                      ' than this number of commits into their parent'
                      ' directory.', default=100)
  options = parser.parse_args()
  do(options)


if __name__ == '__main__':
  main()
