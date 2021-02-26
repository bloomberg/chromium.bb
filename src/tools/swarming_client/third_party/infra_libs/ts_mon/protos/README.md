Updating the *.proto files: see go/updating-tsmon-protos

To generate the `*_pb2.py` files from the `*proto` files:

    cd <REPO_ROOT>/packages/infra_libs
    protoc --python_out=. infra_libs/ts_mon/protos/*.proto

protoc version tested: libprotoc 3.11.4
