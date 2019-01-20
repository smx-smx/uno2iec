# Bazel rules to build assembly code using the ACME cross assembler.
# TODO(aeckleder): This rule is non-hermetic and expects a local
# installation of the ACME cross assembler in a specific path.
# Write a proper hermetic rule for this, which compiles acme from source.

def _acme_binary_impl(ctx):
    output = ctx.outputs.out
    ctx.actions.run_shell(
        inputs = ctx.files.srcs,
        outputs = [output],
        command = "/home/weirdsoul/bin/acme -f %s -o %s %s" % (
            ctx.attr.format,
            output.path,
            " ".join([i.path for i in ctx.files.srcs]),
        ),
    )

acme_binary = rule(
    attrs = {
        "format": attr.string(default = "plain"),
        "srcs": attr.label_list(allow_files = True),
    },
    outputs = {"out": "%{name}.o"},
    implementation = _acme_binary_impl,
)

def _bin_array_impl(ctx):
    # The list of arguments we pass to the script.
    args = [ctx.attr.symbol] + [ctx.file.file.path] + [ctx.outputs.out.path]

    # Action to call the script.
    ctx.actions.run(
        inputs = [ctx.file.file],
        outputs = [ctx.outputs.out],
        arguments = args,
        executable = ctx.executable._bin_to_array,
    )

bin_array = rule(
    attrs = {
        "symbol": attr.string(mandatory = True),
        "file": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "_bin_to_array": attr.label(
            executable = True,
            cfg = "host",
            allow_files = True,
            default = Label("//assembly:bin_to_array"),
        ),
    },
    outputs = {"out": "%{name}.h"},
    implementation = _bin_array_impl,
)
