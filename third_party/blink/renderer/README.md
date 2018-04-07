# Blink directory structure

This document describes a high-level architecture of Blink's top-level directories.

## core/ and modules/

core/ and modules/ are directories to implement web platform features
defined in the specs. IDL files and their implementations should go to
core/ and modules/.

Note that the specs do not have a notion of "core" and "modules".
The distinction between core/ and modules/ is for implementational convenience
to avoid putting everything in core/ (which decreases code modularity and
increases build time). Basically web platform features that are tighly coupled with
HTML, CSS and other fundamental parts of DOM should go to core/.
Other web platform features should go to modules/.

In terms of dependencies, modules/ can depend on core/.
core/ cannot depend on modules/. modules/xxx/ can depend on modules/yyy/.

## bindings/

bindings/ is a directory to put files that heavily use V8 APIs.

In terms of dependencies, bindings/core/ and core/ are in the same link unit.
The only difference is how heavily they are using V8 APIs.
If a given file is using a lot of V8 APIs, it should go to bindings/core/.
Otherwise, it should go to core/.
(The same principle applies to bindings/modules/ and modules/.)

The rationale for this split is: V8 APIs are complex, error-prone and
security-sensitive, so we want to put V8 API usages in one directory.

## platform/

platform/ is a directory to implement low-level libraries of Blink.
For example, platform/scheduler/ implements a task scheduler for all tasks
posted by Blink. To avoid putting everything in core/ and modules/,
consider factoring out low-level functionalities to platform/.

platform/wtf/ is a directory to implement Blink-specific containers
(e.g., Vector, HashTable, String).

In terms of dependencies, core/ and modules/ can depend on platform/.
platform/ cannot depend on core/ and modules/.

## controller/

controller/ is a directory to implement high-level functionalities
on top of core/ and modules/. Functionalities that use or drive Blink
should go to controller/.

In terms of dependencies, controller/ can depend on core/ and modules/.
core/ and modules/ cannot depend on controller/.

## devtools/

devtools/ implements a client-side of the Chrome DevTools, including all JS &
CSS to run the DevTools webapp.

In terms of dependencies, devtools/ is a stand-alone directory.

## build/

build/ contains scripts to build Blink.

In terms of dependencies, build/ is a stand-alone directory.

## Directory dependencies

Dependencies only flow in the following order:

- public/web/
- controller/
- modules/ and bindings/modules/
- core/ and bindings/core/
- platform/
- public/platform/, //base/, V8 etc.

See [this diagram](https://docs.google.com/document/d/1yYei-V76q3Mb-5LeJfNUMitmj6cqfA5gZGcWXoPaPYQ/edit).

devtools/ and build/ are stand-alone directories.

## Type dependencies

core/, modules/, bindings/, platform/ and controller/ can use std:: types and
types defined in Chromium. The philosophy is that we should
share as much code between Chromium and Blink as possible.

However, there are a couple of types that really need to be optimized
for Blink's workload (e.g., Vector, HashTable, Bind, AtomicString).
These types are defined in platform/wtf/. If there is an equivalent in
platform/wtf/, Blink must use the type in platform/wtf/ instead of the type
defined in Chromium. For example, Blink should not use std::vector
(except places where a conversion between std::vector and WTF::Vector is needed).

To prevent use of random types, we control allowed types by whitelisting
them in DEPS and a [presubmit script](../Tools/Scripts/audit-non-blink-usage.py).

## Mojo

core/, modules/, bindings/, platform/ and controller/ can use Mojo and
directly talk with the browser process. This allows removal of unnecessary
public APIs and abstraction layers and it is highly recommended.

## Contact

If you have any questions about the directory architecture and dependencies,
reach out to platform-architecture-dev@chromium.org!

