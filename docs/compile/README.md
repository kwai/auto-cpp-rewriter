## 编译

```bash
# 假设当前目录在 workspace 目录下
git clone https://github.com/llvm/llvm-project.git
cd llvm-project
mkdir build
cd build

cmake -S ../llvm -G Ninja -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;mlir" -DLLVM_BUILD_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Release  -DLLVM_ENABLE_ASSERTIONS=ON -DCMAKE_C_COMPILER="/home/liuzhishan/gcc-8.3.0/bin/gcc" -DCMAKE_CXX_COMPILER="/home/liuzhishan/gcc-8.3.0/bin/g++"

ninja
```