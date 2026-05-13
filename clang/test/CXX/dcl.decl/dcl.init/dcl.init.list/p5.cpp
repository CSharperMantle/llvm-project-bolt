// RUN: %clang_cc1 %std_cxx11- -triple x86_64-none-linux-gnu -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 %std_cxx11- -DVERIFY -fsyntax-only -verify %s

namespace std {
using size_t = decltype(sizeof(int));

template <class E> class initializer_list {
  const E *begin_;
  size_t size_;

public:
  constexpr initializer_list() : begin_(nullptr), size_(0) {}
  constexpr initializer_list(const E *begin, size_t size)
      : begin_(begin), size_(size) {}
  constexpr const E *begin() const { return begin_; }
  constexpr size_t size() const { return size_; }
};
} // namespace std

// CHECK-DAG: @[[STATIC_DOUBLES:.+]] = private constant [3 x double] [double 1.000000e+00, double 2.000000e+00, double 3.000000e+00], align 8

// [dcl.init.list] example 12.
void f(std::initializer_list<double> il);

void g(float x) {
  // CHECK-LABEL: define{{.*}} void @_Z1gf(
  // CHECK: alloca [3 x double]
  f({1, x, 3});
}

void h() {
  // CHECK-LABEL: define{{.*}} void @_Z1hv()
  // CHECK-NOT: alloca [3 x double]
  // CHECK: store ptr @[[STATIC_DOUBLES]], ptr
  // CHECK: call void @_Z1fSt16initializer_listIdE(
  f({1, 2, 3});
}

struct A {
  mutable int i;
};

void q(std::initializer_list<A>);

void r() {
  // CHECK-LABEL: define{{.*}} void @_Z1rv()
  // CHECK: alloca [3 x %struct.A]
  q({A{1}, A{2}, A{3}});
}

struct Dtor {
  int i;
  ~Dtor();
};

void f_with_dtor(std::initializer_list<Dtor>);

void non_trivial_dtor() {
  // CHECK-LABEL: define{{.*}} void @_Z16non_trivial_dtorv()
  // CHECK: alloca [3 x %struct.Dtor]
  // CHECK: call void @_ZN4DtorD1Ev(
  f_with_dtor({{1}, {2}, {3}});
}

#ifdef VERIFY
struct C5 {
  std::initializer_list<int> il; // expected-note {{'std::initializer_list' member declared here}}
  C5() : il{1, 2, 3} {}
  // expected-error@-1 {{backing array for 'std::initializer_list' member 'il' is a temporary object whose lifetime would be shorter than the lifetime of the constructed object}}
};
#endif
