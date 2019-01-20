# Bazel BUILD file for the commandline utility.
# The code built by these rules acts as a host and will try to
# talk to a 1541 floppy disc drive or some other IEC bus device.

cc_library(
    name = "iec_host_lib",
    srcs = [
        "iec_host_lib.cc",
    ],
    hdrs = [
        "iec_host_lib.h",
    ],
    deps = [
        ":utils",
        "@boost//:format",
    ],
)

cc_test(
    name = "iec_host_lib_test",
    srcs = [
        "iec_host_lib_test.cc",
    ],
    deps = [
        ":iec_host_lib",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "utils",
    srcs = [
        "utils.cc",
    ],
    hdrs = [
        "utils.h",
    ],
    deps = [
        "@boost//:format",
    ],
)

cc_test(
    name = "utils_test",
    srcs = [
        "utils_test.cc",
    ],
    deps = [
        ":utils",
        "@com_github_google_googletest//:gtest_main",
    ],
)

# A tool to copy a 1541 floppy disc to a .d64 image and vice
# versa using the IEC host library.
cc_binary(
    name = "disccopy",
    linkopts = [ "-lpthread" ],
    srcs = [
        "disccopy.cc",
        "//assembly:format_h",
    ],
    deps = [
        ":iec_host_lib",
        "@boost//:format",
        "@boost//:program_options",
    ],
)
