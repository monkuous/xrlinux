# xrlinux
Linux port to [xr17032](https://github.com/xrarch)

## Progress

- binutils: complete
- gcc:
  - compiler: mostly complete (only lacking support for very obscure features, e.g. nested functions)
  - libgcc: compiles (untested)
  - libstdc++: blocked on glibc
- linux kernel: functional but incomplete (lacking mouse driver and smp support)

## Building

When setting up a build directory, make sure to set JINX_ARCH correctly:
```sh
mkdir build
cd build
../jinx init .. ARCH=xr17032
```
