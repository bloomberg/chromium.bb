# WebView developer documentation

**Shortlink:** http://go/webview-docs

This directory contains developer documentation for Android WebView.

*** promo
Googlers may wish to consult http://go/clank-webview for Google-specific
developer guides.
***

Please see the markdown files in this directory for detailed developer guides.

## What is Android WebView?

Android WebView is an Android system component for displaying web content. This
(and the related Android classes) are implemented by the code in the
`//android_webview/` folder.

Android WebView is a content embedder, meaning it depends on code in
`//content/` and lower layers (ex. `//net/`, `//base/`), but does not depend on
sibling layers such as `//chrome/`.

## Want to use WebView in an Android app?

Please consult https://developer.android.com/reference/android/webkit/WebView,
and the related classes in the `android.webkit` package. You may also be
interested in our
[`androidx.webkit`](https://developer.android.com/reference/androidx/webkit/WebViewCompat)
API documentation.
