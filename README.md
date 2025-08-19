# xrlinux
Linux port to [xr17032](https://github.com/xrarch)

## Progress

- binutils: functional but incomplete (lacking diagnostics and pseudo-instruction support)
- gcc: compiles but non-functional

## Building

When setting up a build directory, make sure to set JINX_ARCH correctly:
```sh
mkdir build
cd build
../jinx init .. ARCH=xr17032
```
