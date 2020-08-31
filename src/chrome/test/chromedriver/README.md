# ChromeDriver

This file contains high-level info about how ChromeDriver works and how to
contribute. If you are looking for information on how to use ChromeDriver,
please see the [ChromeDriver user site](https://chromedriver.chromium.org/).

ChromeDriver is an implementation of the
[WebDriver standard](https://w3c.github.io/webdriver/),
which allows users to automate testing of their website across browsers.

## Getting Started

Build ChromeDriver by building the `chromedriver` target, e.g.,

```
ninja -C out/Default chromedriver
```

This will create an executable binary in the build folder named
`chromedriver[.exe]`.

Once built, ChromeDriver can be used with various third-party libraries that
support WebDriver protocol, including language bindings provided by Selenium.

For testing purposes, ChromeDriver can be used interactively with python.
The following code uses our own testing API, not the commonly used Python
binding provided by Selenium.

```python
$ export PYTHONPATH=<THIS_DIR>/server:<THIS_DIR>/client
$ python
>>> import server
>>> import chromedriver
>>> cd_server = server.Server('/path/to/chromedriver/executable')
>>> driver = chromedriver.ChromeDriver(cd_server.GetUrl())
>>> driver.Load('http://www.google.com')
>>> driver.Quit()
>>> cd_server.Kill()
```

By default, ChromeDriver will look in its own directory for Chrome to use.
If Chrome is not found there, it will use the system installed Chrome.

To use ChromeDriver with Chrome on Android pass the Android package name in the
chromeOptions.androidPackage capability when creating the driver. The path to
adb_commands.py and the adb tool from the Android SDK must be set in PATH. For
more detailed instructions see the user site.

## Architecture

ChromeDriver is shipped separately from Chrome. It controls Chrome out of
process through [DevTools](https://chromedevtools.github.io/devtools-protocol/).
ChromeDriver is a standalone server which
communicates with the WebDriver client via the WebDriver wire protocol, which
is essentially synchronous JSON commands over HTTP. WebDriver clients are
available in many languages, and many are available from the open source
[Selenium WebDriver project](https://www.selenium.dev/).
ChromeDriver uses the webserver from
[net/server](https://source.chromium.org/chromium/chromium/src/+/master:net/server/).

Additional information is available on the following pages:
* [Threading](docs/threading.md): ChromeDriver threading model.
* [Chrome Connection](docs/chrome_connection.md):
  How ChromeDriver connects to Chrome and controls it.
* [DevTools Event Listener](docs/event_listener.md):
  How ChromeDriver responds to events from Chrome DevTools.
* [Run JavaScript Code](docs/run_javascript.md):
  How ChromeDriver sends JavaScript code to Chrome for execution.

## Code structure (relative to this file)

* [(this directory)](.):
Implements chromedriver commands.

* [chrome/](chrome/):
A basic interface for controlling Chrome. Should not depend on or reference
WebDriver-related code or concepts.

* [js/](js/):
Javascript helper scripts.

* [net/](net/):
Code to deal with network communication, such as connection to DevTools.

* [client/](client/):
Code for a python client.

* [server/](server/):
Code for the chromedriver server.
A python wrapper to the chromedriver server.

* [extension/](extension/):
An extension used for automating the desktop browser.

* [test/](test/):
Integration tests.

## Testing

There are several test suites for verifying ChromeDriver's correctness.
For details, see the [testing page](docs/testing.md).

## Contributing

Find an open issue and submit a patch for review by an individual listed in
the `OWNERS` file in this directory. Issues are tracked in
[ChromeDriver issue tracker](https://crbug.com/chromedriver).
