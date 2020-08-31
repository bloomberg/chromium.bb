## Feature Policy Guide
### How to add a new feature to feature policy

Feature policy (see [spec](https://wicg.github.io/feature-policy/)) is a
mechanism that allows developers to selectively enable and disable various
[browser features and
APIs](https://cs.chromium.org/chromium/src/third_party/blink/public/mojom/feature_policy/feature_policy.mojom)
(e.g, "vibrate", "fullscreen", "usb", etc.). A feature policy can be defined
via a HTTP header and/or an iframe "allow" attribute.

Below is an example of a header policy (note that the header should be kept in
one line, split into multiple for clarity reasons):

    Feature-Policy: vibrate 'none'; geolocation 'self' https://example.com; camera *


- `vibrate` is disabled for all browsing contexts;
- `geolocation` is disabled for all browsing contexts except for its own
  origin and those whose origin is "https://example.com";
- `camera` is enabled for all browsing contexts.

Below is an example of a container policy:

    <iframe allowpaymentrequest allow='vibrate; fullscreen'></iframe>

OR

    <iframe allowpaymentrequest allow="vibrate 'src'; fullscreen 'src'"></iframe>


- `payment` is enabled (via `allowpaymentrequest`) on all browsing contexts
 within the iframe;
- `vibrate` and `fullscreen` are enabled on the origin of the URL of the
  iframe's `src` attribute.

Combined with a header policy and a container policy, [inherited
policy](https://wicg.github.io/feature-policy/#inherited-policy) defines the
availability of a feature.
See more details for how to [define an inherited policy for
feature](https://wicg.github.io/feature-policy/#define-inherited-policy)

#### Adding a new feature to feature policy
A step-to-step guide with examples.

##### Shipping features behind a flag
If the additional feature is unshipped, or if the correct behaviour with feature
policy is undetermined, consider shipping the feature behind a runtime-enabled feature.

##### Define new feature
1. Feature policy features are defined in
`third_party/blink/renderer/core/feature_policy/feature_policy_features.json5`. Add the new feature,
placing any runtime-enabled feature or origin trial dependencies in its "depends_on" field as
described in the file's comments.

2. Append the new feature enum with a brief description as well in
`third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom`

3. In `third_party/blink/renderer/platform/feature_policy/feature_policy.cc`,
add an entry to `FeaturePolicy::GetDefaultFeatureList` with the default value
to use for the new feature.

##### Integrate the feature behaviour with feature policy
1. The most common way to check if features are enabled is `ExecutionContext::IsFeatureEnabled`.

2. Examples:
- `vibrate`: `NavigatorVibration::vibrate()`
- `payment`: `AllowedToUsePaymentRequest()`
- `usb`: `USB::getDevices()`

##### Write web-platform-tests
To test the new feature with feature policy, refer to
`third_party/blink/web_tests/external/wpt/feature-policy/README.md` for
instructions on how to use the feature policy test framework.

#### Contacts
For more questions, please feel free to reach out to:
iclelland@chromium.org
(Emerita: loonybear@)
