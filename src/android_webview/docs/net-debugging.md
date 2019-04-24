# Net debugging in WebView

## Net log

WebView supports the `kLogNetLog` flag to log debugging network info to a JSON
file on disk.

*** aside
For more info on commandline flags, see
[commandline-flags.md](./commandline-flags.md).
***

*** note
**Note:** this requires either a `userdebug` or `eng` Android build (you can
check with `adb shell getprop ro.build.type`). Flags cannot be enabled on
production builds of Android.
***

1. Pick a name for the JSON file (any name will work, e.g., `jsonFile=foo.json`)
1. Kill the app, if running
1. Set the netlog flag:
   ```sh
   build/android/adb_system_webview_command_line --log-net-log=${jsonFile}
   ```
1. Restart the app. Reproduce whatever is of interest, and then kill the app
   when finished
1. Get the netlog off the device:
   ```sh
   # appPackageName is the package name of whatever app you're interested (ex.
   # WebView shell is "org.chromium.webview_shell").
   appDataDir="$(adb shell pm dump ${appPackageName} | grep 'dataDir=' | sed 's/^ *dataDir=//')" && \
   adb pull "${appDataDir}/app_webview/${jsonFile}"
   ```
1. Import the JSON file into [the NetLog
   viewer](https://chromium.googlesource.com/catapult/+/master/netlog_viewer/)

For more details, see the implementation in
[AwUrlRequestContextGetter](/android_webview/browser/net/aw_url_request_context_getter.cc).
For support in the network service code path, see http://crbug.com/902039.
