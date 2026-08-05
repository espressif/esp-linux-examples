# Hello World

`hello-world` is a minimal Linux userspace application for ESP SoCs.

## Prerequisites

- An ESP SoC development board capable of booting Linux.
- An SD card.
- A RISC-V Linux cross-compiler compatible with the target root filesystem.

## Example Folder Contents

The example contains one C source file under `src/` and uses a Makefile to build the `hello-world` executable.

```text
├── kconfig.fragment        Optional Linux kernel configuration requirements
├── Makefile                Build instructions
├── README.md               This file
└── src
    └── hello_world.c       Application source
```

## Build

Ensure that a RISC-V Linux cross-compiler is installed. Ensure the compiler `bin` directory has been added to `PATH`, or pass the complete compiler prefix to `make` manually:

```bash
make CROSS_COMPILE=/path/to/toolchain/bin/riscv64-esp-linux-gnu-
```

Run `make clean` before rebuilding with a different `CROSS_COMPILE` value.
The executable is written to `build/hello-world`.

## Deploy

### Using an SD Card

Insert the Linux SD card into the host and identify its root filesystem partition:

```bash
lsblk -f
```

Select the Linux root filesystem partition, normally the ext4 partition labelled `rootfs`. Do not select the FAT boot partition.

Create a mount point and mount the root filesystem partition:

```bash
sudo mkdir -p /mnt/rootfs
sudo mount /dev/sdX2 /mnt/rootfs
```

Replace `/dev/sdX2` with the root filesystem partition reported by `lsblk -f`. If the desktop has already mounted the partition, either use its displayed mount point or unmount it before mounting it at `/mnt/rootfs`.

Copy the executable into the SD card root filesystem and set its executable permissions:

```bash
sudo install -m 0755 build/hello-world \
    /mnt/rootfs/usr/bin/hello-world
```

Flush pending writes and safely unmount the partition:

```bash
sync
sudo umount /mnt/rootfs
```

Insert the SD card into the target and boot Linux. Run:

```bash
hello-world
```

## Example Output

```text
# hello-world
Hello, ESP Linux!
```

## Kernel Configuration Fragment

The included `kconfig.fragment` shows where an example can declare additional kernel configuration requirements. This example does not require any additional kernel options, so the fragment contains no `CONFIG_` entries.

To merge a fragment when working directly with the Linux kernel:

```bash
ARCH=riscv scripts/kconfig/merge_config.sh \
    -O <build-dir> \
    <base-config> \
    <config-fragment>
```

## Troubleshooting

- Permission denied when running: Check that the installed executable has execute permission with `ls -l /usr/bin/hello-world`.