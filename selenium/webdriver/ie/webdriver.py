#!/usr/bin/python
#
# Copyright 2008-2010 WebDriver committers
# Copyright 2008-2010 Google Inc.
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

from selenium.webdriver.common import utils
from selenium.webdriver.remote.webdriver import WebDriver as RemoteWebDriver
from selenium.webdriver.common.desired_capabilities import DesiredCapabilities
from selenium.webdriver.remote.command import Command
from selenium.common.exceptions import WebDriverException
import base64
from service import Service

DEFAULT_TIMEOUT = 30
DEFAULT_PORT = 0

class WebDriver(RemoteWebDriver):

    def __init__(self, executable_path='IEDriverServer.exe', 
                    port=DEFAULT_PORT, timeout=DEFAULT_TIMEOUT):
        self.port = port
        if self.port == 0:
            self.port = utils.free_port()

        self.iedriver = Service(executable_path, port=self.port)
        self.iedriver.start()

        RemoteWebDriver.__init__(
            self,
            command_executor='http://localhost:%d' % self.port,
            desired_capabilities=DesiredCapabilities.INTERNETEXPLORER)

    def quit(self):
        RemoteWebDriver.quit(self)
        self.iedriver.stop()

    def save_screenshot(self, filename):
        """
        Gets the screenshot of the current window. Returns False if there is
        any IOError, else returns True. Use full paths in your filename.
        """
        png = RemoteWebDriver.execute(self, Command.SCREENSHOT)['value']
        try:
            f = open(filename, 'wb')
            f.write(base64.decodestring(png))
            f.close()
        except IOError:
            return False
        finally:
            del png
        return True
