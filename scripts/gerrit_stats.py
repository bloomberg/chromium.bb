#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import getpass
import logging
import optparse

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import gerrit


def _FindUserEmails(helper, username=None):
  """Find the emails for a given user that gerrit knows about.

  Args:
    helper: GerritHelper object.
    username: A gerrit username.  If none, then the current username
      according to getpass.getuser() will be assumed.
  Returns:
    List of email addresses.
  """
  if not username:
    username = getpass.getuser()

  # Try querying both initially; if it works, we can save a query.
  emails = ["%s@%s" % (username, domain)
            for domain in ("google.com", "chromium.org")]
  owners = ["owner:%s" % x for x in emails]
  try:
    helper.Query('( %s ) limit:0' % ' OR '.join(owners))
    return emails
  except gerrit.GerritException:
    # find the offender.
    pass

  recomposed = []
  for email in emails:
    try:
      helper.Query("%s limit:0" % email)
      recomposed.append(email)
    except gerrit.GerritException:
      pass

  if not recomposed:
    raise Exception("no email addresses found for %r" % username)

  return recomposed


def main(argv):
  parser = optparse.OptionParser(usage=
      "usage: prog [--internal|--external] [--query|--age] "
      "[email_addresses|usernames]"
      "\n"
      "\nIf no email addresses or usernames are given, then 'self' is assumed."
                                )
  parser.add_option("-i", "--internal", default=False, action="store_true",
                    help="Query gerrit-int.")
  parser.add_option("-e", "--external", dest="internal", action="store_false",
                    help="Query gerrit.  Note this is the default.")
  parser.add_option("--age", default="6mon",
                    help="Limit results to within this gerrit age query.  See "
                    "http://goo.gl/AdJy3 for the age form; to see the last 3 "
                    "months, it would be '3mon'.  Defaults to the last 6 "
                    "months.")
  parser.add_option("--query", default=None,
                    help="Freeform gerrit query.  If specified, it overrides "
                    "the normal querying behaviour.  Use only if you know "
                    "what you're doing.")

  opts, args = parser.parse_args(argv)

  cros_build_lib.logger.setLevel(logging.INFO)
  summary_lines = []

  helper = gerrit.GetGerritHelper(
      constants.INTERNAL_REMOTE if opts.internal else constants.EXTERNAL_REMOTE)

  # Expand args into chromium.org/google.com emails or just 'self'.
  if args:
    recomposed_args = []
    for arg in args:
      if "@" not in arg:
        recomposed_args.extend(_FindUserEmails(helper, arg))
      else:
        recomposed_args.append(arg)
    target_emails = frozenset(recomposed_args)
  else:
    target_emails = frozenset(_FindUserEmails(helper))

  owners = ["owner:%s" % x for x in target_emails]
  reviewers = ["reviewer:%s" % x for x in target_emails]

  # Get owner stats.
  query_tokens = []
  if opts.query:
    # User has provided a custom query.
    query_tokens.append(opts.query)
    cros_build_lib.Info('Looking for CLs with custom query.')
  else:
    query_tokens.append("( %s )" % " OR ".join(owners))
    query_tokens.append("-age:%s" % opts.age)
    cros_build_lib.Info('Looking for CLs owned by %s within last %s.',
                        ', '.join(target_emails), opts.age)

  query = " AND ".join(query_tokens)
  owner_changes = helper.Query(query, sort="lastUpdated")

  # Create summary information about owned CLs.
  summary_lines.append('\nOwner stats for %s:' % ', '.join(target_emails))
  summary_lines.append('  total CLs: %i' % len(owner_changes))
  stats = {}
  for change in owner_changes:
    status = change.status.lower()
    stats[status] = stats.get(status, 0) + 1

  if 'new' in stats:
    stats['open'] = stats.pop('new')
  for status, value in sorted(stats.iteritems(), key=lambda x:x[0]):
    summary_lines.append('  %s: %i' % (status, value))

  # Get reviewer stats.
  cros_build_lib.Info('Looking for CLs reviewed by %s within last %s.',
                      ', '.join(target_emails), opts.age)
  query = ('( ( %s ) AND NOT ( %s ) ) AND -age:%s' %
           (' OR '.join(reviewers), ' OR '.join(owners), opts.age))
  reviewer_changes = helper.Query(query, current_patch=False)

  lgtms = plusones = owner_lgtms = 0
  for change in reviewer_changes:
    # TODO(?): Still need a way to get the following info from CL:
    # 1) Include LGTMs on earlier patches.  It looks like that info is sort
    # of included in the json returned for the following query:
    # detailed_cl = helper.Query(cl.gerrit_number, current_patch=False)
    # But that info is not preserved in GerritPatch class.
    # 2) Count the number of comments (by targets) on this CL.  It is not
    # clear how to get that included in the json response.

    # Loop through all "approvals" on CL.  These include the code-review value,
    # the verified value, and the commit-queue value.
    # pylint: disable=W0212
    lgtmed = plusoned = owner_lgtmed = False
    for approval in change._approvals:
      approval_type = approval.get('type')
      approval_value = int(approval.get('value', 0))

      lgtm = ('CRVW', 2) == (approval_type, approval_value)
      plusone = ('CRVW', 1) == (approval_type, approval_value)

      approval_email = approval['by'].get('email')

      # See if this approval was given by the target person or people.
      if approval_email in target_emails:
        lgtmed = lgtmed or lgtm
        plusoned = plusoned or plusone

      # See if this approval was given by the owner of the CL.
      if approval_email and change.owner_email == approval_email:
        owner_lgtmed = owner_lgtmed or lgtm

    if plusoned:
      plusones += 1

    if lgtmed:
      lgtms += 1
    elif owner_lgtmed:
      # Only counter owner LGTMs that were not also LGTM by target.
      owner_lgtms += 1

  # Create summary information about reviewed CLs.
  summary_lines.append('\nReviewer stats for %s:' % ', '.join(target_emails))
  summary_lines.append('Only stats for final (committed) patch are available.')
  summary_lines.append('  Total CLs: %i (listed as reviewer)' %
                       len(reviewer_changes))
  summary_lines.append('  LGTMs: %i' % lgtms)
  summary_lines.append('  +1s  : %i' % plusones)
  summary_lines.append('  LGTMs from CL owners (possibly carried forward): %i' %
                       owner_lgtms)

  cros_build_lib.Info('\n%s', '\n'.join(summary_lines))
