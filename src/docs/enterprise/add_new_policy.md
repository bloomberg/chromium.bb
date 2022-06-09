# Policy Settings in Chrome

## Summary

Chrome exposes a different set of configurations to administrators. These
configurations are called policy and they give administrators more advanced
controls than the normal users. With different device management tools,
an administrator can deliver these polices to many users. Here is the
[help center article](https://support.google.com/chrome/a/answer/9037717?hl=en)
that talks about Chrome policy and its deployment.

## Do I need a policy

Usually you need a policy when

-   Launching a new feature. Create a policy so that the admin can disable or
    enable the feature for all users.

-   Deprecating an old feature. Create a policy to give enterprise users more
    time to migrate away from the feature.

## Adding a new policy

1.  Think carefully about the name and the desired semantics of the new policy:
    -   Choose a name that is consistent with the existing naming scheme. Prefer
        "XXXEnabled" over "EnableXXX" because the former is more glanceable and
        sorts better.
    -   Consider the foreseeable future and try to avoid conflicts with possible
        future extensions or use cases.
    -   Negative policies (*Disable*, *Disallow*) are verboten because setting
        something to "true" to disable it confuses people.
2.  Declare the policy in the
    [policy_templates.json](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json)
    -   This file contains meta-level descriptions of all policies and is used
        to generate code, policy templates (ADM/ADMX for Windows and the
        application manifest for Mac), as well as documentation. Please make
        sure you get the version and feature flags (such as dynamic_refresh and
        supported_on) right.
    -   Here are the most used attributes. Please note that, all attributes
        below other than `supported_on`, `future_on` and
        `default_for_enterprise_users` do not change the code behavior.
        -   `supported_on` and `future_on`: They control the platforms that the
            policy supports. `supported_on` is used for released platforms with
            milestone range while `future_on` is used for unreleased platforms.
            See **Launch a policy** below for more information.
        -   `default_for_enterprise_users`: Its value is applied as a mandatory
            policy for managed users on Chrome OS unless a different setting is
            explicitly set.
        -   `dynamic_refresh`: It tells the admin whether the policy value can
            be changed and take effect without re-launching Chrome.
        -   `per_profile`: It tells the admin whether different policy values
            can be assigned to different profiles.
        -   `can_be_recommended`: It tells the admin whether they can mark the
            policy as recommended and allow the user to override it in the UI,
            using a command line switch or an extension.
    - The complete list of attributes and their expected values can be found in
      the
      [policy_templates.json](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json).
    -   The textual policy description should include the following:
        -   What features of Chrome are affected.
        -   Which behavior and/or UI/UX changes the policy triggers.
        -   How the policy behaves if it's left unset or set to invalid/default
            values. This may seem obvious to you, and it probably is. However,
            this information seems to be provided for Windows Group Policy
            traditionally, and we've seen requests from organizations to
            explicitly spell out the behavior for all possible values and for
            when the policy is unset.
    -   See [description_guidelines.md](description_guidelines.md)
        for additional guidelines when creating a description, including how
        various products should be referenced.
3.  Create a preference and map the policy value to it.
    -   All policy values need to be mapped into a prefs value before being used
        unless the policy is needed before PrefService initialization.
    -   To map the policy:
        1.  Create a prefs and register the prefs in **Local State** or
            **Profile Prefs**. Please note that, this must match the
            `per_profile` attribute in the `policy_templates.json`. We also
            strongly encourage developers to register the prefs with **Profile
            Prefs** if possible, because this gives admin more flexibility of
            policy setup.
        2.  Most policies can be mapped to prefs with `kSimplePolicyMap` in
            [configuration_policy_handler_list_factory.cc](https://cs.chromium.org/chromium/src/chrome/browser/policy/configuration_policy_handler_list_factory.cc?type=cs&q=kSimplePolicyMap&g=0&l=150).
            If the policy needs additional verification or processing, please
            implement a `ConfigurationPolicyHandler` to do so.
        3.  Test the mapping by adding policy to
            [policy_test_cases.json](https://cs.chromium.org/chromium/src/chrome/test/data/policy/policy_test_cases.json?q=policy_test_case) (see [instructions](https://cs.chromium.org/chromium/src/docs/enterprise/policy_pref_mapping_test.md)).
        4.  iOS platform has its own
            [configuration_policy_handler_list_factory.mm](https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/browser/policy/configuration_policy_handler_list_factory.mm)
            and
            [policy_test_cases.json](https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/test/data/policy/policy_test_cases.json)
            file.
4.  Disable the user setting UI when the policy is applied.
    -   If your feature can be controlled by GUI in `chrome://settings`, the
        associated option should be disabled when the policy controlling it is
        managed.
        -   `PrefService:Preference::IsManaged` reveals whether a prefs value
            comes from policy or not.
        -   The setting needs an
            [indicator](https://cs.chromium.org/chromium/src/ui/webui/resources/images/business.svg)
            to tell users that the setting is enforced by the administrator.
5.  Support `dynamic_refresh` if possible.
    -   We strongly encourage developers to make their policies support this
        attribute. It means the admin can change the policy value and Chrome
        will honor the change at run-time without requiring a restart of the
        browser. Chrome OS does not always support non-dynamic profile policies.
        Please verify with a Chrome OS policy owner if your profile policy does
        not support dynamic refresh on Chrome OS.
    -   Most of the time, this requires a
        [PrefChangeRegistrar](https://cs.chromium.org/chromium/src/components/prefs/pref_change_registrar.h)
        to listen to the preference change notification and update UI or
        browser behavior right away.
6.  Adding a device policy for Chrome OS.
    -   Most policies that are used by the browser can be shared between desktop
        and Chrome OS. However, you need a few additional steps for a device
        policy on Chrome OS.
        -   Add a message for your policy in
            `components/policy/proto/chrome_device_policy.proto`. Please note
            that all proto fields are optional.
        -   Update
            `chrome/browser/ash/policy/core/device_policy_decoder.{h,cc}`
            for the new policy.
7.  Test the policy.
    -   Add a test to verify the policy. You can add a test in
        `chrome/browser/policy/<area>_policy_browsertest.cc` or with the policy
        implementation. For example, a network policy test can be put into
        `chrome/browser/net`. Ideally, your test would set the policy, fire up
        the browser, and interact with the browser just as a user would do to
        check whether the policy takes effect.
8.  Manually testing your policy.
    -   Windows: The simplest way to test is to write the registry keys manually
        to `Software\Policies\Chromium` (for Chromium builds) or
        `Software\Policies\Google\Chrome` (for Google Chrome branded builds). If
        you want to test policy refresh, you need to use group policy tools and
        gpupdate; see
        [Windows Quick Start](https://www.chromium.org/administrators/windows-quick-start).
    -   Mac: See
        [Mac Quick Start](https://www.chromium.org/administrators/mac-quick-start)
        (section "Debugging")
    -   Linux: See
        [Linux Quick Start](https://www.chromium.org/administrators/linux-quick-start)
        (section "Set Up Policies")
    -   Chrome OS and Android are more complex to test, as a full end-to-end
        test requires network transactions to the policy test server.
        Instructions on how to set up the policy test server and have the
        browser talk to it are here:
        [Running the cloud policy test server](https://www.chromium.org/developers/how-tos/enterprise/running-the-cloud-policy-test-server).
        If you'd just like to do a quick test for Chrome OS, the Linux code is
        also functional on CrOS, see
        [Linux Quick Start](https://www.chromium.org/administrators/linux-quick-start).
9.  If you are adding a new policy that supersedes an older one, verify that the
    new policy works as expected even if the old policy is set (allowing us to
    set both during the transition time when Chrome versions honoring the old
    and the new policies coexist).
10. If your policy has interactions with other policies, make sure to document,
    test and cover these by automated tests.

## Launch a policy
1.  When adding a new policy, put the platforms it will be supported in the
    `future_on` list.
    - The policy is hidden from any auto-generated template or documentation on
      those platforms.
    - The policy will also be unavailable on Beta and Stable channel unless it's
      enabled specifically by
      [EnableExperimentalPolicies](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=EnableExperimentalPolicies)
      policy.
2.  Implement the policy, get launch approval if necessary.
3.  If the policy needs to be tested with small set of users first, keep the
    platforms in the `future_on` list and running the tester program with the
    [EnableExperimentalPolicies](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=EnableExperimentalPolicies)
    policy.
4.  Move the launched platforms from `future_on` to `supported_on` and set the
    'since_version' of those platforms to the milestone for which the launch
    approval was granted.
5.  If the 'since_version' is set to a earlier milestone, you need to merge
    back all necessary commits.

**Do not use finch to control policy launch process.**

Policies are inherently switches that admins will turn on if they need. Getting
inconsistent behavior based on factors outside of their control only causes
confusion and is source for support requests. Use the step 3 above if the policy
needs external testers before being officially announced.

## Deprecating a policy

A policy is deprecated when admins should stop using it. This is often because
a new policy was been released that should be used instead.

When marking a policy as deprecated, it still needs to work the same as before
in Chrome. If you wish to remove the functionality, you'll need to changed the
supported_on field. See "Removing support for a policy" below for more details.

### Steps
1. Update policy_templates.json, marking the policy as deprecated and updating
   the description to describe when and why the policy as deprecated and what
   admins should be doing instead.
1. Update the policy handling code. This is generally ensuring that if the old
   policy is set, the values are propagated to the new policies if they were
   unset.
1. Notify chromium-enterprise@chromium.org to ensure this deprecation is
   mentioned in the enterprise release notes.

## Removing support for a policy

Generally speaking, policies shouldn't be removed from Chrome. Although they can
be deprecated fairly easily, removing support for a policy is a much bigger
issue, because admins might be relying on that functionality.

The two main reasons for removing support for a policy are:
1.  It was a policy that was always documented as having a limited lifespan,
    such as an escape hatch policy.
1.  The feature this policy impacts was removed from Chrome.

If the policy was never launched, it can also be deleted from
policy_templates.json instead of just being marked as no longer supported.
In this case, please remember to add the deleted id to deleted_policy_ids.

If you want to remove support for another reason, please reach out to someone in
[ENTERPRISE_POLICY_OWNERS](https://cs.chromium.org/chromium/src/components/policy/resources/ENTERPRISE_POLICY_OWNERS)
to ensure this is okay. The general preference is to leave policies as
deprecated, but still supported.

When removing support for a policy, update `supported_on` to correctly list the
last milestone the policy is supported on. Please also set 'deprecated' to True
if the policy skipped past the deprecation state.

### Steps
1. Update policy_templates.json, marking the policy as no longer supported.
   Also marking as deprecated if not previously done.
1. Update the related test in the `policy_test_cases.json`.
    - If the policy is going to be removed in the current milestone, remove the
      the test at the same time.
    - If the policy is going to be removed in the future milestone, remove the
      test **after** policy support ended.
1. Remove the policy handling code.
1. Notify chromium-enterprise@chromium.org to ensure this removal of support is
   mentioned in the enterprise release notes.

## Examples

Here is an example based on the instructions above. It's a good, simple place to
get started:
[https://chromium-review.googlesource.com/c/chromium/src/+/1742209](https://chromium-review.googlesource.com/c/chromium/src/+/1742209)


## Modifying existing policies

If you are planning to modify an existing policy, please send out a one-pager to
client- and server-side stakeholders explaining the planned change.

There are a few noteworthy pitfalls that you should be aware of when updating
the code that handles existing policy settings, in particular:

- Make sure the policy metadata is up-to-date, in particular `supported_on`, and
the feature flags.
- In general, don't change policy semantics in a way that is incompatible
(as determined by user/admin-visible behavior) with previous semantics. **In
particular, consider that existing policy deployments may affect both old and
new browser versions, and both should behave according to the admin's
intentions**.
- **An important pitfall is that adding an additional allowed
value to an enum policy may cause compatibility issues.** Specifically, an
administrator may use the new policy value, which makes older Chrome versions
that may still be deployed (and don't understand the new value) fall back to
the default behavior. Carefully consider if this is OK in your case. Usually,
it is preferred to create a new policy with the additional value and deprecate
the old one.
- Don't rely on the cloud policy server for policy migrations because
this has been proven to be error prone. To the extent possible, all
compatibility and migration code should be contained in the client.
- It is OK to expand semantics of policy values as long as the previous policy
description is compatible with the new behavior (see the "extending enum"
pitfall above however).
- It is OK to update feature implementations and the policy
description when Chrome changes as long as the intended effect of the policy
remains intact.
- The process for removing policies is to deprecate them first,
wait a few releases (if possible) and then drop support for them. Make sure you
put the deprecated flag if you deprecate a policy.

### Presubmit Checks when Modifying Existing Policies

To enforce the above rules concerning policy modification and ensure no
backwards incompatible changes are introduced, presubmit checks
will be performed on every change to
[policy_templates.json](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json).

The presubmit checks perform the following verifications:

1.  It verifies whether a policy is considered **unreleased** before allowing a
    change. A policy is considered unreleased if **any** of the following
    conditions are true:

    1.  It is an unchanged policy marked as `future: true`.
    2.  All the `supported_versions` of the policy satisfy **any** of the
        following conditions
        1.  The unchanged supported major version is >= the current major
            version stored in the VERSION file at tip of tree. This covers the
            case of a policy that has just recently been added, but has not yet
            been released to a stable branch.
        2.  The changed supported version == unchanged supported version + 1 and
            the changed supported version is equal to the version stored in the
            VERSION file at tip of tree. This check covers the case of
            "unreleasing" a policy after a new stable branch has been cut, but
            before a new stable release has rolled out. Normally such a change
            should eventually be merged into the stable branch before the
            release.
    3. `supported_on` list is empty.

2.  If the policy is considered **unreleased**, all changes to it are allowed.

3.  However if the policy is released then the following verifications
    are performed on the delta between the original policy and the changed
    policy.

    1.  Released policies cannot be removed.
    2.  Released policies cannot have their type changed (e.g. from bool to
        Enum).
    3.  Released policies cannot have the `future: true` flag added to it. This
        flag can only be set on a new policy.
    4.  Released policies can only add additional `supported_on` versions. They
        cannot remove or modify existing values for this field except for the
        special case above for determining if a policy is released. Policy
        support end version (adding "-xx" ) can however be added to the
        supported_on version to specify that a policy will no longer be
        supported going forward (as long as the initial `supported_on` version
        is not changed).
    5.  Released policies cannot be renamed (this is the equivalent of a
        delete + add).
    6.  Released policies cannot change their `device_only` flag. This flag can
        only be set on a new policy.
    7.  Released policies with non dict types cannot have their schema changed.
        1.  For enum types this means values cannot be renamed or removed (these
            should be marked as deprecated instead).
        2.  For int types, we will allow making the minimum and maximum values
            less restrictive than the existing values.
        3.  For string types, we will allow the removal of the "pattern"
            property to allow the validation to be less restrictive.
        4.  We will allow addition to any list type values only at the end of
            the list of values and not in the middle or at the beginning (this
            restriction will cover the list of valid enum values as well).
        5.  These same restrictions will apply recursively to all property
            schema definitions listed in a dictionary type policy.
    8.  Released dict policies cannot remove or modify any existing keys in
        their schema. They can only add new keys to the schema.
        1.  Dictionary policies can have some of their "required" fields removed
            in order to be less restrictive.

4.  A policy is **partially released**  if both `supported_on` and
    `future_on` list are not empty.

5.  The **partially released** policy is considered as a **released** policy
    and only the `future_on` list can be modified freely. However, any
    platform in the `supported_on` list cannot be moved back to the `future_on`
    list.


## Cloud Policy

**For Googlers only**: The Cloud Policy will be maintained by the Admin console
team.
[See instructions here](https://docs.google.com/document/d/1QgDTWISgOE8DVwQSSz8x5oKrI3O_qAvOmPryE5DQPcw/edit?usp=sharing)
on how to update the Cloud Policy.

## Post policy update

Once the policy is added or modified, nothing else needs to be taken
care of by the Chromium developers. However, there are a few things that will be
updated based on the json file. Please note that there is no ETA for
everything listed below.

* [Policy templates](https://dl.google.com/dl/edgedl/chrome/policy/policy_templates.zip)
  will be updated automatically.
* [Policy documentation](https://cloud.google.com/docs/chrome-enterprise/policies/)
  will be updated automatically.

## Targeting features at commercial users

The recommended method to target commercial users is to create a policy to
control the behavior of a feature. You can for example create a feature only
for consumer users by setting `default_for_enterprise_users` to false; however,
it should only be used when the default enterprise behavior should be different
than regular consumer behavior.

------

### Additional Notes

1. policy_templates.json is actually a Python dictionary even though the file
   name contains *json*.
2. The `future_on` flag can disable policy on Beta of Stable channel only if the
   policy value is copied to `PrefService` in Step 3 of **Adding a new policy**.
