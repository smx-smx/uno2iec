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

cc_library(
    name = "drive_interface",
    hdrs = [
        "drive_interface.h",
    ],
    deps = [
        ":utils",
    ],
)

cc_library(
    name = "cbm1541_drive",
    srcs = [
        "cbm1541_drive.cc",
        "//assembly:format_h",
        "//assembly:write_block_h",
    ],
    hdrs = [
        "cbm1541_drive.h",
    ],
    deps = [
        ":drive_interface",
        ":iec_host_lib",
        ":utils",
        "@boost//:format",
    ],
)

cc_test(
    name = "cbm1541_drive_test",
    srcs = [
        "cbm1541_drive_test.cc",
    ],
    deps = [
        ":cbm1541_drive",
        ":iec_host_lib",
        "@boost//:format",
        "@com_github_google_googletest//:gtest_main",
    ],
)

# A tool to copy a 1541 floppy disc to a .d64 image and vice
# versa using the IEC host library.
cc_binary(
    name = "disccopy",
    srcs = [
        "disccopy.cc",
        "//assembly:format_h",
        "//assembly:write_block_h",
    ],
    linkopts = ["-lpthread"],
    deps = [
        ":cbm1541_drive",
        ":drive_interface",
        ":iec_host_lib",
        "@boost//:format",
        "@boost//:program_options",
    ],
)
