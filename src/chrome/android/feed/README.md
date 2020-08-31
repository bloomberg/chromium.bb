# Feed Host UI

The feed Java package provides support for showing a list of article
suggestions rendered by the third party
[Feed](https://chromium.googlesource.com/feed) library in Chrome UI.

This directory contains two mirrored packages that provide real and dummy
implementations of classes to facilitate compiling out dependencies on
[//third_party/feed_library/](../../../third_party/feed_library/) and
[//components/feed/](../../../components/feed/) when the `enable_feed_in_chrome`
build flag is disabled. The public classes and methods in real/dummy used by
[//chrome/android/java/](../java/) must have identical signatures.