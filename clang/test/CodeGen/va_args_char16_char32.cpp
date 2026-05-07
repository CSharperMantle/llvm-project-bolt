// RUN: %clang_cc1 -std=c++11 -triple x86_64-unknown-linux-gnu \
// RUN:   -emit-llvm -o - %s | FileCheck %s

__builtin_va_list ap;

// CHECK-LABEL: define {{.*}} @_Z3foov
void foo() {
  enum E1 : char16_t {};
  enum E2 : char32_t {};

  (void)__builtin_va_arg(ap, E1);
  // CHECK: %vaarg.addr = phi ptr
  // CHECK-NEXT: %{{.*}} = load i32, ptr %vaarg.addr

  (void)__builtin_va_arg(ap, E2);
  // CHECK: %vaarg.addr{{.*}} = phi ptr
  // CHECK-NEXT: %{{.*}} = load i32, ptr %vaarg.addr
}