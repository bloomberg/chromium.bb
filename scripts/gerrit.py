#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command line interface to Gerrit-on-borg instances.

Internal Note:
To expose a function directly to the command line interface, name your function
with the prefix "UserAct".
"""

from __future__ import print_function

import inspect
import os
import re

from chromite.cbuildbot import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import gerrit
from chromite.lib import gob_util
from chromite.lib import terminal


COLOR = None

# Map the internal names to the ones we normally show on the web ui.
GERRIT_APPROVAL_MAP = {
    'COMR': ['CQ', 'Commit Queue   ',],
    'CRVW': ['CR', 'Code Review    ',],
    'SUBM': ['S ', 'Submitted      ',],
    'TBVF': ['TV', 'Trybot Verified',],
    'VRIF': ['V ', 'Verified       ',],
}

# Order is important -- matches the web ui.  This also controls the short
# entries that we summarize in non-verbose mode.
GERRIT_SUMMARY_CATS = ('CR', 'CQ', 'V',)


def red(s):
  return COLOR.Color(terminal.Color.RED, s)


def green(s):
  return COLOR.Color(terminal.Color.GREEN, s)


def blue(s):
  return COLOR.Color(terminal.Color.BLUE, s)


def limits(cls):
  """Given a dict of fields, calculate the longest string lengths

  This allows you to easily format the output of many results so that the
  various cols all line up correctly.
  """
  lims = {}
  for cl in cls:
    for k in cl.keys():
      # Use %s rather than str() to avoid codec issues.
      # We also do this so we can format integers.
      lims[k] = max(lims.get(k, 0), len('%s' % cl[k]))
  return lims


def GetApprovalSummary(_opts, cls):
  """Return a dict of the most important approvals"""
  approvs = dict([(x, '') for x in GERRIT_SUMMARY_CATS])
  if 'approvals' in cls['currentPatchSet']:
    for approver in cls['currentPatchSet']['approvals']:
      cats = GERRIT_APPROVAL_MAP.get(approver['type'])
      if not cats:
        cros_build_lib.Warning('unknown gerrit approval type: %s',
                               approver['type'])
        continue
      cat = cats[0].strip()
      val = int(approver['value'])
      if not cat in approvs:
        # Ignore the extended categories in the summary view.
        continue
      elif approvs[cat] is '':
        approvs[cat] = val
      elif val < 0:
        approvs[cat] = min(approvs[cat], val)
      else:
        approvs[cat] = max(approvs[cat], val)
  return approvs


def PrintCl(opts, cls, lims, show_approvals=True):
  """Pretty print a single result"""
  if not lims:
    lims = {'url': 0, 'project': 0}

  status = ''
  if show_approvals and not opts.verbose:
    approvs = GetApprovalSummary(opts, cls)
    for cat in GERRIT_SUMMARY_CATS:
      if approvs[cat] is '':
        functor = lambda x: x
      elif approvs[cat] < 0:
        functor = red
      else:
        functor = green
      status += functor('%s:%2s ' % (cat, approvs[cat]))

  print('%s %s%-*s %s' % (blue('%-*s' % (lims['url'], cls['url'])), status,
                          lims['project'], cls['project'], cls['subject']))

  if show_approvals and opts.verbose:
    for approver in cls['currentPatchSet'].get('approvals', []):
      functor = red if int(approver['value']) < 0 else green
      n = functor('%2s' % approver['value'])
      t = GERRIT_APPROVAL_MAP.get(approver['type'], [approver['type'],
                                                     approver['type']])[1]
      print('      %s %s %s' % (n, t, approver['by']['email']))


def _MyUserInfo():
  username = os.environ['USER']
  emails = ['%s@%s' % (username, domain)
            for domain in ('google.com', 'chromium.org')]
  reviewers = ['reviewer:%s' % x for x in emails]
  owners = ['owner:%s' % x for x in emails]
  return emails, reviewers, owners


def FilteredQuery(opts, query):
  """Query gerrit and filter/clean up the results"""
  ret = []

  for cl in opts.gerrit.Query(query, raw=True):
    # Gerrit likes to return a stats record too.
    if not 'project' in cl:
      continue

    # Strip off common leading names since the result is still
    # unique over the whole tree.
    if not opts.verbose:
      for pfx in ('chromeos', 'chromiumos', 'overlays', 'platform'):
        if cl['project'].startswith('%s/' % pfx):
          cl['project'] = cl['project'][len(pfx) + 1:]

    ret.append(cl)

  if opts.sort in ('number',):
    key = lambda x: int(x[opts.sort])
  else:
    key = lambda x: x[opts.sort]
  return sorted(ret, key=key)


def ChangeNumberToCommit(opts, idx):
  """Given a gerrit CL #, return the revision info

  This is the form that the gerrit ssh interface expects.
  """
  cl = opts.gerrit.QuerySingleRecord(idx, raw=True)
  return cl['currentPatchSet']['revision']


def IsApprover(cl, users):
  """See if the approvers in |cl| is listed in |users|"""
  # See if we are listed in the approvals list.  We have to parse
  # this by hand as the gerrit query system doesn't support it :(
  # http://code.google.com/p/gerrit/issues/detail?id=1235
  if 'approvals' not in cl['currentPatchSet']:
    return False

  if isinstance(users, basestring):
    users = (users,)

  for approver in cl['currentPatchSet']['approvals']:
    if (approver['by']['email'] in users and
        approver['type'] == 'CRVW' and
        int(approver['value']) != 0):
      return True

  return False


def UserActTodo(opts):
  """List CLs needing your review"""
  emails, reviewers, owners = _MyUserInfo()
  cls = FilteredQuery(opts, '( %s ) status:open NOT ( %s )' %
                            (' OR '.join(reviewers), ' OR '.join(owners)))
  cls = [x for x in cls if not IsApprover(x, emails)]
  lims = limits(cls)
  for cl in cls:
    PrintCl(opts, cl, lims)


def UserActMine(opts):
  """List your CLs with review statuses"""
  _, _, owners = _MyUserInfo()
  cls = FilteredQuery(opts, '( %s ) status:new' % (' OR '.join(owners),))
  lims = limits(cls)
  for cl in cls:
    PrintCl(opts, cl, lims)


def UserActInspect(opts, idx):
  """Inspect CL number <n>"""
  cl = FilteredQuery(opts, idx)
  if cl:
    PrintCl(opts, cl[0], None)
  else:
    print('no results found for CL %s' % idx)


def UserActReview(opts, idx, num):
  """Mark CL <n> with code review status [-2,-1,0,1,2]"""
  opts.gerrit.SetReview(idx, labels={'Code-Review': num})


def UserActVerify(opts, idx, num):
  """Mark CL <n> with verify status [-1,0,1]"""
  opts.gerrit.SetReview(idx, labels={'Verified': num})


def UserActReady(opts, idx, num):
  """Mark CL <n> with ready status [-1,0,1]"""
  opts.gerrit.SetReview(idx, labels={'Commit-Queue': num})


def UserActSubmit(opts, idx):
  """Submit CL <n>"""
  opts.gerrit.SubmitChange(idx)


def UserActAbandon(opts, idx):
  """Abandon CL <n>"""
  opts.gerrit.AbandonChange(idx)


def UserActRestore(opts, idx):
  """Restore CL <n> that was abandoned"""
  opts.gerrit.RestoreChange(idx)


def UserActReviewers(opts, idx, *args):
  """Add/remove reviewers' emails for CL <n> (prepend with '~' to remove)"""
  emails = args
  # Allow for optional leading '~'.
  email_validator = re.compile(r'^[~]?%s$' % constants.EMAIL_REGEX)
  add_list, remove_list, invalid_list = [], [], []

  for x in emails:
    if not email_validator.match(x):
      invalid_list.append(x)
    elif x[0] == '~':
      remove_list.append(x[1:])
    else:
      add_list.append(x)

  if invalid_list:
    cros_build_lib.Die(
        'Invalid email address(es): %s' % ', '.join(invalid_list))

  if add_list or remove_list:
    opts.gerrit.SetReviewers(idx, add=add_list, remove=remove_list)


def UserActMessage(opts, idx, message):
  """Add a message to CL <n>"""
  opts.gerrit.SetReview(idx, msg=message)


def UserActDeletedraft(opts, idx):
  """Delete draft patch set <n>"""
  opts.gerrit.DeleteDraft(idx)


def main(argv):
  # Locate actions that are exposed to the user.  All functions that start
  # with "UserAct" are fair game.
  act_pfx = 'UserAct'
  actions = [x for x in globals() if x.startswith(act_pfx)]

  usage = """%(prog)s [options] <action> [action args]

There is no support for doing line-by-line code review via the command line.
This helps you manage various bits and CL status.

Example:
  $ gerrit todo             # List all the CLs that await your review.
  $ gerrit mine             # List all of your open CLs.
  $ gerrit inspect 28123    # Inspect CL 28123 on the public gerrit.
  $ gerrit inspect *28123   # Inspect CL 28123 on the internal gerrit.
  $ gerrit verify 28123 1   # Mark CL 28123 as verified (+1).

Actions:"""
  indent = max([len(x) - len(act_pfx) for x in actions])
  for a in sorted(actions):
    usage += '\n  %-*s: %s' % (indent, a[len(act_pfx):].lower(),
                               globals()[a].__doc__)

  parser = commandline.ArgumentParser(usage=usage)
  parser.add_argument('-i', '--internal', dest='gob', action='store_const',
                      const=constants.INTERNAL_GOB_INSTANCE,
                      help='Query internal Chromium Gerrit instance')
  parser.add_argument('-g', '--gob',
                      help='Gerrit (on borg) instance to query (default: %s)' %
                           (constants.EXTERNAL_GOB_INSTANCE))
  parser.add_argument('--sort', default='number',
                      help='Key to sort on (number, project)')
  parser.add_argument('-v', '--verbose', default=False, action='store_true',
                      help='Be more verbose in output')
  parser.add_argument('args', nargs='+')
  opts = parser.parse_args(argv)

  # pylint: disable=W0603
  global COLOR
  COLOR = terminal.Color(enabled=opts.color)

  # TODO: This sucks.  We assume that all actions which take an argument are
  # a CL #.  Or at least, there's no other reason for it to start with a *.
  # We do this to automatically select internal vs external gerrit as this
  # convention is encoded in much of our system.  However, the rest of this
  # script doesn't expect (or want) the leading *.
  args = opts.args
  if len(args) > 1:
    if args[1][0] == '*':
      opts.gob = constants.INTERNAL_GOB_INSTANCE
      args[1] = args[1][1:]

  if opts.gob is None:
    opts.gob = constants.EXTERNAL_GOB_INSTANCE

  opts.gerrit = gerrit.GetGerritHelper(gob=opts.gob, print_cmd=opts.debug)
  opts.Freeze()

  # Now look up the requested user action and run it.
  cmd = args[0].lower()
  args = args[1:]
  functor = globals().get(act_pfx + cmd.capitalize())
  if functor:
    argspec = inspect.getargspec(functor)
    if argspec.varargs:
      if len(args) < len(argspec.args):
        parser.error('incorrect number of args: %s expects at least %s' %
                     (cmd, len(argspec.args)))
    elif len(argspec.args) - 1 != len(args):
      parser.error('incorrect number of args: %s expects %s' %
                   (cmd, len(argspec.args) - 1))
    try:
      functor(opts, *args)
    except (cros_build_lib.RunCommandError, gerrit.GerritException,
            gob_util.GOBError) as e:
      cros_build_lib.Die(e.message)
  else:
    parser.error('unknown action: %s' % (cmd,))
