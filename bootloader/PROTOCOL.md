# The xrlinux Boot Protocol

## Binary Format

Kernel images are flat binaries with the following header. All fields are little-endian.

| Offset | Size | Name         | Description                                                                       |
|--------|------|--------------|-----------------------------------------------------------------------------------|
| 0x00   | 4    | Magic        | Magic number identifying this file as an xrlinux kernel image. Set to 0x584c5258. |
| 0x04   | 2    | MinorVersion | The minor version of the protocol. This document describes minor version 0.       |
| 0x06   | 2    | MajorVersion | The major version of the protocol. This document describes major version 2.       |
| 0x08   | 4    | VirtualAddr  | Virtual address of this header.                                                   |
| 0x0c   | 4    | MSize        | Number of bytes in memory that the kernel image occupies, including BSS.          |
| 0x10   | 4    | Entry        | Virtual address of the kernel entry point.                                        |
| 0x14   | 4    | Flags        | Optional loader features requested by the kernel. See below.                      |
| 0x18   | 4    | DtbAddress   | Virtual address to map the device tree at.                                        |
| 0x1c   | 4    | MaxDtbEnd    | Highest virtual address that the device tree is allowed to occupy.                |

The following bits are valid in `Flags`:

| Bit | Name   | Description                                                      |
|-----|--------|------------------------------------------------------------------|
| 0   | MapDtb | If set, the bootloader must map the device tree at `DtbAddress`. |

`MajorVersion` and `MinorVersion` describe the version of the protocol that the kernel supports. The major version of
the protocol is incremented when backwards compatibility is broken (for example, when fields are removed from
the header or when new mandatory behavior is added); when the bootloader encounters an unknown major version, the only
fields whose existence it can rely on are `Magic`, `MinorVersion`, and `MajorVersion`. The minor version of the protocol
is incremented when new features are added in a backwards-compatible way (for example, a new field being added to the
header that can safely be ignored by the bootloader or a previously-undefined part of the entry state being defined).
Additionally, when the major version is incremented, the minor version is reset to zero.

`VirtualAddr` and `MSize` specify the virtual memory area that the kernel will be loaded in. See
[Kernel Image](#kernel-image).

`Entry` specifies the virtual address of the kernel entry point. This must be in the range of `VirtualAddr` (inclusive)
to `VirtualAddr + MSize` (exclusive).

`Flags` specifies which optional bootloader behavior the kernel opts in to. The same backwards compatibility rules as
the rest of the protocol apply to individual bits within this field; if a new bit can be safely ignored by bootloaders,
it results in a minor version increment, and otherwise in a major version increment.

`DtbAddress` and `MaxDtbEnd` specify the virtual memory area that the device tree will be mapped too. See
[Device Tree](#device-tree). If `MapDtb` is not set in `Flags`, bootloaders must ignore these fields. `MaxDtbEnd` must
be greater than or equal to `PageAlignUp(DtbAddress)`.

## Machine State

On entry, the machine state is as follows:
- Data and instruction caches coherent with main memory.
- `RS`: `T` is 0, `M` is 0, `U` is 0. All other bits are in an undefined state.
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
page-sized and page-aligned, with 4-byte entries. The entry format matches that of the CPU's TB. All non-valid entries
are zero (not just the V bit). All valid entries have the V, W, G, and K bits set, and N and all available bits clear.

The page tables map the following areas:

### Kernel Image

This area spans from `PageAlignDown(Header.VirtualAddr)` to `PageAlignUp(Header.VirtualAddr + Header.MSize)`.

The kernel image is mapped here such that the header appears at `Header.VirtualAddr`. All parts of this area not covered
by the kernel image are zeroed. This area is allowed to be physically discontiguous.

### Device Tree

> [!NOTE]
> This area is only present if `MapDtb` is set in `Header.Flags`.

This area spans from `PageAlignUp(Header.DtbAddress)` to `PageAlignUp(PageAlignUp(Header.DtbAddress) + DtbSize)`.

This area is physically contiguous, with `PageAlignUp(Header.DtbAddress)` mapping to the physical address passed in
`A1`. If this area contains addresses beyond `Header.MaxDtbEnd`, the bootloader must refuse to start the kernel. Parts
of this area not covered by the device tree are garbage.
