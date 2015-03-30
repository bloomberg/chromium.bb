GN "Generate Ninja"

GN is a meta-build system that generates ninja files. It's meant to be faster
and simpler than GYP.

Chromium uses binary versions of GN downloaded from Google Cloud Storage during
gclient runhooks, so that we don't have to worry about bootstrapping a build
of GN from scratch.

Instructions for how to upload the .sha1 digests of the GN binaries can be
found at https://code.google.com/p/chromium/wiki/UpdateGNBinaries.
