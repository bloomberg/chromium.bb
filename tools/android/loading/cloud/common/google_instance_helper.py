# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
from googleapiclient import (discovery, errors)
import time

from googleapiclient import (discovery, errors)


class GoogleInstanceHelper(object):
  """Helper class for the Google Compute API, allowing to manage groups of
  instances more easily. Groups of instances are identified by a tag."""

  def __init__(self, credentials, project, logger):
    self._compute_api = discovery.build('compute','v1', credentials=credentials)
    self._project = project
    self._api_url = 'https://www.googleapis.com/compute/v1/projects/' + project
    self._zone = 'europe-west1-c'
    self._logger = logger

  def _ExecuteApiRequest(self, request, retry_count=3):
    """ Executes a Compute API request and returns True on success."""
    self._logger.info('Compute API request:')
    self._logger.info(request.to_json())
    try:
      response = request.execute()
      self._logger.info('Compute API response:')
      self._logger.info(response)
      return True
    except errors.HttpError as err:
      error_content = self._GetErrorContent(err)
      error_reason = self._GetErrorReason(error_content)
      if error_reason == 'resourceNotReady' and retry_count > 0:
        # Retry after a delay
        delay_seconds = 1
        self._logger.info(
            'Resource not ready, retrying in %i seconds.' % delay_seconds)
        time.sleep(delay_seconds)
        return self._ExecuteApiRequest(request, retry_count - 1)
      else:
        self._logger.error('Compute API error (reason: "%s"):\n%s' % (
            error_reason, err))
        if error_content:
          self._logger.error('Error details:\n%s' % error_content)
        return False

  def _GetTemplateName(self, tag):
    """Returns the name of the instance template associated with tag."""
    return 'template-' + tag

  def _GetInstanceGroupName(self, tag):
    """Returns the name of the instance group associated with tag."""
    return 'group-' + tag

  def _GetErrorContent(self, error):
    """Returns the contents of an error returned by the Compute API as a
    dictionary.
    """
    if not error.resp.get('content-type', '').startswith('application/json'):
      return None
    return json.loads(error.content)

  def _GetErrorReason(self, error_content):
    """Returns the error reason as a string."""
    if not error_content:
      return None
    if (not error_content.get('error') or
        not error_content['error'].get('errors')):
      return None
    error_list = error_content['error']['errors']
    if len(error_list) == 0:
      return None
    return error_list[0].get('reason', '')

  def CreateTemplate(self, tag, bucket):
    """Creates an instance template for instances identified by tag and using
    bucket for deployment. Returns True if successful.
    """
    image_url = 'https://www.googleapis.com/compute/v1/projects/' \
                'ubuntu-os-cloud/global/images/ubuntu-1404-trusty-v20160406'
    request_body = {
        'name': self._GetTemplateName(tag),
        'properties': {
            'machineType': 'n1-standard-1',
            'networkInterfaces': [{
                'network': self._api_url + '/global/networks/default',
                'accessConfigs': [{
                    'name': 'external-IP',
                    'type': 'ONE_TO_ONE_NAT'
            }]}],
            'disks': [{
                'type': 'PERSISTENT',
                'boot': True,
                'autoDelete': True,
                'mode': 'READ_WRITE',
                'initializeParams': {'sourceImage': image_url}}],
            'canIpForward': False,
            'scheduling': {
                'automaticRestart': True,
                'onHostMaintenance': 'MIGRATE',
                'preemptible': False},
            'serviceAccounts': [{
                'scopes': [
                    'https://www.googleapis.com/auth/cloud-platform',
                    'https://www.googleapis.com/auth/cloud-taskqueue'],
                'email': 'default'}],
            'metadata': { 'items': [
                {'key': 'cloud-storage-path',
                 'value': bucket},
                {'key': 'startup-script-url',
                 'value': 'gs://%s/deployment/startup-script.sh' % bucket},
                {'key': 'taskqueue-tag', 'value': tag}]}}}
    request = self._compute_api.instanceTemplates().insert(
        project=self._project, body=request_body)
    return self._ExecuteApiRequest(request)

  def DeleteTemplate(self, tag):
    """Deletes the instance template associated with tag. Returns True if
    successful.
    """
    request = self._compute_api.instanceTemplates().delete(
        project=self._project,
        instanceTemplate=self._GetTemplateName(tag))
    return self._ExecuteApiRequest(request)

  def CreateInstances(self, tag, instance_count):
    """Creates an instance group associated with tag. The instance template must
    exist for this to succeed. Returns True if successful.
    """
    template_url = '%s/global/instanceTemplates/%s' % (
        self._api_url, self._GetTemplateName(tag))
    request_body = {
        'zone': self._zone, 'targetSize': instance_count,
        'baseInstanceName': 'instance-' + tag,
        'instanceTemplate': template_url,
        'name': self._GetInstanceGroupName(tag)}
    request = self._compute_api.instanceGroupManagers().insert(
        project=self._project, zone=self._zone,
        body=request_body)
    return self._ExecuteApiRequest(request)

  def DeleteInstance(self, tag, instance_hostname):
    """Deletes one instance from the instance group identified with tag. Returns
    True if successful.
    """
    # The instance hostname may be of the form <name>.c.<project>.internal but
    # only the <name> part should be passed to the compute API.
    name = instance_hostname.split('.')[0]
    instance_url = self._api_url + (
        "/zones/%s/instances/%s" % (self._zone, name))
    request = self._compute_api.instanceGroupManagers().deleteInstances(
        project=self._project, zone=self._zone,
        instanceGroupManager=self._GetInstanceGroupName(tag),
        body={'instances': [instance_url]})
    return self._ExecuteApiRequest(request)
