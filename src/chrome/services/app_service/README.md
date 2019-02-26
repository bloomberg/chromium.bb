# App Service

Chrome, and especially Chrome OS, has apps, e.g. chat apps and camera apps.

There are a number (lets call it `M`) of different places or app Consumers,
usually UI (user interfaces) related, where users interact with their installed
apps: e.g. launcher, search bar, shelf, New Tab Page, the `chrome://apps` page,
permissions or settings pages, picking and running default handlers for URLs,
MIME types or intents, etc.

There is also a different number (`N`) of app platforms or app Providers:
built-in apps, extension-backed apps, PWAs (progressive web apps), ARC++
(Android apps), Crostini (Linux apps), etc.

Historically, each of the `M` Consumers hard-coded each of the `N` Providers,
leading to `M×N` code paths that needed maintaining, or updating every time `M`
or `N` was incremented.

This document describes the App Service, an intermediary between app Consumers
and app Providers. This simplifies `M×N` code paths to `M+N` code paths, each
side with a uniform API, reducing code duplication and improving behavioral
consistency. This service (a Mojo service) could potentially be spun out into a
new process, for the usual
[Servicification](https://www.chromium.org/servicification) benefits (e.g.
self-contained services are easier to test and to sandbox), and would also
facilitate Chrome OS apps that aren't tied to the browser, e.g. Ash apps.

The App Service can be decomposed into a number of aspects. In all cases, it
provides to Consumers a uniform API over the various Provider implementations,
for these aspects:

- App Registry: list the installed apps.
- App Icon Factory: load an app's icon, at various resolutions.
- App Runner: launch apps and track app instances.
- App Installer: install, uninstall and update apps.
- App Coordinator: keep system-wide settings, e.g. default handlers.

Some things are still the responsbility of individual Consumers or Providers.
For example, the order in which the apps' icons are presented in the launcher
is a launcher-specific detail, not a system-wide detail, and is managed by the
launcher, not the App Service. Similarly, Android-specific VM configuration is
Android-specific, not generalizable system-wide, and is managed by the Android
provider (ARC++).


## Profiles

Talk of *the* App Service is an over-simplification. There will be *an* App
Service instance per Profile, as apps can be installed for *a* Profile.

Note that this doesn't require the App Service to know about Profiles. Instead,
Profile-specific code (e.g. a KeyedService) finds the Mojo service Connector
for a Profile, creates an App Service and binds the two (App Service and
Connector), but the App Service itself doesn't know about Profiles per se.


# App Registry

The App Registry's one-liner mission is:

  - I would like to be able to for-each over all the apps in Chrome.

An obvious initial design for the App Registry involves three actors (Consumers
⇔ Service ⇔ Providers) with the middle actor (the App Registry Mojo service)
being a relatively thick implementation with a traditional `GetFoo`, `SetBar`,
`AddObserver` style API. The drawback is that Consumers are often UI surfaces
and UI code likes synchronous APIs, but Mojo APIs are generally asynchronous,
especially as it may cross process boundaries.

Instead, we use four actors (Consumers ↔ Proxy ⇔ Service ⇔ Providers), with the
Consumers ↔ Proxy connection being synchronous and in-process, lighter than the
async / out-of-process ⇔ connections. The Proxy implementation is relatively
thick and the Service implementation is relatively thin, almost trivially so.
Being able to for-each over all the apps is:

    for (const auto& app : GetAppServiceProxy(profile).GetCache().GetAllApps()) {
      DoSomethingWith(app);
    }

The Proxy is expected to be in the same process as its Consumers, and the Proxy
would be a singleton (per Profile) within that process: Consumers would connect
to *the* in-process Proxy. If all of the app UI code is in the browser process,
the Proxy would also be in the browser process. If app UI code migrated to e.g.
a separate Ash process, then the Proxy would move with them. There might be
multiple Proxies, one per process (per Profile).


## Code Location

Some code is tied to a particular process, some code is not. For example, the
per-Profile `AppServiceProxy` obviously contains Profile-related code (i.e. a
`KeyedService`, so that browser code can find *the* `AppServiceProxy` for a
given Profile) that is tied to being in the browser process. The
`AppServiceProxy` also contains process-agnostic code (code that could
conceivably be used by an `AppServiceProxy` living in an Ash process), such as
code to cache and update the set of known apps (as in, the `App` Mojo type).
Specifically, the `AppServiceProxy` code is split into two locations, one under
`//chrome/browser` and one not:

  - `//chrome/browser/apps/app_service`
  - `//chrome/services/app_service`

On the Provider side, code specific to extension-backed applications or web
applications (as opposed to ARC++ or Crostini applications) lives under:

  - `//chrome/browser/extensions`
  - `//chrome/browser/web_applications`


## Matchmaking and Publish / Subscribe

The `AppService` itself does not have an `GetAllApps` method. It doesn't do
much, and it doesn't keep much state. Instead, the App Registry aspect of the
`AppService` is little more than a well known meeting place for `Publisher`s
(i.e. Providers) and `Subscriber`s (i.e. Proxies) to discover each other. An
analogy is that it's a matchmaker for `Publisher`s and `Subscriber`s, although
it matches all to all instead of one to one. `Publisher`s don't meet
`Subscriber`s directly, they meet the matchmaker, who introduces them to
`Subscriber`s.

Once a `Publisher` and `Subscriber` connect, the Pub-side sends the Sub-side a
stream of `App`s (calling the `Subscriber`'s `OnApps` method). On the initial
connection, the `Publisher` calls `OnApps` with "here's all the apps that I
(the `Publisher`) know about", with additional `OnApps` calls made as apps are
installed, uninstalled, updated, etc.

As mentioned, the App Registry aspect of the `AppService` doesn't do much. Its
part of the `AppService` Mojo interface is:

    interface AppService {
      // App Registry methods.
      RegisterPublisher(Publisher publisher, AppType app_type);
      RegisterSubscriber(Subscriber subscriber, ConnectOptions? opts);

      // Some additional methods; not App Registry related.
    };

    interface Publisher {
      // App Registry methods.
      Connect(Subscriber subscriber, ConnectOptions? opts);

      // Some additional methods; not App Registry related.
    };

    interface Subscriber {
      OnApps(array<App> deltas);
    };

    enum AppType {
      kUnknown,
      kArc,
      kCrostini,
      kWeb,
    };

    struct ConnectOptions {
      // TBD: some way to represent l10n info such as the UI language.
    };

Whenever a new `Publisher` is registered, it is connected with all of the
previously registered `Subscriber`s, and vice versa. Once a `Publisher` is
connected directly to a `Subscriber`, the `AppService` is no longer involved.
Even as new apps are installed, uninstalled, updated, etc., the app's
`Publisher` talks directly to each of its (previously connected) `Subscriber`s,
without involving the `AppService`.

TBD: whether we need un-registration and dis-connect mechanisms.


## The App Type

The one Mojo struct type, `App`, represents both a state, "an app that's ready
to run", and a delta or change in state, "here's what's new about an app".
Deltas include events like "an app was just installed" or "just uninstalled" or
"its icon was updated".

This is achieved by having every `App` field (other than `App.app_type` and
`App.app_id`) be optional. Either optional in the Mojo sense, with type `T?`
instead of a plain `T`, or if that isn't possible in Mojo (e.g. for integer or
enum fields), as a semantic convention above the Mojo layer: 0 is reserved to
mean "unknown". For example, the `App.show_in_launcher` field is a
`OptionalBool`, not a `bool`.

An `App.readiness` field represents whether an app is installed (i.e. ready to
launch), uninstalled or otherwise disabled. "An app was just installed" is
represented by a delta whose `readiness` is `kReady` and the old state's
`readiness` being some other value. This is at the Mojo level. At the C++
level, the `AppUpdate` wrapper type (see below) can provide helper
`WasJustInstalled` methods.

The `App`, `Readiness` and `OptionalBool` types are:

    struct App {
      AppType app_type;
      string app_id;

      // The fields above are mandatory. Everything else below is optional.

      Readiness readiness;
      string? name;
      IconKey? icon_key;
      OptionalBool show_in_launcher;
      // etc.
    };

    enum Readiness {
      kUnknown = 0,
      kReady,                // Installed and launchable.
      kDisabledByBlacklist,  // Disabled by SafeBrowsing.
      kDisabledByPolicy,     // Disabled by admin policy.
      kDisabledByUser,       // Disabled by explicit user action.
      kUninstalledByUser,
    };

    enum OptionalBool {
      kUnknown = 0,
      kFalse,
      kTrue,
    };

    // struct IconKey is discussed in the "App Icon Factory" section.

A new state can be mechanically computed from an old state and a delta (both of
which have the same type: `App`). Specifically, last known value wins. Any
known field in the delta overwrites the corresponding field in the old state,
any unknown field in the delta is ignored. For example, if an app's name
changed but its icon didn't, the delta's `App.name` field (a
`base::Optional<std::string>`) would be known (not `base::nullopt`) and copied
over but its `App.icon` field would be unknown (`base::nullopt`) and not copied
over.

The current state is thus the merger or sum of all previous deltas, including
the initial state being a delta against the ground state of "all unknown". The
`AppServiceProxy` tracks the state of its apps, and implements the
(in-process) Observer pattern so that UI surfaces can e.g. update themselves as
new apps are installed. There's only one method, `OnAppUpdate`, as opposed to
separate `OnAppInstalled`, `OnAppUninstalled`, `OnAppNameChanged`, etc.
methods. An `AppUpdate` is a pair of `App` values: old state and delta.

    class AppRegistryCache {
     public:
      class Observer : public base::CheckedObserver {
       public:
        ~Observer() override {}
        virtual void OnAppUpdate(const AppUpdate& update) = 0;
      };

      // Etc.
    };


# App Icon Factory

Icon data (even compressed as a PNG) is bulky, relative to the rest of the
`App` type. `Publisher`s will generally serve icon data lazily, on demand,
especially as the desired icon resolutions (e.g. 64dip or 256dip) aren't known
up-front. Instead of sending an icon at all possible resolutions, the
`Publisher` sends an `IconKey`: enough information (when combined with the
`AppType app_type` and `string app_id`) to load the icon at given resoultions.
The `IconKey` is an `IconType icon_type` plus additional data (`uint64 u_key`
and `string s_key`) whose semantics depend on the `icon_type`.

For example, some icons are statically built into the Chrome or Chrome OS
binary, as PNG-formatted resources, and can be loaded (synchronously, without
sandboxing). They can be loaded from a `u_key` resource ID. Some icons are
dynamically (and asynchronously) loaded from the extension database on disk.
They can be loaded from the `app_id` alone.

TBD: for extension-backed icons, some sort of timestamp or version in the
`IconKey` (probably encoded in the `u_key`) so that an icon update results in a
different `IconKey`.

Consumers (via the `AppServiceProxy`) can always ask the `AppService` to load
an icon. As an optimization, if the `AppServiceProxy` knows how to load an icon
for a given `IconKey`, it can skip the Mojo round trip and bulk data IPC and
load it directly instead. For example, it may know how to load icons from a
statically built resource ID.

    interface AppService {
      // App Icon Factory methods.
      LoadIcon(
          AppType app_type,
          string app_id,
          IconKey icon_key,
          IconCompression icon_compression,
          int32 size_hint_in_dip) => (IconValue icon_value);

      // Some additional methods; not App Icon Factory related.
    };

    interface Publisher {
      // App Icon Factory methods.
      LoadIcon(
          string app_id,
          IconKey icon_key,
          IconCompression icon_compression,
          int32 size_hint_in_dip) => (IconValue icon_value);

      // Some additional methods; not App Icon Factory related.
    };

    enum IconType {
      kUnknown,
      kExtension,
      kResource,
    };

    struct IconKey {
      IconType icon_type;
      // The semantics of u_key and s_key depend on the icon_type.
      uint64 u_key;
      string s_key;
    };

    enum IconCompression {
      kUnknown,
      kUncompressed,
      kCompressed,
    };

    struct IconValue {
      IconCompression icon_compression;
      gfx.mojom.ImageSkia? uncompressed;
      array<uint8>? compressed;
    };

TBD: post-processing effects like rounded corners, badges or grayed out (for
disabled apps) icons.


# App Runner

Each `Publisher` has (`Publisher`-specific) implementations of e.g. launching an
app and populating a context menu. The `AppService` presents a uniform API to
trigger these, forwarding each call on to the relevant `Publisher`:

    interface AppService {
      // App Runner methods.
      Launch(AppType app_type, string app_id, LaunchOptions? opts);
      // etc.

      // Some additional methods; not App Runner related.
    };

    interface Publisher {
      // App Runner methods.
      Launch(string app_id, LaunchOptions? opts);
      // etc.

      // Some additional methods; not App Runner related.
    };

    struct LaunchOptions {
      // TBD.
    };

TBD: details for context menus.

TBD: be able to for-each over all the app *instances*, including multiple
instances (e.g. multiple windows) of the one app.


# App Installer

This includes Provider-facing API (not Consumer-facing API like the majority of
the `AppService`) to help install and uninstall apps consistently. For example,
one part of app installation is adding an icon shortcut (e.g. on the Desktop
for Windows, on the Shelf for Chrome OS). This helper code should be written
once (in the `AppService`), not `N` times in `N` Providers.

TBD: details.


# App Coordinator

This keeps system-wide or for-apps-as-a-whole preferences and settings, e.g.
out of all of the installed apps, which app has the user preferred for photo
editing. Consumer- or Provider-specific settings, e.g. icon order in the Chrome
OS shelf, or Crostini VM configuration, is out of scope of the App Service.

TBD: details.


---

Updated on 2018-11-22.
