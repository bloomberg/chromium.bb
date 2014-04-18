# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class CalendarForwardBackwardPage(page_module.PageWithDefaultRunNavigate):

  """ Why: Click forward(4x) and backwards(4x) repeatedly """

  def __init__(self, page_set):
    super(CalendarForwardBackwardPage, self).__init__(
      url='https://www.google.com/calendar/',
      page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.credentials = 'google'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/calendar_forward_backward.json'
    self.name = 'calendar_forward_backward'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="navForward"]'
      }))
    action_runner.RunAction(JavascriptAction(
      {
        'expression': '''
          (function() {
            var elem = document.createElement('meta');
            elem.name='viewport';
            elem.content='initial-scale=1';
            document.body.appendChild(elem);
          })();'''
      }))

  def RunEndure(self, action_runner):
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'div[class~="navForward"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="navForward"]'
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'div[class~="navForward"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="navForward"]'
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'div[class~="navForward"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="navForward"]'
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'div[class~="navForward"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="navBack"]'
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'div[class~="navBack"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="navBack"]'
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'div[class~="navBack"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="navBack"]'
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'div[class~="navBack"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="navBack"]'
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'div[class~="navBack"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'div[class~="navForward"]'
      }))


class CalendarForwardBackwardPageSet(page_set_module.PageSet):

  """ Chrome Endure test for Google Calendar. """

  def __init__(self):
    super(CalendarForwardBackwardPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/calendar_forward_backward.json')

    self.AddPage(CalendarForwardBackwardPage(self))
