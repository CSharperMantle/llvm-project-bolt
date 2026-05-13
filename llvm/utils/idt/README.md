# Interface Definition Scanner (idt)

This is a vendored copy of [`compnerd/ids`](https://github.com/compnerd/ids),
a libclang-based tool for identifying and annotating the public interface of a
C++ project (e.g. adding `LLVM_ABI` markers to declarations that need them for
the `LLVM_BUILD_LLVM_DYLIB` build).

It is used by `.github/workflows/ids-check.yml` together with
`llvm/utils/git/ids-check-helper.py` to flag missing ABI annotations on PRs.

## Why vendored?

idt's heuristics for "what needs exporting" were designed for typical libraries
and don't all hold for LLVM (e.g. the workflow needs to distinguish out-of-line
`.cpp` definitions from inline-in-header definitions). Vendoring lets us patch
the tool to fit LLVM's needs without round-tripping through the upstream repo.

## Build

This is a **standalone** CMake project; it is not built as part of the main
LLVM build. It links against a pre-installed LLVM/Clang (typically the
`libclang-*-dev` package on the CI runner), which keeps build time small
(seconds, not the half-hour required to build clang from source).

```sh
cmake -B build -S . -G Ninja \
  -D LLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm/ \
  -D Clang_DIR=/usr/lib/llvm-22/lib/cmake/clang/
ninja -C build
```

## License

idt is BSD-3-Clause licensed (see `LICENSE.TXT`). LLVM is Apache-2.0 with LLVM
exceptions; the BSD-3-Clause notice is preserved on the vendored source files.
