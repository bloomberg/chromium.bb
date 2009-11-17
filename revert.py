#!/usr/bin/python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Tool to quickly revert a change.

import exceptions
import optparse
import os
import sys
import xml

import gcl
import gclient
import gclient_scm
import gclient_utils

class ModifiedFile(exceptions.Exception):
  pass
class NoModifiedFile(exceptions.Exception):
  pass
class NoBlameList(exceptions.Exception):
  pass
class OutsideOfCheckout(exceptions.Exception):
  pass


def UniqueFast(list):
  list = [item for item in set(list)]
  list.sort()
  return list


def GetRepoBase():
  """Returns the repository base of the root local checkout."""
  info = gclient_scm.CaptureSVNInfo('.')
  root = info['Repository Root']
  url = info['URL']
  if not root or not url:
    raise exceptions.Exception("I'm confused by your checkout")
  if not url.startswith(root):
    raise exceptions.Exception("I'm confused by your checkout", url, root)
  return url[len(root):] + '/'


def CaptureSVNLog(args):
  command = ['log', '--xml']
  if args:
    command += args
  output = gclient_scm.CaptureSVN(command)
  dom = gclient_utils.ParseXML(output)
  entries = []
  if dom:
    # /log/logentry/
    #   @revision
    #   author|date
    #   paths/
    #     path (@kind&@action)
    for node in dom.getElementsByTagName('logentry'):
      paths = []
      for path in node.getElementsByTagName('path'):
        item = {
          'kind': path.getAttribute('kind'),
          'action': path.getAttribute('action'),
          'path': path.firstChild.nodeValue,
        }
        paths.append(item)
      entry = {
        'revision': int(node.getAttribute('revision')),
        'author': gclient_utils.GetNamedNodeText(node, 'author'),
        'date': gclient_utils.GetNamedNodeText(node, 'date'),
        'paths': paths,
      }
      entries.append(entry)
  return entries


def Revert(revisions, force=False, commit=True, send_email=True, message=None,
           reviewers=None):
  """Reverts many revisions in one change list.

  If force is True, it will override local modifications.
  If commit is True, a commit is done after the revert.
  If send_mail is True, a review email is sent.
  If message is True, it is used as the change description.
  reviewers overrides the blames email addresses for review email."""

  # Use the oldest revision as the primary revision.
  changename = "revert%d" % revisions[len(revisions)-1]
  if not force and os.path.exists(gcl.GetChangelistInfoFile(changename)):
    print "Error, change %s already exist." % changename
    return 1

  # Move to the repository root and make the revision numbers sorted in
  # decreasing order.
  local_root = gcl.GetRepositoryRoot()
  os.chdir(local_root)
  revisions.sort(reverse=True)
  revisions_string = ",".join([str(rev) for rev in revisions])
  revisions_string_rev = ",".join([str(-rev) for rev in revisions])

  # Get all the modified files by the revision. We'll use this list to optimize
  # the svn merge.
  logs = []
  for revision in revisions:
    logs.extend(CaptureSVNLog(["-r", str(revision), "-v"]))

  files = []
  blames = []
  repo_base = GetRepoBase()
  for log in logs:
    for file in log['paths']:
      file_name = file['path']
      # Remove the /trunk/src/ part. The + 1 is for the last slash.
      if not file_name.startswith(repo_base):
        raise OutsideOfCheckout(file_name)
      files.append(file_name[len(repo_base):])
    blames.append(log['author'])

  # On Windows, we need to fix the slashes once they got the url part removed.
  if sys.platform == 'win32':
    # On Windows, gcl expect the correct slashes.
    files = [file.replace('/', os.sep) for file in files]

  # Keep unique.
  files = UniqueFast(files)
  blames = UniqueFast(blames)
  if not reviewers:
    reviewers = blames
  else:
    reviewers = UniqueFast(reviewers)

  # Make sure there's something to revert.
  if not files:
    raise NoModifiedFile
  if not reviewers:
    raise NoBlameList

  if blames:
    print "Blaming %s\n" % ",".join(blames)
  if reviewers != blames:
    print "Emailing %s\n" % ",".join(reviewers)
  print "These files were modified in %s:" % revisions_string
  print "\n".join(files)
  print ""

  # Make sure these files are unmodified with svn status.
  status = gclient_scm.scm.SVN.CaptureStatus(files)
  if status:
    if force:
      # TODO(maruel): Use the tool to correctly revert '?' files.
      gcl.RunShell(["svn", "revert"] + files)
    else:
      raise ModifiedFile(status)
  # svn up on each of these files
  gcl.RunShell(["svn", "up"] + files)

  files_status = {}
  # Extract the first level subpaths. Subversion seems to degrade
  # exponentially w.r.t. repository size during merges. Working at the root
  # directory is too rough for svn due to the repository size.
  roots = UniqueFast([file.split(os.sep)[0] for file in files])
  for root in roots:
    # Is it a subdirectory or a files?
    is_root_subdir = os.path.isdir(root)
    need_to_update = False
    if is_root_subdir:
      os.chdir(root)
      file_list = []
      # List the file directly since it is faster when there is only one file.
      for file in files:
        if file.startswith(root):
          file_list.append(file[len(root)+1:])
      if len(file_list) > 1:
        # Listing multiple files is not supported by svn merge.
        file_list = ['.']
        need_to_update = True
    else:
      # Oops, root was in fact a file in the root directory.
      file_list = [root]
      root = "."

    print "Reverting %s in %s/" % (revisions_string, root)
    if need_to_update:
      # Make sure '.' revision is high enough otherwise merge will be
      # unhappy.
      retcode = gcl.RunShellWithReturnCode(['svn', 'up', '.', '-N'])[1]
      if retcode:
        print 'svn up . -N failed in %s/.' % root
        return retcode

    command = ["svn", "merge", "-c", revisions_string_rev]
    command.extend(file_list)
    (output, retcode) = gcl.RunShellWithReturnCode(command, print_output=True)
    if retcode:
      print "'%s' failed:" % command
      return retcode

    # Grab the status
    lines = output.split('\n')
    for line in lines:
      if line.startswith('---'):
        continue
      if line.startswith('Skipped'):
        print ""
        raise ModifiedFile(line[9:-1])
      # Update the status.
      status = line[:5] + '  '
      file = line[5:]
      if is_root_subdir:
        files_status[root + os.sep + file] = status
      else:
        files_status[file] = status

    if is_root_subdir:
      os.chdir('..')

  # Transform files_status from a dictionary to a list of tuple.
  files_status = [(files_status[file], file) for file in files]

  description = "Reverting %s." % revisions_string
  if message:
    description += "\n\n"
    description += message
  # Don't use gcl.Change() since it prompts the user for infos.
  change_info = gcl.ChangeInfo(changename, 0, 0, description, files_status,
                               local_root)
  change_info.Save()

  upload_args = ['--no_presubmit', '-r', ",".join(reviewers)]
  if send_email:
    upload_args.append('--send_mail')
  if commit:
    upload_args.append('--no_try')
  gcl.UploadCL(change_info, upload_args)

  retcode = 0
  if commit:
    gcl.Commit(change_info, ['--no_presubmit', '--force'])
    # TODO(maruel):  gclient sync (to leave the local checkout in an usable
    # state)
    retcode = gclient.Main(["gclient.py", "sync"])
  return retcode


def Main(argv):
  usage = (
"""%prog [options] [revision numbers to revert]
Revert a set of revisions, send the review to Rietveld, sends a review email
and optionally commit the revert.""")

  parser = optparse.OptionParser(usage=usage)
  parser.add_option("-c", "--commit", default=False, action="store_true",
                    help="Commits right away.")
  parser.add_option("-f", "--force", default=False, action="store_true",
                    help="Forces the local modification even if a file is "
                         "already modified locally.")
  parser.add_option("-n", "--no_email", default=False, action="store_true",
                    help="Inhibits from sending a review email.")
  parser.add_option("-m", "--message", default=None,
                    help="Additional change description message.")
  parser.add_option("-r", "--reviewers", action="append",
                    help="Reviewers to send the email to. By default, the list "
                         "of commiters is used.")
  if len(argv) < 2:
    parser.print_help()
    return 1;

  options, args = parser.parse_args(argv)
  revisions = []
  try:
    for item in args[1:]:
      revisions.append(int(item))
  except ValueError:
    parser.error("You need to pass revision numbers.")
  if not revisions:
    parser.error("You need to pass revision numbers.")
  retcode = 1
  try:
    if not os.path.exists(gcl.GetInfoDir()):
      os.mkdir(gcl.GetInfoDir())
    retcode = Revert(revisions, options.force, options.commit,
                     not options.no_email, options.message, options.reviewers)
  except NoBlameList:
    print "Error: no one to blame."
  except NoModifiedFile:
    print "Error: no files to revert."
  except ModifiedFile, e:
    print "You need to revert these files since they were already modified:"
    print "".join(e.args)
    print "You can use the --force flag to revert the files."
  except OutsideOfCheckout, e:
    print "Your repository doesn't contain ", str(e)

  return retcode


if __name__ == "__main__":
  sys.exit(Main(sys.argv))
