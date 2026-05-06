// REQUIRES: host-supports-jit
// RUN: clang-repl -Xcc -fno-color-diagnostics < %s 2>&1 | FileCheck %s

extern "C" int printf(const char *, ...);
template <typename T> T my_pow(T a, T b) { return a * b; }

(10-)*my_pow(2, 2);
// CHECK: error: expected expression
// CHECK: error: Parsing failed.

printf("%d\n", my_pow(2, 2));
// CHECK: 4

%quit