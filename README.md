# CyrilCC 编译器

**一个支持类 C 语言的编译器项目**，将源代码编译为自定义汇编语言，并通过虚拟机执行。


## 📋 目录

- [快速开始](#快速开始)
- [项目概述](#项目概述)
- [环境要求](#环境要求)
- [构建与运行](#构建与运行)
- [测试验证](#测试验证)
- [项目结构](#项目结构)
- [技术细节](#技术细节)


## 🚀 快速开始

### 一键运行测试

```bash
# 1. 构建项目
meson setup build && meson compile -C build

# 2. 运行所有测试（28个测试用例）
meson test -C build -v

# 3. 手动测试示例
./build/cyrilcc test/testcase/int.m -o int.s
./build/asm int.s
./build/machine int.o < test/testcase/int.in
```

### 检查要点

✅ **编译器可执行文件**：`build/cyrilcc`（可修改部分）  
✅ **汇编器/虚拟机**：`build/asm` 和 `build/machine`（不可修改）  
✅ **测试通过率**：运行 `meson test -C build` 查看  
✅ **源代码组织**：`src/` 目录包含完整的编译器实现


## 📖 项目概述

### 编译流程

```
源代码 (.m) → [cyrilcc 编译器] → 汇编 (.s) → [asm 汇编器] → 目标文件 (.o) → [machine 虚拟机] → 执行
```

### 支持特性

| 类别 | 支持内容 |
|------|---------|
| **数据类型** | `int`, `char`, 指针, 数组, 结构体 |
| **控制流** | `if/else`, `while`, `for`, `switch` |
| **函数** | 函数定义、调用、递归 |
| **优化** | SSA、GVN、SCCP、Mem2Reg、deSSA 等 |

### 可用测试用例（28个）

```bash
# 基本类型（6个）
int, char, char-int, ptr-int, ptr-char, ptr-char-int

# 数组（3个）
arr, arr-while, arr-struct

# 结构体（3个）
struct, struct-arr, struct-ptr

# 控制流（6个）
if, switch, while, while-bc, for, for-bc

# 函数（2个）
func-int, func-char
```


## 🔧 环境要求

### 必需工具

| 工具 | 版本要求 | 用途 |
|------|---------|------|
| **C++23 编译器** | GCC 或 Clang | 编译 C++ 源代码 |
| **Meson** | ≥ 0.55 | 构建系统 |
| **Flex** | - | 词法分析器生成 |
| **Bison** | - | 语法分析器生成 |

### 快速安装

**macOS:**
```bash
brew install meson flex bison
```

**Ubuntu/Debian:**
```bash
sudo apt update && sudo apt install build-essential meson flex bison
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc-c++ meson flex bison
```

**Arch Linux:**
```bash
sudo pacman -S base-devel meson flex bison
```


## 🛠️ 构建与运行

### 构建步骤

```bash
# 1. 配置构建目录
meson setup build

# 2. 编译项目（生成 cyrilcc, asm, machine 三个可执行文件）
meson compile -C build

# 3. 清理（如需重新构建）
rm -rf build
```

### 使用流程

**完整的三步编译执行流程：**

```bash
# 步骤 1: 源代码 → 汇编代码
./build/cyrilcc source.m -o output.s

# 步骤 2: 汇编代码 → 目标文件
./build/asm output.s                    # 生成 output.o

# 步骤 3: 目标文件 → 执行
./build/machine output.o                # 运行程序
./build/machine output.o < input.txt    # 带输入运行
```

**完整示例（测试 int.m）：**

```bash
./build/cyrilcc test/testcase/int.m -o int.s
./build/asm int.s
./build/machine int.o < test/testcase/int.in
# 输出应与 test/testcase/int.out 一致
```


## ✅ 测试验证

### 自动测试（推荐）

```bash
# 运行所有测试（28个测试用例）
meson test -C build

# 查看详细输出
meson test -C build -v

# 运行特定测试
meson test -C build int char struct
```

### 手动测试

**使用测试脚本：**
```bash
./run_compiler_test.sh \
  ./build/cyrilcc \
  ./build/asm \
  ./build/machine \
  test/testcase/int.m \
  test/testcase/int.in \
  test/testcase/int.out
```

**手动逐步测试：**
```bash
# 任选一个测试用例，例如 struct.m
./build/cyrilcc test/testcase/struct.m -o struct.s
./build/asm struct.s
./build/machine struct.o < test/testcase/struct.in > output.txt
diff output.txt test/testcase/struct.out  # 应无差异
```

### 测试用例结构

每个测试用例包含三个文件：
- **`.m`** - 源代码文件
- **`.in`** - 标准输入数据
- **`.out`** - 预期输出结果

位置：`test/testcase/` 目录


## 📁 项目结构

### 完整目录树

```
cyrilcc/
├── src/                    # 🔨 编译器实现（可修改部分）
│   ├── lexer.l            # 词法分析器（Flex）
│   ├── parser.y           # 语法分析器（Bison）
│   ├── ast.cpp/hpp        # AST 构建和语义分析
│   ├── ir.hpp             # 中间表示（IR）
│   ├── asm_gen.hpp        # 汇编代码生成
│   ├── type.hpp           # 类型系统
│   ├── pass.hpp           # Pass 基础框架
│   └── pass/              # 优化 Pass 实现
│       ├── mem2reg.hpp    # 内存到寄存器提升
│       ├── sccp.hpp       # 稀疏条件常量传播
│       ├── GVNPass.hpp    # 全局值编号
│       ├── deSSA.hpp      # SSA 解除
│       └── dom_analysis.hpp # 支配树分析
│
├── asm-machine/           # ⚙️ 汇编器和虚拟机（不可修改）
│   ├── asm.l              # 汇编器词法分析
│   ├── asm.y              # 汇编器语法分析
│   ├── inst.h             # 指令集定义
│   └── machine.c          # 虚拟机实现
│
├── test/                  # 📝 测试用例
│   ├── testcase/          # 标准测试（.m, .in, .out）
│   ├── basic/             # 基础功能测试
│   ├── advanced/          # 高级特性测试
│   └── optimized/         # 优化效果测试
│
├── docs/                  # 📚 文档
│   ├── asm_format.md      # 汇编格式说明
│   ├── IR_format.md       # IR 格式说明
│   ├── AST_node.md        # AST 节点说明
│   └── testcase.md        # 测试用例说明
│
├── meson.build            # Meson 构建配置
├── run_compiler_test.sh   # 测试脚本
└── README.md              # 本文件
```

### 关键文件说明

| 文件 | 类型 | 说明 |
|------|------|------|
| **src/\*** | 可修改 | 编译器实现，包括前端、优化、后端 |
| **asm-machine/\*** | 不可修改 | 课程提供的汇编器和虚拟机 |
| **test/testcase/\*** | 参考 | 标准测试用例，用于验证正确性 |
| **docs/\*** | 参考 | 详细的技术文档 |


## 🔍 技术细节

### 编译器实现架构

#### 可修改部分（编译器实现）
`src/` 目录下的所有文件都是编译器的实现代码：
- 词法/语法分析器：`lexer.l`, `parser.y`
- AST 构建：`ast.cpp`, `ast.hpp`
- 中间表示和优化：`ir.hpp`, `pass/` 目录下的各种 Pass
- 目标代码生成：`asm_gen.hpp`

#### 不可修改部分（汇编器和虚拟机）
`asm-machine/` 目录下的文件是**课程提供的基础设施**，用于作业检查：
- **汇编器** (`asm.l`, `asm.y`)：将编译器生成的汇编代码转换为目标文件
  - 输入：`.s` 汇编文件（由编译器生成）
  - 输出：`.o` 目标文件
- **虚拟机** (`machine.c`)：执行目标文件
  - 输入：`.o` 目标文件
  - 输出：程序执行结果
- **指令集定义** (`inst.h`)：定义了虚拟机支持的指令集

编译器需要生成符合 `asm-machine/` 定义的汇编格式，详见 `docs/asm_format.md`。

### 寄存器约定

**寄存器分配（类 MIPS 架构）：**

| 寄存器 | 用途 | 调用约定 |
|--------|------|---------|
| R0 | FLAG（标志寄存器） | 特殊 |
| R1 | IP（指令指针） | 特殊 |
| R2 | 返回值 / 参数1 | 调用者保存 |
| R3-R5 | 参数2-4 | 调用者保存 |
| R6-R7 | 未使用 | - |
| R8-R10, R13 | 临时寄存器 | 调用者保存 |
| R11 | FP（帧指针） | 被调用者保存 |
| R12 | SP（栈指针） | 被调用者保存 |
| R14 | RA（返回地址） | 特殊 |
| R15 | I/O 数据寄存器 | 特殊 |

### 栈帧布局

栈向下增长（地址递减）：

```
+-------------------+
| ...               |  <-- 调用者栈帧
| Arg 5             |  高地址
| Arg 4             |
+-------------------+ ---
| Old FP            |  \
+-------------------+  | 被调用者栈帧
| Return Address    |  |
+-------------------+ ---  <-- R11 (FP) 指向这里
| Local Var 1       |  |
+-------------------+  |
| Local Var 2       |  |
+-------------------+  |
| Temp Variable     |  /
+-------------------+ ---  <-- R12 (SP) 指向这里（低地址）
```

### 函数调用约定

**调用者 (Caller) 职责：**

1. 保存调用者保存寄存器（R8-R10, R13）
2. 将参数 0-3 加载到 R2-R5
3. 将参数 4+ 逆序压栈
4. `LOD R14, <return_label>` 设置返回地址
5. `JMP <func_label>` 跳转到函数
6. 返回后清理栈参数，从 R2 获取返回值
7. 恢复调用者保存寄存器

**被调用者 (Callee) 职责：**

1. 保存旧 FP：`STO (R12), R11; SUB R12, #4`
2. 保存 RA：`STO (R12), R14; SUB R12, #4`
3. 设置新 FP：`LOD R11, R12`
4. 分配栈空间：`SUB R12, #frame_size`
5. 执行函数体
6. 返回时恢复栈帧并跳转：`JMP R14`

### 重要文档

- **汇编格式**：`docs/asm_format.md` - 完整的指令集和汇编语法
- **IR 格式**：`docs/IR_format.md` - 中间表示的定义
- **AST 节点**：`docs/AST_node.md` - 抽象语法树节点说明
- **测试用例**：`docs/testcase.md` - 测试用例详细说明
