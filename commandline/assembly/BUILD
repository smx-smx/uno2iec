load(':acme.bzl', 'acme_binary')
load(':acme.bzl', 'bin_array')

package(default_visibility = ["//visibility:public"])

acme_binary(
    name = "format",
    format = "plain",
    srcs = [
        "format.asm"
    ],
    includes = [
    	"definitions.asm",
    ],
)

cc_binary(
    name = "bin_to_array",
    srcs = [
        "bin_to_array.cc",
    ],
    deps = [
        "@boost//:format",
    ],    
)

bin_array(
    name = "format_h",
    file = ":format",
    symbol = "format_bin",
)
