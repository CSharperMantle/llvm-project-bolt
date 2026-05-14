// RUN: %clang_cc1 -x cuda --cuda-device-only %s -S -o - | FileCheck %s

// CHECK: st.global.b32
// CHECK: st.global.b32

__global__ void kernel(int **X, int x, int y) {
    X[x][y] = x * y;
    X[y][x] = x + y;
}
