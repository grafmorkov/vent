# Vent
Vent is a fast, simple dependency installer.

> Currently, only Linux is fully supported. `SOURCE_SYSTEM` is not available on Windows.
---

# Requirements
* C11 Compiler
* CMake 3.16
* libcurl
* libarchive
---

# Build
```sh
git clone https://github.com/grafmorkov/vent.git

cd vent
cmake -B build
cmake --build build
```
---

# Usage
Vent uses `.vent` configuration files instead of large `.json` files.
Example of config:

```vent
std glm glfw

archive https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.8/llvm-project-22.1.8.src.tar.xz .vent/llvm

git https://github.com/grafmorkov/quark-lang .vent/quark-lang

system vulkan-loader
```

`std` - install a package from the `std/` directory
`archive` - download and extract an archive
`git` - clone a Git repository
`system` - install a package using the user's system package manager.

> Packages are searched by name only, so this may not work consistently across different Linux distributions.
---

# Notes
Vent is still in beta, and its goal is to keep repositories cleaner and make package integration easier.
