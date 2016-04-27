# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import time

import flask
from google.appengine.api import (app_identity, taskqueue)
from google.appengine.ext import deferred
from oauth2client.client import GoogleCredentials

from common.clovis_task import ClovisTask
import common.google_instance_helper
from memory_logs import MemoryLogs


# Global variables.
clovis_logger = logging.getLogger('clovis_frontend')
clovis_logger.setLevel(logging.DEBUG)
project_name = app_identity.get_application_id()
instance_helper = common.google_instance_helper.GoogleInstanceHelper(
    credentials=GoogleCredentials.get_application_default(),
    project=project_name,
    logger=clovis_logger)
app = flask.Flask(__name__)


def Render(message, memory_logs):
  return flask.render_template(
      'log.html', body=message, log=memory_logs.Flush().split('\n'))


def PollWorkers(tag, start_time, timeout_hours):
  """Checks if there are workers associated with tag, by polling the instance
  group. When all workers are finished, the instance group and the instance
  template are destroyed.
  After some timeout delay, the instance group is destroyed even if there are
  still workers associated to it, which has the effect of killing all these
  workers.

  Args:
    tag (string): Tag of the task that is polled.
    start_time (float): Time when the polling started, as returned by
                        time.time().
    timeout_hours (int): Timeout after which workers are terminated.
  """
  if (time.time() - start_time) > (3600 * timeout_hours):
    clovis_logger.error('Worker timeout for tag %s, shuting down.' % tag)
    deferred.defer(DeleteInstanceGroup, tag)
    return

  clovis_logger.info('Polling workers for tag: ' + tag)
  live_instance_count = instance_helper.GetInstanceCount(tag)
  clovis_logger.info('%i live instances for tag %s.' % (
      live_instance_count, tag))

  if live_instance_count > 0 or live_instance_count == -1:
    clovis_logger.info('Retry later, instances still alive for tag: ' + tag)
    poll_interval_minutes = 10
    deferred.defer(PollWorkers, tag, start_time,
                   _countdown=(60 * poll_interval_minutes))
    return

  clovis_logger.info('Scheduling instance group destruction for tag: ' + tag)
  deferred.defer(DeleteInstanceGroup, tag)


def CreateInstanceTemplate(task):
  """Create the Compute Engine instance template that will be used to create the
  instances.
  """
  backend_params = task.BackendParams()
  instance_count = backend_params.get('instance_count', 0)
  if instance_count <= 0:
    clovis_logger.info('No template required.')
    return True
  bucket = backend_params.get('storage_bucket')
  if not bucket:
    clovis_logger.error('Missing bucket in backend_params.')
    return False
  return instance_helper.CreateTemplate(task.BackendParams()['tag'], bucket)


def CreateInstances(task):
  """Creates the Compute engine requested by the task."""
  backend_params = task.BackendParams()
  instance_count = backend_params.get('instance_count', 0)
  if instance_count <= 0:
    clovis_logger.info('No instances to create.')
    return True
  return instance_helper.CreateInstances(backend_params['tag'], instance_count)


def DeleteInstanceGroup(tag, try_count=0):
  """Deletes the instance group associated with tag, and schedules the deletion
  of the instance template."""
  clovis_logger.info('Instance group destruction for tag: ' + tag)
  if not instance_helper.DeleteInstanceGroup(tag):
    clovis_logger.info('Instance group destruction failed for: ' + tag)
    if try_count <= 5:
      deferred.defer(DeleteInstanceGroup, tag, try_count + 1, _countdown=60)
      return
    clovis_logger.error('Giving up group destruction for: ' + tag)
  clovis_logger.info('Scheduling instance template destruction for tag: ' + tag)
  # Wait a little before deleting the instance template, because it may still be
  # considered in use, causing failures.
  deferred.defer(DeleteInstanceTemplate, tag, _countdown=30)


def DeleteInstanceTemplate(tag, try_count=0):
  """Deletes the instance template associated with tag."""
  clovis_logger.info('Instance template destruction for tag: ' + tag)
  if not instance_helper.DeleteTemplate(tag):
    clovis_logger.info('Instance template destruction failed for: ' + tag)
    if try_count <= 5:
      deferred.defer(DeleteInstanceTemplate, tag, try_count + 1, _countdown=60)
      return
    clovis_logger.error('Giving up template destruction for: ' + tag)
  clovis_logger.info('Cleanup complete for tag: ' + tag)


def StartFromJsonString(http_body_str):
  """Main function handling a JSON task posted by the user."""
  # Set up logging.
  memory_logs = MemoryLogs(clovis_logger)
  memory_logs.Start()

  # Load the task from JSON.
  task = ClovisTask.FromJsonString(http_body_str)
  if not task:
    clovis_logger.error('Invalid JSON task.')
    return Render('Invalid JSON task:\n' + http_body_str, memory_logs)

  task_tag = task.BackendParams()['tag']

  # Create the instance template if required.
  if not CreateInstanceTemplate(task):
    return Render('Template creation failed.', memory_logs)

  # Split the task in smaller tasks.
  sub_tasks = []
  if task.Action() == 'trace':
    sub_tasks = SplitTraceTask(task)
  else:
    error_string = 'Unsupported action: %s.' % task.Action()
    clovis_logger.error(error_string)
    return Render(error_string, memory_logs)

  if not EnqueueTasks(sub_tasks, task_tag):
    return Render('Task creation failed.', memory_logs)

  # Start the instances if required.
  if not CreateInstances(task):
    return Render('Instance creation failed.', memory_logs)

  # Start polling the progress.
  clovis_logger.info('Creating worker polling task.')
  first_poll_delay_minutes = 10
  timeout_hours = task.BackendParams().get('timeout_hours', 5)
  deferred.defer(PollWorkers, task_tag, time.time(), timeout_hours,
                 _countdown=(60 * first_poll_delay_minutes))

  return Render('Success', memory_logs)


def SplitTraceTask(task):
  """Splits a tracing task with potentially many URLs into several tracing tasks
  with few URLs.
  """
  clovis_logger.debug('Splitting trace task.')
  action_params = task.ActionParams()
  urls = action_params['urls']

  # Split the task in smaller tasks with fewer URLs each.
  urls_per_task = 1
  sub_tasks = []
  for i in range(0, len(urls), urls_per_task):
    sub_task_params = action_params.copy()
    sub_task_params['urls'] = [url for url in urls[i:i+urls_per_task]]
    sub_tasks.append(ClovisTask(task.Action(), sub_task_params,
                                task.BackendParams()))
  return sub_tasks


def EnqueueTasks(tasks, task_tag):
  """Enqueues a list of tasks in the Google Cloud task queue, for consumption by
  Google Compute Engine.
  """
  q = taskqueue.Queue('clovis-queue')
  retry_options = taskqueue.TaskRetryOptions(task_retry_limit=3)
  # Add tasks to the queue by groups.
  # TODO(droger): This supports thousands of tasks, but maybe not millions.
  # Defer the enqueuing if it times out.
  group_size = 100
  callbacks = []
  try:
    for i in range(0, len(tasks), group_size):
      group = tasks[i:i+group_size]
      taskqueue_tasks = [
          taskqueue.Task(payload=task.ToJsonString(), method='PULL',
                         tag=task_tag, retry_options=retry_options)
          for task in group]
      rpc = taskqueue.create_rpc()
      q.add_async(task=taskqueue_tasks, rpc=rpc)
      callbacks.append(rpc)
    for callback in callbacks:
      callback.get_result()
  except Exception as e:
    clovis_logger.error('Exception:' + type(e).__name__ + ' ' + str(e.args))
    return False
  clovis_logger.info('Pushed %i tasks with tag: %s.' % (len(tasks), task_tag))
  return True


@app.route('/')
def Root():
  """Home page: redirect to the static form."""
  return flask.redirect('/static/form.html')


@app.route('/form_sent', methods=['POST'])
def StartFromForm():
  """HTML form endpoint."""
  data_stream = flask.request.files.get('json_task')
  if not data_stream:
    return 'failed'
  http_body_str = data_stream.read()
  return StartFromJsonString(http_body_str)


@app.errorhandler(404)
def PageNotFound(e):  # pylint: disable=unused-argument
  """Return a custom 404 error."""
  return 'Sorry, Nothing at this URL.', 404


@app.errorhandler(500)
def ApplicationError(e):
  """Return a custom 500 error."""
  return 'Sorry, unexpected error: {}'.format(e), 499
