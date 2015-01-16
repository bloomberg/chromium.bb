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
import pprint
import re

from chromite.cbuildbot import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import gob_util
from chromite.lib import terminal


COLOR = None

# Map the internal names to the ones we normally show on the web ui.
GERRIT_APPROVAL_MAP = {
    'COMR': ['CQ', 'Commit Queue   ',],
    'CRVW': ['CR', 'Code Review    ',],
    'SUBM': ['S ', 'Submitted      ',],
    'TRY':  ['T ', 'Trybot Ready   ',],
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


# TODO: This func really needs to be merged into the core gerrit logic.
def GetGerrit(opts, cl=None):
  """Auto pick the right gerrit instance based on the |cl|

  Args:
    opts: The general options object.
    cl: A CL taking one of the forms: 1234 *1234 chromium:1234

  Returns:
    A tuple of a gerrit object and a sanitized CL #.
  """
  gob = opts.gob
  if not cl is None:
    if cl.startswith('*'):
      gob = constants.INTERNAL_GOB_INSTANCE
      cl = cl[1:]
    elif ':' in cl:
      gob, cl = cl.split(':', 1)

  if not gob in opts.gerrit:
    opts.gerrit[gob] = gerrit.GetGerritHelper(gob=gob, print_cmd=opts.debug)

  return (opts.gerrit[gob], cl)


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
  if opts.raw:
    # Special case internal Chrome GoB as that is what most devs use.
    # They can always redirect the list elsewhere via the -g option.
    if opts.gob == constants.INTERNAL_GOB_INSTANCE:
      print(constants.INTERNAL_CHANGE_PREFIX, end='')
    print(cls['number'])
    return

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
  email = git.GetProjectUserEmail(constants.CHROMITE_DIR)
  [username, _, domain] = email.partition('@')
  if domain in ('google.com', 'chromium.org'):
    emails = ['%s@%s' % (username, domain)
              for domain in ('google.com', 'chromium.org')]
  else:
    emails = [email]
  reviewers = ['reviewer:%s' % x for x in emails]
  owners = ['owner:%s' % x for x in emails]
  return emails, reviewers, owners


def FilteredQuery(opts, query):
  """Query gerrit and filter/clean up the results"""
  ret = []

  if opts.branch is not None:
    query += ' branch:%s' % opts.branch
  if opts.project is not None:
    query += ' project: %s' % opts.project
  if opts.topic is not None:
    query += ' topic: %s' % opts.topic

  helper, _ = GetGerrit(opts)
  for cl in helper.Query(query, raw=True, bypass_cache=False):
    # Gerrit likes to return a stats record too.
    if not 'project' in cl:
      continue

    # Strip off common leading names since the result is still
    # unique over the whole tree.
    if not opts.verbose:
      for pfx in ('chromeos', 'chromiumos', 'overlays', 'platform',
                  'third_party'):
        if cl['project'].startswith('%s/' % pfx):
          cl['project'] = cl['project'][len(pfx) + 1:]

    ret.append(cl)

  if opts.sort in ('number',):
    key = lambda x: int(x[opts.sort])
  else:
    key = lambda x: x[opts.sort]
  return sorted(ret, key=key)


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
  cls = FilteredQuery(opts, ('( %s ) status:open NOT ( %s )' %
                             (' OR '.join(reviewers), ' OR '.join(owners))))
  cls = [x for x in cls if not IsApprover(x, emails)]
  lims = limits(cls)
  for cl in cls:
    PrintCl(opts, cl, lims)


def UserActSearch(opts, query):
  """List CLs matching the Gerrit <search query>"""
  cls = FilteredQuery(opts, query)
  lims = limits(cls)
  for cl in cls:
    PrintCl(opts, cl, lims)


def UserActMine(opts):
  """List your CLs with review statuses"""
  _, _, owners = _MyUserInfo()
  UserActSearch(opts, '( %s ) status:new' % (' OR '.join(owners),))


def UserActInspect(opts, *args):
  """Inspect CL number <n> [n ...]"""
  for arg in args:
    cl = FilteredQuery(opts, arg)
    if cl:
      PrintCl(opts, cl[0], None)
    else:
      print('no results found for CL %s' % arg)


def UserActReview(opts, *args):
  """Mark CL <n> [n ...] with code review status <-2,-1,0,1,2>"""
  num = args[-1]
  for arg in args[:-1]:
    helper, cl = GetGerrit(opts, arg)
    helper.SetReview(cl, labels={'Code-Review': num}, dryrun=opts.dryrun)
UserActReview.arg_min = 2


def UserActVerify(opts, *args):
  """Mark CL <n> [n ...] with verify status <-1,0,1>"""
  num = args[-1]
  for arg in args[:-1]:
    helper, cl = GetGerrit(opts, arg)
    helper.SetReview(cl, labels={'Verified': num}, dryrun=opts.dryrun)
UserActVerify.arg_min = 2


def UserActReady(opts, *args):
  """Mark CL <n> [n ...] with ready status <0,1,2>"""
  num = args[-1]
  for arg in args[:-1]:
    helper, cl = GetGerrit(opts, arg)
    helper.SetReview(cl, labels={'Commit-Queue': num}, dryrun=opts.dryrun)
UserActReady.arg_min = 2


def UserActTrybotready(opts, *args):
  """Mark CL <n> [n ...] with trybot-ready status <0,1>"""
  num = args[-1]
  for arg in args[:-1]:
    helper, cl = GetGerrit(opts, arg)
    helper.SetReview(cl, labels={'Trybot-Ready': num}, dryrun=opts.dryrun)
UserActTrybotready.arg_min = 2


def UserActSubmit(opts, *args):
  """Submit CL <n> [n ...]"""
  for arg in args:
    helper, cl = GetGerrit(opts, arg)
    helper.SubmitChange(cl, dryrun=opts.dryrun)


def UserActAbandon(opts, *args):
  """Abandon CL <n> [n ...]"""
  for arg in args:
    helper, cl = GetGerrit(opts, arg)
    helper.AbandonChange(cl, dryrun=opts.dryrun)


def UserActRestore(opts, *args):
  """Restore CL <n> [n ...] that was abandoned"""
  for arg in args:
    helper, cl = GetGerrit(opts, arg)
    helper.RestoreChange(cl, dryrun=opts.dryrun)


def UserActReviewers(opts, cl, *args):
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
    helper, cl = GetGerrit(opts, cl)
    helper.SetReviewers(cl, add=add_list, remove=remove_list,
                        dryrun=opts.dryrun)


def UserActMessage(opts, cl, message):
  """Add a message to CL <n>"""
  helper, cl = GetGerrit(opts, cl)
  helper.SetReview(cl, msg=message, dryrun=opts.dryrun)


def UserActTopic(opts, topic, *args):
  """Set |topic| for CL number <n> [n ...]"""
  for arg in args:
    helper, arg = GetGerrit(opts, arg)
    helper.SetTopic(arg, topic, dryrun=opts.dryrun)


def UserActDeletedraft(opts, *args):
  """Delete draft patch set <n> [n ...]"""
  for arg in args:
    helper, cl = GetGerrit(opts, arg)
    helper.DeleteDraft(cl, dryrun=opts.dryrun)


def UserActAccount(opts):
  """Get user account information."""
  helper, _ = GetGerrit(opts)
  pprint.PrettyPrinter().pprint(helper.GetAccount())


def main(argv):
  # Locate actions that are exposed to the user.  All functions that start
  # with "UserAct" are fair game.
  act_pfx = 'UserAct'
  actions = [x for x in globals() if x.startswith(act_pfx)]

  usage = """%(prog)s [options] <action> [action args]

There is no support for doing line-by-line code review via the command line.
This helps you manage various bits and CL status.

For general Gerrit documentation, see:
  https://gerrit-review.googlesource.com/Documentation/
The Searching Changes page covers the search query syntax:
  https://gerrit-review.googlesource.com/Documentation/user-search.html

Example:
  $ gerrit todo             # List all the CLs that await your review.
  $ gerrit mine             # List all of your open CLs.
  $ gerrit inspect 28123    # Inspect CL 28123 on the public gerrit.
  $ gerrit inspect *28123   # Inspect CL 28123 on the internal gerrit.
  $ gerrit verify 28123 1   # Mark CL 28123 as verified (+1).
Scripting:
  $ gerrit ready `gerrit --raw mine` 1      # Mark *ALL* of your public CLs \
ready.
  $ gerrit ready `gerrit --raw -i mine` 1   # Mark *ALL* of your internal CLs \
ready.

Actions:"""
  indent = max([len(x) - len(act_pfx) for x in actions])
  for a in sorted(actions):
    cmd = a[len(act_pfx):]
    # Sanity check for devs adding new commands.  Should be quick.
    if cmd != cmd.lower().capitalize():
      raise RuntimeError('callback "%s" is misnamed; should be "%s"' %
                         (cmd, cmd.lower().capitalize()))
    usage += '\n  %-*s: %s' % (indent, cmd.lower(), globals()[a].__doc__)

  parser = commandline.ArgumentParser(usage=usage)
  parser.add_argument('-i', '--internal', dest='gob', action='store_const',
                      default=constants.EXTERNAL_GOB_INSTANCE,
                      const=constants.INTERNAL_GOB_INSTANCE,
                      help='Query internal Chromium Gerrit instance')
  parser.add_argument('-g', '--gob',
                      default=constants.EXTERNAL_GOB_INSTANCE,
                      help=('Gerrit (on borg) instance to query (default: %s)' %
                            (constants.EXTERNAL_GOB_INSTANCE)))
  parser.add_argument('--sort', default='number',
                      help='Key to sort on (number, project)')
  parser.add_argument('--raw', default=False, action='store_true',
                      help='Return raw results (suitable for scripting)')
  parser.add_argument('-n', '--dry-run', default=False, action='store_true',
                      dest='dryrun',
                      help='Show what would be done, but do not make changes')
  parser.add_argument('-v', '--verbose', default=False, action='store_true',
                      help='Be more verbose in output')
  parser.add_argument('-b', '--branch',
                      help='Limit output to the specific branch')
  parser.add_argument('-p', '--project',
                      help='Limit output to the specific project')
  parser.add_argument('-t', '--topic',
                      help='Limit output to the specific topic')
  parser.add_argument('args', nargs='+')
  opts = parser.parse_args(argv)

  # A cache of gerrit helpers we'll load on demand.
  opts.gerrit = {}
  opts.Freeze()

  # pylint: disable=W0603
  global COLOR
  COLOR = terminal.Color(enabled=opts.color)

  # Now look up the requested user action and run it.
  cmd = opts.args[0].lower()
  args = opts.args[1:]
  functor = globals().get(act_pfx + cmd.capitalize())
  if functor:
    argspec = inspect.getargspec(functor)
    if argspec.varargs:
      arg_min = getattr(functor, 'arg_min', len(argspec.args))
      if len(args) < arg_min:
        parser.error('incorrect number of args: %s expects at least %s' %
                     (cmd, arg_min))
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
