"""Generate Flatbuffer binary from json."""

load(
    "//tensorflow:tensorflow.bzl",
    "clean_dep",
    "tf_binary_additional_srcs",
    "tf_cc_shared_object",
    "tf_cc_test",
)
load("//tensorflow/lite:special_rules.bzl", "tflite_copts_extra")
load("//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")
load("@bazel_skylib//rules:build_test.bzl", "build_test")

def tflite_copts():
    """Defines common compile time flags for TFLite libraries."""
    copts = [
        "-DFARMHASH_NO_CXX_STRING",
    ] + select({
        clean_dep("//tensorflow:android_arm"): [
            "-mfpu=neon",
        ],
        clean_dep("//tensorflow:ios_x86_64"): [
            "-msse4.1",
        ],
        clean_dep("//tensorflow:windows"): [
            "/DTFL_COMPILE_LIBRARY",
            "/wd4018",  # -Wno-sign-compare
        ],
        "//conditions:default": [
            "-Wno-sign-compare",
        ],
    }) + select({
        clean_dep("//tensorflow:optimized"): ["-O3"],
        "//conditions:default": [],
    }) + select({
        clean_dep("//tensorflow:android"): [
            "-ffunction-sections",  # Helps trim binary size.
            "-fdata-sections",  # Helps trim binary size.
        ],
        "//conditions:default": [],
    }) + select({
        clean_dep("//tensorflow:windows"): [],
        "//conditions:default": [
            "-fno-exceptions",  # Exceptions are unused in TFLite.
        ],
    })

    return copts + tflite_copts_extra()

def tflite_copts_warnings():
    """Defines common warning flags used primarily by internal TFLite libraries."""

    # TODO(b/155906820): Include with `tflite_copts()` after validating clients.

    return select({
        clean_dep("//tensorflow:windows"): [
            # We run into trouble on Windows toolchains with warning flags,
            # as mentioned in the comments below on each flag.
            # We could be more aggressive in enabling supported warnings on each
            # Windows toolchain, but we compromise with keeping BUILD files simple
            # by limiting the number of config_setting's.
        ],
        "//conditions:default": [
            "-Wall",
        ],
    })

EXPORTED_SYMBOLS = clean_dep("//tensorflow/lite/java/src/main/native:exported_symbols.lds")
LINKER_SCRIPT = clean_dep("//tensorflow/lite/java/src/main/native:version_script.lds")

def tflite_linkopts_unstripped():
    """Defines linker flags to reduce size of TFLite binary.

       These are useful when trying to investigate the relative size of the
       symbols in TFLite.

    Returns:
       a select object with proper linkopts
    """

    # In case you wonder why there's no --icf is because the gains were
    # negligible, and created potential compatibility problems.
    return select({
        clean_dep("//tensorflow:android"): [
            "-latomic",  # Required for some uses of ISO C++11 <atomic> in x86.
            "-Wl,--no-export-dynamic",  # Only inc syms referenced by dynamic obj.
            "-Wl,--gc-sections",  # Eliminate unused code and data.
            "-Wl,--as-needed",  # Don't link unused libs.
        ],
        "//conditions:default": [],
    })

def tflite_jni_linkopts_unstripped():
    """Defines linker flags to reduce size of TFLite binary with JNI.

       These are useful when trying to investigate the relative size of the
       symbols in TFLite.

    Returns:
       a select object with proper linkopts
    """

    # In case you wonder why there's no --icf is because the gains were
    # negligible, and created potential compatibility problems.
    return select({
        clean_dep("//tensorflow:android"): [
            "-latomic",  # Required for some uses of ISO C++11 <atomic> in x86.
            "-Wl,--gc-sections",  # Eliminate unused code and data.
            "-Wl,--as-needed",  # Don't link unused libs.
        ],
        "//conditions:default": [],
    })

def tflite_symbol_opts():
    """Defines linker flags whether to include symbols or not."""
    return select({
        clean_dep("//tensorflow:debug"): [],
        clean_dep("//tensorflow/lite:tflite_keep_symbols"): [],
        "//conditions:default": [
            "-s",  # Omit symbol table, for all non debug builds
        ],
    })

def tflite_linkopts():
    """Defines linker flags to reduce size of TFLite binary."""
    return tflite_linkopts_unstripped() + tflite_symbol_opts()

def tflite_jni_linkopts():
    """Defines linker flags to reduce size of TFLite binary with JNI."""
    return tflite_jni_linkopts_unstripped() + tflite_symbol_opts()

def tflite_jni_binary(
        name,
        copts = tflite_copts(),
        linkopts = tflite_jni_linkopts(),
        linkscript = LINKER_SCRIPT,
        exported_symbols = EXPORTED_SYMBOLS,
        linkshared = 1,
        linkstatic = 1,
        testonly = 0,
        deps = [],
        tags = [],
        srcs = [],
        visibility = None):  # 'None' means use the default visibility.
    """Builds a jni binary for TFLite."""
    linkopts = linkopts + select({
        clean_dep("//tensorflow:macos"): [
            "-Wl,-exported_symbols_list,$(location {})".format(exported_symbols),
            "-Wl,-install_name,@rpath/" + name,
        ],
        clean_dep("//tensorflow:windows"): [],
        "//conditions:default": [
            "-Wl,--version-script,$(location {})".format(linkscript),
            "-Wl,-soname," + name,
        ],
    })
    native.cc_binary(
        name = name,
        copts = copts,
        linkshared = linkshared,
        linkstatic = linkstatic,
        deps = deps + [linkscript, exported_symbols],
        srcs = srcs,
        tags = tags,
        linkopts = linkopts,
        testonly = testonly,
        visibility = visibility,
    )

def tflite_cc_shared_object(
        name,
        copts = tflite_copts(),
        linkopts = [],
        linkstatic = 1,
        per_os_targets = False,
        **kwargs):
    """Builds a shared object for TFLite."""
    tf_cc_shared_object(
        name = name,
        copts = copts,
        linkstatic = linkstatic,
        linkopts = linkopts + tflite_jni_linkopts(),
        framework_so = [],
        per_os_targets = per_os_targets,
        **kwargs
    )

def tf_to_tflite(name, src, options, out):
    """Convert a frozen tensorflow graphdef to TF Lite's flatbuffer.

    Args:
      name: Name of rule.
      src: name of the input graphdef file.
      options: options passed to TFLite Converter.
      out: name of the output flatbuffer file.
    """

    toco_cmdline = " ".join([
        "$(location //tensorflow/lite/python:tflite_convert)",
        "--enable_v1_converter",
        ("--graph_def_file=$(location %s)" % src),
        ("--output_file=$(location %s)" % out),
    ] + options)
    native.genrule(
        name = name,
        srcs = [src],
        outs = [out],
        cmd = toco_cmdline,
        tools = ["//tensorflow/lite/python:tflite_convert"] + tf_binary_additional_srcs(),
    )

def DEPRECATED_tf_to_tflite(name, src, options, out):
    """DEPRECATED Convert a frozen tensorflow graphdef to TF Lite's flatbuffer, using toco.

    Please use tf_to_tflite instead.
    TODO(b/138396996): Migrate away from this deprecated rule.

    Args:
      name: Name of rule.
      src: name of the input graphdef file.
      options: options passed to TOCO.
      out: name of the output flatbuffer file.
    """

    toco_cmdline = " ".join([
        "$(location //tensorflow/lite/toco:toco)",
        "--input_format=TENSORFLOW_GRAPHDEF",
        "--output_format=TFLITE",
        ("--input_file=$(location %s)" % src),
        ("--output_file=$(location %s)" % out),
    ] + options)
    native.genrule(
        name = name,
        srcs = [src],
        outs = [out],
        cmd = toco_cmdline,
        tools = ["//tensorflow/lite/toco:toco"] + tf_binary_additional_srcs(),
    )

def tflite_to_json(name, src, out):
    """Convert a TF Lite flatbuffer to JSON.

    Args:
      name: Name of rule.
      src: name of the input flatbuffer file.
      out: name of the output JSON file.
    """

    flatc = "@flatbuffers//:flatc"
    schema = "//tensorflow/lite/schema:schema.fbs"
    native.genrule(
        name = name,
        srcs = [schema, src],
        outs = [out],
        cmd = ("TMP=`mktemp`; cp $(location %s) $${TMP}.bin &&" +
               "$(location %s) --raw-binary --strict-json -t" +
               " -o /tmp $(location %s) -- $${TMP}.bin &&" +
               "cp $${TMP}.json $(location %s)") %
              (src, flatc, schema, out),
        tools = [flatc],
    )

def json_to_tflite(name, src, out):
    """Convert a JSON file to TF Lite's flatbuffer.

    Args:
      name: Name of rule.
      src: name of the input JSON file.
      out: name of the output flatbuffer file.
    """

    flatc = "@flatbuffers//:flatc"
    schema = "//tensorflow/lite/schema:schema_fbs"
    native.genrule(
        name = name,
        srcs = [schema, src],
        outs = [out],
        cmd = ("TMP=`mktemp`; cp $(location %s) $${TMP}.json &&" +
               "$(location %s) --raw-binary --unknown-json --allow-non-utf8 -b" +
               " -o /tmp $(location %s) $${TMP}.json &&" +
               "cp $${TMP}.bin $(location %s)") %
              (src, flatc, schema, out),
        tools = [flatc],
    )

# This is the master list of generated examples that will be made into tests. A
# function called make_XXX_tests() must also appear in generate_examples.py.
# Disable a test by adding it to the denylists specified in
# generated_test_models_failing().
def generated_test_models():
    return [
        "abs",
        "add",
        "add_n",
        "arg_min_max",
        "avg_pool",
        "batch_to_space_nd",
        "cast",
        "ceil",
        "concat",
        "constant",
        "conv",
        "conv_relu",
        "conv_relu1",
        "conv_relu6",
        "conv2d_transpose",
        "conv_with_shared_weights",
        "conv_to_depthwiseconv_with_shared_weights",
        "cos",
        "depthwiseconv",
        "depth_to_space",
        "div",
        "elu",
        "equal",
        "exp",
        "embedding_lookup",
        "expand_dims",
        "eye",
        "fill",
        "floor",
        "floor_div",
        "floor_mod",
        "fully_connected",
        "fused_batch_norm",
        "gather",
        "gather_nd",
        "gather_with_constant",
        "global_batch_norm",
        "greater",
        "greater_equal",
        "hardswish",
        "identity",
        "sum",
        "l2norm",
        "l2norm_shared_epsilon",
        "l2_pool",
        "leaky_relu",
        "less",
        "less_equal",
        "local_response_norm",
        "log_softmax",
        "log",
        "logical_and",
        "logical_or",
        "logical_xor",
        "lstm",
        "matrix_diag",
        "matrix_set_diag",
        "max_pool",
        "maximum",
        "mean",
        "minimum",
        "mirror_pad",
        "mul",
        "nearest_upsample",
        "neg",
        "not_equal",
        "one_hot",
        "pack",
        "pad",
        "padv2",
        "placeholder_with_default",
        "prelu",
        "pow",
        "range",
        "rank",
        "reduce_any",
        "reduce_max",
        "reduce_min",
        "reduce_prod",
        "relu",
        "relu1",
        "relu6",
        "reshape",
        "resize_bilinear",
        "resize_nearest_neighbor",
        "resolve_constant_strided_slice",
        "reverse_sequence",
        "reverse_v2",
        "round",
        "rsqrt",
        "scatter_nd",
        "shape",
        "sigmoid",
        "sin",
        "slice",
        "softmax",
        "space_to_batch_nd",
        "space_to_depth",
        "sparse_to_dense",
        "split",
        "splitv",
        "sqrt",
        "square",
        "squared_difference",
        "squeeze",
        "strided_slice",
        "strided_slice_1d_exhaustive",
        "strided_slice_np_style",
        "sub",
        "tanh",
        "tile",
        "topk",
        "transpose",
        "transpose_conv",
        "unfused_gru",
        "unique",
        "unpack",
        "unroll_batch_matmul",
        "where",
        "zeros_like",
    ]

# List of models that fail generated tests for the conversion mode.
# If you have to disable a test, please add here with a link to the appropriate
# bug or issue.
def generated_test_models_failing(conversion_mode):
    """Returns the list of failing test models.

    Args:
      conversion_mode: Conversion mode.

    Returns:
      List of failing test models for the conversion mode.
    """
    if conversion_mode == "toco-flex":
        # These tests mean to fuse multiple TF ops to TFLite fused RNN & LSTM
        # ops (e.g. LSTM, UnidirectionalSequentceLSTM) with some semantic
        # changes (becoming stateful). We have no intention to make these
        # work in Flex.
        return [
            "lstm",
            "unidirectional_sequence_lstm",
            "unidirectional_sequence_rnn",
        ]
    elif conversion_mode == "forward-compat":
        return [
            "merged_models",  # b/150647401
        ]
    return [
        "merged_models",  # b/150647401
    ]

def generated_test_models_successful(conversion_mode):
    """Returns the list of successful test models.

    Args:
      conversion_mode: Conversion mode.

    Returns:
      List of successful test models for the conversion mode.
    """
    return [test_model for test_model in generated_test_models() if test_model not in generated_test_models_failing(conversion_mode)]

def generated_test_conversion_modes():
    """Returns a list of conversion modes."""

    return ["toco-flex", "forward-compat", "", "mlir-quant"]

def common_test_args_for_generated_models(conversion_mode, failing):
    """Returns test args for generated model tests.

    Args:
      conversion_mode: Conversion mode.
      failing: True if the generated model test is failing.

    Returns:
      test args of generated models.
    """
    args = []

    # Flex conversion shouldn't suffer from the same conversion bugs
    # listed for the default TFLite kernel backend.
    if conversion_mode == "toco-flex":
        args.append("--ignore_known_bugs=false")

    return args

def common_test_tags_for_generated_models(conversion_mode, failing):
    """Returns test tags for generated model tests.

    Args:
      conversion_mode: Conversion mode.
      failing: True if the generated model test is failing.

    Returns:
      tags for the failing generated model tests.
    """
    tags = []

    # Forward-compat coverage testing is largely redundant, and contributes
    # to coverage test bloat.
    if conversion_mode == "forward-compat":
        tags.append("nozapfhahn")

    if failing:
        return ["notap", "manual"]

    return tags

def generated_test_models_all():
    """Generates a list of all tests with the different converters.

    Returns:
      List of tuples representing:
            (conversion mode, name of test, test tags, test args).
    """
    conversion_modes = generated_test_conversion_modes()
    tests = generated_test_models()
    options = []
    for conversion_mode in conversion_modes:
        failing_tests = generated_test_models_failing(conversion_mode)
        for test in tests:
            failing = test in failing_tests
            if conversion_mode:
                test += "_%s" % conversion_mode
            tags = common_test_tags_for_generated_models(conversion_mode, failing)
            args = common_test_args_for_generated_models(conversion_mode, failing)
            options.append((conversion_mode, test, tags, args))
    return options

def merged_test_model_name():
    """Returns the name of merged test model.

    Returns:
      The name of merged test model.
    """
    return "merged_models"

def max_number_of_test_models_in_merged_zip():
    """Returns the maximum number of merged test models in a zip file.

    Returns:
      Maximum number of merged test models in a zip file.
    """
    return 15

def number_of_merged_zip_file(conversion_mode):
    """Returns the number of merged zip file targets.

    Returns:
      Number of merged zip file targets.
    """
    m = max_number_of_test_models_in_merged_zip()
    return (len(generated_test_models_successful(conversion_mode)) + m - 1) // m

def merged_test_models():
    """Generates a list of merged tests with the different converters.

    This model list should be referred only if :generate_examples supports
    --no_tests_limit and --test_sets flags.

    Returns:
      List of tuples representing:
            (conversion mode, name of group, test tags, test args).
    """
    conversion_modes = generated_test_conversion_modes()
    tests = generated_test_models()
    options = []
    for conversion_mode in conversion_modes:
        test = merged_test_model_name()
        if conversion_mode:
            test += "_%s" % conversion_mode
        successful_tests = generated_test_models_successful(conversion_mode)
        if len(successful_tests) > 0:
            tags = common_test_tags_for_generated_models(conversion_mode, False)

            # Only non-merged tests are executed on TAP.
            # Merged test rules are only for running on the real device environment.
            if "notap" not in tags:
                tags.append("notap")
            args = common_test_args_for_generated_models(conversion_mode, False)
            n = number_of_merged_zip_file(conversion_mode)
            for i in range(n):
                test_i = "%s_%d" % (test, i)
                options.append((conversion_mode, test_i, tags, args))
    return options

def flags_for_merged_test_models(test_name, conversion_mode):
    """Returns flags for generating zipped-example data file for merged tests.

    Args:
      test_name: str. Test name in the form of "<merged_model_name>_[<conversion_mode>_]%d".
      conversion_mode: str. Which conversion mode to run with. Comes from the
        list above.

    Returns:
      Flags for generating zipped-example data file for merged tests.
    """
    prefix = merged_test_model_name() + "_"
    if not test_name.startswith(prefix):
        fail(msg = "Invalid test name " + test_name + ": test name should start " +
                   "with " + prefix + " when using flags of merged test models.")

    # Remove prefix and conversion_mode from the test name
    # to extract merged test index number.
    index_string = test_name[len(prefix):]
    if conversion_mode:
        index_string = index_string.replace("%s_" % conversion_mode, "")

    # If the maximum number of test models in a file is 15 and the number of
    # successful test models are 62, 5 zip files will be generated.
    # To assign the test models fairly among these files, each zip file
    # should contain 12 or 13 test models. (62 / 5 = 12 ... 2)
    # Each zip file will have 12 test models and the first 2 zip files will have
    # 1 more test model each, resulting [13, 13, 12, 12, 12] assignment.
    # So Zip file 0, 1, 2, 3, 4 and 5 will have model[0:13], model[13:26],
    # model[26,38], model[38,50] and model[50,62], respectively.
    zip_index = int(index_string)
    num_merged_zips = number_of_merged_zip_file(conversion_mode)
    test_models = generated_test_models_successful(conversion_mode)

    # Each zip file has (models_per_zip) or (models_per_zip+1) test models.
    models_per_zip = len(test_models) // num_merged_zips

    # First (models_remaining) zip files have (models_per_zip+1) test models each.
    models_remaining = len(test_models) % num_merged_zips
    if zip_index < models_remaining:
        # Zip files [0:models_remaining] have (models_per_zip+1) models.
        begin = (models_per_zip + 1) * zip_index
        end = begin + (models_per_zip + 1)
    else:
        # Zip files [models_remaining:] have (models_per_zip) models.
        begin = models_per_zip * zip_index + models_remaining
        end = begin + models_per_zip
    tests_csv = ""
    for test_model in test_models[begin:end]:
        tests_csv += "%s," % test_model
    if tests_csv != "":
        tests_csv = tests_csv[:-1]  # Remove trailing comma.
    return " --no_tests_limit --test_sets=%s" % tests_csv

def gen_zip_test(
        name,
        test_name,
        conversion_mode,
        test_tags,
        test_args,
        additional_test_tags_args = {},
        **kwargs):
    """Generate a zipped-example test and its dependent zip files.

    Args:
      name: str. Resulting cc_test target name
      test_name: str. Test targets this model. Comes from the list above.
      conversion_mode: str. Which conversion mode to run with. Comes from the
        list above.
      test_tags: tags for the generated cc_test.
      test_args: the basic cc_test args to be used.
      additional_test_tags_args: a dictionary of additional test tags and args
        to be used together with test_tags and test_args. The key is an
        identifier which can be in creating a test tag to identify a set of
        tests. The value is a tuple of list of additional test tags and args to
        be used.
      **kwargs: tf_cc_test kwargs
    """
    toco = "//tensorflow/lite/toco:toco"
    flags = ""
    if conversion_mode == "toco-flex":
        flags += " --ignore_converter_errors --run_with_flex"
    elif conversion_mode == "forward-compat":
        flags += " --make_forward_compat_test"
    elif conversion_mode == "mlir-quant":
        flags += " --mlir_quantizer"
    if test_name.startswith(merged_test_model_name() + "_"):
        flags += flags_for_merged_test_models(test_name, conversion_mode)

    gen_zipped_test_file(
        name = "zip_%s" % test_name,
        file = "%s.zip" % test_name,
        toco = toco,
        flags = flags + " --save_graphdefs",
    )
    tf_cc_test(
        name,
        args = test_args,
        tags = test_tags + ["gen_zip_test"],
        **kwargs
    )
    for key, value in additional_test_tags_args.items():
        extra_tags, extra_args = value
        extra_tags.append("gen_zip_test_%s" % key)
        tf_cc_test(
            name = "%s_%s" % (name, key),
            args = test_args + extra_args,
            tags = test_tags + extra_tags,
            **kwargs
        )

def gen_zipped_test_file(name, file, toco, flags):
    """Generate a zip file of tests by using :generate_examples.

    Args:
      name: str. Name of output. We will produce "`file`.files" as a target.
      file: str. The name of one of the generated_examples targets, e.g. "transpose"
      toco: str. Pathname of toco binary to run
      flags: str. Any additional flags to include
    """
    native.genrule(
        name = file + ".files",
        cmd = (("$(locations :generate_examples) --toco $(locations {0}) " +
                " --zip_to_output {1} {2} $(@D)").format(toco, file, flags)),
        outs = [file],
        tools = [
            ":generate_examples",
            toco,
        ],
    )

    native.filegroup(
        name = name,
        srcs = [file],
    )

def gen_selected_ops(name, model, namespace = "", **kwargs):
    """Generate the library that includes only used ops.

    Args:
      name: Name of the generated library.
      model: TFLite models to interpret, expect a list in case of multiple models.
      namespace: Namespace in which to put RegisterSelectedOps.
      **kwargs: Additional kwargs to pass to genrule.
    """
    out = name + "_registration.cc"
    tool = clean_dep("//tensorflow/lite/tools:generate_op_registrations")
    tflite_path = "//tensorflow/lite"

    # isinstance is not supported in skylark.
    if type(model) != type([]):
        model = [model]

    input_models_args = " --input_models=%s" % ",".join(
        ["$(location %s)" % f for f in model],
    )

    native.genrule(
        name = name,
        srcs = model,
        outs = [out],
        cmd = ("$(location %s) --namespace=%s --output_registration=$(location %s) --tflite_path=%s %s") %
              (tool, namespace, out, tflite_path[2:], input_models_args),
        tools = [tool],
        **kwargs
    )

def flex_dep(target_op_sets):
    if "SELECT_TF_OPS" in target_op_sets:
        return ["//tensorflow/lite/delegates/flex:delegate"]
    else:
        return []

def gen_model_coverage_test(src, model_name, data, failure_type, tags, size = "medium"):
    """Generates Python test targets for testing TFLite models.

    Args:
      src: Main source file.
      model_name: Name of the model to test (must be also listed in the 'data'
        dependencies)
      data: List of BUILD targets linking the data.
      failure_type: List of failure types (none, toco, crash, inference, evaluation)
        expected for the corresponding combinations of op sets
        ("TFLITE_BUILTINS", "TFLITE_BUILTINS,SELECT_TF_OPS", "SELECT_TF_OPS").
      tags: List of strings of additional tags.
    """
    i = 0
    for target_op_sets in ["TFLITE_BUILTINS", "TFLITE_BUILTINS,SELECT_TF_OPS", "SELECT_TF_OPS"]:
        args = []
        if failure_type[i] != "none":
            args.append("--failure_type=%s" % failure_type[i])
        i = i + 1

        # Avoid coverage timeouts for large/enormous tests.
        coverage_tags = ["nozapfhahn"] if size in ["large", "enormous"] else []
        native.py_test(
            name = "model_coverage_test_%s_%s" % (model_name, target_op_sets.lower().replace(",", "_")),
            srcs = [src],
            main = src,
            size = size,
            args = [
                "--model_name=%s" % model_name,
                "--target_ops=%s" % target_op_sets,
            ] + args,
            data = data,
            srcs_version = "PY3",
            python_version = "PY3",
            tags = [
                "no_gpu",  # Executing with TF GPU configurations is redundant.
                "no_oss",
                "no_windows",
            ] + tags + coverage_tags,
            deps = [
                "//third_party/py/tensorflow",
                "//tensorflow/lite/testing/model_coverage:model_coverage_lib",
                "//tensorflow/lite/python:lite",
                "//tensorflow/python:client_testlib",
            ] + flex_dep(target_op_sets),
        )

def tflite_custom_cc_library(
        name,
        models = [],
        srcs = [],
        deps = [],
        visibility = ["//visibility:private"]):
    """Generates a tflite cc library, stripping off unused operators.

    This library includes the TfLite runtime as well as all operators needed for the given models.
    Op resolver can be retrieved using tflite::CreateOpResolver method.

    Args:
        name: Str, name of the target.
        models: List of models. This TFLite build will only include
            operators used in these models. If the list is empty, all builtin
            operators are included.
        srcs: List of files implementing custom operators if any.
        deps: Additional dependencies to build all the custom operators.
        visibility: Visibility setting for the generated target. Default to private.
    """
    real_srcs = []
    real_srcs.extend(srcs)
    real_deps = []
    real_deps.extend(deps)

    if models:
        gen_selected_ops(
            name = "%s_registration" % name,
            model = models,
        )
        real_srcs.append(":%s_registration" % name)
        real_srcs.append("//tensorflow/lite:create_op_resolver_with_selected_ops.cc")
    else:
        # Support all operators if `models` not specified.
        real_deps.append("//tensorflow/lite:create_op_resolver_with_builtin_ops")

    native.cc_library(
        name = name,
        srcs = real_srcs,
        hdrs = [
            "//tensorflow/lite:create_op_resolver.h",
        ],
        copts = tflite_copts(),
        linkopts = select({
            "//tensorflow:windows": [],
            "//conditions:default": ["-lm", "-ldl"],
        }),
        deps = depset([
            "//tensorflow/lite:framework",
            "//tensorflow/lite/kernels:builtin_ops",
        ] + real_deps),
        visibility = visibility,
    )

def tflite_custom_android_library(
        name,
        models = [],
        srcs = [],
        deps = [],
        custom_package = "org.tensorflow.lite",
        visibility = ["//visibility:private"],
        include_xnnpack_delegate = True,
        include_nnapi_delegate = True):
    """Generates a tflite Android library, stripping off unused operators.

    Note that due to a limitation in the JNI Java wrapper, the compiled TfLite shared binary
    has to be named as tensorflowlite_jni.so so please make sure that there is no naming conflict.
    i.e. you can't call this rule multiple times in the same build file.

    Args:
        name: Name of the target.
        models: List of models to be supported. This TFLite build will only include
            operators used in these models. If the list is empty, all builtin
            operators are included.
        srcs: List of files implementing custom operators if any.
        deps: Additional dependencies to build all the custom operators.
        custom_package: Name of the Java package. It is required by android_library in case
            the Java source file can't be inferred from the directory where this rule is used.
        visibility: Visibility setting for the generated target. Default to private.
        include_xnnpack_delegate: Whether to include the XNNPACK delegate or not.
        include_nnapi_delegate: Whether to include the NNAPI delegate or not.
    """
    tflite_custom_cc_library(name = "%s_cc" % name, models = models, srcs = srcs, deps = deps, visibility = visibility)

    delegate_deps = []
    if include_nnapi_delegate:
        delegate_deps.append("//tensorflow/lite/delegates/nnapi/java/src/main/native")
    if include_xnnpack_delegate:
        delegate_deps.append("//tensorflow/lite/delegates/xnnpack:xnnpack_delegate")

    # JNI wrapper expects a binary file called `libtensorflowlite_jni.so` in java path.
    tflite_jni_binary(
        name = "libtensorflowlite_jni.so",
        linkscript = "//tensorflow/lite/java:tflite_version_script.lds",
        # Do not sort: "native_framework_only" must come before custom tflite library.
        deps = [
            "//tensorflow/lite/java/src/main/native:native_framework_only",
            ":%s_cc" % name,
        ] + delegate_deps,
    )

    native.cc_library(
        name = "%s_jni" % name,
        srcs = ["libtensorflowlite_jni.so"],
        visibility = visibility,
    )

    android_library(
        name = name,
        manifest = "//tensorflow/lite/java:AndroidManifest.xml",
        srcs = ["//tensorflow/lite/java:java_srcs"],
        deps = [
            ":%s_jni" % name,
            "@org_checkerframework_qual",
        ],
        custom_package = custom_package,
        visibility = visibility,
    )

    aar_with_jni(
        name = "%s_aar" % name,
        android_library = name,
    )

def tflite_custom_c_library(
        name,
        models = [],
        **kwargs):
    """Generates a tflite cc library, stripping off unused operators.

    This library includes the C API and the op kernels used in the given models.

    Args:
        name: Str, name of the target.
        models: List of models. This TFLite build will only include
            operators used in these models. If the list is empty, all builtin
            operators are included.
       **kwargs: custom c_api cc_library kwargs.
    """
    op_resolver_deps = "//tensorflow/lite:create_op_resolver_with_builtin_ops"
    if models:
        gen_selected_ops(
            name = "%s_registration" % name,
            model = models,
        )

        native.cc_library(
            name = "%s_create_op_resolver" % name,
            srcs = [
                ":%s_registration" % name,
                "//tensorflow/lite:create_op_resolver_with_selected_ops.cc",
            ],
            hdrs = ["//tensorflow/lite:create_op_resolver.h"],
            copts = tflite_copts(),
            deps = [
                "//tensorflow/lite:op_resolver",
                "//tensorflow/lite:framework",
                "//tensorflow/lite/kernels:builtin_ops",
            ],
        )
        op_resolver_deps = "%s_create_op_resolver" % name

    native.cc_library(
        name = name,
        srcs = ["//tensorflow/lite/c:c_api_srcs"],
        hdrs = ["//tensorflow/lite/c:c_api.h"],
        copts = tflite_copts(),
        deps = [
            op_resolver_deps,
            "//tensorflow/lite/c:common",
            "//tensorflow/lite/c:c_api_types",
            "//tensorflow/lite:builtin_ops",
            "//tensorflow/lite:framework",
            "//tensorflow/lite:version",
            "//tensorflow/lite/core/api",
            "//tensorflow/lite/delegates:interpreter_utils",
            "//tensorflow/lite/delegates/nnapi:nnapi_delegate",
            "//tensorflow/lite/kernels/internal:compatibility",
        ],
        **kwargs
    )

def tflite_combine_cc_tests(
        name,
        deps_conditions,
        extra_cc_test_tags = [],
        extra_build_test_tags = [],
        **kwargs):
    """Combine all certain cc_tests into a single cc_test and a build_test.

    Args:
      name: the name of the combined cc_test.
      deps_conditions: the list of deps that those cc_tests need to have in
          order to be combined.
      extra_cc_test_tags: the list of extra tags appended to the created
          combined cc_test.
      extra_build_test_tags: the list of extra tags appended to the created
          corresponding build_test for the combined cc_test.
      **kwargs: kwargs to pass to the cc_test rule of the test suite.
    """
    combined_test_srcs = {}
    combined_test_deps = {}
    for r in native.existing_rules().values():
        # We only include cc_test.
        if not r["kind"] == "cc_test":
            continue

        # Tests with data, args or special build option are not counted.
        if r["data"] or r["args"] or r["copts"] or r["defines"] or \
           r["includes"] or r["linkopts"] or r["additional_linker_inputs"]:
            continue

        # We only consider a single-src-file unit test.
        if len(r["srcs"]) > 1:
            continue

        dep_attr = r["deps"]
        if type(dep_attr) != type(()) and type(dep_attr) != type([]):
            # Attributes based on select() is not supported for simplicity.
            continue

        # The test has to depend on :test_main
        if not any([v in deps_conditions for v in dep_attr]):
            continue

        combined_test_srcs.update({s: True for s in r["srcs"]})
        combined_test_deps.update({d: True for d in r["deps"]})

    if combined_test_srcs:
        native.cc_test(
            name = name,
            size = "large",
            srcs = list(combined_test_srcs),
            tags = ["manual", "notap"] + extra_cc_test_tags,
            deps = list(combined_test_deps),
            **kwargs
        )
        build_test(
            name = "%s_build_test" % name,
            targets = [":%s" % name],
            tags = [
                "manual",
                "tflite_portable_build_test",
            ] + extra_build_test_tags,
        )
