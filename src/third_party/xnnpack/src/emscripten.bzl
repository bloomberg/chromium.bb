"""Emscripten-specific build definitions for XNNPACK."""

def xnnpack_emscripten_minimal_linkopts():
    """Minimal Emscripten-specific linkopts for binaries."""
    return [
        "-s ASSERTIONS=0",
        "-s ENVIRONMENT=node,shell,web",
        "-s ERROR_ON_UNDEFINED_SYMBOLS=1",
        "-s EXIT_RUNTIME=1",
    ]

def xnnpack_emscripten_test_linkopts():
    """Emscripten-specific linkopts for unit tests."""
    return [
        "-s ASSERTIONS=2",
        "-s ENVIRONMENT=node,shell,web",
        "-s ERROR_ON_UNDEFINED_SYMBOLS=1",
        "-s DEMANGLE_SUPPORT=1",
        "-s EXIT_RUNTIME=1",
        "-s ALLOW_MEMORY_GROWTH=1",
        "--pre-js $(location :preamble.js.lds)",
    ]

def xnnpack_emscripten_benchmark_linkopts():
    """Emscripten-specific linkopts for benchmarks."""
    return [
        "-s ASSERTIONS=1",
        "-s ENVIRONMENT=node,shell,web",
        "-s ERROR_ON_UNDEFINED_SYMBOLS=1",
        "-s EXIT_RUNTIME=1",
        "-s ALLOW_MEMORY_GROWTH=1",
        "-s TOTAL_MEMORY=445644800",  # 425M
        "--pre-js $(location :preamble.js.lds)",
    ]

def xnnpack_emscripten_deps():
    """Emscripten-specific dependencies for unit tests and benchmarks."""
    return [
        ":preamble.js.lds",
    ]
