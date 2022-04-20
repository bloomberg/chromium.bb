"""
This file contains general helper macros that make our BUILD.bazel files easier to read.
"""

# https://github.com/bazelbuild/bazel-skylib
load("@bazel_skylib//lib:selects.bzl", _selects = "selects")
load("@rules_python//python:defs.bzl", _py_binary = "py_binary")
load("@py_deps//:requirements.bzl", _requirement = "requirement")
load("@bazel_gazelle//:def.bzl", _gazelle = "gazelle")

# re-export symbols that are commonly used or that are not supported in G3
# (and thus we need to stub out)
selects = _selects
py_binary = _py_binary
requirement = _requirement
gazelle = _gazelle

def select_multi(values_map, default, name = ""):
    """select() but allowing multiple matches of the keys.

    select_multi works around a restriction in native select() that prevents multiple
    keys from being matched unless one is a strict subset of another. For some features,
    we allow multiple of that component to be active. For example, with codecs, we let
    the clients mix and match anywhere from 0 built in codecs to all of them.

    select_multi takes a given map and turns it into several distinct select statements
    that have the effect of using any values associated with any active keys.
    For example, if the following parameters are passed in:
        values_map = {
            ":alpha": ["apple", "apricot"],
            ":beta": ["banana"],
            ":gamma": ["grapefruit"],
        },
        default = []
    it will be unrolled into the following select statements
        [] + select({
            ":apple": ["apple", "apricot"],
            "//conditions:default": [],
        }) + select({
            ":beta": ["banana"],
            "//conditions:default": [],
        }) + select({
            ":gamma": ["grapefruit"],
            "//conditions:default": [],
        })

    Args:
        values_map: dictionary of labels to a list of labels, just like select()
        default: list of labels, the value that should be used if any of the options do not match.
            This is typically an empty list
        name: string unused, https://github.com/bazelbuild/buildtools/blob/master/WARNINGS.md#unnamed-macro

    Returns:
        A list of values that is filled in by the generated select statements.
    """
    if len(values_map) == 0:
        return default
    rv = []
    for key, value in values_map.items():
        rv += select({
            key: value,
            "//conditions:default": default,
        })
    return rv

def generated_cc_atom(name, enforce_iwyu = False, **kwargs):
    """A self-annotating label for a generated cc_library for exactly one file.

    Args:
        name: string, the name of the cc_library
        enforce_iwyu: boolean, if true, this file will fail to compile if the headers to not comply
            with the include-what-you-use standards. This does not affect dependencies nor
            dependents, only the file listed in srcs/hdrs.
        **kwargs: All other arguments are passed verbatim to cc_library
    """
    if len(kwargs.get("srcs", [])) > 1 or len(kwargs.get("hdrs", [])) > 1:
        fail("Cannot have more than one src or hdr file in generated_cc_atom")
    if len(kwargs.get("srcs", [])) > 0 and len(kwargs.get("hdrs", [])) > 0:
        fail("Cannot set both srcs and hdrs in generated_cc_atom")
    if len(kwargs.get("srcs", [])) == 0 and len(kwargs.get("hdrs", [])) == 0:
        fail("Must set exactly one of srcs or hdrs in generated_cc_atom")
    deps = kwargs.get("deps", [])
    deps.append("//bazel:defines_from_flags")
    kwargs["deps"] = deps

    features = kwargs.get("features", [])
    if enforce_iwyu:
        features.append("skia_opt_file_into_iwyu")
    native.cc_library(
        name = name,
        features = features,
        **kwargs
    )

# buildifier: disable=unnamed-macro
# buildifier: disable=native-package
def enforce_iwyu_on_package():
    """A self-annotating macro to set force_iwyu = True on all rules in this package."""
    native.package(features = ["skia_opt_file_into_iwyu"])
