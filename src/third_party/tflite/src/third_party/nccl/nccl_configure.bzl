"""Repository rule for NCCL configuration.

`nccl_configure` depends on the following environment variables:

  * `TF_NCCL_VERSION`: Installed NCCL version or empty to build from source.
  * `NCCL_INSTALL_PATH` (deprecated): The installation path of the NCCL library.
  * `NCCL_HDR_PATH` (deprecated): The installation path of the NCCL header 
    files.
  * `TF_CUDA_PATHS`: The base paths to look for CUDA and cuDNN. Default is
    `/usr/local/cuda,usr/`.

"""

load(
    "//third_party/gpus:cuda_configure.bzl",
    "enable_cuda",
    "find_cuda_config",
)
load(
    "//third_party/remote_config:common.bzl",
    "config_repo_label",
    "get_cpu_value",
    "get_host_environ",
)

_CUDA_TOOLKIT_PATH = "CUDA_TOOLKIT_PATH"
_NCCL_HDR_PATH = "NCCL_HDR_PATH"
_NCCL_INSTALL_PATH = "NCCL_INSTALL_PATH"
_TF_CUDA_COMPUTE_CAPABILITIES = "TF_CUDA_COMPUTE_CAPABILITIES"
_TF_NCCL_VERSION = "TF_NCCL_VERSION"
_TF_NEED_CUDA = "TF_NEED_CUDA"

_DEFINE_NCCL_MAJOR = "#define NCCL_MAJOR"
_DEFINE_NCCL_MINOR = "#define NCCL_MINOR"
_DEFINE_NCCL_PATCH = "#define NCCL_PATCH"

_NCCL_DUMMY_BUILD_CONTENT = """
filegroup(
  name = "LICENSE",
  visibility = ["//visibility:public"],
)

cc_library(
  name = "nccl",
  visibility = ["//visibility:public"],
)
"""

_NCCL_ARCHIVE_BUILD_CONTENT = """
filegroup(
  name = "LICENSE",
  data = ["@nccl_archive//:LICENSE.txt"],
  visibility = ["//visibility:public"],
)

alias(
  name = "nccl",
  actual = "@nccl_archive//:nccl",
  visibility = ["//visibility:public"],
)
"""

def _label(file):
    return Label("//third_party/nccl:{}".format(file))

def _create_local_nccl_repository(repository_ctx):
    # Resolve all labels before doing any real work. Resolving causes the
    # function to be restarted with all previous state being lost. This
    # can easily lead to a O(n^2) runtime in the number of labels.
    # See https://github.com/tensorflow/tensorflow/commit/62bd3534525a036f07d9851b3199d68212904778
    find_cuda_config_path = repository_ctx.path(Label("@org_tensorflow//third_party/gpus:find_cuda_config.py.gz.base64"))

    nccl_version = get_host_environ(repository_ctx, _TF_NCCL_VERSION, "")
    if nccl_version:
        nccl_version = nccl_version.split(".")[0]

    cuda_config = find_cuda_config(repository_ctx, find_cuda_config_path, ["cuda"])
    cuda_version = cuda_config["cuda_version"].split(".")
    cuda_major = cuda_version[0]
    cuda_minor = cuda_version[1]

    if nccl_version == "":
        # Alias to open source build from @nccl_archive.
        repository_ctx.file("BUILD", _NCCL_ARCHIVE_BUILD_CONTENT)

        config_wrap = {
            "%{use_bin2c_path}": "False",
        }
        if (int(cuda_major), int(cuda_minor)) <= (10, 1):
            config_wrap["%{use_bin2c_path}"] = "True"

        repository_ctx.template(
            "build_defs.bzl",
            _label("build_defs.bzl.tpl"),
            config_wrap,
        )
    else:
        # Create target for locally installed NCCL.
        config = find_cuda_config(repository_ctx, find_cuda_config_path, ["nccl"])
        config_wrap = {
            "%{nccl_version}": config["nccl_version"],
            "%{nccl_header_dir}": config["nccl_include_dir"],
            "%{nccl_library_dir}": config["nccl_library_dir"],
        }
        repository_ctx.template("BUILD", _label("system.BUILD.tpl"), config_wrap)

def _create_remote_nccl_repository(repository_ctx, remote_config_repo):
    repository_ctx.template(
        "BUILD",
        config_repo_label(remote_config_repo, ":BUILD"),
        {},
    )

    nccl_version = get_host_environ(repository_ctx, _TF_NCCL_VERSION, "")
    if nccl_version == "":
        repository_ctx.template(
            "build_defs.bzl",
            config_repo_label(remote_config_repo, ":build_defs.bzl"),
            {},
        )

def _nccl_autoconf_impl(repository_ctx):
    if (not enable_cuda(repository_ctx) or
        get_cpu_value(repository_ctx) not in ("Linux", "FreeBSD")):
        # Add a dummy build file to make bazel query happy.
        repository_ctx.file("BUILD", _NCCL_DUMMY_BUILD_CONTENT)
    elif get_host_environ(repository_ctx, "TF_NCCL_CONFIG_REPO") != None:
        _create_remote_nccl_repository(repository_ctx, get_host_environ(repository_ctx, "TF_NCCL_CONFIG_REPO"))
    else:
        _create_local_nccl_repository(repository_ctx)

_ENVIRONS = [
    _CUDA_TOOLKIT_PATH,
    _NCCL_HDR_PATH,
    _NCCL_INSTALL_PATH,
    _TF_NCCL_VERSION,
    _TF_CUDA_COMPUTE_CAPABILITIES,
    _TF_NEED_CUDA,
    "TF_CUDA_PATHS",
]

remote_nccl_configure = repository_rule(
    implementation = _create_local_nccl_repository,
    environ = _ENVIRONS,
    remotable = True,
    attrs = {
        "environ": attr.string_dict(),
    },
)

nccl_configure = repository_rule(
    implementation = _nccl_autoconf_impl,
    environ = _ENVIRONS,
)
"""Detects and configures the NCCL configuration.

Add the following to your WORKSPACE FILE:

```python
nccl_configure(name = "local_config_nccl")
```

Args:
  name: A unique name for this workspace rule.
"""
