#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
from chromite.buildbot import constants
from chromite.lib import gerrit
import logging


def find_user(helper, username):
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
    "[email_addresses|usernames]")
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
  if not args and not opts.query:
    parser.error("no querying parameters given.")

  logging.getLogger().setLevel(logging.WARNING)
  query = []

  helper = gerrit.GerritHelper.FromRemote(
      constants.INTERNAL_REMOTE if opts.internal else constants.EXTERNAL_REMOTE)
  recomposed_args = []
  for arg in args:
    if "@" not in arg:
      recomposed_args.extend(find_user(helper, arg))
    else:
      recomposed_args.append(arg)

  args = set(recomposed_args)
  addresses = [x.replace(":", "\:") for x in args]
  owners = ["owner:%s" % x for x in addresses]
  reviewers = ["reviewer:%s" % x for x in addresses]

  if opts.query:
    query.append(opts.query)
  elif args:
    query.append("( %s )" % " OR ".join(owners))
    query.append("-age:%s" % opts.age)

  query = " AND ".join(query)


  results = helper.Query(query, current_patch=False, sort="lastUpdated")
  targets = frozenset(args)

  if not args:
    print "query resulted in %i owned CLs." % len(results)
    print "no email addresses given so unable to do scoring stats"
    return 0

  print "Owner stats: %i CLs." % len(results)
  stats = {}
  for x in results:
    status = x["status"].lower()
    stats[status] = stats.get(status, 0) + 1
  if "new" in stats:
    stats["open"] = stats.pop("new")
  for status, value in sorted(stats.iteritems(), key=lambda x:x[0]):
    print "  %s: %i" % (status, value)

  # Get approval stats.
  requested = helper.Query(
      "( %s  AND NOT ( %s ) ) AND -age:%s" % (" OR ".join(reviewers),
                                              " OR ".join(owners), opts.age),
      current_patch=False, options=("--all-approvals",))

  total_scored_patchsets = scoring = 0
  lgtms = 0
  for cl in requested:
    touched_it = False
    lgtmed = False
    for patchset in cl.get("patchSets", ()):
      for approval in patchset.get("approvals", ()):
        if targets.intersection([approval["by"].get("email")]):
          touched_it = True
          if (("CRVW", 2) ==
              (approval.get("type"), int(approval.get("value", 0)))):
            lgtmed = True
          total_scored_patchsets += 1
    if touched_it:
      scoring += 1
    if lgtmed:
      lgtms += 1

  # Find comments; gerrit doesn't give that information in --all-approvals
  # unfortunately.
  comments = helper.Query(
      "( %s AND NOT ( %s ) ) AND -age:%s" % (" OR ".join(reviewers),
                                             " OR ".join(owners), opts.age),
      options=("--comments", "--patch-sets"), raw=True, current_patch=False)
  requested_review = len(comments)

  commented_changes = total_comments = 0
  for change in comments:
    touched = False
    for comment in change.get("comments", ()):
      if comment["reviewer"].get("email", None) in targets:
        total_comments += 1
        touched = True
    if touched:
      commented_changes += 1

  print ("Requested as a reviewer for %i changes, commented on %i, "
         "%i comments total; lgtm'd %i changes" % (
            requested_review, commented_changes,
            total_comments, lgtms))
  ratio = float(total_scored_patchsets)/scoring if total_scored_patchsets else 0
  print ("Review stats: %i Changes scored; %i individual patchsets scored "
         "(for a %2.2fx scoring/change rate)" % (
             scoring, total_scored_patchsets, ratio))
