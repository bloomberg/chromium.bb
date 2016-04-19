# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import flask
from google.appengine.api import taskqueue
import json
import os
import sys
import uuid

from common.clovis_task import ClovisTask


app = flask.Flask(__name__)


def StartFromJson(http_body_str):
  """Creates a new batch of tasks from its JSON representation."""
  task = ClovisTask.FromJsonDict(http_body_str)
  if not task:
    return 'Invalid JSON task:\n%s\n' % http_body_str

  task_tag = task.TaskqueueTag()
  if not task_tag:
    task_tag = uuid.uuid1()

  sub_tasks = []
  if task.Action() == 'trace':
    sub_tasks = SplitTraceTask(task)
  else:
    return 'Unsupported action: %s\n' % task.Action()

  return EnqueueTasks(sub_tasks, task_tag)


def SplitTraceTask(task):
  """Split a tracing task with potentially many URLs into several tracing tasks
  with few URLs.
  """
  params = task.Params()
  urls = params['urls']

  # Split the task in smaller tasks with fewer URLs each.
  urls_per_task = 1
  sub_tasks = []
  for i in range(0, len(urls), urls_per_task):
    sub_task_params = params.copy()
    sub_task_params['urls'] = [url for url in urls[i:i+urls_per_task]]
    sub_tasks.append(ClovisTask(task.Action(), sub_task_params,
                                task.TaskqueueTag()))
  return sub_tasks


def EnqueueTasks(tasks, task_tag):
  """Enqueues a list of tasks in the Google Cloud task queue, for consumption by
  Google Compute Engine.
  """
  q = taskqueue.Queue('clovis-queue')
  retry_options = taskqueue.TaskRetryOptions(task_retry_limit=3)
  # Add tasks to the queue by groups.
  # TODO(droger): This support to thousands of tasks, but maybe not millions.
  # Defer the enqueuing if it times out.
  # is too large.
  group_size = 100
  callbacks = []
  try:
    for i in range(0, len(tasks), group_size):
      group = tasks[i:i+group_size]
      taskqueue_tasks = [
          taskqueue.Task(payload=task.ToJsonDict(), method='PULL', tag=task_tag,
                         retry_options=retry_options)
          for task in group]
      rpc = taskqueue.create_rpc()
      q.add_async(task=taskqueue_tasks, rpc=rpc)
      callbacks.append(rpc)
    for callback in callbacks:
      callback.get_result()
  except Exception as e:
    return 'Exception:' + type(e).__name__ + ' ' + str(e.args) + '\n'
  return 'pushed %i tasks with tag: %s\n' % (len(tasks), task_tag)


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
  return StartFromJson(http_body_str)


@app.errorhandler(404)
def PageNotFound(e):  # pylint: disable=unused-argument
  """Return a custom 404 error."""
  return 'Sorry, Nothing at this URL.', 404


@app.errorhandler(500)
def ApplicationError(e):
  """Return a custom 500 error."""
  return 'Sorry, unexpected error: {}'.format(e), 499
