# xrlinux
Linux port to [xr17032](https://github.com/xrarch)

## Progress

- binutils: functional but incomplete (lacking diagnostics and pseudo-instruction support)
- gcc:
  - compiler: functional but incomplete (lacking exception handling and most atomic rmw operations)
  - libgcc: blocked on compiler exception handling support
  - libstdc++: blocked on libgcc and glibc
- linux kernel: functional but incomplete (lacking mouse driver and smp support)

## Building

When setting up a build directory, make sure to set JINX_ARCH correctly:
```sh
mkdir build
cd build
../jinx init .. ARCH=xr17032
```
