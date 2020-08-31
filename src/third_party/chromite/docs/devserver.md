# Dev Server

[TOC]

## Overview

The dev server allows you to update your Chromium OS machine with new builds
without having to copy them via a USB drive. It also serves as a static file
repository, allowing you to install your own packages and scripts making it easy
to customize your machine for development purposes.

Note: Every time you create a Chromium OS build, the URL of the dev server
corresponding to your development machine is put into `/etc/lsb-release`. This
file can be changed post-installation to manage a test machine's update source
or can be overridden in `/mnt/stateful_partition/etc/lsb-release`.

## Starting the Dev Server

Note: Before you start the dev server, you must have already run
`build_packages.sh`. If you have not, please run this now.

The first step in using the dev server is starting the web server on your
development machine:

```bash
(chroot)$ start_devserver
```

## Using the Dev Server

### Options

Note that by default the devserver serves the latest image and runs on port
8080. You can change this among many other things. Here is a brief description
of some of the options. Please see `start_devserver --help` for more
information.

*   `--use_cached` - Forces the devserver to use the `update.gz` it finds in its
    static dir. This allows you to pre-generate an update payload and place it
    in the devserver's static file hosting directory
    (`~/src/platform/dev/static`).
*   `--src_image` - Generates a delta update based from the target image on the
    source image. If the source image you provide is not the image on the
    machine, the delta update will fail. Note that delta updates despite their
    name run slower than full updates (but will save you bandwidth).
*   `--pregenerate_update` - Generates the update before starting the dev
    server. This keeps from getting nasty timeouts for big update payloads
    _(necessary for most delta updates)._
*   `--image` - Serve this image to a machine that requests it.
*   `--port` - Start the devserver on this port.
*   `-t` - From the directory (using the latest image and/or using the archive
    logic) use the test image found in that directory
    (`chromiumos_test_image.bin`)
*   `--static_dir` - Serve images from a file structure. Clients can prefix
    which image they want in their request by modifying their `omaha_url`. By
    default this changes the static directory to `~/src/platform/dev/static`.

## Using Devserver to Stage and Serve Artifacts from Google Storage

A running devserver listens for calls to its stage and xBuddy RPCs.

### Stage RPC

An abbreviated version of the documentation for the stage RPC is copied here,
but the up to date version can be found by going to host:port/doc/stage on any
running devserver.

Downloads and caches the artifacts Google Storage URL. Returns once these have
been downloaded on the devserver. A call to this will attempt to cache
non-specified artifacts in the background for the given from the given URL
following the principle of spatial locality. Spatial locality of different
artifacts is explicitly defined in the build_artifact module. These artifacts
will then be available from the `static/` sub-directory of the devserver.

To download the autotest and test suites tarballs:

```
http://devserver_url:/stage?archive_url=gs://your_url/path&artifacts=autotest,test_suites
```

To download the full update payload:

```
http://devserver_url:/stage?archive_url=gs://your_url/path&artifacts=full_payload
```

To download just a file called `blah.bin`:

```
http://devserver_url:/stage?archive_url=gs://your_url/path&files=blah.bin
```

### xBuddy RPC

See [xBuddy for Devserver].

## Feature Requests

There are many things that can be done to improve this system. Feel free to make
suggestions and submit patches.

[xBuddy for Devserver]: https://sites.google.com/a/chromium.org/dev/chromium-os/how-tos-and-troubleshooting/using-the-dev-server/xbuddy-for-devserver#TOC-Devserver-rpc:-xbuddy
