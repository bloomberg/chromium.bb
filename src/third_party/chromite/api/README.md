# Build API

Welcome to the Build API.

### proto/
This directory contains all of the raw .proto files.
You will find message, service, and method definitions, and their configurations.

* build_api.proto contains service and method option definitions.
* common.proto contains well shared messages.

### gen/
The generated protobuf messages.

**Do not edit files in this package directly!**

The proto can be compiled using the `compile_build_api_proto` script in the api directory.
The protoc call is executed inside the chroot to ensure a standard protoc version is used.

### controller/

This directory contains the entry point for all of the implemented services.
The protobuf service module option (defined in build_api.proto) references the module in this
package.
The functions in this package should all operate as one would expect a controller to operate in an
MVC application - translating the request into the internal representation that's passed along to
the relevant service(s), then translates their output to a specified response format.
