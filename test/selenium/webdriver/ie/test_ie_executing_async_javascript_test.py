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


from selenium import webdriver
from selenium.test.selenium.webdriver.common import executing_async_javascript_test 
from selenium.test.selenium.webdriver.common.webserver import SimpleWebServer

def setup_module(module):
    webserver = SimpleWebServer()
    webserver.start()
    IeExecutingAsyncJavaScriptTests.webserver = webserver
    IeExecutingAsyncJavaScriptTests.driver = webdriver.connect('ie')


class IeExecutingAsyncJavaScriptTests(executing_async_javascript_test.ExecutingAsyncJavaScriptTests):
    pass


def teardown_module(module):
    IeExecutingAsyncJavaScriptTests.driver.quit()
    IeExecutingAsyncJavaScriptTests.webserver.stop()

