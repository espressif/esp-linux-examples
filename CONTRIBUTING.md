# Contributing

## Scope

This repository targets ESP SoCs running Linux. Keep shared documentation and
examples platform-independent where possible, and state hardware-specific
requirements in the relevant guide or example.

## License

Documentation and repository infrastructure are licensed under the Apache
License 2.0. Example source code and build files are licensed under CC0-1.0
unless a file explicitly states otherwise. Add the corresponding SPDX license
identifier to each new source and build file.

## Adding documentation

1. Place English documentation under the appropriate section in `docs/en/`.
2. Add each new page to the relevant Sphinx `toctree`.
3. Build the documentation locally with `build-docs -l en build` from the
   `docs/` directory and resolve any new warnings.

## Adding an example

1. Place the example under the appropriate category in `examples/`.
2. Keep each merge request focused on one example or one shared documentation
   topic.
3. Include a `README.rst`, source code under `src/`, and a build file.
4. Document prerequisites, build and deployment steps, expected output.
5. Document troubleshooting, this will be very useful for users and developers,
   the requirements are described below.
6. Add the example README to the Sphinx documentation through a wrapper page
   under `docs/en/examples/`.
7. Link to shared procedures under `docs/` instead of copying them.
8. Do not commit generated binaries or build directories.

## Troubleshooting and debugging

Examples should demonstrate not only how to use a feature, but also how to
analyse issues. Linux features commonly involve userspace APIs, one or more
kernel frameworks or subsystems, and the underlying hardware drivers. Cover the
applicable layers in each example README:

1. **Userspace:** Show how to inspect the application with tools such as GDB,
   core dumps, `strace`, application logs, or `dmesg`.
2. **Framework / Subsystem:** Show how to inspect the standard Linux framework
   state with the relevant tools and interfaces, such as `media-ctl -p`,
   `modetest`, or `drm_info`.
3. **Hardware driver:** Explain how to inspect hardware-specific state exposed
   by the driver, including relevant debugfs nodes, error counters, DMA state,
   or register dumps.

For each applicable diagnostic method, provide the command to run, describe the
important normal and abnormal output, and explain what the result indicates or
what to check next. Link to shared subsystem debugging guides instead of
duplicating common procedures in every example.

Prefer standard framework diagnostics when they are available. Use debugfs for
hardware-specific diagnostics, not as an interface required for normal
operation.

Keep a merge request in Draft until its implementation and validation are
complete.
