# Chrome OS testing config

These config files are used to determine which tests need to be run for
particular commits.

This configuration is done with Starlark.

The human-readable configuration, as well as helper methods, live in this
directory, while their generated Protocol Buffer-JSON files live in the
generated/ subdirectory.

## Regenerating the configs

To regenerate the JSON files, run the regenerate\_configs.sh script in this
directory.

## Proto locations

The Protocol Buffers that describe these configs' output JSON format live in
the lucicfg repo. It'd be nice to have those protos here, but due to a
limitation of our Starlark interpreter, the protos must live with the
interpreter itself.

Those files can be found here:

https://chromium.googlesource.com/infra/luci/luci-go/+/master/lucicfg/external/crostesting/proto/config
