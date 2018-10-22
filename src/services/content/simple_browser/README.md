This directory contains an implementation of a simple Content Service client
application which serves as a tool for manual developer testing of the Content
Service, a straightforward example browser application consuming the Content
Service, and potentially as a target for driving automated Content Service
integration tests.

The simple_browser application can run in an isolated sandboxed process on
platforms which support the UI Service (currently only Chrome OS), or within the
browser process on platforms which otherwise support NavigableContentsView
embedding (currently Chrome OS, Linux, Mac, and Windows).

To play around with simple_browser today, run a DCHECK-enabled build of Chrome
with `--launch-in-process-simple-browser` on any supported platform listed
above.

To test the sandboxed, out-of-process version, use a DCHECK-enabled Chrome OS
build and run with both `--enable-features=Mash` and `--launch-simple-browser`
flags.
