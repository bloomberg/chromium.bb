# Chromium Java style guide

_For other languages, please see the [Chromium style
guides](https://chromium.googlesource.com/chromium/src/+/master/styleguide/styleguide.md)._

Chromium follows the [Android Open Source style
guide](http://source.android.com/source/code-style.html) unless an exception
is listed below.

## Style

* Copyright header should use
  [Chromium](https://chromium.googlesource.com/chromium/src/+/master/styleguide/styleguide.md)
  style.
* TODO should follow chromium convention i.e. `TODO(username)`.
* Use of ```assert``` statements are encouraged.
* Fields should not be explicitly initialized to default values (see
  [here](https://groups.google.com/a/chromium.org/d/topic/chromium-dev/ylbLOvLs0bs/discussion)).
* For automated style checking install
  [checkstyle](https://sites.google.com/a/chromium.org/dev/developers/checkstyle).

## Location

"Top level directories" are defined as directories with a GN file, such as
[//base](https://chromium.googlesource.com/chromium/src/+/master/base/)
and
[//content](https://chromium.googlesource.com/chromium/src/+/master/content/),
Chromium Java should live in a directory named
`<top level directory>/android/java`, with a package name
`org.chromium.<top level directory>`.  Each top level directory's Java should
build into a distinct JAR that honors the abstraction specified in a native
[checkdeps](https://chromium.googlesource.com/chromium/buildtools/+/master/checkdeps/checkdeps.py)
(e.g. `org.chromium.base` does not import `org.chromium.content`).  The full
path of any java file should contain the complete package name.

For example, top level directory `//base` might contain a file named
`base/android/java/org/chromium/base/Class.java`. This would get compiled into a
`chromium_base.jar` (final JAR name TBD).

`org.chromium.chrome.browser.foo.Class` would live in
`chrome/android/java/org/chromium/chrome/browser/foo/Class.java`.

New `<top level directory>/android` directories should have an `OWNERS` file
much like
[//base/android/OWNERS](https://chromium.googlesource.com/chromium/src/+/master/base/android/OWNERS).

## Asserts

The Chromium build system strips asserts in release builds (via ProGuard) and
enables them in debug builds (or when `dcheck_always_on=true`) (via a [build
step](https://codereview.chromium.org/2517203002)). You should use asserts in
the [same
scenarios](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md#CHECK_DCHECK_and-NOTREACHED)
where C++ DCHECK()s make sense. For multi-statement asserts, use
`org.chromium.base.BuildConfig.DCHECK_IS_ON` to guard your code.

Example assert:

```java
assert someCallWithSideEffects() : "assert description";
```

Example use of `DCHECK_IS_ON`:

```java
if (org.chromium.base.BuildConfig.DCHECK_IS_ON) {
   if (!someCallWithSideEffects()) {
     throw new AssertionError("assert description");
   }
}
```
