This directory contains CQ client library to be distributed to other repos. If
you need to modify some files in this directory, please make sure that you are
changing the canonical version of the source code and not one of the copies,
which should only be updated as a whole using Glyco (when available, see
http://crbug.com/489420).

The canonical version is located at `https://chrome-internal.googlesource.com/
infra/infra_internal/+/master/commit_queue/cq_client`.

To generate `cq_pb2.py`, please use protoc version 2.6.1:

    cd commit_queue/cq_client
    protoc cq.proto --python_out $(pwd)

Additionally, please make sure to use proto3-compatible syntax, e.g. no default
values, no required fields. Ideally, we should use proto3 generator already,
however alpha version thereof is still unstable.
