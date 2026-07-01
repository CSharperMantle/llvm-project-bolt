/// Check that --instrument-load-profiles correctly instruments a jump-table
/// indirect branch, records the effective addresses in the profile, and
/// enables jump-table ICP during the subsequent BOLT optimization pass.

// REQUIRES: system-linux,bolt-runtime

// RUN: %clang %cflags %s -o %t.exe -Wl,-q -pie -fpie

// RUN: llvm-bolt %t.exe --instrument --instrument-load-profiles \
// RUN:   --instrumentation-file=%t.fdata -o %t.instr

// RUN: %t.instr

// RUN: cat %t.fdata | FileCheck %s --check-prefix=CHECK-MEMINFO

// RUN: llvm-bolt %t.exe --data %t.fdata \
// RUN:   --reorder-blocks=ext-tsp --icp=all --icp-jump-tables-targets \
// RUN:   -o %t.bolt 2>&1 | FileCheck %s --check-prefix=CHECK-ICP

// RUN: %t.exe
// RUN: %t.bolt | FileCheck %s --check-prefix=CHECK-OUTPUT

// CHECK-MEMINFO: 4 switch_func {{[0-9a-f]+}} 3 [unknown] {{[0-9a-f]+}} 1

// CHECK-ICP: ICP total jump table callsites = 1

// CHECK-OUTPUT: 1006

#include <stdio.h>

__attribute__((noinline)) int h0(int v) { return v + 100; }
__attribute__((noinline)) int h1(int v) { return v + 200; }
__attribute__((noinline)) int h2(int v) { return v + 300; }
__attribute__((noinline)) int h3(int v) { return v + 400; }

__attribute__((noinline)) int switch_func(int n) {
  int r;
  switch (n) {
    case 0: r = h0(0); break;
    case 1: r = h1(1); break;
    case 2: r = h2(2); break;
    case 3: r = h3(3); break;
    default: r = -1; break;
  }
  return r;
}

int main(void) {
  volatile int a = 0, b = 1, c = 2, d = 3;
  int s = 0;
  s += switch_func(a);  // 100
  s += switch_func(b);  // 201
  s += switch_func(c);  // 302
  s += switch_func(d);  // 403
  printf("%d\n", s);
  return 0;
}
