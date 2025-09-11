# The xrlinux Boot Protocol

## Binary Format

Kernel images are flat binaries with the following header. All fields are little-endian.

| Offset | Size | Name         | Description                                                                       |
|--------|------|--------------|-----------------------------------------------------------------------------------|
| 0x00   | 4    | Magic        | Magic number identifying this file as an xrlinux kernel image. Set to 0x584c5258. |
| 0x04   | 2    | MinorVersion | The minor version of the protocol. This document describes minor version 0.       |
| 0x06   | 2    | MajorVersion | The major version of the protocol. This document describes major version 1.       |
| 0x08   | 4    | VirtualAddr  | Virtual address of this header.                                                   |
| 0x0c   | 4    | Entry        | Virtual address of the kernel entry point.                                        |
| 0x10   | 4    | MSize        | Number of bytes in memory that the kernel image occupies, including BSS.          |

When the protocol is changed in a backwards-compatible way, `MinorVersion` is incremented without changing
`MajorVersion`.

When the protocol is changed in a way that breaks backwards compatibility, `MajorVersion` is incremented and
`MinorVersion` is reset to 0.

## Machine State

On entry, the machine state is as follows:
- Data and instruction caches coherent with main memory.
- `RS`: `M` is 0, `U` is 0. All other bits are in an undefined state.
- `PC`: Set to the physical address corresponding to `Header.Entry`.
- `A0`: Same as `PC`. Note that this cannot be used to access kernel code or data outside the page of the entry point, 
  as the kernel image is not necessarily contiguous in physical memory.
- `A1`: Set to the physical address of the device tree blob.
- `A2`: Set to the total number of CPUs that the kernel image will be executed on.
- `A3`: Set to the protocol minor version supported by the bootloader.
- `S0`: Set to the physical address of the bootloader-provided root page table, as described below.

Anything not mentioned is in an undefined state, including `SP` and the contents of the TBs.

On a multiprocessor system, all processors start the kernel with the machine state described above.

> [!CAUTION]
> Some processors may have jumped to the kernel while others are still running bootloader or firmware code.
> Therefore, the kernel must keep track of how many processors have jumped to its entrypoint. Until this number matches
> the total number of CPUs (passed in `A2`), the only areas of memory that the kernel is allowed to access are its own
> kernel image, the provided page tables, and the device tree blob. MMIO is also not allowed to be accessed while in
> this state.

## Page Tables

Though the MMU is disabled upon entry to the kernel, the bootloader does construct page tables for the kernel to use.
This is to allow the kernel image to be noncontiguous in physical memory, as some machines may lack large physically
contiguous areas of memory.

These page tables only map one area of memory: the kernel image. Specifically, they map the virtual address range
starting at `AlignDown(Header.VirtualAddr, PAGE_SIZE)` and ending at
`AlignUp(Header.VirtualAddr + Header.MSize, PAGE_SIZE)` as RWX. The kernel image is loaded into memory such that,
when the provided page tables are used, it is virtually contiguous with the header at `Header.VirtualAddr`. Mapped
areas not covered by the on-disk kernel image are zeroed.

The structure used by the page tables is a two-level tree, where bits 22 through 31 of the virtual address determine the
index into the top-level table, and bits 12 through 21 determine the index into the bottom-level table. Each table is
page-sized and page-aligned, with 4-byte entries. Empty entries are entirely zero. Non-empty entries
have bits 0 through 2 set, bit 3 clear, bit 4 set, bits 5 through 24 set to the physical address of the pointed-to page
divided by the page size, and bits 25 through 31 clear. This entry format matches the CPU's format for TB entries.

