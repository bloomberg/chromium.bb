"""loads the jpeg library, used by TF."""

load("//third_party:repo.bzl", "tf_http_archive")

def repo():
    tf_http_archive(
        name = "libjpeg_turbo",
        urls = [
            "https://storage.googleapis.com/mirror.tensorflow.org/github.com/libjpeg-turbo/libjpeg-turbo/archive/2.1.0.tar.gz",
            "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/2.1.0.tar.gz",
        ],
        sha256 = "d6b7790927d658108dfd3bee2f0c66a2924c51ee7f9dc930f62c452f4a638c52",
        strip_prefix = "libjpeg-turbo-2.1.0",
        build_file = "//third_party/jpeg:BUILD.bazel",
        system_build_file = "//third_party/jpeg:BUILD.system",
    )
