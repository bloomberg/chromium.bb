The aim of this application is to provide a quick way to test different
networks.

It contains one testcase and a default network model (network_model.h), default
input data (input_data.h) and default expected output data
(expected_output_data.h). The header files were created using the `xxd` command.

The default model is a single MaxPool2D operator, with an input shape of {1, 4,
4, 1} and an output shape of {1, 2, 2, 1}.

In order to use another model, input data, or expected output data, simply
specify the path to the new header files when running make as seen below.

The variables in the specified header files (array and array length) needs to
have the same name and type as the ones in the default header files. The include
guards also needs to be the same. When swapping out the network model, it is
likely that the memory allocated by the interpreter needs to be increased to fit
the new model. This is done by using the `ARENA_SIZE` option when running
`make`.

```
make -f tensorflow/lite/micro/tools/make/Makefile network_tester_test \
                  NETWORK_MODEL=path/to/network_model.h \
                  INPUT_DATA=path/to/input_data.h \
                  OUTPUT_DATA=path/to/expected_output_data.h \
                  ARENA_SIZE=<tensor arena size in bytes> \
                  NUM_BYTES_TO_PRINT=<number of bytes to print> \
                  COMPARE_OUTPUT_DATA=no
```

`NETWORK_MODEL`: The path to the network model header. \
`INPUT_DATA`: The path to the input data. \
`OUTPUT_DATA`: The path to the expected output data. \
`ARENA_SIZE`: The size of the memory to be allocated (in bytes) by the
interpreter. \
`NUM_BYTES_TO_PRINT`: The number of bytes of the output data to print. \
Defaults to 0 if not specified. \
`COMPARE_OUTPUT_DATA`: If set to "no" the output data is not compared to the
expected output data. This could be useful e.g. if the execution time needs to
be minimized, or there is no expected output data. If omitted, the output data
is compared to the expected output.
