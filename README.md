# Vent
Vent is a fast, simple dependency installer.

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

# Supported Platforms

Linux (tested on major distros)

Windows (via MinGW/MSYS2 or Visual Studio)

macOS (experimental)

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

`std <name>` - install a package from the `std/` directory;

`archive <url> <target_dir>` - download and extract an archive;

`git <url> <target_dir>` - clone a Git repository;

`system <package>` - install a package using the user's system package manager.

> Packages are searched by name only, so this may not work consistently across different Linux distributions.


Execution:
```sh
vent -j 2 example.vent
```

`-j 2` - installs dependencies in parallel, using up to **2 concurrent jobs**.

---

# Notes
* Vent is still in beta
* it's goal is to keep repositories cleaner and make package integration easier.

# License
GPL-3.0
