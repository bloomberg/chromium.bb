# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

import flask
from google.appengine.api import (app_identity, taskqueue)
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
  """Creates the Compute engine requested by the task"""
  backend_params = task.BackendParams()
  instance_count = backend_params.get('instance_count', 0)
  if instance_count <= 0:
    clovis_logger.info('No instances to create.')
    return True
  return instance_helper.CreateInstances(backend_params['tag'], instance_count)


def StartFromJsonString(http_body_str):
  """Main function handling a JSON task posted by the user"""
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
    return Render('Task creation failed', memory_logs)

  # Start the instances if required.
  if not CreateInstances(task):
    return Render('Instance creation failed', memory_logs)

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
  clovis_logger.info('Pushed %i tasks with tag: %s' % (len(tasks), task_tag))
  return True


@app.route('/')
def Root():
  """Home page: redirect to the static form."""
  return flask.redirect('/static/form.html')


@app.route('/form_sent', methods=['POST'])
def StartFromForm():
  """HTML form endpoint"""
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
