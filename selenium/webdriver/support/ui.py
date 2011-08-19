# Copyright 2011 WebDriver committers
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

import time
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import TimeoutException

POLL_FREQUENCY = 0.5  # How long to sleep inbetween checks to the


class WebDriverWait(object):

    def __init__(self, driver, timeout, poll_frequency=POLL_FREQUENCY):
        """Constructor, takes a WebDriver instance and timeout in seconds."""
        self._driver = driver
        self._timeout = timeout
        self._poll = poll_frequency
        # avoid the divide by zero
        if self._poll == 0:
            self._poll = POLL_FREQUENCY

    def Until(self, method):
        """Calls the method provided with the driver as an argument until the \
        return value is not Falsy."""
        for _ in xrange(max(1, int(self._timeout/self._poll))):
            try:
                value = method(self._driver)
                if value:
                    return value
            except NoSuchElementException:
                pass
            time.sleep(self._poll)
        raise TimeoutException()
