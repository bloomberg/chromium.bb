# Runtime Enabled Features
## Overview
Runtime flags enable Blink developers the ability to control access Chromium users have to new features they implement. Features that are hidden behind a runtime flag are known as Runtime Enabled Features. It is a requirement of the Blink Launch Process to implement new web exposed features behind a runtime flag until an Intent To Ship has been approved.

## Adding A Runtime Enabled Feature
Runtime Enabled Features are defined in runtime_enabled_features.json5 in alphabetical order. Add your feature's flag to this file and the rest will be generated for you automatically.

Example:
```js
{
  name: "AmazingNewFeature",
  status: "experimental",
}
```
The status of the feature controls when it will be enabled in the Blink engine.

| Status Value | Web feature enabled during web tests with content_shell [1] | Web feature enabled as part of web experimental features [2] | Web feature enabled in stable release | Non-web exposed feature enabled through a command line flag [3]
|:---:|:---:|:---:|:---:|:---:|
| <missing\> | No | No | No | Yes |
| `test` | Yes | No | No | No |
| `experimental` | Yes | Yes | No | No |
| `stable` | Yes | Yes | Yes | No |

\[1]: content_shell will not enable experimental/test features by default, the `--run-web-tests` flag used as part of running web tests enables this behaviour.

\[2]: Navigate to about:flags in the URL bar and turn on "Enable experimental web platform features" (formerly, "Enable experimental WebKit features") **or** run Chromium with `--enable-experimental-web-platform-features` (formerly, --enable-experimental-webkit-features).
Works in all Chromium channels: canary, dev, beta, and stable.

\[3]: For features that are not web exposed features but require code in Blink to be triggered. Such feature can have a about:flags entry or be toggled based on other signals. Such entries should be called out in a comment to differentiate them from stalled entries.

### Platform-specific Feature Status
For features that do not have the same status on every platform, you can specify their status using a dictionary value.

For example in the declaration below:
```js
{
  name: "NewFeature",
  status: {
    "Android": "test",
    "ChromeOS": "experimental",
    "Win": "stable",
    "default": "",
  }
}
```
the feature has the status `test` on Android, `experimental` on Chrome OS and `stable` on Windows and no status on the other platforms.
The status of all the not-specified platforms is set using the `default` key. For example, the declaration:
```js
status: {
  "Android": "stable",
  "default", "experimental",
}
```
will set the feature status to  `experimental` on all platforms except on Android (which will be set to `stable`).

**Note:** Omitting `default` from the status dictionary will be treated the same as writing `"default": ""`.

**Note:** You can find the list of all supported platforms in [runtime_enabled_features.json5 status declaration][supportedPlatforms].

### Guidelines for Setting Feature Status
Any in-development feature can be added with no status, the only requirement is that code OWNERS are willing to have the code landed in the tree (as for any commit).

* For a feature to be marked `status=test`, it must be in a sufficient state to permit internal testing.  For example, enabling it should not be known to easily cause crashes, leak memory, or otherwise significantly effect the reliability of bots.  Consideration should also be given to the potential for loss of test coverage of shipping behavior.  For example, if a feature causes a new code path to be taken instead of an existing one, it is possible that some valuable test coverage and regression protection could be lost by setting a feature to `status=test`.  Consider using a [virtual test suite] when it's important to keep testing both old and new code paths.

* For a feature to be marked `status=experimental`, it should be far enough along to permit testing by early adopter web developers.  Many chromium enthusiasts run with `--enable-experimental-web-platform-features`, and so promoting a feature to experimental status can be a good way to get early warning of any stability or compatibility problems.  If such problems are discovered (e.g. major websites being seriously broken when the feature is enabled), the feature should be demoted back to `status=test` to avoid creating undue problems for such users.  It's notoriously difficult to diagnose a bug report from a user who neglects to mention that they have this flag enabled.  Often a feature will be set to experimental status long before it's implementation is complete, and while there is still substantial churn on the API design.  Features in this state are not expected to work completely, just do something of value which developers may want to provide feedback on.

* For a feature to be marked `status=stable`, it must be complete and ready for use by all chrome users. Often this means it has gotten approval via the [blink launch process]. However, for features which are not web observable (e.g. a flag to track a large-scale code refactoring), this approval is not needed. In rare cases a feature may be tested on canary and dev channels by temporarily setting it to `status=stable`, with a comment pointing to a bug marked `Release-Block-Beta` tracking setting the feature back to `status=experimental` before the branch for beta.

When a feature has shipped and is no longer at risk of needing to be disabled, it's associated RuntimeEnableFeatures entry should be removed entirely.  Permanent features should generally not have flags.

If a feature is not stable and no longer under active development, remove `status=test/experimental` on it (and consider deleting the code implementing the feature).

### Runtime Enabled CSS Properties

If your feature is adding new CSS Properties you will need to use the runtime_flag argument in [renderer/core/css/css_properties.json5][cssProperties].

## Using A Runtime Enabled Feature

### C++ Source Code
Add this include:
```cpp
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
```
This will provide following static methods to check/set whether your feature is enabled:
```cpp
bool RuntimeEnabledFeatures::AmazingNewFeatureEnabled();
void RuntimeEnabledFeatures::SetAmazingNewFeatureEnabled(bool enabled);
```
**Note:** MethodNames and  FeatureNames are in UpperCamelCase. This is handled automatically in code generators, and works even if the feature's flag name begins with an acronym such as "CSS", "IME", or "HTML".
For example "CSSMagicFeature" becomes `RuntimeEnabledFeatures::CSSMagicFeatureEnabled()` and `RuntimeEnabledFeatures::SetCSSMagicFeatureEnabled(bool)`.


### IDL Files
Use the [Blink extended attribute] `[RuntimeEnabled]` as in `[RuntimeEnabled=AmazingNewFeature]` in your IDL definition.

**Note:** FeatureNames are in UpperCamelCase; please use this case in IDL files.

You can guard the entire interface, as in this example:
```
[
    RuntimeEnabled=AmazingNewFeature  // Guard the entire interface.
] interface AmazingNewObject {
    attribute DOMString amazingNewAttribute;
    void amazingNewMethod();
};
```
Alternatively, you can guard individual definition members:
```
interface ExistingObject {
    attribute DOMString existingAttribute;
    // Guarded attribute.
    [RuntimeEnabled=AmazingNewFeature] attribute DOMString amazingNewAttribute;
    // Guarded method.
    [RuntimeEnabled=AmazingNewFeature] void amazingNewMethod();
};
```
**Note:** You *cannot* guard individual arguments, as this is very confusing and error-prone. Instead, use overloading and guard the overloads.

For example, instead of:
```
interface ExistingObject {
    foo(long x, [RuntimeEnabled=FeatureName] optional long y); // Don't do this!
};
```
do:
```
interface ExistingObject {
    // Overload can be replaced with optional if [RuntimeEnabled] is removed
    foo(long x);
    [RuntimeEnabled=FeatureName] foo(long x, long y);
};
```

**Warning:** You will not be able to change the enabled state of these at runtime as the V8 object templates definitions are created during start up and will not be updated during runtime.

## Web Tests (JavaScript)
Test whether a feature is enabled using:
```javascript
internals.runtimeFlags.amazingNewFeatureEnabled
```
This attribute is read only and cannot be changed.

**Note:** The `internals` JavaScript API is only available in ContentShell for use by web tests and does not appear in released versions of Chromium.

### Running Web Tests
When content_shell is run with `--stable-release-mode` flag, test-only features (ones listed in [runtime_enabled_features.json5] with `test`) are turned off.

## Generated Files
[renderer/build/scripts/make_runtime_features.py][make_runtime_features.py] uses [runtime_enabled_features.json5] to generate:
```
<compilation directory>/gen/third_party/blink/renderer/platform/runtime_enabled_features.h
<compilation directory>/gen/third_party/blink/renderer/platform/runtime_enabled_features.cc
```
[renderer/build/scripts/make_internal_runtime_flags.py][make_internal_runtime_flags.py] uses [runtime_enabled_features.json5] to generate:
```
<compilation directory>/gen/third_party/blink/renderer/core/testing/internal_runtime_flags.idl
<compilation directory>/gen/thrid_party/blink/renderer/core/testing/internal_runtime_flags.h
```
[renderer/bindings/scripts/code_generator_v8.py][code_generator_v8.py] uses the generated `internal_runtime_flags.idl` to generate:
```
<compilation directory>/gen/third_party/blink/renderer/bindings/core/v8/v8_internal_runtime_flags.h
<compilation directory>/gen/third_party/blink/renderer/bindings/core/v8/v8_internal_runtime_flags.cc
```
## Command-line Switches
`content` provides two switches which can be used to turn runtime enabled features on or off, intended for use during development. They are exposed by both `content_shell` and `chrome`.
```
--enable-blink-features=SomeNewFeature,SomeOtherNewFeature
--disable-blink-features=SomeOldFeature
```
After applying most other feature settings, the features requested feature settings (comma-separated) are changed. "disable" is applied later (and takes precedence), regardless of the order the switches appear on the command line. These switches only affect Blink's state. Some features may need to be switched on in Chromium as well; in this case, a specific flag is required.

**Announcement**
https://groups.google.com/a/chromium.org/d/msg/blink-dev/JBakhu5J6Qs/re2LkfEslTAJ



[supportedPlatforms]: <https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/platform/runtime_enabled_features.json5#36>
[cssProperties]: <https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/css/css_properties.json5>
[virtual test suite]: <https://chromium.googlesource.com/chromium/src/+/master/docs/testing/web_tests.md#testing-runtime-flags>
[blink launch process]: <https://www.chromium.org/blink/launching-features>
[Blink extended attribute]: <https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/bindings/IDLExtendedAttributes.md>
[make_runtime_features.py]: <https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/build/scripts/make_runtime_features.py>
[runtime_enabled_features.json5]: <https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/platform/runtime_enabled_features.json5>
[make_internal_runtime_flags.py]: <https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/build/scripts/make_internal_runtime_flags.py>
[code_generator_v8.py]: <https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/bindings/scripts/code_generator_v8.py>
