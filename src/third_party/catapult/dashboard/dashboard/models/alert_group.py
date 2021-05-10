# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The database model for an "Anomaly", which represents a step up or down."""
from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import uuid

from google.appengine.ext import ndb

# Move import of protobuf-dependent code here so that all AppEngine work-arounds
# have a chance to be live before we import any proto code.
from dashboard import sheriff_config_client


class RevisionRange(ndb.Model):
  repository = ndb.StringProperty()
  start = ndb.IntegerProperty()
  end = ndb.IntegerProperty()

  def IsOverlapping(self, b):
    if not b or self.repository != b.repository:
      return False
    return max(self.start, b.start) <= min(self.end, b.end)


class BugInfo(ndb.Model):
  project = ndb.StringProperty()
  bug_id = ndb.IntegerProperty()


class AlertGroup(ndb.Model):
  name = ndb.StringProperty(indexed=True)
  domain = ndb.StringProperty(indexed=True)
  subscription_name = ndb.StringProperty(indexed=True)
  created = ndb.DateTimeProperty(indexed=False, auto_now_add=True)
  updated = ndb.DateTimeProperty(indexed=False, auto_now_add=True)

  class Status(object):
    unknown = 0
    untriaged = 1
    triaged = 2
    bisected = 3
    closed = 4

  status = ndb.IntegerProperty(indexed=False)

  class Type(object):
    test_suite = 0
    logical = 1
    reserved = 2

  group_type = ndb.IntegerProperty(
      indexed=False,
      choices=[Type.test_suite, Type.logical, Type.reserved],
      default=Type.test_suite,
  )
  active = ndb.BooleanProperty(indexed=True)
  revision = ndb.LocalStructuredProperty(RevisionRange)
  bug = ndb.LocalStructuredProperty(BugInfo)
  project_id = ndb.StringProperty(indexed=True, default='chromium')
  bisection_ids = ndb.StringProperty(repeated=True)
  anomalies = ndb.KeyProperty(repeated=True)

  def IsOverlapping(self, b):
    return (self.name == b.name and self.domain == b.domain
            and self.subscription_name == b.subscription_name
            and self.project_id == b.project_id
            and self.group_type == b.group_type
            and self.revision.IsOverlapping(b.revision))

  @classmethod
  def GetType(cls, anomaly_entity):
    if anomaly_entity.alert_grouping:
      return cls.Type.logical
    return cls.Type.test_suite

  @classmethod
  def GenerateAllGroupsForAnomaly(cls,
                                  anomaly_entity,
                                  sheriff_config=None,
                                  subscriptions=None):
    if subscriptions is None:
      sheriff_config = (sheriff_config
                        or sheriff_config_client.GetSheriffConfigClient())
      subscriptions, _ = sheriff_config.Match(anomaly_entity.test.string_id(),
                                              check=True)
    names = anomaly_entity.alert_grouping or [anomaly_entity.benchmark_name]

    return [
        cls(
            id=str(uuid.uuid4()),
            name=group_name,
            domain=anomaly_entity.master_name,
            subscription_name=s.name,
            project_id=s.monorail_project_id,
            status=cls.Status.untriaged,
            group_type=cls.GetType(anomaly_entity),
            active=True,
            revision=RevisionRange(
                repository='chromium',
                start=anomaly_entity.start_revision,
                end=anomaly_entity.end_revision,
            ),
        ) for s in subscriptions for group_name in names
    ]

  @classmethod
  def GetGroupsForAnomaly(cls, anomaly_entity, subscriptions):
    names = anomaly_entity.alert_grouping or [anomaly_entity.benchmark_name]
    group_type = cls.GetType(anomaly_entity)
    revision = RevisionRange(
        repository='chromium',
        start=anomaly_entity.start_revision,
        end=anomaly_entity.end_revision,
    )
    groups = []
    for name in names:
      subscription_names = [s.name for s in subscriptions]
      groups.extend(g for g in cls.Get(name, group_type, revision)
                    if g.subscription_name in subscription_names and all(
                        not added.IsOverlapping(g) for added in groups))
      all_groups = cls.GenerateAllGroupsForAnomaly(
          anomaly_entity,
          subscriptions=subscriptions,
      )
      if not groups or not all(
          any(g1.IsOverlapping(g2) for g2 in groups) for g1 in all_groups):
        groups += cls.Get('Ungrouped', cls.Type.reserved, None)
    return [g.key for g in groups]

  @classmethod
  def GetByID(cls, group_id):
    return ndb.Key('AlertGroup', group_id).get()

  @classmethod
  def Get(cls, group_name, group_type, revision_info, active=True):
    query = cls.query(
        cls.active == active,
        cls.name == group_name,
    )
    if not revision_info:
      return [
          group for group in query.fetch() if group.group_type == group_type
      ]
    return [
        group for group in query.fetch()
        if revision_info and revision_info.IsOverlapping(group.revision)
        and group.group_type == group_type
    ]

  @classmethod
  def GetAll(cls, active=True):
    groups = cls.query(cls.active == active).fetch()
    return groups or []
