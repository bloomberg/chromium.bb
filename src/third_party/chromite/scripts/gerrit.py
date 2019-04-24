# -*- coding: utf-8 -*-
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
import json
import re
import sys

from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gerrit
from chromite.lib import gob_util
from chromite.lib import memoize
from chromite.lib import terminal
from chromite.lib import uri_lib


# Locate actions that are exposed to the user.  All functions that start
# with "UserAct" are fair game.
ACTION_PREFIX = 'UserAct'


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
  if cl is not None:
    if cl.startswith('*'):
      gob = config_lib.GetSiteParams().INTERNAL_GOB_INSTANCE
      cl = cl[1:]
    elif ':' in cl:
      gob, cl = cl.split(':', 1)

  if not gob in opts.gerrit:
    opts.gerrit[gob] = gerrit.GetGerritHelper(gob=gob, print_cmd=opts.debug)

  return (opts.gerrit[gob], cl)


def GetApprovalSummary(_opts, cls):
  """Return a dict of the most important approvals"""
  approvs = dict([(x, '') for x in GERRIT_SUMMARY_CATS])
  for approver in cls.get('currentPatchSet', {}).get('approvals', []):
    cats = GERRIT_APPROVAL_MAP.get(approver['type'])
    if not cats:
      logging.warning('unknown gerrit approval type: %s', approver['type'])
      continue
    cat = cats[0].strip()
    val = int(approver['value'])
    if not cat in approvs:
      # Ignore the extended categories in the summary view.
      continue
    elif approvs[cat] == '':
      approvs[cat] = val
    elif val < 0:
      approvs[cat] = min(approvs[cat], val)
    else:
      approvs[cat] = max(approvs[cat], val)
  return approvs


def PrettyPrintCl(opts, cl, lims=None, show_approvals=True):
  """Pretty print a single result"""
  if lims is None:
    lims = {'url': 0, 'project': 0}

  status = ''
  if show_approvals and not opts.verbose:
    approvs = GetApprovalSummary(opts, cl)
    for cat in GERRIT_SUMMARY_CATS:
      if approvs[cat] in ('', 0):
        functor = lambda x: x
      elif approvs[cat] < 0:
        functor = red
      else:
        functor = green
      status += functor('%s:%2s ' % (cat, approvs[cat]))

  print('%s %s%-*s %s' % (blue('%-*s' % (lims['url'], cl['url'])), status,
                          lims['project'], cl['project'], cl['subject']))

  if show_approvals and opts.verbose:
    for approver in cl['currentPatchSet'].get('approvals', []):
      functor = red if int(approver['value']) < 0 else green
      n = functor('%2s' % approver['value'])
      t = GERRIT_APPROVAL_MAP.get(approver['type'], [approver['type'],
                                                     approver['type']])[1]
      print('      %s %s %s' % (n, t, approver['by']['email']))


def PrintCls(opts, cls, lims=None, show_approvals=True):
  """Print all results based on the requested format."""
  if opts.raw:
    site_params = config_lib.GetSiteParams()
    pfx = ''
    # Special case internal Chrome GoB as that is what most devs use.
    # They can always redirect the list elsewhere via the -g option.
    if opts.gob == site_params.INTERNAL_GOB_INSTANCE:
      pfx = site_params.INTERNAL_CHANGE_PREFIX
    for cl in cls:
      print('%s%s' % (pfx, cl['number']))

  elif opts.json:
    json.dump(cls, sys.stdout)

  else:
    if lims is None:
      lims = limits(cls)

    for cl in cls:
      PrettyPrintCl(opts, cl, lims=lims, show_approvals=show_approvals)


def _Query(opts, query, raw=True, helper=None):
  """Queries Gerrit with a query string built from the commandline options"""
  if opts.branch is not None:
    query += ' branch:%s' % opts.branch
  if opts.project is not None:
    query += ' project: %s' % opts.project
  if opts.topic is not None:
    query += ' topic: %s' % opts.topic

  if helper is None:
    helper, _ = GetGerrit(opts)
  return helper.Query(query, raw=raw, bypass_cache=False)


def FilteredQuery(opts, query, helper=None):
  """Query gerrit and filter/clean up the results"""
  ret = []

  logging.debug('Running query: %s', query)
  for cl in _Query(opts, query, raw=True, helper=helper):
    # Gerrit likes to return a stats record too.
    if not 'project' in cl:
      continue

    # Strip off common leading names since the result is still
    # unique over the whole tree.
    if not opts.verbose:
      for pfx in ('aosp', 'chromeos', 'chromiumos', 'external', 'overlays',
                  'platform', 'third_party'):
        if cl['project'].startswith('%s/' % pfx):
          cl['project'] = cl['project'][len(pfx) + 1:]

      cl['url'] = uri_lib.ShortenUri(cl['url'])

    ret.append(cl)

  if opts.sort == 'unsorted':
    return ret
  if opts.sort == 'number':
    key = lambda x: int(x[opts.sort])
  else:
    key = lambda x: x[opts.sort]
  return sorted(ret, key=key)


def UserActTodo(opts):
  """List CLs needing your review"""
  cls = FilteredQuery(opts, ('reviewer:self status:open NOT owner:self '
                             'label:Code-Review=0,user=self '
                             'NOT label:Verified<0'))
  PrintCls(opts, cls)


def UserActSearch(opts, query):
  """List CLs matching the search query"""
  cls = FilteredQuery(opts, query)
  PrintCls(opts, cls)
UserActSearch.usage = '<query>'


def UserActMine(opts):
  """List your CLs with review statuses"""
  if opts.draft:
    rule = 'is:draft'
  else:
    rule = 'status:new'
  UserActSearch(opts, 'owner:self %s' % (rule,))


def _BreadthFirstSearch(to_visit, children, visited_key=lambda x: x):
  """Runs breadth first search starting from the nodes in |to_visit|

  Args:
    to_visit: the starting nodes
    children: a function which takes a node and returns the nodes adjacent to it
    visited_key: a function for deduplicating node visits. Defaults to the
      identity function (lambda x: x)

  Returns:
    A list of nodes which are reachable from any node in |to_visit| by calling
    |children| any number of times.
  """
  to_visit = list(to_visit)
  seen = set(map(visited_key, to_visit))
  for node in to_visit:
    for child in children(node):
      key = visited_key(child)
      if key not in seen:
        seen.add(key)
        to_visit.append(child)
  return to_visit


def UserActDeps(opts, query):
  """List CLs matching a query, and all transitive dependencies of those CLs"""
  cls = _Query(opts, query, raw=False)

  @memoize.Memoize
  def _QueryChange(cl, helper=None):
    return _Query(opts, cl, raw=False, helper=helper)

  def _ProcessDeps(cl, deps, required):
    """Yields matching dependencies for a patch"""
    # We need to query the change to guarantee that we have a .gerrit_number
    for dep in deps:
      if not dep.remote in opts.gerrit:
        opts.gerrit[dep.remote] = gerrit.GetGerritHelper(
            remote=dep.remote, print_cmd=opts.debug)
      helper = opts.gerrit[dep.remote]

      # TODO(phobbs) this should maybe catch network errors.
      changes = _QueryChange(dep.ToGerritQueryText(), helper=helper)

      # Handle empty results.  If we found a commit that was pushed directly
      # (e.g. a bot commit), then gerrit won't know about it.
      if not changes:
        if required:
          logging.error('CL %s depends on %s which cannot be found',
                        cl, dep.ToGerritQueryText())
        continue

      # Our query might have matched more than one result.  This can come up
      # when CQ-DEPEND uses a Gerrit Change-Id, but that Change-Id shows up
      # across multiple repos/branches.  We blindly check all of them in the
      # hopes that all open ones are what the user wants, but then again the
      # CQ-DEPEND syntax itself is unable to differeniate.  *shrug*
      if len(changes) > 1:
        logging.warning('CL %s has an ambiguous CQ dependency %s',
                        cl, dep.ToGerritQueryText())
      for change in changes:
        if change.status == 'NEW':
          yield change

  def _Children(cl):
    """Yields the Gerrit and CQ-Depends dependencies of a patch"""
    for change in _ProcessDeps(cl, cl.PaladinDependencies(None), True):
      yield change
    for change in _ProcessDeps(cl, cl.GerritDependencies(), False):
      yield change

  transitives = _BreadthFirstSearch(
      cls, _Children,
      visited_key=lambda cl: cl.gerrit_number)

  transitives_raw = [cl.patch_dict for cl in transitives]
  PrintCls(opts, transitives_raw)
UserActDeps.usage = '<query>'


def UserActInspect(opts, *args):
  """Show the details of one or more CLs"""
  cls = []
  for arg in args:
    helper, cl = GetGerrit(opts, arg)
    change = FilteredQuery(opts, 'change:%s' % cl, helper=helper)
    if change:
      cls.extend(change)
    else:
      logging.warning('no results found for CL %s', arg)
  PrintCls(opts, cls)
UserActInspect.usage = '<CLs...>'


def UserActReview(opts, *args):
  """Mark CLs with a code review status"""
  num = args[-1]
  for arg in args[:-1]:
    helper, cl = GetGerrit(opts, arg)
    helper.SetReview(cl, labels={'Code-Review': num},
                     dryrun=opts.dryrun, notify=opts.notify)
UserActReview.arg_min = 2
UserActReview.usage = '<CLs...> <-2|-1|0|1|2>'


def UserActVerify(opts, *args):
  """Mark CLs with a verified status"""
  num = args[-1]
  for arg in args[:-1]:
    helper, cl = GetGerrit(opts, arg)
    helper.SetReview(cl, labels={'Verified': num},
                     dryrun=opts.dryrun, notify=opts.notify)
UserActVerify.arg_min = 2
UserActVerify.usage = '<CLs...> <-1|0|1>'


def UserActReady(opts, *args):
  """Mark CLs with a ready status"""
  num = args[-1]
  for arg in args[:-1]:
    helper, cl = GetGerrit(opts, arg)
    helper.SetReview(cl, labels={'Commit-Queue': num},
                     dryrun=opts.dryrun, notify=opts.notify)
UserActReady.arg_min = 2
UserActReady.usage = '<CLs...> <0|1>'


def UserActTrybotready(opts, *args):
  """Mark CLs with a trybot-ready status"""
  num = args[-1]
  for arg in args[:-1]:
    helper, cl = GetGerrit(opts, arg)
    helper.SetReview(cl, labels={'Trybot-Ready': num},
                     dryrun=opts.dryrun, notify=opts.notify)
UserActTrybotready.arg_min = 2
UserActTrybotready.usage = '<CLs...> <0|1>'


def UserActSubmit(opts, *args):
  """Submit CLs"""
  for arg in args:
    helper, cl = GetGerrit(opts, arg)
    helper.SubmitChange(cl, dryrun=opts.dryrun)
UserActSubmit.usage = '<CLs...>'


def UserActAbandon(opts, *args):
  """Abandon CLs"""
  for arg in args:
    helper, cl = GetGerrit(opts, arg)
    helper.AbandonChange(cl, dryrun=opts.dryrun)
UserActAbandon.usage = '<CLs...>'


def UserActRestore(opts, *args):
  """Restore CLs that were abandoned"""
  for arg in args:
    helper, cl = GetGerrit(opts, arg)
    helper.RestoreChange(cl, dryrun=opts.dryrun)
UserActRestore.usage = '<CLs...>'


def UserActReviewers(opts, cl, *args):
  """Add/remove reviewers' emails for a CL (prepend with '~' to remove)"""
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
UserActReviewers.usage = '<CL> <emails...>'


def UserActAssign(opts, cl, assignee):
  """Set the assignee for a CL"""
  helper, cl = GetGerrit(opts, cl)
  helper.SetAssignee(cl, assignee, dryrun=opts.dryrun)
UserActAssign.usage = '<CL> <assignee>'


def UserActMessage(opts, cl, message):
  """Add a message to a CL"""
  helper, cl = GetGerrit(opts, cl)
  helper.SetReview(cl, msg=message, dryrun=opts.dryrun)
UserActMessage.usage = '<CL> <message>'


def UserActTopic(opts, topic, *args):
  """Set a topic for one or more CLs"""
  for arg in args:
    helper, arg = GetGerrit(opts, arg)
    helper.SetTopic(arg, topic, dryrun=opts.dryrun)
UserActTopic.usage = '<topic> <CLs...>'


def UserActPrivate(opts, cl, private_str):
  """Set the private bit on a CL to private"""
  try:
    private = cros_build_lib.BooleanShellValue(private_str, False)
  except ValueError:
    raise RuntimeError('Unknown "boolean" value: %s' % private_str)

  helper, cl = GetGerrit(opts, cl)
  helper.SetPrivate(cl, private)
UserActPrivate.usage = '<CL> <private str>'


def UserActSethashtags(opts, cl, *args):
  """Add/remove hashtags on a CL (prepend with '~' to remove)"""
  hashtags = args
  add = []
  remove = []
  for hashtag in hashtags:
    if hashtag.startswith('~'):
      remove.append(hashtag[1:])
    else:
      add.append(hashtag)
  helper, cl = GetGerrit(opts, cl)
  helper.SetHashtags(cl, add, remove, dryrun=opts.dryrun)
UserActSethashtags.usage = '<CL> <hashtags...>'


def UserActDeletedraft(opts, *args):
  """Delete draft CLs"""
  for arg in args:
    helper, cl = GetGerrit(opts, arg)
    helper.DeleteDraft(cl, dryrun=opts.dryrun)
UserActDeletedraft.usage = '<CLs...>'


def UserActAccount(opts):
  """Get the current user account information"""
  helper, _ = GetGerrit(opts)
  acct = helper.GetAccount()
  if opts.json:
    json.dump(acct, sys.stdout)
  else:
    print('account_id:%i  %s <%s>' %
          (acct['_account_id'], acct['name'], acct['email']))


def _GetActionUsages():
  """Formats a one-line usage and doc message for each action."""
  actions = [x for x in globals() if x.startswith(ACTION_PREFIX)]
  actions.sort()

  cmds = [x[len(ACTION_PREFIX):] for x in actions]

  # Sanity check names for devs adding new commands.  Should be quick.
  for cmd in cmds:
    expected_name = cmd.lower().capitalize()
    if cmd != expected_name:
      raise RuntimeError('callback "%s" is misnamed; should be "%s"' %
                         (cmd, expected_name))

  functions = [globals()[x] for x in actions]
  usages = [getattr(x, 'usage', '') for x in functions]
  docs = [x.__doc__ for x in functions]

  action_usages = []
  cmd_indent = len(max(cmds, key=len))
  usage_indent = len(max(usages, key=len))
  for cmd, usage, doc in zip(cmds, usages, docs):
    action_usages.append('  %-*s %-*s : %s' %
                         (cmd_indent, cmd.lower(), usage_indent, usage, doc))

  return '\n'.join(action_usages)


def GetParser():
  """Returns the parser to use for this module."""
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
  $ gerrit --json search 'assignee:self'    # Dump all pending CLs in JSON.

Actions:
"""
  usage += _GetActionUsages()

  site_params = config_lib.GetSiteParams()
  parser = commandline.ArgumentParser(usage=usage)
  parser.add_argument('-i', '--internal', dest='gob', action='store_const',
                      default=site_params.EXTERNAL_GOB_INSTANCE,
                      const=site_params.INTERNAL_GOB_INSTANCE,
                      help='Query internal Chromium Gerrit instance')
  parser.add_argument('-g', '--gob',
                      default=site_params.EXTERNAL_GOB_INSTANCE,
                      help=('Gerrit (on borg) instance to query (default: %s)' %
                            (site_params.EXTERNAL_GOB_INSTANCE)))
  parser.add_argument('--sort', default='number',
                      help='Key to sort on (number, project); use "unsorted" '
                           'to disable')
  parser.add_argument('--raw', default=False, action='store_true',
                      help='Return raw results (suitable for scripting)')
  parser.add_argument('--json', default=False, action='store_true',
                      help='Return results in JSON (suitable for scripting)')
  parser.add_argument('-n', '--dry-run', default=False, action='store_true',
                      dest='dryrun',
                      help='Show what would be done, but do not make changes')
  parser.add_argument('--ne', '--no-emails', default=True, action='store_false',
                      dest='send_email',
                      help='Do not send email for some operations '
                           '(e.g. ready/review/trybotready/verify)')
  parser.add_argument('-v', '--verbose', default=False, action='store_true',
                      help='Be more verbose in output')
  parser.add_argument('-b', '--branch',
                      help='Limit output to the specific branch')
  parser.add_argument('--draft', default=False, action='store_true',
                      help="Show draft changes (applicable to 'mine' only)")
  parser.add_argument('-p', '--project',
                      help='Limit output to the specific project')
  parser.add_argument('-t', '--topic',
                      help='Limit output to the specific topic')
  parser.add_argument('action', help='The gerrit action to perform')
  parser.add_argument('args', nargs='*', help='Action arguments')

  return parser


def main(argv):
  parser = GetParser()
  opts = parser.parse_args(argv)

  # A cache of gerrit helpers we'll load on demand.
  opts.gerrit = {}

  # Convert user friendly command line option into a gerrit parameter.
  opts.notify = 'ALL' if opts.send_email else 'NONE'
  opts.Freeze()

  # pylint: disable=global-statement
  global COLOR
  COLOR = terminal.Color(enabled=opts.color)

  # Now look up the requested user action and run it.
  functor = globals().get(ACTION_PREFIX + opts.action.capitalize())
  if functor:
    argspec = inspect.getargspec(functor)
    if argspec.varargs:
      arg_min = getattr(functor, 'arg_min', len(argspec.args))
      if len(opts.args) < arg_min:
        parser.error('incorrect number of args: %s expects at least %s' %
                     (opts.action, arg_min))
    elif len(argspec.args) - 1 != len(opts.args):
      parser.error('incorrect number of args: %s expects %s' %
                   (opts.action, len(argspec.args) - 1))
    try:
      functor(opts, *opts.args)
    except (cros_build_lib.RunCommandError, gerrit.GerritException,
            gob_util.GOBError) as e:
      cros_build_lib.Die(e.message)
  else:
    parser.error('unknown action: %s' % (opts.action,))
