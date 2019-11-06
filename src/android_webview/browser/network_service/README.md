# Android WebView Network Service

This folder contains Android WebView's code for interacting with the Network
Service (including code shared with the legacy code path, while that path is
still supported). For details on the Network Service in general, see
[`//services/network/`](/services/network/README.md).

## In-process

Android WebView aims to run with the Network Service in-process
(`features::kNetworkServiceInProcess`). For details, see
https://crbug.com/882650.

## Testing with the Network Service

Please see [general testing
instructions](/android_webview/docs/test-instructions.md). Until the feature is
on-by-default, you can enable the Network Service in tests like so:

```shell
$ autoninja -C out/Default webview_instrumentation_test_apk
$ out/Default/bin/run_webview_instrumentation_test_apk \
    --enable-features="NetworkService,NetworkServiceInProcess" \
    -f <some test filter>  # See general instructions for test filtering
```
