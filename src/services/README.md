# Service Development Guidelines

[TOC]

## Overview

If you're looking for general documentation on the Service Manager, what a
"service" is, and how to build one, see the
[Service Manager &amp; Services](/services/service_manager/README.md)
documentation instead of this document.

The top-level `//services` directory contains the sources, public Mojo interface
definitions, and public client libraries for a number of essential services,
designated as **Chrome Foundation Services**. If you think of Chrome as a
"portable OS," Chrome Foundation Services can be thought of as the core system
services of that OS.

Each subdirectory here corresponds to a service that:

- implements
  [`Service`](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/service.h?rcl=ebec02bcca6e327b2e01855ce43ab1bec0aeef27&l=21)
  and is thus a client of the Service Manager
- generally focuses on a subset of functionality or features which are
  thematically or functionally related in a way that makes sense given the name
  of the service
- could logically run in an isolated process for security or performance
  isolation, depending on the constraints of the host OS

*** aside
Note that there are other parts of the tree which aggregate
slightly-less-than-foundational service definitions, such as services specific
to the Chrome browser, defined in `//chrome/services`. The motivations, advice,
and standards discussed in this document apply to all service definitions in the
Chromium tree.
***

The `//services/service_manager` directory contains the implementation and
public APIs of the Service Manager itself, including an embedder API with which
software applications (such as Chrome) can embed the Service Manager to manage
their own multiprocess service architecture.

One of the main motivations for expressing Chromium as a collection of services
is long-term maintainability and code health. Because service API boundaries are
strictly limited to Mojo interfaces, state owned and managed by each service is
strongly isolated from other components in the system.

Another key motivation is general modularity and reusability: in the past there
have been a number of missed opportunities for potential new features or
Chromium-based products due to the browser's generally monolothic and inflexible
system design. With the Service Manager &amp; services providing scaffolding for
system components, it becomes progressively easier to build out newer use cases
with *e.g.* a smaller resource footprint, or a different process model, or even
a more granular binary distribution.

## Service Standards

As outlined above, individual services are intended for graceful reusability
across a broad variety of use cases. To enable this goal, we have rigorous
standards on services' structure and public API design. Before doing significant
work in `//services` (or other places where services are defined), please
internalize these standards. All Chromium developers are responsible for
upholding them!

### Public Service APIs

In creating and maintaining a service's public API, please respect the following
principles:

- The purpose of a service should be readily apparent.
- The supported client use cases of the service should be easy for a new
  consumer to understand.
- The service should use idioms and design patterns consistent with other
  services.
- From the service's public API documentation and tests, it should be feasible
  to develop a new implementation of the service which satisfies existing
  clients and doesn't require mimicking internal implementation details of the
  existing service.
- Perhaps most important of all, a service's public API should be designed with
  multiple hypothetical clients in mind, *not* focused on supporting only a
  single narrow use known at development time. **Always be thinking about the
  future!**

If you're working on a new service and have concerns or doubts about API design,
please post to
[services-dev@chromium.org](https://groups.google.com/a/chromium.org/forum#!forum/services-dev)
and ask for help. The list is generally quite responsive, and it's loaded with
people who have done a lot of work on services.

### Service API Design Tips

#### Using Interface Factories to Establish Context

One common pitfall when designing service APIs is to write something like:

``` cpp
interface GoatTeleporter {
  // Sets the client interface pipe for this teleporter. Must be called before
  // other interface methods.
  SetClient(GoatTeleporterClient client);

  TeleportGoat(string name);
};

interface GoatTeleporterClient {
  TeleporterReady();
};
```

The problem with this approach is that a client may easily fail to call
`SetClient` before calling `TeleportGoat`. When such ordering requirements are
necessary, the service can benefit clients by designing an API that is harder
to fail at. For example:

``` cpp
interface GoatTeleporterFactory {
  GetGoatTeleporter(GoatTeleporter& request, GoatTeleporterClient client);
};

interface GoatTeleporter {
  TeleportGoat(string name);
};
```

Instead of exposing `GoatTeleporter` directly to other services, the service can
expose `GoatTeleporterFactory` instead. Now it's impossible for a client to
acquire a functioning `GoatTeleporter` pipe without also providing a
corresponding client pipe to complement it.

### Service &amp; Interface Naming

Just some basic tips for service and interface naming:

- Strive to give your service a name that makes it immediately obvious what the
  service is for (*e.g.*, `"network"`, `"metrics"`) rather than a meaningless
  codename like `"cromulator_3000"`.
- Avoid the usage of `"Service"` in interface names. While the term "service"
  is overloaded in Chromium and certainly has plenty of valid interpretations,
  in the context of Service Manager services it has a very specific meaning
  and should not be overloaded further if possible. An interface which exposes a
  control API for a goat teleporter can just be called `GoatTeleporter`, not
  `GoatTeleporterService`.

- Strive to avoid conceptual layering violations in naming and documentation --
  *e.g.*, avoid referencing Blink or Content concepts like "renderers" or
  "frame hosts".

- Use the names `FooClient` and `FooObserver` consistently in interfaces. If
  there is an expected 1:1 correspondence between a Foo and its client interface
  counterpart, that counterpart should most likely be called `FooClient`. If
  there is expected to be 1-to-many correspondence between a Foo and its
  counterpart clients, the client interface may be better named `FooObserver`.

### Service Directory &amp; Dependency Structure

Services typically follow a canonical directory structure:

```
//services/service_name/               # Private implementation
                        public/
                               mojom/  # Mojom interfaces
                               cpp/    # C++ client libraries (optional)
                               java/   # Java client libararies (optional, rare)
                               js/     # JS client libraries (optional, rare)
```

As a general rule, **nothing below `/public` can depend on the private service
implementation** (*i.e.* things above `/public`). Enforcing this principle makes
it much easier to keep the service's state well-isolated from the rest of the
system.

Generally the language-specific client libraries are built against only the
public mojom API of the service (and usually few other common dependencies like
`//base` and `//mojo`), and they bootstrap connections to those interfaces by
using public Service Manager APIs like
[`Connector`](/services/service_manager/README.md#Connectors).

Even in the private service implementation, services should not depend on very
large components like Content, Chrome, or Blink.

*** aside
NOTE: Exceptions to the above rule are made in rare cases where Blink or V8 is
actually required as part of the service implementation. For example
`"data_decoder"` uses Blink implementation to decode common image formats, and
`"proxy_resolver"` uses V8 to execute proxy autoconfig scripts.
***

### Service Documentation

- Every service should have a top-level `README.md` that explains the purpose and
  supported usage models of the service.

- Every public interface should be documented within its Mojom file at both the
  interface level and indivudal message level.

- Interface documentation should be complete enough to serve as test
  specifications. If the method returns information of a user's accounts, what
  should happen if the user is not signed in? If the method makes a request for
  an access token, what happens if a client makes a second method call before
  the first one has completed? If the method returns a nullable object, under
  which conditions will it be null?

- Avoid writing interface documentation which is unnecessarily prescriptive
  about implementation details. Keep in mind that these are **interface**
  definitions, not implementations thereof.

- Avoid writing documentation which is tailored to a specific client.

### Service Testing

- Try to cover service implementation details with unit tests tied as closely
  as possible to the private implementation object or method being tested,
  rather than exercising implementation details through public API surface.

- For integration tests, try to have tests cover as much of the public API
  surface as possible while mocking out as little of the underlying service as
  possible.

- Treat the public API tests as "conformance tests" which clearly demonstrate
  what expectations and guarantees are supposed to be upheld by *any*
  implementation of the service's APIs.

- Take advantage of the
  [test support library](https://cs.chromium.org/chromium/src/services/service_manager/public/cpp/test/)
  provided by the Service Manager. In particular, `TestConnectorFactory` is
  useful for driving public API tests with the service running inside the test
  process, and `TestServiceManager` makes it possible to easily cover
  out-of-process testing scenarios while faking out as little of the system as
  possible.

## Adding a New Service

See the [Service Manager documentation](/services/service_manager/README.md) for
more details regarding how to define a service and expose or consume interfaces
to and from it, as well as how to make the service available to an application's
runtime environment.

Please start a thread on
[services-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/services-dev)
if you want to propose the introduction of a new service.

If you are servicifying an existing Chromium feature, please check out
[Servicifying Chromium Features](/docs/servicification.md).

## Other Docs

Here are some other external documents that aren't quite fully captured by any
documents in the Chromium tree. Beware of obsolete information:

- [High-level Design Doc](https://docs.google.com/document/d/15I7sQyQo6zsqXVNAlVd520tdGaS8FCicZHrN0yRu-oU)
- [Servicification Homepage](https://sites.google.com/a/chromium.org/dev/servicification)

## Additional Support

You can always post to
[services-dev@chromium.org](https://groups.google.com/a/chromium.org/forum#!forum/services-dev)
with questions or concerns about anything related to service development.
