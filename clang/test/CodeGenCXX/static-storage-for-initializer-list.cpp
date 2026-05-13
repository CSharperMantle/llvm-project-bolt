// RUN: %clang_cc1 %std_cxx11- -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck --check-prefixes=CHECK,X86,SIZE %s
// RUN: %clang_cc1 %std_cxx11- -triple x86_64-unknown-linux-gnu -fmerge-all-constants -emit-llvm -o - %s | FileCheck --check-prefix=MERGE %s
// RUN: %clang_cc1 %std_cxx11- -triple x86_64-unknown-linux-gnu -DSTART_END -emit-llvm -o - %s | FileCheck --check-prefixes=CHECK,X86,END %s
// RUN: %clang_cc1 %std_cxx11- -triple amdgcn-amd-amdhsa -emit-llvm -o - %s | FileCheck --check-prefixes=CHECK,AMDGCN,SIZE %s
// RUN: %clang_cc1 %std_cxx11- -triple x86_64-unknown-linux-gnu --embed-dir=%S/Inputs -DEMBED -emit-llvm -o - %s | FileCheck --check-prefix=EMBED %s

namespace std {
typedef decltype(sizeof(int)) size_t;

template <class _E> class initializer_list {
#ifdef START_END
  const _E *__begin_;
  const _E *__end_;
  constexpr initializer_list(const _E *__b, const _E *__e)
      : __begin_(__b), __end_(__e) {}
#else
  const _E *__begin_;
  size_t __size_;
  constexpr initializer_list(const _E *__b, size_t __s)
      : __begin_(__b), __size_(__s) {}
#endif

public:
  typedef _E value_type;
  typedef const _E &reference;
  typedef const _E &const_reference;
  typedef size_t size_type;
  typedef const _E *iterator;
  typedef const _E *const_iterator;

#ifdef START_END
  constexpr initializer_list() : __begin_(nullptr), __end_(nullptr) {}
  constexpr size_t size() const { return __end_ - __begin_; }
  constexpr const _E *end() const { return __end_; }
#else
  constexpr initializer_list() : __begin_(nullptr), __size_(0) {}
  constexpr size_t size() const { return __size_; }
  constexpr const _E *end() const { return __begin_ + __size_; }
#endif
  constexpr const _E *begin() const { return __begin_; }
};
} // namespace std

// X86-DAG: @[[THREE_INTS:.+]] = private constant [3 x i32] [i32 1, i32 2, i32 3], align 4
// X86-DAG: @[[LOCAL_INTS:.+]] = private constant [3 x i32] [i32 4, i32 5, i32 6], align 4
// X86-DAG: @[[LIFETIME_INTS:.+]] = private constant [3 x i32] [i32 7, i32 8, i32 9], align 4
// X86-DAG: @[[CLASS_VALUES:.+]] = private constant [2 x %{{.*}}ConstexprClass] [%{{.*}}ConstexprClass { i32 11 }, %{{.*}}ConstexprClass { i32 12 }], align 4
// X86-DAG: @[[ISSUE_A:_ZGRN11issue104487L1aE_]] = internal constant [1 x i8] c"x", align 1
// X86-DAG: @[[ISSUE_B:_ZGRN11issue104487L1bE_]] = internal constant [1 x i8] c"x", align 1
// EMBED-DAG: @[[EMBEDDED_BYTES:.+]] = private constant [2 x i8] c"jk", align 1
// MERGE-DAG: @_ZGRN11issue104487L1aE_ = internal constant [1 x i8] c"x", align 1
// MERGE-DAG: @_ZGRN11issue104487L1bE_ = internal constant [1 x i8] c"x", align 1
// AMDGCN-DAG: @[[THREE_INTS:.+]] = private addrspace(4) constant [3 x i32] [i32 1, i32 2, i32 3], align 4
// AMDGCN-DAG: @[[LOCAL_INTS:.+]] = private addrspace(4) constant [3 x i32] [i32 4, i32 5, i32 6], align 4
// AMDGCN-DAG: @[[LIFETIME_INTS:.+]] = private addrspace(4) constant [3 x i32] [i32 7, i32 8, i32 9], align 4
// AMDGCN-DAG: @[[CLASS_VALUES:.+]] = private addrspace(4) constant [2 x %{{.*}}ConstexprClass] [%{{.*}}ConstexprClass { i32 11 }, %{{.*}}ConstexprClass { i32 12 }], align 4
// AMDGCN-DAG: @[[ISSUE_A:_ZGRN11issue104487L1aE_]] = internal addrspace(1) constant [1 x i8] c"x", align 1
// AMDGCN-DAG: @[[ISSUE_B:_ZGRN11issue104487L1bE_]] = internal addrspace(1) constant [1 x i8] c"x", align 1

void take_ints(std::initializer_list<int>);

void constant_call() {
  // CHECK-LABEL: define{{.*}} void @_Z13constant_callv()
  // CHECK-NOT: alloca [3 x i32]
  // X86: store ptr @[[THREE_INTS]], ptr
  // AMDGCN: store ptr addrspacecast (ptr addrspace(4) @[[THREE_INTS]] to ptr), ptr
  // SIZE: store i64 3,
  // END: store ptr getelementptr inbounds nuw (i8, ptr @[[THREE_INTS]], i64 12),
  // CHECK: call void @_Z9take_intsSt16initializer_listIiE(
  take_ints({1, 2, 3});
}

void local_list() {
  // CHECK-LABEL: define{{.*}} void @_Z10local_listv()
  // CHECK-NOT: alloca [3 x i32]
  // X86: store ptr @[[LOCAL_INTS]], ptr
  // AMDGCN: store ptr addrspacecast (ptr addrspace(4) @[[LOCAL_INTS]] to ptr), ptr
  std::initializer_list<int> il = {4, 5, 6};
  take_ints(il);
}

void empty_list() {
  // CHECK-LABEL: define{{.*}} void @_Z10empty_listv()
  // CHECK-NOT: alloca [0 x i32]
  // CHECK: call void @_Z9take_intsSt16initializer_listIiE(
  take_ints({});
}

void non_constant(int x) {
  // CHECK-LABEL: define{{.*}} void @_Z12non_constanti(
  // CHECK: alloca [3 x i32]
  // CHECK: store i32 %{{.*}},
  take_ints({1, x, 3});
}

void take_volatile(std::initializer_list<volatile int>);

void volatile_element() {
  // CHECK-LABEL: define{{.*}} void @_Z16volatile_elementv()
  // CHECK: alloca [2 x i32]
  take_volatile({21, 22});
}

struct MutableMember {
  mutable int value;
  constexpr MutableMember(int v) : value(v) {}
};
void take_mutable(std::initializer_list<MutableMember>);

void mutable_member() {
  // CHECK-LABEL: define{{.*}} void @_Z14mutable_memberv()
  // CHECK: alloca [2 x %{{.*}}MutableMember]
  take_mutable({MutableMember(31), MutableMember(32)});
}

struct NestedMutable {
  MutableMember member;
  constexpr NestedMutable(int v) : member(v) {}
};
void take_nested_mutable(std::initializer_list<NestedMutable>);

void recursive_mutable() {
  // CHECK-LABEL: define{{.*}} void @_Z17recursive_mutablev()
  // CHECK: alloca [2 x %{{.*}}NestedMutable]
  take_nested_mutable({NestedMutable(41), NestedMutable(42)});
}

struct NonTrivialDtor {
  int value;
  ~NonTrivialDtor();
};
void take_dtor(std::initializer_list<NonTrivialDtor>);

void non_trivial_dtor() {
  // CHECK-LABEL: define{{.*}} void @_Z16non_trivial_dtorv()
  // CHECK: alloca [2 x %{{.*}}NonTrivialDtor]
  // CHECK: call void @_ZN14NonTrivialDtorD1Ev(
  take_dtor({{51}, {52}});
}

void take_ints_ref(const std::initializer_list<int> &);

void lifetime_extension() {
  // CHECK-LABEL: define{{.*}} void @_Z18lifetime_extensionv()
  // CHECK-NOT: alloca [3 x i32]
  // X86: store ptr @[[LIFETIME_INTS]], ptr
  // AMDGCN: store ptr addrspacecast (ptr addrspace(4) @[[LIFETIME_INTS]] to ptr), ptr
  const auto &il = std::initializer_list<int>{7, 8, 9};
  take_ints_ref(il);
}

struct ConstexprClass {
  int value;
  constexpr ConstexprClass(int v) : value(v) {}
};
void take_class(std::initializer_list<ConstexprClass>);

void literal_class() {
  // CHECK-LABEL: define{{.*}} void @_Z13literal_classv()
  // CHECK-NOT: alloca [2 x %{{.*}}ConstexprClass]
  // X86: store ptr @[[CLASS_VALUES]], ptr
  // AMDGCN: store ptr addrspacecast (ptr addrspace(4) @[[CLASS_VALUES]] to ptr), ptr
  take_class({ConstexprClass(11), ConstexprClass(12)});
}

#ifdef EMBED
void take_bytes(std::initializer_list<unsigned char>);

void embedded_bytes() {
  // EMBED-LABEL: define{{.*}} void @_Z14embedded_bytesv()
  // EMBED-NOT: alloca [2 x i8]
  // EMBED: store ptr @[[EMBEDDED_BYTES]], ptr
  // EMBED: store i64 2,
  // EMBED: call void @_Z10take_bytesSt16initializer_listIhE(
  take_bytes({
#embed <jk.txt> // expected-warning {{#embed is a Clang extension}}
  });
}
#endif

namespace issue104487 {
static constexpr std::initializer_list<char> a = {'x'};
static constexpr std::initializer_list<char> b = {'x'};
static constexpr bool direct_value = a.begin() == b.begin();

bool direct() {
  // CHECK-LABEL: define{{.*}} noundef zeroext i1 @_ZN11issue1044876directEv()
  // CHECK: ret i1 false
  // MERGE-LABEL: define{{.*}} noundef zeroext i1 @_ZN11issue1044876directEv()
  // MERGE: ret i1 false
  return direct_value;
}

bool runtime() {
  // CHECK-LABEL: define{{.*}} noundef zeroext i1 @_ZN11issue1044877runtimeEv()
  // CHECK: store volatile ptr
  // CHECK: store volatile ptr
  // CHECK: icmp eq ptr
  // MERGE-LABEL: define{{.*}} noundef zeroext i1 @_ZN11issue1044877runtimeEv()
  // MERGE: @_ZN11issue104487L1aE
  // MERGE: @_ZN11issue104487L1bE
  // MERGE: icmp eq ptr
  const char *volatile ap = a.begin();
  const char *volatile bp = b.begin();
  return ap == bp;
}
} // namespace issue104487
