// RUN: %clang %cflags -O2 -o %t %s
// RUN: llvm-bolt --print-cfg --print-only=switch_func -o %t.null %t 2>&1 | FileCheck %s

// CHECK: IsSimple    : 1
// CHECK: JUMPTABLE @
// CHECK: Jump table {{.+}} for function switch_func at 0x[[#]] with a total count of [[#]]:

extern void h0(void), h1(void), h2(void), h3(void);

__attribute__((noinline)) void switch_func(int n) {
  switch (n) {
    case 0: h0(); break;
    case 1: h1(); break;
    case 2: h2(); break;
    case 3: h3(); break;
  }
}

void _start(void) {
  switch_func(0);
  switch_func(1);
  switch_func(2);
  switch_func(3);
}
