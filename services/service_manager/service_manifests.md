# Service Manifests

[TOC]

## Overview

Manifest files are used to configure security properties and
permissions for services, such as listing allowed sets of interfaces or
specifying a sandbox type. The files use JSON format and are usually
placed in the same directory as the service source files, but the path
is configurable in the BUILD.gn file for the service
(see [README.md](README.md#build-targets) for an example).

## Terminology

The Service Manager is responsible for starting new service instances on-demand,
and a given service may have any number of concurrently running instances.
The Service Manager disambiguates service instances by their unique identity.
A service's **identity** is represented by the 4-tuple of its service name,
instance group, instance ID, and a globally unique ID:

### Service name

A free-form -- typically short -- string identifying the the specific service
being run in the instance.

### Instance Group

Service instances started by the Service Manager are organized into
*instance groups*. Typically a service instance can only connect to other
service instances in its own group. Exceptions are made for targeted services
which are designed to be accessible cross-group (such as singleton services), as
well as for connecting services which are sufficiently privileged to connect to
any service instance in the system.

Instance group is represented by a `base::Token` in `service_manager::Identity`.
When instance group is omitted from an Identity passed with an interface request
to the Service Manager, the Service Manager assumes that the request should be
routed to a service instance in the requestor's same instance group.

In Chrome, every `BrowserContext` has a randomly generated instance group ID
associated with it, and this is used to isolate the service instances which run
on behalf of specific profiles, including renderers.

### Instance ID

Another `base::Token` in `service_manager::Identity`, used to disambiguate
multiple instances of a service for the same user. Every instance has an
instance ID, but it typically takes on the default value of a zero-token.

### Globally Unique ID

Every service instance started by the Service Manager is assigned a randomly
generated `base::Token` value designated as the instance's
**globally unique ID**. This value is always unique to a single instance across
time and space.

### Connections

Every service instance has a Connector API it can use to issue requests to the
Service Manager. One such request is `BindInterface`, which allows the service
to bind an interface within another service instance.

When binding an interface, the **source identity** refers to the service
initiating the bind request, and the **target identity** refers to the
destination service instance. Based on both the source and target identities,
the Service Manager may choose to start a new service instance, reuse an
existing instance as the destination for the bind request or deny the request.

### Interface provider

InterfaceProvider is a Mojo
[interface](https://cs.chromium.org/chromium/src/services/service_manager/public/mojom/interface_provider.mojom)
for providing an implementation of an interface by name. It is implemented by
different hosts (for frames and workers) in the browser, and the manifest
allows to specify different sets of capabilities exposed by these hosts.

## File structure

### name (string)

A unique identifier that is used to refer to the service.

### display\_name (string)

A human-readable name which can have any descriptive value. Not user-visible.

### sandbox\_type (string)

An optional field that provides access to several types of sandboxes.
Inside a sandbox, by default the service is essentially restricted to CPU and
memory access only. Common values are:

* `utility` (default) -  also allows full access to one configurable directory;
* `none` - no sandboxing is applied;
* `none_and_elevated` - under Windows, no sandboxing is applied and privileges
are elevated.

If the service cannot run with a sandbox type of utility, elevated, or none,
please reach out to the security team.

### options (dictionary)

This field can be used to specify values for any of the following options:

#### instance\_sharing (string)

Determines which parameters result in the creation of a new service
instance on an incoming service start/interface bind request.

Possible values:

##### none (default)

When one service is connecting to another, checks are performed to either find
an existing instance that matches the target identity or create a new one if
no match is found.

By default, all four identity components (service name, instance group, instance
ID, and globally unique ID) are used to find a match.

See
[advice](https://chromium.googlesource.com/chromium/src/+/master/docs/servicification.md#is-your-service-global-or-per_browsercontext)
on using this option.

Example:
[identity](https://cs.chromium.org/chromium/src/services/identity/manifest.json)

##### shared\_across\_instance\_groups

In this case, the instance group parameter is ignored when looking for a
matching target instance, so an existing instance can be reused regardless of
which instance group the connecting service belongs to.

Example:
[data_decoder](https://cs.chromium.org/chromium/src/services/data_decoder/manifest.json)

##### singleton

In this case, both instance group and instance ID parameters are ignored when
an interface request is targeting the service. Only one service instance is
created, and all interface requests targeting the service will be routed to that
instance.

Example:
[download_manager](https://cs.chromium.org/chromium/src/chrome/browser/android/download/manifest.json)

#### can\_connect\_to\_instances\_in\_any\_group (bool)

This option allows a service to make outgoing requests with a target instance
group other than its own.

**Note:** this privilege must only be granted to services that are trusted
at the same level as the browser process itself.

Example:
[content_browser](https://cs.chromium.org/chromium/src/content/public/app/mojo/content_browser_manifest.json)

The browser process manages all `BrowserContexts`, so it needs this privilege
to act on behalf of different users.

#### can\_connect\_to\_other\_services\_with\_any\_instance\_name (bool)

This option allows a service to specify an instance ID that is
different from the service name when connecting.

**Note:** this privilege must only be granted to services that are trusted
at the same level as the browser process itself.

Example:
[chrome_browser](https://cs.chromium.org/chromium/src/chrome/app/chrome_manifest.json)

Code in chrome_browser calls an XML parsing library function, which generates a
random instance ID to
[isolate unrelated decode operations](https://cs.chromium.org/chromium/src/services/data_decoder/public/cpp/safe_xml_parser.cc?l=50).

#### can\_create\_other\_service\_instances (bool)

This option allows a service to register arbitrary new service instances it
creates on its own.

**Note:** this privilege must only be granted to services that are trusted
at least at the same level as the Service Manager itself.

Example:
[content_browser](https://cs.chromium.org/chromium/src/content/public/app/mojo/content_browser_manifest.json)

The browser manages render processes, and thus needs this privilege to manage
the content_renderer instances on behalf of the service manager.

### interface\_provider\_specs (dictionary)

The interface provider spec is a dictionary keyed by interface provider
name, with each value representing the capability spec for that
provider.
Each capability spec defines an optional "provides" key and an optional
"requires" key.

Every interface provider spec (often exclusively) contains one standard
capability spec named “service_manager:connector”. This is the
capability spec enforced when inter-service connections are made from a
service's `Connector` interface.

Some other examples of capability specs are things like "navigation:frame",
which enforces capability specs for interfaces retrieved through a
frame's `InterfaceProvider`.

See [README.md](README.md#service-manifests) for some examples.

**Note:** Since multiple interface provider support makes the manifest files
harder to understand, there is a plan to simplify this section
(see https://crbug.com/718652 for more details).

#### provides (dictionary)

This optional section specifies a set of capabilities provided by the service.
A capability is a set of accessible interfaces.

For example, suppose we have the following capabilities:

* useful_capability
  * useful\_interface\_1
  * useful\_interface\_2
* better\_capability
  * better\_interface

The `provides` section would be:
``` json
  "provides": {
    "useful_capability": [
      "useful_interface_1",
      "useful_interface_2" ],
    "better_capability": [
      "better_interface" ],
  }
```

#### requires (dictionary)

This optional section is also a dictionary, keyed by remote service
names (the service name must match the "name" value in the remote service's
manifest). Each value is a list of capabilities required by this service
from the listed remote service. This section does not name interfaces,
only capabilities.

For example, if our capability requires service "some_capability" from
service "another_service", the "requires" section will look like this:

``` json
"requires": {
  "another_service": [ "some_capability" ]
```

An asterisk is a wildcard which means that any listed capabilities are
required of any service that provides them. For example:

``` json
"requires": {
  "*": [ "some_capability" ]
```

In the above example, this service can access any interface provided as part
of a capability named "some_capability" in any service on the system.

While generally discouraged, there are use cases for wildcards.
Consider building something like a UI shell with a launcher that wants to
tell any service "please launch with some default UI". The services providing
a "launch" capability would be expected to include access to an
"`app_launcher.mojom.Launcher`" interface as part of that capability, with an
implementation that does something useful like open some default UI for the
service:

``` json
"provides": {
  "launch": [ "app_launcher.mojom.Launcher" ]
}
```

Then our app launcher service would expect to be able to bind
`app_launcher.mojom.Launcher` in any service that provides that capability:


``` json
"requires": {
   "*" : [ "launch" ]
}
```

### required\_files (dictionary)

Allows the (sandboxed) service to specify a list of platform-specific files it
needs to access from disk while running. Each file is keyed by an arbitrary
name chosen by the service, and references a file path relative to the Service
Manager embedder (e.g. the Chrome binary) at runtime.

Files specified here will be opened by the Service Manager before launching a
new instance of this service, and their opened file descriptors will be passed
to the new sandboxed process. The file descriptors may be accessed via
`base::FileDescriptorStore` using the corresponding key string from the
manifest.

**Note:** This is currently only supported on Android and Linux-based desktop
builds.

#### path (string)

Path to the file relative to the executable location at runtime.

#### platform (string)

The platform this file is required on.
Possible values:

* `linux`
* `android`
