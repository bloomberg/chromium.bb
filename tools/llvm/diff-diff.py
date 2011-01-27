#!/usr/bin/python
# Copyright 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import sys
import subprocess
import os
import tempfile

def main():
  if len(sys.argv) not in (2,3):
    print "Syntax: %s <repo_dir> [merge_rev]" % (sys.argv[0])
    print "If merge_rev is omitted, this script produces a"
    print "diff-diff of the merge in the working directory."
    sys.exit(0)

  repo_dir = sys.argv[1]
  merge_rev = None
  if len(sys.argv) == 3:
    merge_rev = sys.argv[2]

  diffdiff(repo_dir, merge_rev)
  return 0

def diffdiff(repo_dir, merge_rev = None):
  """ Print the 'diff diff' of a merge.

      This is roughly equivalent to doing:
        hg diff vendor:pnacl-sfi > tmp1  (before the merge)
        hg diff vendor:pnacl-sfi > tmp2  (after the merge)
        diff tmp1 tmp2                   (this is the diff-diff)

      If merge_rev is None, prints the diff-diff of the
      current merge in progress (in the working directory).
      If merge_rev is specified, the diff-diff of an previous
      (already committed) merge is generated."""

  entries = GetMercurialLog(repo_dir)

  if merge_rev:
    if merge_rev not in entries:
      Fatal("Can't find revision '%s'" % merge_rev)
    cur = entries[merge_rev]
    if len(cur['parent']) != 2:
      Fatal("Specified revision is not a merge!")
    p1,p2 = cur['parent']
  else:
    cur = None
    p1,p2 = GetWorkingDirParents(entries, repo_dir)

  # Make p1 the "vendor" branch parent
  if p2['branch'] == 'vendor':
    p1,p2 = p2,p1

  # Identify and verify the situation
  if cur:
    assert(cur['branch'] == 'pnacl-sfi')
  assert(p1['branch'] == 'vendor')
  assert(p2['branch'] == 'pnacl-sfi')
  assert(len(p1['parent']) == 1)
  prev_vendor = p1['parent'][0]
  prev_merge = FindPreviousMerge(p2)
  assert(prev_vendor['branch'] == 'vendor')
  assert(prev_merge['branch'] == 'pnacl-sfi')
  assert(prev_vendor in prev_merge['parent'])

  diff1 = MakeTemp(hg_diff(repo_dir, prev_vendor, p2))
  diff2 = MakeTemp(hg_diff(repo_dir, p1, cur))

  out,err,ret = Run(None, ['diff',diff1,diff2], errexit=False)
  if ret == 2:
    Fatal("diff failed (%d): %s" % (ret, err))
  if ret == 0:
    Fatal("diff output empty")
  if ret != 1:
    Fatal("Unexpected return value from diff (%d)" % ret)
  sys.stdout.write(out)

  DeleteTemp(diff1)
  DeleteTemp(diff2)
  return 0

def hg_diff(dir, r1, r2):
  assert(r1 is not None)
  if r2 is None:
    return Run(dir, "hg diff -r %s" % r1['changeset'])
  else:
    return Run(dir, "hg diff -r %s:%s" % (r1['changeset'], r2['changeset']))

def MakeTemp(contents):
  """ Create a temporary file with specific contents,
      and return the filename """

  f,tempname = tempfile.mkstemp(prefix='diff-diff-tmp')
  f = open(tempname, 'w')
  f.write(contents)
  f.close()
  return tempname

def DeleteTemp(tempname):
  os.remove(tempname)

def Fatal(msg):
  print msg
  sys.exit(1)

def Run(dir, args, errexit=True):
  if isinstance(args, str):
    args = args.split(' ')
  p = subprocess.Popen(args, cwd=dir,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
  out, err = p.communicate()
  if not errexit:
    return out, err, p.returncode
  if p.returncode:
    Fatal("%s (%d): (STDERR): %s" % (' '.join(args), p.returncode, err))
  if len(err) > 0:
    Fatal("%s (STDERR): %s" % (' '.join(args), err))
  return out


def GetWorkingDirParents(entries, dir):
  out = Run(dir, 'hg identify -i')
  revs = out.split('+')

  # If this is a merge, hg identify -i returns:
  #   changeset1+changeset2+
  if len(revs) != 3:
    Fatal("Working directory is not a merge. Please specify a revision!")
  return entries[revs[0]], entries[revs[1]]

def GetMercurialLog(dir):
  """ Read the Mercurial log into a dictionary of log entries """
  entries = []
  out = Run(dir, 'hg log')
  blocks = out.strip().split('\n\n')
  for b in blocks:
    entry = dict()
    entry['parent'] = []
    for line in b.split('\n'):
      sep = line.find(':')
      tag = line[0:sep]
      val = line[sep+1:].strip()
      if tag == 'parent':
        entry[tag] = entry[tag] + [ val ]
      else:
        entry[tag] = val
    entries.append(entry)

  # Remove revision ids
  for entry in entries:
    entry['changeset'] = entry['changeset'].split(':')[1]
    entry['parent'] = [ s.split(':')[1] for s in entry['parent'] ]

  # Add missing parent info for consecutive entries
  last_entry = []
  for entry in reversed(entries):
    if len(entry['parent']) == 0:
      entry['parent'] = last_entry
    last_entry = [ entry['changeset'] ]

  # Convert to a dictionary indexed by changeset
  entries = dict([(entry['changeset'], entry) for entry in entries])

  # Convert the parents to direct entry references
  for cs, entry in entries.iteritems():
    entry['parent'] = [ entries[r] for r in entry['parent'] ]

  return entries

def FindPreviousMerge(entry):
  p = entry
  while len(p['parent']) == 1:
    p = p['parent'][0]
  if len(p['parent']) != 2:
    Fatal("Couldn't find previous merge for %s!" % entry['changeset'])
  return p

if __name__ == '__main__':
  sys.exit(main())
