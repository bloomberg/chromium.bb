# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import abc
import subprocess
import jinja2
import tempfile
import datetime
import logging
import typing
import os

import utils
import browsers


def GetTemplateFileForBrowser(browser_driver: browsers.BrowserDriver,
                              template_file: str) -> str:
  if browser_driver.name == "safari":
    return f"safari_{template_file}"
  else:
    return template_file


class ScenarioOSADriver(abc.ABC):
  """Base Class encapsulating OSA script driving a scenario, with setup and tear
     down.
  """

  def __init__(self, scenario_name, duration: datetime.timedelta):
    self.name = scenario_name
    self.script_process = None
    self.osa_script = None
    self.duration = duration

  def Launch(self):
    """Starts the driver script.
    """
    assert self.osa_script is not None
    logging.debug(f"Starting scenario {self.name}")
    self.script_process = subprocess.Popen(['osascript', self.osa_script.name])

  def Wait(self):
    """Waits for the script to complete.
    """
    assert self.script_process is not None, "Driver wasn't launched."
    logging.debug(f"Waiting for scenario {self.name}")
    self.script_process.wait()

  def TearDown(self):
    """Terminates the script if currently running and ensures related processes
       are cleaned up.
    """
    logging.debug(f"Tearing down scenario {self.name}")
    if self.script_process:
      utils.TerminateProcess(self.script_process)
    self.osa_script.close()

  @abc.abstractmethod
  def Summary(self):
    """Returns a dictionary describing the scenarios parameters.
    """
    pass

  def IsRunning(self) -> bool:
    """Returns true if the script is currently running.
    """
    return self.script_process.poll() is None

  def _CompileTemplate(self, template_file: str, extra_args: typing.Dict):
    """Compiles script `template_file`, feeding `extra_args` into a temporary
       file.
    """
    loader = jinja2.FileSystemLoader(
        os.path.join(os.path.dirname(__file__), "driver_scripts_templates"))
    env = jinja2.Environment(loader=loader)
    template = env.get_template(template_file)
    self.osa_script = tempfile.NamedTemporaryFile('w+t')
    self.osa_script.write(template.render(**extra_args))
    self.osa_script.flush()
    self._args = extra_args

  def Summary(self):
    """Returns a dictionary describing the scenarios parameters.
    """
    return {'name': self.name, **self._args}


class ScenarioWithBrowserOSADriver(ScenarioOSADriver):
  """Specialisation for OSA script that runs with a browser.
  """

  def __init__(self, scenario_name, browser_driver: browsers.BrowserDriver,
               duration: datetime.timedelta):
    super().__init__(f"{browser_driver.name}_{scenario_name}", duration)
    self.browser = browser_driver

  def Launch(self):
    self.browser.Launch()
    super().Launch()

  def TearDown(self):
    super().TearDown()
    self.browser.TearDown()

  def Summary(self):
    """Returns a dictionary describing the scenarios parameters.
    """
    return {**super().Summary(), 'browser': self.browser.Summary()}

  def _CompileTemplate(self, template_file, extra_args: typing.Dict):
    return super()._CompileTemplate(template_file, {
        "browser": self.browser.process_name,
        **extra_args
    })


class IdleScenario(ScenarioOSADriver):
  """Scenario that lets the system idle.
  """

  def __init__(self, duration: datetime.timedelta, scenario_name="idle"):
    super().__init__(scenario_name, duration)
    self._CompileTemplate("idle", {
        "delay": duration.total_seconds(),
    })


class IdleOnSiteScenario(ScenarioWithBrowserOSADriver):
  """Scenario that lets a browser idle on a web page.
  """

  def __init__(self, browser_driver: browsers.BrowserDriver,
               duration: datetime.timedelta, site_url: str, scenario_name):
    super().__init__(scenario_name, browser_driver, duration)
    self._CompileTemplate(
        GetTemplateFileForBrowser(browser_driver, "idle_on_site"), {
            "idle_site": site_url,
            "delay": duration.total_seconds(),
        })

  @staticmethod
  def Wiki(browser_driver: browsers.BrowserDriver,
           duration: datetime.timedelta):
    return IdleOnSiteScenario(browser_driver, duration,
                              "http://www.wikipedia.com/wiki/Alessandro_Volta",
                              "idle_on_wiki")

  @staticmethod
  def Youtube(browser_driver: browsers.BrowserDriver,
              duration: datetime.timedelta):
    return IdleOnSiteScenario(
        browser_driver, duration,
        "https://www.youtube.com/watch?v=9EE_ICC_wFw?autoplay=1",
        "idle_on_youtube")


class ZeroWindowScenario(ScenarioWithBrowserOSADriver):
  """Scenario that lets a browser idle with no window.
  """

  def __init__(self,
               browser_driver: browsers.BrowserDriver,
               duration: datetime.timedelta,
               scenario_name="zero_window"):
    super().__init__(scenario_name, browser_driver, duration)
    self._CompileTemplate(
        GetTemplateFileForBrowser(browser_driver, "zero_window"), {
            "delay": duration.total_seconds(),
        })


class NavigationScenario(ScenarioWithBrowserOSADriver):
  """Scenario that has a browser navigating on web pages in a loop.
  """
  NAVIGATED_SITES = [
      "https://amazon.com",
      "https://www.amazon.com/s?k=computer&ref=nb_sb_noss_2",
      "https://google.com", "https://www.google.com/search?q=computers",
      "https://www.youtube.com",
      "https://www.youtube.com/results?search_query=computers",
      "https://docs.google.com/document/d/1Ll-8Nvo6JlhzKEttst8GHWCc7_A8Hluy2fX99cy4Sfg/edit?usp=sharing"
  ]

  def __init__(self,
               browser_driver: browsers.BrowserDriver,
               navigation_duration: datetime.timedelta,
               navigation_cycles: int,
               sites=NAVIGATED_SITES,
               scenario_name="navigation"):
    super().__init__(scenario_name, browser_driver,
                     navigation_duration * navigation_cycles * len(sites))
    self._CompileTemplate(
        GetTemplateFileForBrowser(browser_driver, "navigation"), {
            "per_navigation_delay": navigation_duration.total_seconds(),
            "navigation_cycles": navigation_cycles,
            "sites": ",".join([f'"{site}"' for site in sites])
        })


class MeetScenario(ScenarioWithBrowserOSADriver):
  """Scenario that has the browser join a Google Meet room.
  """

  def __init__(self,
               browser_driver: browsers.BrowserDriver,
               duration: datetime.timedelta,
               meeting_id: int,
               scenario_name="meet"):
    super().__init__(scenario_name, browser_driver, duration)
    self._CompileTemplate(GetTemplateFileForBrowser(browser_driver, "meet"), {
        "delay": duration.total_seconds(),
        "meeting_id": meeting_id
    })


def MakeScenarioDriver(scenario_name,
                       browser_driver: browsers.BrowserDriver,
                       meet_meeting_id=None) -> ScenarioOSADriver:
  """Creates scenario driver by name.

  Args:
    scenario_name: Identifier for the scenario to create. Supported scenarios
      are: meet, idle_on_wiki, idle_on_youtube, navigation, zero_window and
      idle.
    browser_driver: Browser the scenario is created with.
    meet_meeting_id: Optional meeting id used for meet scenario.
  """

  if "idle" == scenario_name:
    return IdleScenario(datetime.timedelta(minutes=60))
  if not browser_driver:
    return None
  if "meet" == scenario_name:
    return MeetScenario(browser_driver,
                        datetime.timedelta(minutes=60),
                        meeting_id=meet_meeting_id)
  if "idle_on_wiki" == scenario_name:
    return IdleOnSiteScenario.Wiki(browser_driver,
                                   datetime.timedelta(minutes=60))
  if "idle_on_youtube" == scenario_name:
    return IdleOnSiteScenario.Youtube(browser_driver,
                                      datetime.timedelta(minutes=60))
  if "navigation" == scenario_name:
    return NavigationScenario(
        browser_driver,
        navigation_duration=datetime.timedelta(seconds=15),
        navigation_cycles=70)
  if "zero_window" == scenario_name:
    return ZeroWindowScenario(browser_driver, datetime.timedelta(minutes=60))
  return None
