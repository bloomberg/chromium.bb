#!/usr/bin/python

import optparse
import collections
from chromite.buildbot import constants
from chromite.buildbot import gerrit_helper
import logging

def main(argv):
  parser = optparse.OptionParser(usage=
    "usage: prog [--internal|--external] <--query|email_address> "
    "[email_addresses*]")
  parser.add_option('-i', "--internal", default=False, action='store_true',
                    help="Query gerrit-int.")
  parser.add_option('-e', "--external", dest='internal', action='store_false',
                    help="Query gerrit.  Note this is the default.")
  parser.add_option('--age', default='3mon',
                    help="Limit results to within this gerrit age query.  See "
                    "http://goo.gl/AdJy3 for the age form; to see the last 6 "
                    "months, it would be '6mon'.  Defaults to the last 3 "
                    "months.")
  parser.add_option("--cl-stats", default=False, action='store_true',
                    help="If given, output merged/abandoned/open stats.  "
                    "Note this suppresses comment/scoring analysis.")
  parser.add_option("--query", default=None,
                    help="Freeform gerrit query.  If specified, it overrides the"
                    "normal querying behaviour.  Use only if you know what "
                    "you're doing.")

  opts, args = parser.parse_args(argv)
  if not args and not opts.query:
    parser.error("no querying paramaters given.")

  logging.getLogger().setLevel(logging.WARNING)
  query = []
  if opts.query:
    query.append(opts.query)
  elif args:
    addresses = [x.replace(":", "\:") for x in args]
    if len(args) == 1:
      query.extend(addresses)
    else:
      if not opts.cl_stats:
        targets = ["reviewer:%s" % x for x in addresses]
      targets.extend("owner:%s" % x for x in addresses)
      query.append("( %s )" % " OR ".join(targets))
    query.append("-age:%s" % opts.age)

  query = ' AND '.join(query)

  helper = gerrit_helper.GerritHelper(constants.INTERNAL_REMOTE if opts.internal
                                      else constants.EXTERNAL_REMOTE)
  gerrit_options = () if opts.cl_stats else ('--all-approvals',)
  results = helper.Query(query, current_patch=False, sort='lastUpdated',
                         options=gerrit_options)
  targets = frozenset(args)
  print "query resulted in %i CLs matching." % len(results)
  if not args:
    print "no email addresses given so unable to do scoring stats"
    return 0
  if opts.cl_stats:
    stats = {}
    for x in results:
      status = x['status'].lower()
      stats[status] = x.get(status, 0) + 1
    if 'new' in stats:
      stats['open'] = stats.pop('new')
    for status, value in sorted(stats.iteritems(), key=lambda x:x[0]):
      print "  %s: %i" % (status, value)
    return 0
  owned_by = [x for x in results if x["owner"].get("email") in targets]
  non_owned_by = [x for x in results if x["owner"].get("email") not in targets]
  print "Owner stats: %i CLs." % len(owned_by)
  total_scored_patchsets = scoring = 0
  for cl in non_owned_by:
    touched_it = False
    for patchset in cl.get('patchSets', ()):
      for approval in patchset.get('approvals', ()):
        if targets.intersection([approval['by'].get('email')]):
          touched_it = True
          total_scored_patchsets += 1
    if touched_it:
      scoring += 1
  print "Requested as a reviewer for %i changes" % len(non_owned_by)
  print ("Review stats: %i Changes scored; %i individual patchsets scored "
         "(for a %2.2f scoring/patch rate)" % (
             scoring, total_scored_patchsets,
             (float(total_scored_patchsets)/scoring)))
