# System WebView Shell

WebView team maintains a "shell"--a thin interface over the WebView APIs--to
exercise WebView functionality. The System WebView Shell (AKA "shell browser,"
"WebView shell") is a standalone app implemented [in
chromium](/android_webview/tools/system_webview_shell/). While often used for
manual testing, we also use the shell for automated tests (see our [layout and
page cycler tests](./test-instructions.md#layout-tests-and-page-cycler-tests)).

*** note
This relies on the WebView installed on the system. So if you're trying to
verify local changes to WebView, or run against a specific WebView build, you
must **install WebView first.**
***

*** promo
**Tip:** the shell displays the WebView version (the corresponding [chromium version
number](https://www.chromium.org/developers/version-numbers)) in the title bar
at the top. This can be helpful for checking which WebView version is installed
& selected on the device.
***

## Working with the shell

```sh
# Build and install
$ autoninja -C out/Default system_webview_shell_apk
$ out/Default/bin/system_webview_shell_apk install

# Launch a URL from the commandline, or open the app from the app launcher
$ out/Default/bin/system_webview_shell_apk launch "https://www.google.com/"

# For more commands:
$ out/Default/bin/system_webview_shell_apk --help
```

*** note
**Note:** `system_webview_shell_apk` does not support modifying CLI flags. See
https://crbug.com/959425. Instead, you should modify WebView's flags by
following [commandline-flags.md](./commandline-flags.md).
***
