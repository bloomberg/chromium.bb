#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script requires python2.5 or above, since is relies on using
# the __missing__ method in subclasses of dict.

"""Monitoring the build or try bots.

This is a simple script that periodically scrapes the build and/or try
bot output and look for bot anomalies, sending out email notifications
if any are discovered.

The anomalies that this script can current recognize are:

- a bot is idle, but has a long pending queue
- a bot appears to not be making any progress on its queue
"""

import HTMLParser
import exceptions
import math
import optparse
import re
import subprocess
import sys
import time
import urllib2
import urlparse

notifications_to = 'troopers@chromium.org'
notifications_cc = 'bsy@google.com,nacl-eng@google.com'

# internally accessible URLs
try_url = 'http://chromegw.corp.google.com/i/tryserver.nacl/waterfall'
build_url = 'http://chromegw.corp.google.com/i/client.nacl/waterfall'

# externally accessible URLs; likely (?) delayed.
# try_url='http://build.chromium.org/p/tryserver.nacl/waterfall'
# build_url='http://build.chromium.org/p/client.nacl/waterfall'

# global, to be filled in later
options = None

# Processors/annotators have "requires", "provides" attributes and
# induces a topological sort.  The start of a run would begin with an
# empty data object and processors that have an empty "requires" list;
# these processors pulls the relevant bot's raw HTML into the run's
# data object.  This provides 'trybot-raw-html' or
# 'buildbot-raw-html', and the next stage would parse the HTML data
# into a more convenient form, providing 'trybot-parsed-html', etc.
# Later stages would extract the pending queue length, whether the bot
# has made any progress since the last poll, etc.
#
# This is somewhat simpler than a petri net, in that tokens are not
# consumed, so there's a monotonicity property at each poll stage.
# Data tags cannot be deleted.
#
# Tags used for "requires"/"provides" are strings that map to
# arbitrary objects; using string keys might seem to permit
# typographical errors, except that if a provided tag is misspelled,
# none of the stages that require it can fire.  Similarly, if one of a
# processor's requires tag is misspelled, it would never be able to
# run.  Thus, the "data object" above is really just a plain
# dictionary, initially empty.
#
# Processors are objects, and thus can/will have state.  In
# particular, a progress-since-last-time processor would remember how
# far a particular bot had gotten in the last iteration(s), and
# compare the current build progress against that.


# A simple log interface.  The |detail_level| of a log message is
# associated with the log message, and the run-time
# |options.verbosity| specifies the level of detail that the user
# wants to see.  If |detail_level| is LOG_FATAL, then the message is
# considered to be "fatal" -- it will generate an LogFatal exception.
# (We don't catch LogFatal exceptions.)

LOG_FATAL=-1


class LogFatal(exceptions.Exception):
  def __init__(self, args=None):
    self.args = args


def log(detail_level, format_string, *args):
  if detail_level <= LOG_FATAL:
    raise LogFatal(format_string % args)
  if options.verbosity > detail_level:
    sys.stdout.write(format_string % args)


class Node(object):

  def __init__(self):
    # set of nodes n with which there is an edge (self, n)
    self._node_src = set()
    # set of nodes n with which there is an edge (n, self)
    self._node_dst = set()

  def requires(self):
    return []

  def provides(self):
    return []

  def process(self, data):
    pass

  def name(self):
    return str(self)

  def add_edge_to(self, dst_node):
    log(2, 'add %s -> %s\n', self.name(), dst_node.name())
    self._node_src.add(dst_node)
    dst_node._node_dst.add(self)

  def remove_edge_to(self, dst_node):
    log(2, 'del %s -> %s\n', self.name(), dst_node.name())
    self._node_src.remove(dst_node)
    dst_node._node_dst.remove(self)

  def in_degree(self):
    return len(self._node_dst)

  def out_degree(self):
    return len(self._node_src)

  def dependent_nodes(self):
    return self._node_src

  def providing_nodes(self):
    return self._node_dst


class Tag(object):

  def __init__(self, name):
    self.name = name
    self.pending_dependent_nodes = []
    self.providing_node = None


class TagDict(dict):

  def __missing__(self, tag):
    v = Tag(tag)
    self[tag] = v
    return v


# Process each node.  For each tag in a node's provides tag list, if
# it is the first to provide that tag, enter it into a dictionary as
# source node for the tag; if there is a non-empty list of pending
# dependent nodes that depend on the tag (requires it), process these
# nodes by creating edges from the source node to the dependent nodes,
# and clearing the dependent list afterwords.  For each requires tag
# of the node, find source in dictionary.  If a providing node exists,
# create edge; otherwise add node to pending dependents list.  When
# all nodes are processed, we should have a graph structure with which
# we can run Kahn's topological sort algorithm.

class Graph(object):

  def __init__(self):
    self._nodes = set()
    self._tags = {}

  def add_node(self, node):
    self._nodes.add(node)

  def create_graph_edges(self):
    tag_dict = TagDict()
    for node in self._nodes:
      log(2, 'Process node %s\n', node.name())
      for tag in node.provides():
        log(2, 'provides: %s\n', tag)
        tag_entry = tag_dict[tag]
        if tag_entry.providing_node is None:
          # we are first (or only) providing node; process dependents, if any
          tag_entry.providing_node = node
          for dep_node in tag_entry.pending_dependent_nodes:
            node.add_edge_to(dep_node)
          tag_entry.pending_dependent_nodes = []
        else:
          log(LOG_FATAL, 'duplicate provider for %s, ignoring\n', tag)
      for tag in node.requires():
        log(2, 'requires: %s\n', tag)
        tag_entry = tag_dict[tag]
        if tag_entry.providing_node is None:
          tag_entry.pending_dependent_nodes.append(node)
        else:
          tag_entry.providing_node.add_edge_to(node)

  def execution_order(self):
    # Kahn's topological sort algorithm.  See
    # http://en.wikipedia.org/wiki/Topological_sorting
    #
    # Pick all nodes that have in-degree zero and put them into a
    # ready list for processing -- these don't depend on anything and
    # can execute right away.  list.  As each is processed and placed
    # into the execution order list, their outgoing edges are removed;
    # as we remove edges, we look at the nodes to which these edges
    # were incident on and check their degrees.  This process should
    # create some new nodes with in-degree zero, those the
    # dependencies of which would be satisfied by the execution of the
    # earlier nodes; whenever we encounter a node that now also has an
    # in-degree of zero, we move the node to the ready list.  We
    # iterate until all nodes are processed / the ready list is empty.
    self.create_graph_edges()
    execution_order = []
    not_dst = filter(lambda n: n.in_degree() == 0, self._nodes)
    assert len(not_dst) > 0
    while len(not_dst) > 0:
      node = not_dst.pop()
      execution_order.append(node)
      for pn in list(node.dependent_nodes()):
        node.remove_edge_to(pn)
        if pn.in_degree() == 0:
          not_dst.append(pn)
    assert len(execution_order) == len(self._nodes)
    return execution_order


class UrlFetcher(Node):

  def __init__(self, url, data_key):
    Node.__init__(self)
    self._url = url
    self._provides = data_key

  def provides(self):
    return [self._provides]

  def process(self, data):
    src = None
    try:
      src = urllib2.urlopen(self._url, timeout=options.timeout)
    except urllib2.URLError, e:
      sys.stderr.write('urllib2.urlopen error %s, waterfall status page down\n'
                       % str(e))
    if src is not None:
      try:
        data[self._provides] = src.read()
      except IOError, e:
        sys.stderr.write('urllib2 file object read error, network trouble?\n')
        data[self._provides] = ''

  def name(self):
    return 'UrlFetcher(%s,%s)' % (self._url, self._provides)


class MockScraper(Node):

  def __init__(self, filepath, data_key):
    Node.__init__(self)
    self._provides = data_key
    self._filepath = filepath

  def provides(self):
    return [self._provides]

  def process(self, data):
    # return mock data
    data[self._provides] = open(self._filepath).read()

  def name(self):
    return 'MockScraper(%s,%s)' % (self._filepath, self._provides)


class MockSink(Node):

  def __init__(self, tag):
    Node.__init__(self)
    self._requires = tag

  def requires(self):
    return [self._requires]

  def process(self, data):
    sys.stdout.write('%s\n' % str(data[self._requires]))

  def name(self):
    return 'MockSink(%s)' % self._requires


class Parser(HTMLParser.HTMLParser):
  """This class scrapes bot output for bot status.

The expected input is HTML where:

- The bot status page is a giant table, where the build status is a TR
  of class 'LastBuild', with TD elements of class 'LastBuild success',
  'LastBuild retry', 'LastBuild failure'.  Outside of the anchor text,
  immediately following the A element, the data is the bot status
  information.  All three (bot URL, bot name, and current bot status)
  are collected.

- Within the TD, the bot (relative) URL is the HREF URL in an A tag,
  and the data (anchor text) is the name of the bot, both of which are
  collected.

- Another row of the bot status page is a TR of class 'Acitivity',
  with TDs of class 'Activity building', 'Activity idle', 'Activity
  offline', with the text of the TD containing information such as the
  number of offline bots, etc.  The parser just collects the activity
  text for a later stage processor to handle.

When the parser is done, the list attributes bot_names, bot_urls,
bot_status, and bot_activity will contain the scraped information.
The order of appearance is the order found in the table.  If the
scraping is successful, the lengths of these lists will be identical.
Anything else should be considered a parse/scrape error.

  """

  def __init__(self, base_url):
    HTMLParser.HTMLParser.__init__(self)
    self._state = 0
    self._base_url = base_url
    self.bot_names = []
    self.bot_urls = []
    self.bot_status = []
    self.bot_activity = []
    self._data_is_botname = False
    self._data_is_status = False
    self._data_is_activity = False
    self._activity = []

  def handle_starttag(self, tag, attrs):
    log(2, 'tag(%s,%s)\n', tag, str(attrs))
    amap = dict(attrs)
    if tag == 'td' and amap.get('class', '').startswith('LastBuild'):
      self._state = 1
    elif self._state == 1 and tag == 'a' and 'href' in amap:
      self._data_is_botname = True
      log(2, 'boturl(%s)\n', amap['href'])
      self.bot_urls.append(urlparse.urljoin(self._base_url, amap['href']))
    elif (tag == 'td' and
          amap.get('class', '').startswith('Activity ')):
      self._data_is_activity = True

  def handle_endtag(self, tag):
    log(2, 'closetag(%s)\n', tag)
    if tag == 'td':
      self._state = 0
      if self._data_is_activity:
        self._data_is_activity = False
        self.bot_activity.append(self._activity)
        self._activity = []

  def handle_data(self, data):
    log(2, 'data(%s)\n', data)
    if self._data_is_botname:
      data = data.strip()
      log(1, 'botname(%s)\n', data)
      self.bot_names.append(data)
      self._data_is_botname = False
      self._data_is_status = True
      return
    if self._data_is_activity:
      data = data.strip()
      log(1, 'activity(%s)\n', data)
      self._activity.append(data)
      return
    if self._data_is_status:
      data = data.strip()
      log(1, 'status(%s)\n', data)
      self.bot_status.append(data)
      self._data_is_status = False


class ProcessBotData(Node):

  def __init__(self, base_url, requires, provides):
    Node.__init__(self)
    self._parser = Parser(base_url)
    self._requires = requires
    self._provides = provides

  def requires(self):
    return [self._requires]

  def provides(self):
    return [self._provides]

  def process(self, data):
    p = self._parser
    p.feed(data[self._requires])
    p.close()
    assert len(p.bot_names) == len(p.bot_urls)
    assert len(p.bot_names) == len(p.bot_status)
    assert len(p.bot_names) == len(p.bot_activity)
    data[self._provides] = zip(p.bot_names,
                               p.bot_urls,
                               p.bot_status,
                               p.bot_activity)


class LongQueueChecker(Node):

  def __init__(self, tag, out_tag, activities,
               queue_length_regex, max_queue_len,
               alerter):
    Node.__init__(self)
    self._requires = tag
    self._provides = out_tag
    self._activities = activities  # ['idle', 'offline']
    self._queue_length_regex = queue_length_regex
    self._max_queue_len = max_queue_len
    self._alerter = alerter

  def requires(self):
    return [self._requires]

  def provides(self):
    return [self._provides]

  def process(self, data):
    queue_info = {}
    for botname, url, status, activity in data[self._requires]:
      queue_len = 0
      if activity[0] in self._activities:
        for activity_detail in activity[1:]:
          mobj = self._queue_length_regex.search(activity_detail)
          if mobj is not None:
            queue_len = int(mobj.group(1))
            if queue_len > self._max_queue_len:
              self._alerter.alert((botname, url, status, activity))
      queue_info[botname] = queue_len
      log(1, 'queue_len[%s] = %f\n', botname, queue_len)
    data[self._provides] = queue_info


# Check build/trybot queue progress.  This might not be appropriate
# for all builders, esp very busy ones, since there could be false
# positives.
#
# The idea is as follows: the LongQueueChecker already extracts the
# current queue length, and ProgressCheck simply looks at how its
# value changes over time.  We do not look at the bot's pending build
# requests to check that the identities of the pending builds aren't
# changing, so we could very well think that no progress is being made
# when in reality the incoming jobs are arriving at a rate equal to
# the completion rate, maintaining the queue length.  (This is
# probably a feature TODO.)
#
# Anyhow, this processing node just remembers the last queue length.
# If the new queue length has decreased from that value, we assume
# that the bot is alive and healthy and reset the health status to
# 1.0.  If the bot's queue length has remained the same or increased,
# we decrease the health status using a low-pass filter -- so that
# only after many lack-of-progress events would we declare the bot to
# be unhealthy and generate an alert.
#
# mechanics of the low pass filter.
#
# In continuous form, the response to H_0(t) (the Heaviside step
# function, discontinuous step occuring at time 0) is given by
#
#   v(t) = 1 - math.exp(-t/RC).
#
# The feedback parameter for the discrete time simulation is, when t =
# n * period, just
#
#   math.exp(-(period / (RC))),
#
# Now, since the value of filter_coefficient is math.exp(-1/RC), to
# take the time period into account, we need to compute
#
#   math.pow(math.exp(-1/RC), period)
#
# normally we measure RC time in seconds, but for the purposes of this
# low pass filter we use minutes since we want the time decay to be
# slow.
#
# req_tags should be a list of strings, e.g., ['trybot-status',
# 'trybot-queue-len']

class ProgressCheck(Node):

  def __init__(self, req_tags, alerter,
               filter_coefficient, health_threshold_time):
    Node.__init__(self)
    self._requires = req_tags
    self._alerter = alerter
    self._filter_coefficient = math.pow(filter_coefficient,
                                        options.period / 60.0)
    # We want the threshold to be set so that it would take 4 hours of
    # zero queue progress before an alert is generated.  This should
    # cover most cases of false positives generated by a steadily fed
    # queue that just happens to not shrink in length much.
    health_threshold_for_alert = math.pow(self._filter_coefficient,
                                          health_threshold_time)
    self._health_threshold_for_alert = health_threshold_for_alert
    self._last_queue_len = {}
    self._health = {}

  def requires(self):
    return self._requires

  def process(self, data):
    qlen = data[self._requires[1]]
    for botname, url, status, activity in data[self._requires[0]]:
      last_queue_len = self._last_queue_len.get(botname, 0)
      if qlen[botname] == 0 or qlen[botname] < last_queue_len:
        log(1, 'bot %s made progress\n', botname)
        curr_health = 1.0
      else:
        curr_health = 0.0
        # Consider making this term like
        #   (qlen[botname] - last_queue_len)
        # to accelerate health decrease when the queue is growing.
      orig_health = self._health.get(botname, 1.0)
      self._health[botname] = ((orig_health * self._filter_coefficient) +
                               (curr_health * (1.0 - self._filter_coefficient)))
      log(1, 'bot %s health %f, coeff %f\n',
          botname, self._health[botname], self._filter_coefficient)
      if self._health[botname] < self._health_threshold_for_alert:
        self._alerter.alert((botname, url, self._health[botname]))


class Monitor(object):

  def __init__(self, bot_description, message_prefix=''):
    self._bot_description = bot_description
    self._message_prefix = message_prefix
    self._bad_bots = []

  def init(self):
    self._bad_bots = []

  def alert(self, info):
    self._bad_bots.append(info)

  def flush(self):
    if len(self._bad_bots) == 0:
      return
    log(0, 'alert: %s', self._bot_description)
    cmdline = [options.binmail_cmd, '-s', self._bot_description]
    if options.notifications_cc != '':
      cmdline += ['-c', options.notifications_cc ]
    if options.notifications_to != '':
      cmdline += options.notifications_to.split(',')
    p = subprocess.Popen(cmdline, stdin=subprocess.PIPE)
    if self._message_prefix != '':
      p.stdin.write(self._message_prefix)
    self.write_alert_body(p.stdin)
    if options.verbosity > 1:
      self.write_alert_body(sys.stdout)
    p.stdin.close()

  def write_alert_body(self, stream):
    raise NotImplementedError(type(self).__name__ + '.write_alert_body(' +
                              str(stream) + ')')


class QueueMonitor(Monitor):

  def __init__(self, *args, **kwargs):
    Monitor.__init__(self, *args, **kwargs)

  def write_alert_body(self, stream):
    for t in self._bad_bots:
      botname, url, status, activity = t
      stream.write('botname: %s\nurl: %s\nstatus: %s\n\n%s\n\n\n' %
                   (botname, url, status, '\n'.join(activity)))


class ProgressMonitor(Monitor):

  def __init__(self, *args, **kwargs):
    Monitor.__init__(self, *args, **kwargs)

  def write_alert_body(self, stream):
    for t in self._bad_bots:
      stream.write(
        'botname %s\nurl: %s\nQueue Progress (load-avg style) %f\n\n'
        % t)


def Main(argv):
  global options

  parser = optparse.OptionParser()
  parser.add_option('-v', '--verbosity', dest='verbosity',
                    type='int', default=0,
                    help='set verbosity level for debug output')
  parser.add_option('-p', '--poll_period', dest='period',
                    type='int', default=30 * 60,
                    help='bot polling period, in seconds')
  parser.add_option('-t', '--trybot', dest='trybot', default=False,
                    action='store_true', help='monitor trybot')
  parser.add_option('-b', '--buildbot', dest='buildbot',
                    default=False, action='store_true',
                    help='monitor buildbot')
  parser.add_option('-T', '--trybot-url', dest='trybot_url',
                    default=try_url, help='trybot waterfall url')
  parser.add_option('-B', '--buildbot-url', dest='buildbot_url',
                    default=build_url, help='buildbot waterfall url')
  parser.add_option('-m', '--mock', dest='mock',
                    default=False, action='store_true',
                    help='use mock data sources')
  parser.add_option('-q', '--queue-len-max', dest='max_queue_len',
                    type='int', default=10,
                    help=('maximum bot work queue length beyond which'
                          ' alerts are generated'))
  parser.add_option('-n', '--notifications-to', dest='notifications_to',
                    default=notifications_to,
                    help='comma separated list of email addresses')
  parser.add_option('-c', '--notifications-cc', dest='notifications_cc',
                    default=notifications_cc,
                    help='comma separated list of email addresses')
  parser.add_option('-f', '--fetch-timeout', dest='timeout',
                    type='int', default=10,
                    help='urllib2 fetch timeout')
  parser.add_option('-M', '--binmail-cmd', dest='binmail_cmd',
                    default='mail',
                    help='name of binmail compatible mailer')
  parser.add_option('--time-const', dest='time_const',
                    default=0.9, type='float',
                    help='exponential decay time constant for health alert')
  parser.add_option('--health-time', dest='health_time',
                    default=4 * 60, type='float',
                    help=('approx duration of "bad health" (minutes) before' +
                          ' generating health alert'))

  options, args = parser.parse_args(argv)

  if not options.trybot and not options.buildbot:
    sys.stdout.write('No bots enabled; nothing to do.  Bye.\n')
    return
  message_prefix = """
The health monitoring trigger uses the queue length as an indication
of bot health.  When the queue length is non-zero and unchanged or
increases, the bot is heuristically assumed to be in poor condition.
This data is put through a low-pass filter, so that it should take
many samples indicating poor health before an alert message (like this
one) is generated.

"""

  graph = Graph()
  try_alerter = QueueMonitor(
    'NaCl BotMon: trybots queue-length threshold trigger')
  build_alerter = QueueMonitor(
    'NaCl BotMon: buildbot queue-length threshold trigger')
  try_progress_alerter = ProgressMonitor(
    'NaCl BotMon: trybot queue progress trigger',
    message_prefix=message_prefix)
  build_progress_alerter = ProgressMonitor(
    'NaCl BotMon: buildbot queue progress trigger',
    message_prefix=message_prefix)
  if options.trybot:
    graph.add_node(ProcessBotData(options.trybot_url,
                                  'trybot-raw-html',
                                  'trybot-status'))
    if options.verbosity > 1:
      graph.add_node(MockSink('trybot-status'))
    graph.add_node(LongQueueChecker('trybot-status',
                                    'trybot-queue-len',
                                    ['idle', 'offline'],
                                    re.compile(r'(\d+) pending'),
                                    options.max_queue_len,
                                    try_alerter))
    graph.add_node(
      ProgressCheck(['trybot-status', 'trybot-queue-len'],
                    try_progress_alerter,
                    options.time_const,
                    options.health_time))
    if options.mock:
      graph.add_node(MockScraper('testdata/trybot-testdata.html',
                                 'trybot-raw-html'))
    else:
      graph.add_node(UrlFetcher(options.trybot_url, 'trybot-raw-html'))

  if options.buildbot:
    graph.add_node(ProcessBotData(options.buildbot_url,
                                  'buildbot-raw-html',
                                  'buildbot-status'))
    if options.verbosity > 1:
      graph.add_node(MockSink('buildbot-status'))
    graph.add_node(LongQueueChecker('buildbot-status',
                                    'buildbot-queue-len',
                                    ['idle', 'offline'],
                                    re.compile(r'(\d+) pending'),
                                    options.max_queue_len,
                                    build_alerter))
    graph.add_node(ProgressCheck(['buildbot-status', 'buildbot-queue-len'],
                                 build_progress_alerter,
                                 options.time_const,
                                 options.health_time))
    if options.mock:
      graph.add_node(MockScraper('testdata/buildbot-testdata.html',
                                 'buildbot-raw-html'))
    else:
      graph.add_node(UrlFetcher(options.buildbot_url, 'buildbot-raw-html'))
  execution_order = graph.execution_order()

  if options.verbosity > 1:
    sys.stdout.write('Execution order: ')
    sep = ''
    for n in execution_order:
      sys.stdout.write('%s%s' % (sep,n.name()))
      sep = ', '
    sys.stdout.write('\n')

  while True:
    d = {}
    try_alerter.init()
    build_alerter.init()
    for n in execution_order:
      n.process(d)
    try_alerter.flush()
    try_progress_alerter.flush()
    build_alerter.flush()
    build_progress_alerter.flush()
    sys.stdout.flush()
    d = None  # allow gc before sleeping
    time.sleep(options.period)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
