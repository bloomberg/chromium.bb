# Copyright 2008-2009 WebDriver committers
# Copyright 2008-2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import base64
import httplib
from selenium.webdriver.common.exceptions import ErrorInResponseException
from selenium.webdriver.remote.command import Command
from selenium.webdriver.remote.webdriver import WebDriver as RemoteWebDriver
from selenium.webdriver.firefox.webelement import WebElement
from selenium.webdriver.firefox.firefoxlauncher import FirefoxLauncher
from selenium.webdriver.firefox.firefox_profile import FirefoxProfile
from selenium.webdriver.firefox.extensionconnection import ExtensionConnection
import urllib2


class WebDriver(RemoteWebDriver):
    """The main interface to use for testing,
    which represents an idealised web browser."""

    def __init__(self, profile=None, timeout=30):
        """Creates a webdriver instance.

        Args:
          profile: a FirefoxProfile object (it can also be a profile name,
                   but the support for that may be removed in future, it is
                   recommended to pass in a FirefoxProfile object)
          timeout: the amount of time to wait for extension socket
        """
        self.browser = FirefoxLauncher()
        if type(profile) == str:
            # This is to be Backward compatible because we used to take a
            # profile name
            profile = FirefoxProfile(name=profile)
        if not profile:
            profile = FirefoxProfile()
        self.browser.launch_browser(profile)
        RemoteWebDriver.__init__(self,
            command_executor=ExtensionConnection(timeout),
            browser_name='firefox', platform='ANY', version='',
            javascript_enabled=True)

    def _execute(self, command, params=None):
        try:
            return RemoteWebDriver._execute(self, command, params)
        except ErrorInResponseException, e:
            # Legacy behavior: calling close() multiple times should not raise
            # an error
            if command != Command.CLOSE and command != Command.QUIT:
                raise e
        except urllib2.URLError, e:
            # Legacy behavior: calling quit() multiple times should not raise
            # an error
            if command != Command.QUIT:
                raise e
    
    def find_element_by_css_selector(self, css_selector):
        """Find and return an element by CSS selector."""
        return self._find_element_by("css selector", css_selector)
    
    def find_elements_by_css_selector(self, css_selector):
        """Find and return list of multiple elements by CSS selector."""
        return self._find_elements_by("css selector", css_selector)
    
    def set_preference(self, name, value):
        """
        Set a preference in about:config via user.js. Format is name, value.
        For example, set_preference("app.update.auto", "false")
        """
        FFProfile = FirefoxProfile()
        pref_dict = FFProfile.prefs
        pref_dict[name] = value
        FFProfile._update_user_preference(pref_dict)

    def get_preferences(self):
        """View the current list of preferences set in about:config."""
        FFProfile = FirefoxProfile()
        return FFProfile.prefs

    def create_web_element(self, element_id):
        """Override from RemoteWebDriver to use firefox.WebElement."""
        return WebElement(self, element_id)

    def quit(self):
        """Quits the driver and close every associated window."""
        try:
            RemoteWebDriver.quit(self)
        except httplib.BadStatusLine:
            # Happens if Firefox shutsdown before we've read the response from
            # the socket.
            pass
        self.browser.kill()

    def save_screenshot(self, filename):
        """
        Gets the screenshot of the current window. Returns False if there is
        any IOError, else returns True. Use full paths in your filename.
        """
        png = self._execute(Command.SCREENSHOT)['value']
        try:
            f = open(filename, 'w')
            f.write(base64.decodestring(png))
            f.close()
        except IOError:
            return False
        finally:
            del png
        return True
