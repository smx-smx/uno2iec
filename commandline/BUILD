# Bazel BUILD file for the commandline utility.
# The code built by these rules acts as a host and will try to
# talk to a 1541 floppy disc drive or some other IEC bus device.

cc_library(
    name = "iec_host_lib",
    hdrs = [
      "iec_host_lib.h",
    ],
    srcs = [
      "iec_host_lib.cc",
    ],
)

# A tool to copy a 1541 floppy disc to a .d64 image and vice
# versa using the IEC host library.
cc_binary(
    name = "disccopy",
    srcs = [
      "disccopy.cc",
    ],
    deps = [
      ":iec_host_lib",
      "@boost//:algorithm",
    ],
)
