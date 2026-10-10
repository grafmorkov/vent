# Vent
Vent is a fast, simple dependency installer for repositories.

# Why Vent?

Vent is not a full development environment manager.

Tools like Nix, devenv and mise solve bigger problems:
- Nix/devenv create reproducible environments;
- mise manages language versions and tools.

Vent is just a simple way to describe and install project dependencies.

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

---

# Installing

| Distro | PkgManager | PkgName |
|--------|------------|---------|
| Arch   | yay/paru   | vent-cli |
| Gentoo | emerge     | sys-apps/vent |
| Nix    | nix        | planned |
| Other  | -          | github release |

Other package managers are not supported yet

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

`std <name>` - install a package from the [vent-std](https://github.com/grafmorkov/vent-std/tree/main) repository;

`archive <url> <target_dir>` - download and extract an archive;

`git <url> <target_dir>` - clone a Git repository;

`system <package>` - install a package using the user's system package manager.

> Packages are searched by name only, so this may not work consistently across different Linux distributions.

### Sections

Sections let you keep dependencies for different languages and platforms in one `.vent` file. 
For example, you can specify Linux-only packages, add dependencies for a specific architecture, or keep C++ dependencies separate from Quant ones.

A section header is a list of keys separated by dots, like `[quant.linux.aarch64]`. All keys in the header must match the current project configuration for that section to apply. Dependencies outside any section are always included.

```vent
[quant]
foo git https://github.com/x/foo @v1.2.0

[quant.linux]
system vulkan-loader

[quant.linux.aarch64]
archive https://x/qarm.tar.xz qarm

[cpp]
git https://github.com/glfw/glfw .vent/glfw
```

Section keys can specify a language, operating system, architecture, or an exact target:

| Key | Values |
|-----|--------|
| Language | `quant` (or `qu`), `cpp`, `c` |
| OS | `linux`, `windows`, `macos`, `zeropoint` |
| Architecture | `x86_64`, `aarch64` |
| Target | `x86_64-linux`, `aarch64-linux`, `x86_64-windows`, `aarch64-zeropoint` |

Inside a `[quant]` section, the package name comes first. Vent uses it as the module namespace, for example, `load "foo::..."`. In other sections, the dependency command comes first, as shown above.

The optional `@ref` suffix pins a dependency version. For Git repositories, it can be a tag, branch, or commit. For archives, it specifies the expected SHA-256 hash.

You can use `--lang` to select a specific language section and `--target` to select a target. For example:

```sh
vent --lang quant --target aarch64-linux example.vent
```

This lets you choose which language and target-specific dependencies to install without maintaining separate configuration files.

Execution:
```sh
vent -j 2 example.vent
```

`-j 2` - installs dependencies in parallel, using up to **2 concurrent jobs**.

Result:
```sh
vent -j 2 example.vent

  Vent Dependency Resolver
  file: example.vent


  Dependency Graph:
  ├── std glm (glm.vent)
  │   └── git https://github.com/g-truc/glm (clone → .vent/glm)
  └── std glfw (glfw.vent)
      └── git https://github.com/glfw/glfw (clone → .vent/glfw)

  Resolving dependencies...

   ✓ .vent/glfw — cached (0.0s)
   ✓ .vent/glm — cached (0.0s)

  ───────────────────────────
   ✓ 2 packages resolved successfully (0.0s)
```
> Lightning fast thanks to the global cache. (~/.vent/cache on Linux, %USERPROFILE%\.vent\cache on Windows).

---

# Notes
* Vent is still in beta
* it's goal is to keep repositories cleaner and make package integration easier.

# License
GPL-3.0
