# The xrlinux Boot Protocol

## Binary Format

Kernel images are flat binaries with the following header. All fields are little-endian.

| Offset | Size | Name         | Description                                                                       |
|--------|------|--------------|-----------------------------------------------------------------------------------|
| 0x00   | 4    | Magic        | Magic number identifying this file as an xrlinux kernel image. Set to 0x584c5258. |
| 0x04   | 2    | MinorVersion | The minor version of the protocol. This document describes minor version 0.       |
| 0x06   | 2    | MajorVersion | The major version of the protocol. This document describes major version 0.       |
| 0x08   | 4    | Entry        | Offset between this header and the entry point.                                   |
| 0x0c   | 4    | MSize        | Number of bytes in memory that the kernel image occupies, including BSS.          |

When the protocol is changed in a backwards-compatible way, `MinorVersion` is incremented without changing
`MajorVersion`.

When the protocol is changed in a way that breaks backwards compatibility, `MajorVersion` is incremented and
`MinorVersion` is reset to 0.

The bootloader will allocate a page-aligned region of `MSize` bytes, and load the kernel image into it. Any space
between the end of the kernel image and the end of the region is filled with zeroes.

## Machine State

On entry, the machine state is as follows:
- `RS`: `T` is 0, `M` is 0, `I` is 0, `U` is 0. All other bits are in an undefined state.
- `PC`: Set to the address the kernel image was loaded at, offset by `Header.Entry`.
- `A0`: Same as `PC`.
- `A1`: Set to the physical address of the device tree blob.
- `A2`: Set to the total number of CPUs that the kernel image will be executed on.
- `A3`: Set to the protocol minor version supported by the bootloader.

Anything not mentioned is in an undefined state, including `SP` and the contents of the TBs.

On a multiprocessor system, all processors start the kernel with the machine state described above.

> [!CAUTION]
> Some processors may have jumped to the kernel while others are still running bootloader or firmware code.
> Therefore, the kernel must keep track of how many processors have jumped to its entrypoint. Until this number matches
> the total number of CPUs (passed in `A2`), the only areas of memory that the kernel is allowed to access are its own
> kernel image (the area from `PC - Header.Entry` to `PC - Header.Entry + Header.MSize`) and the device tree blob.
> MMIO is also not allowed to be accessed while in this state.
