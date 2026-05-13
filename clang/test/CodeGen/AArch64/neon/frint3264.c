// REQUIRES: aarch64-registered-target

// RUN:                   %clang_cc1_cg_arm64_neon -target-feature +v8.5a           -emit-llvm %s -disable-O0-optnone | opt -S -passes=mem2reg,sroa | FileCheck %s --check-prefixes=LLVM
// RUN: %if cir-enabled %{%clang_cc1_cg_arm64_neon -target-feature +v8.5a -fclangir -emit-llvm %s -disable-O0-optnone | opt -S -passes=mem2reg,sroa | FileCheck %s --check-prefixes=LLVM %}
// RUN: %if cir-enabled %{%clang_cc1_cg_arm64_neon -target-feature +v8.5a -fclangir -emit-cir  %s -disable-O0-optnone |                               FileCheck %s --check-prefixes=CIR %}

#include <arm_neon.h>

// LLVM-LABEL: @test_vrnd32x_f32(
// CIR-LABEL: @vrnd32x_f32(
float32x2_t test_vrnd32x_f32(float32x2_t a) {
// CIR: cir.call_llvm_intrinsic "aarch64.neon.frint32x" {{%.*}} : (!cir.vector<2 x !cir.float>) -> !cir.vector<2 x !cir.float>

// LLVM: [[VRND32X_I:%.*]] = call <2 x float> @llvm.aarch64.neon.frint32x.v2f32(<2 x float> {{.*}})
  return vrnd32x_f32(a);
}

// LLVM-LABEL: @test_vrnd32xq_f32(
// CIR-LABEL: @vrnd32xq_f32(
float32x4_t test_vrnd32xq_f32(float32x4_t a) {
// CIR: cir.call_llvm_intrinsic "aarch64.neon.frint32x" {{%.*}} : (!cir.vector<4 x !cir.float>) -> !cir.vector<4 x !cir.float>

// LLVM: [[VRND32X_I:%.*]] = call <4 x float> @llvm.aarch64.neon.frint32x.v4f32(<4 x float> {{.*}})
  return vrnd32xq_f32(a);
}

// LLVM-LABEL: @test_vrnd32z_f32(
// CIR-LABEL: @vrnd32z_f32(
float32x2_t test_vrnd32z_f32(float32x2_t a) {
// CIR: cir.call_llvm_intrinsic "aarch64.neon.frint32z" {{%.*}} : (!cir.vector<2 x !cir.float>) -> !cir.vector<2 x !cir.float>

// LLVM: [[VRND32Z_I:%.*]] = call <2 x float> @llvm.aarch64.neon.frint32z.v2f32(<2 x float> {{.*}})
  return vrnd32z_f32(a);
}

// LLVM-LABEL: @test_vrnd32zq_f32(
// CIR-LABEL: @vrnd32zq_f32(
float32x4_t test_vrnd32zq_f32(float32x4_t a) {
// CIR: cir.call_llvm_intrinsic "aarch64.neon.frint32z" {{%.*}} : (!cir.vector<4 x !cir.float>) -> !cir.vector<4 x !cir.float>

// LLVM: [[VRND32Z_I:%.*]] = call <4 x float> @llvm.aarch64.neon.frint32z.v4f32(<4 x float> {{.*}})
  return vrnd32zq_f32(a);
}

// LLVM-LABEL: @test_vrnd64x_f32(
// CIR-LABEL: @vrnd64x_f32(
float32x2_t test_vrnd64x_f32(float32x2_t a) {
// CIR: cir.call_llvm_intrinsic "aarch64.neon.frint64x" {{%.*}} : (!cir.vector<2 x !cir.float>) -> !cir.vector<2 x !cir.float>

// LLVM: [[VRND64X_I:%.*]] = call <2 x float> @llvm.aarch64.neon.frint64x.v2f32(<2 x float> {{.*}})
  return vrnd64x_f32(a);
}

// LLVM-LABEL: @test_vrnd64xq_f32(
// CIR-LABEL: @vrnd64xq_f32(
float32x4_t test_vrnd64xq_f32(float32x4_t a) {
// CIR: cir.call_llvm_intrinsic "aarch64.neon.frint64x" {{%.*}} : (!cir.vector<4 x !cir.float>) -> !cir.vector<4 x !cir.float>

// LLVM: [[VRND64X_I:%.*]] = call <4 x float> @llvm.aarch64.neon.frint64x.v4f32(<4 x float> {{.*}})
  return vrnd64xq_f32(a);
}

// LLVM-LABEL: @test_vrnd64z_f32(
// CIR-LABEL: @vrnd64z_f32(
float32x2_t test_vrnd64z_f32(float32x2_t a) {
// CIR: cir.call_llvm_intrinsic "aarch64.neon.frint64z" {{%.*}} : (!cir.vector<2 x !cir.float>) -> !cir.vector<2 x !cir.float>

// LLVM: [[VRND64Z_I:%.*]] = call <2 x float> @llvm.aarch64.neon.frint64z.v2f32(<2 x float> {{.*}})
  return vrnd64z_f32(a);
}

// LLVM-LABEL: @test_vrnd64zq_f32(
// CIR-LABEL: @vrnd64zq_f32(
float32x4_t test_vrnd64zq_f32(float32x4_t a) {
// CIR: cir.call_llvm_intrinsic "aarch64.neon.frint64z" {{%.*}} : (!cir.vector<4 x !cir.float>) -> !cir.vector<4 x !cir.float>

// LLVM: [[VRND64Z_I:%.*]] = call <4 x float> @llvm.aarch64.neon.frint64z.v4f32(<4 x float> {{.*}})
  return vrnd64zq_f32(a);
}
