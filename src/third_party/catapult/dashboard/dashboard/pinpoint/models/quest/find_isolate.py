# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import urlparse

from dashboard.pinpoint.models import isolate
from dashboard.pinpoint.models.quest import execution
from dashboard.pinpoint.models.quest import quest
from dashboard.services import buildbucket_service
from dashboard.services import gerrit_service


BUCKET = 'master.tryserver.chromium.perf'


class BuildError(Exception):
  """Raised when the build fails."""


class IsolateNotFoundError(StandardError):
  """Raised when the build succeeds, but Pinpoint can't find the isolate.

  This error is fatal to the Job.
  """


class FindIsolate(quest.Quest):

  def __init__(self, builder, target, bucket):
    self._builder_name = builder
    self._target = target
    self._bucket = bucket

    self._previous_builds = {}

  def __eq__(self, other):
    return (isinstance(other, type(self)) and
            self._bucket == other._bucket and
            self._builder_name == other._builder_name)

  def __str__(self):
    return 'Build'

  def Start(self, change):
    return _FindIsolateExecution(self._builder_name, self._target, self._bucket,
                                 change, self._previous_builds)

  @classmethod
  def FromDict(cls, arguments):
    builder = arguments.get('builder')
    if not builder:
      raise TypeError('Missing "builder" argument.')

    target = arguments.get('target')
    if not target:
      raise TypeError('Missing "target" argument.')

    bucket = arguments.get('bucket')
    if not bucket:
      raise TypeError('Missing "bucket" argument.')

    return cls(builder, target, bucket)


class _FindIsolateExecution(execution.Execution):

  def __init__(self, builder_name, target, bucket, change, previous_builds):
    super(_FindIsolateExecution, self).__init__()
    self._builder_name = builder_name
    self._target = target
    self._bucket = bucket
    self._change = change
    # previous_builds is shared among all Executions of the same Quest.
    self._previous_builds = previous_builds

    self._build = None
    self._build_url = None

  def _AsDict(self):
    details = []
    details.append({
        'key': 'builder',
        'value': self._builder_name,
    })
    if self._build:
      details.append({
          'key': 'build',
          'value': self._build,
          'url': self._build_url if hasattr(self, '_build_url') else None,
      })
    if self._result_arguments:
      details.append({
          'key': 'isolate',
          'value': self._result_arguments['isolate_hash'],
          'url': self._result_arguments['isolate_server'] + '/browse?digest=' +
                 self._result_arguments['isolate_hash'],
      })
    return details

  def _Poll(self):
    if self._CheckIsolateCache():
      return

    if self._build:
      self._CheckBuildStatus()
      return

    self._RequestBuild()

  def _CheckIsolateCache(self):
    """Checks the isolate cache to see if a build is already available.

    Returns:
      True iff the isolate was found, meaning the execution is completed.
    """
    try:
      isolate_server, isolate_hash = isolate.Get(
          self._builder_name, self._change, self._target)
    except KeyError:
      return False

    result_arguments = {
        'isolate_server': isolate_server,
        'isolate_hash': isolate_hash,
    }
    self._Complete(result_arguments=result_arguments)
    return True

  def _CheckBuildStatus(self):
    """Checks on the status of a previously requested build.

    Raises:
      BuildError: The build failed, was canceled, or didn't produce an isolate.
    """
    build = buildbucket_service.GetJobStatus(self._build)['build']

    self._build_url = build.get('url')

    if build['status'] != 'COMPLETED':
      return

    if build['result'] == 'FAILURE':
      raise BuildError('Build failed: ' + build['failure_reason'])
    if build['result'] == 'CANCELED':
      raise BuildError('Build was canceled: ' + build['cancelation_reason'])

    # The build succeeded. Parse the result and complete this Quest.
    properties = json.loads(build['result_details_json'])['properties']

    commit_position = properties['got_revision_cp'].replace('@', '(at)')
    suffix = 'with_patch' if 'patch_storage' in properties else 'without_patch'
    key = '_'.join(('swarm_hashes', commit_position, suffix))

    if self._target not in properties[key]:
      raise IsolateNotFoundError(
          'Buildbucket says the build completed successfully, '
          "but Pinpoint can't find the isolate hash.")

    result_arguments = {
        'isolate_server': properties['isolate_server'],
        'isolate_hash': properties[key][self._target],
    }
    self._Complete(result_arguments=result_arguments)

  @property
  def bucket(self):
    if hasattr(self, '_bucket'):
      return self._bucket
    return BUCKET

  def _RequestBuild(self):
    """Requests a build.

    If a previous Execution already requested a build for this Change, returns
    that build instead of requesting a new one.
    """
    if self._change in self._previous_builds:
      # If another Execution already requested a build, reuse that one.
      self._build = self._previous_builds[self._change]
    else:
      # Request a build!
      buildbucket_info = _RequestBuild(
          self._builder_name, self._change, self.bucket)

      self._build = buildbucket_info['build']['id']
      self._previous_builds[self._change] = self._build


def _RequestBuild(builder_name, change, bucket):
  base_as_dict = change.base_commit.AsDict()
  review_url = base_as_dict.get('review_url')
  if not review_url:
    raise BuildError(
        'Could not find gerrit review url for commit: ' + str(base_as_dict))

  change_id = base_as_dict.get('change_id')
  if not change_id:
    raise BuildError(
        'Could not find gerrit change id for commit: ' + str(base_as_dict))

  url_parts = urlparse.urlparse(review_url)
  base_review_url = urlparse.urlunsplit(
      (url_parts.scheme, url_parts.netloc, '', '', ''))

  change_info = gerrit_service.GetChange(base_review_url, change_id)

  commit_url_parts = urlparse.urlparse(base_as_dict['url'])

  # Note: The ordering here for buildbucket v1 api is important.
  # crbug.com/937392
  builder_tags = []
  if change.patch:
    builder_tags.append(change.patch.BuildsetTags())
  builder_tags.append(
      'buildset:commit/gitiles/%s/%s/+/%s' % (
          commit_url_parts.netloc,
          change_info['project'],
          change.base_commit.git_hash)
  )

  deps_overrides = {dep.repository_url: dep.git_hash for dep in change.deps}
  parameters = {
      'builder_name': builder_name,
      'properties': {
          'clobber': True,
          'revision': change.base_commit.git_hash,
          'deps_revision_overrides': deps_overrides,
      },
  }

  if change.patch:
    parameters['properties'].update(change.patch.BuildParameters())

  # TODO: Look up Buildbucket bucket from builder_name.
  return buildbucket_service.Put(bucket, builder_tags, parameters)
