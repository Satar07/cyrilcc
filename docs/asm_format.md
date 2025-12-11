## 概要约定（全局）

- 寄存器：reg[0..15]（32-bit 整数）
    - reg[0] = R_FLAG（标志寄存器，用于 TST 指令）
    - reg[1] = R_IP（指令指针 / 程序计数器）
    - reg[15] 常被 I/O 指令作为数据寄存器使用（约定）
- 内存：字节寻址，大小 MEMMAX = 256 \* 256 = 65536 字节
- 立即数直接写十进制数即可
- 指令固定 8 字节（64 位）：
    - offset 0-1（2 字节，16-bit）：opcode（无符号）
    - offset 2（1 字节，8-bit）：rx
    - offset 3（1 字节，8-bit）：ry
    - offset 4-7（4 字节，32-bit，小端）：constant（立即值或地址）
- 端序：小端（Little-endian）。整型在内存和文件中以小端序存储/读取。
- 默认执行流：每轮循环提取当前 PC（reg[R_IP]）处的 8 字节指令，除跳转被执行后直接设置 `reg[R_IP]` 并跳到下一轮外，正常指令执行完后自动执行 `reg[R_IP] += 8`。
- 统计：cycle（每条指令 +1 基本），mem_r/mem_w 及 mul_div（乘除计数）。某些指令增加额外周期（见下文）。
- 错误：DIV 除 0 将打印错误并退出；越界内存访问没有运行时检查（由编译器/程序保证）。

## 寄存器名约定（供汇编使用）

- R0 .. R15 或 0..15。汇编器应接受 `R0`, `R1`, ... `R15`。
- 特别：R0 是标志寄存 `FLAG`（由 TST 设置），R1 是 IP（程序计数器）。

## 指令列表（按功能分类）

下面列出每个汇编指令的汇编语法、对应 opcode 宏（来自 inst.h）、编码含义、语义与副作用（周期 / 内存读写 / 标志更新 / 错误）。

说明格式：

- 汇编形式
- opcode 宏 (十六进制)
- 字节布局提示（op(2) rx(1) ry(1) constant(4)）
- 语义（C 风格伪代码）
- 副作用（周期/mem_r/mem_w/flag/error）
- 汇编示例与机器码示例（小端字节表示）

1. 程序控制 / 终止 / 空操作

- END
    - 汇编：END
    - opcode：I_END = 0x00
    - 语义：退出并打印统计
    - 副作用：打印统计并 exit(0)
    - 例：bytes = [0x00,0x00, rx, ry, c0,c1,c2,c3]（rx/ry/constant 通常为 0）
- NOP
    - 汇编：NOP
    - opcode：I_NOP = 0x01
    - 语义：空操作
    - 副作用：无
    - 额外：无

2. 输入输出（I/O）

- OTC（输出字符）
    - 汇编：OTC
    - opcode：I_OTC = 0x02
    - 语义：putchar(reg[15] & 0xFF)
    - 副作用：输出一个字符
    - 示例：将 reg[15] 的低 8 位写到 stdout
- OTI（输出整数）
    - 汇编：OTI
    - opcode：I_OTI = 0x03
    - 语义：printf("%d", reg[15])
- OTS（输出字符串）
    - 汇编：OTS
    - opcode：I_OTS = 0x04
    - 语义：fputs(&mem[reg[15]], stdout) 假设 C 风格 '\0' 结尾
    - 副作用：可能读内存直到 '\0'（但机器不计逐字节读取为 mem_r；仅指令层面未增 mem_r，但实现使用 mem 的字符串输出）
- ITC（输入非空白字符）
    - 汇编：ITC
    - opcode：I_ITC = 0x05
    - 语义：读取首个非空白字符到 reg[15]（跳过空白字符）
    - 副作用：stdin 读取
- ITI（输入整数）
    - 汇编：ITI
    - opcode：I_ITI = 0x06
    - 语义：scanf("%d", &reg[15])

3. 加减乘除与测试

- ADD reg, immediate
    - 汇编：ADD Rx, #imm 或 按汇编器语法: ADD R2, 10
    - opcode：I_ADD_0 = 0x30
    - 语义：reg[rx] = reg[rx] + constant
    - 副作用：无标志更新
    - 例：ADD R2, 10 -> opcode 0x30, rx=2, ry=0, constant=10
- ADD reg, reg
    - 汇编：ADD Rx, Ry
    - opcode：I_ADD_1 = 0x31
    - 语义：reg[rx] = reg[rx] + reg[ry]
- SUB reg, immediate
    - opcode：I_SUB_0 = 0x40
    - 语义：reg[rx] -= constant
- SUB reg, reg
    - opcode：I_SUB_1 = 0x41
    - 语义：reg[rx] -= reg[ry]
- MUL reg, immediate
    - opcode：I_MUL_0 = 0x50
    - 语义：reg[rx] \*= constant
    - 副作用：cycle += 4; mul_div++
- MUL reg, reg
    - opcode：I_MUL_1 = 0x51
    - 同上（使用 reg[ry]）
- DIV reg, immediate
    - opcode：I_DIV_0 = 0x60
    - 语义：if (constant == 0) error exit; else reg[rx] /= constant
    - 副作用：cycle += 4; mul_div++
- DIV reg, reg
    - opcode：I_DIV_1 = 0x61
    - 语义：if (reg[ry] == 0) error exit; else reg[rx] /= reg[ry]
    - 副作用：cycle += 4; mul_div++
- TST reg
    - 汇编：TST Rx
    - opcode：I_TST_0 = 0x70
    - 语义：
        - t = reg[rx];
        - if t == 0 -> reg[R_FLAG] = FLAG_EZ (0)
        - else if t < 0 -> reg[R_FLAG] = FLAG_LZ (1)
        - else -> reg[R_FLAG] = FLAG_GZ (2)
    - 副作用：设置 reg[0]，不修改 PC（除了 +8）

4. 跳转与条件跳转

- JMP LABEL
    - opcode：I_JMP_0 = 0x80
    - 语义：reg[R_IP] = constant (constant = label address)
    - 副作用：直接设置 IP（执行 switch 后使用 continue 跳过 IP+=8）
- JMP reg
    - opcode：I_JMP_1 = 0x81
    - 语义：reg[R_IP] = reg[rx]
- JEZ LABEL / JEZ reg
    - I_JEZ_0 = 0x82 (label)
    - I_JEZ_1 = 0x83 (reg)
    - 语义：若 reg[R_FLAG] == FLAG_EZ 则跳转（到 constant 或 reg[rx]）
- JLZ LABEL / JLZ reg
    - I_JLZ_0 = 0x84
    - I_JLZ_1 = 0x85
    - 语义：若 reg[R_FLAG] == FLAG_LZ 则跳转
- JGZ LABEL / JGZ reg
    - I_JGZ_0 = 0x86
    - I_JGZ_1 = 0x87
    - 语义：若 reg[R_FLAG] == FLAG_GZ 则跳转

5. 载入 / 常量 / 立即 / 寄存器拷贝 / 内存载入形式
   多个变种（LOD_0..LOD_5, LDC_3/4/5 表示载字节而非 32-bit 整数）

- LOD Rx, immediate
    - opcode：I_LOD_0 = 0x10
    - 语义：reg[rx] = constant
- LOD Rx, Ry
    - opcode：I_LOD_1 = 0x11
    - 语义：reg[rx] = reg[ry]
- LOD Rx, Ry + immediate
    - opcode：I_LOD_2 = 0x12
    - 语义：reg[rx] = reg[ry] + constant
- LOD Rx, (immediate) -> load int32 from memory address immediate
    - opcode：I_LOD_3 = 0x13
    - 语义：cycle += 9; mem*r++; reg[rx] = *(int\_)&mem[constant]
    - 说明：读取 4 字节小端整型
- LDC Rx, (immediate) -> load char (byte) into reg
    - opcode：I_LDC_3 = 0x113 (注意：宏为 0x113，超出 1 字节，但机器只读取低 16-bit 作 opcode)
    - 语义：cycle += 9; mem_r++; reg[rx] = mem[constant] （仅低 8 位）
    - 注意：虽然宏值为 0x113，但在文件/内存中写入时仍照 16-bit 存储（0x113 即两字节 0x13 0x01 小端? 实际存入时按整数的两字节低位和高位；详见下面“特殊 opcode 注意”）
- LOD Rx, (Ry)
    - opcode：I_LOD_4 = 0x14
    - 语义：cycle += 9; mem*r++; reg[rx] = *(int\_)&mem[reg[ry]]
- LDC Rx, (Ry)
    - opcode：I_LDC_4 = 0x114
    - 语义：cycle += 9; mem_r++; reg[rx] = mem[reg[ry]]
- LOD Rx, (Ry + imm)
    - opcode：I_LOD_5 = 0x15
    - 语义：cycle += 9; mem*r++; reg[rx] = *(int\_)&mem[reg[ry] + constant]
- LDC Rx, (Ry + imm)
    - opcode：I_LDC_5 = 0x115
    - 语义：cycle += 9; mem_r++; reg[rx] = mem[reg[ry] + constant]

注意 LDC*\* 的宏值在 inst.h 中写为 0x113、0x114、0x115 —— 它们的高位只是为了在宏名上区分，但真实机器使用时，instruction() 在 machine.c 里是：
op = (*(int\_)&(mem[addr])) & 0xffff;
因此 op 是取内存低 16 位，所以写入器/汇编器需要把 opcode 按 16-bit 存到指令前两字节（低字节、次字节）。例如宏 0x113 = 0x0113 → 两字节 [0x13, 0x01]。因此无需特殊处理，只需写入 16-bit 值即可。

6. 存储（写入内存）

- STO (Rx), immediate -> 写入 32-bit 整数 constant 到内存地址 reg[rx]
    - opcode：I_STO_0 = 0x20
    - 语义：cycle += 9; mem*w++; *(int\_)&mem[reg[rx]] = constant
- STC (Rx), immediate -> 写入 8-bit 常数到 mem[reg[rx]]
    - opcode：I_STC_0 = 0x120
- STO (Rx), Ry -> 写 reg[ry] (32-bit) 到 mem[reg[rx]]
    - opcode：I_STO_1 = 0x21
- STC (Rx), Ry -> mem[reg[rx]] = reg[ry] (低 8 位)
    - opcode：I_STC_1 = 0x121
- STO (Rx), Ry + imm -> *(int*)&mem[reg[rx]] = reg[ry] + constant
    - opcode：I_STO_2 = 0x22
- STC (Rx), Ry + imm
    - opcode：I_STC_2 = 0x122
- STO (Rx + imm), Ry -> *(int*)&mem[reg[rx] + constant] = reg[ry]
    - opcode：I_STO_3 = 0x23
- STC (Rx + imm), Ry
    - opcode：I_STC_3 = 0x123

和 LDC 一样，STC 的宏值如 0x120/0x121/0x122/0x123 是 16-bit 值，汇编器应写入两字节按小端序放到指令前两字节。

7. 伪/数据指令（汇编器支持）

- DBN value, count
    - 汇编：DBN <byte_value>, <repeat_count>
    - 语义：输出 <byte_value>（按 1 字节）重复 count 次到目标二进制流（用在汇编器生成数据区）
    - 示例：DBN 0x20, 4 -> 输出 4 个字节 0x20
- DBS v1, v2, v3, ...
    - 汇编：DBS <byte1>, <byte2>, ...
    - 语义：依次输出这些 1 字节值
    - 在 asm.y 中，DBS 被当作顺序产生字节，DBN 可以重复某字节

8. 特殊注意 - opcode 宏超过 0xff/0xfff

- 某些宏写为 0x113 等（超过 8-bit），但机器在内存里读取 opcode 时使用下述逻辑：
  op = (_(int_)&(mem[addr])) & 0xffff;
  因此 op 占 16-bit，汇编器需把宏值写入 16-bit（两字节），例如 0x113 -> bytes 0x13 0x01（低字节 0x13 首）。确认汇编器在生成文件时以小端写入 opcode 的低 16 位。

9. 示例编码与二进制表示（小端）
   指令总体格式 8 字节： [op_low, op_high, rx, ry, const0, const1, const2, const3]

举例：

- 示例：ADD R2, 10 （使用 I_ADD_0 = 0x30，rx=2, ry=0, constant=10）
    - op 0x0030 -> bytes 0x30 0x00
    - rx = 0x02
    - ry = 0x00
    - constant 10 -> 0x0A 0x00 0x00 0x00
    - 最终字节序列：30 00 02 00 0A 00 00 00
- 示例：LDC R3, (100)
    - opcode I_LDC_3 = 0x0113 (0x113)
    - bytes: 0x13 0x01 0x03 0x00 0x64 0x00 0x00 0x00 （0x64 = 100）
- 示例：JMP label (假设 label 地址为 0x0040)
    - opcode I_JMP_0 = 0x80 -> bytes 0x80 0x00
    - rx=0x00, ry=0x00
    - constant = 0x00000040 -> bytes 0x40 0x00 0x00 0x00

10. 汇编语法映射（基于 asm.l / asm.y）

- 指令名字大写，操作数用逗号分隔，寄存器使用 `R<number>`，立即数为十进制常数或标签（LABEL）。
- 括号寻址：
    - LOD R1, (100) -> LOD_3
    - LOD R1, (R2) -> LOD_4
    - LOD R1, (R2 + 4) -> LOD_5
    - LOD R1, R2 + 4 -> LOD_2
    - STO (R3), R4 -> STO_1
    - STO (R3 + 8), R4 -> STO_3 with constant=8
    - STC 同 STO，但写入单字节
- 标签：
    - 定义： label:
    - 使用： 指令可以使用 LABEL 作为立即数（汇编器第一遍解析为地址，第二遍填充 constant）

11. 汇编器实现要点（为编译器提供）

- 指令生成函数：统一 emit_instruction(opcode_16, rx, ry, constant32) -> 写入 8 字节（小端）
- 标签解析：两遍扫描（第一遍记录 label 地址 ip；第二遍填充常量）
- ip 初始为 0；每写一条指令 ip += 8；DBN/DBS 增加 ip 按写的字节数
- 当写 DBN/DBS，注意它们直接写字节到输出（不对齐），但机器取指按 8 字节为单位；编译器通常在数据段与代码段管理上保证 IP 对齐或在程序布局时负责
- 字面立即数与符号：constant 是 32-bit 带符号整数，允许负数（LDC 被用在负 offset 的情形在 asm.y 中用 -($7) 生成负值）
- 端序：写入 constant 与 opcode 时必须小端
- opcode 宏写入：写入为 16-bit（低字节先），例如 for I_LDC_3 (0x113) write bytes [0x13, 0x01]
- 错误检查：在编译器中可检查对 reg number 的范围（0..15），立即数是否超出 32 位等

12. 边界/实现注意与建议（写给编译器作者）

- 对寄存器编号进行严格检查（0..15），并拒绝非法寄存器。
- 对标签：禁止重复定义（汇编器应在第一遍发现重复报错）。
- 对内存访问：生成内存地址前在编译器层面验证不产生负地址；runtime 无越界检查，注意安全。
- 指令对齐：因为 CPU 以字节数组读取指令且 reg[R_IP] 可任意，建议把代码段按照 8 字节对齐，或记录实际的 IP 值并基于它生成跳转目标（这就是汇编器做的事）。
- LDC/STC 与 LOD/STO 区别：LDC/STC 读写单字节（char），LOD/STO 读写 32-bit 整数。编译器应根据数据类型选择合适指令。
- 字符/字符串 I/O：OTS 将从 mem[reg[15]] 输出一个以 0 结尾的字符串；编译器生成字符串常量时，要把字符串写入数据段并把地址置到某个寄存器再调用 OTS。
- 常量池/数据段：DBS/DBN 可用于生成字符串/字节数组，编译器应在数据段维护相对地址并把地址载入寄存器。

---

## 速查表（紧凑版）

（opcode 写十六进制，括号为宏名）

- 0x00 I_END: END
- 0x01 I_NOP: NOP
- 0x02 I_OTC: OTC
- 0x03 I_OTI: OTI
- 0x04 I_OTS: OTS
- 0x05 I_ITC: ITC
- 0x06 I_ITI: ITI
- 0x10 I_LOD_0: LOD R, imm
- 0x11 I_LOD_1: LOD R, R
- 0x12 I_LOD_2: LOD R, R + imm
- 0x13 I_LOD_3: LOD R, (imm) (mem int32)
- 0x113 I_LDC_3: LDC R, (imm) (mem byte)
- 0x14 I_LOD_4: LOD R, (R) (mem int32)
- 0x114 I_LDC_4: LDC R, (R) (mem byte)
- 0x15 I_LOD_5: LOD R, (R + imm) (mem int32)
- 0x115 I_LDC_5: LDC R, (R + imm) (mem byte)
- 0x20 I_STO_0: STO (R), imm (mem int32)
- 0x120 I_STC_0: STC (R), imm (mem byte)
- 0x21 I_STO_1: STO (R), R
- 0x121 I_STC_1: STC (R), R
- 0x22 I_STO_2: STO (R), R + imm
- 0x122 I_STC_2: STC (R), R + imm
- 0x23 I_STO_3: STO (R + imm), R
- 0x123 I_STC_3: STC (R + imm), R
- 0x30 I_ADD_0: ADD R, imm
- 0x31 I_ADD_1: ADD R, R
- 0x40 I_SUB_0: SUB R, imm
- 0x41 I_SUB_1: SUB R, R
- 0x50 I_MUL_0: MUL R, imm
- 0x51 I_MUL_1: MUL R, R
- 0x60 I_DIV_0: DIV R, imm
- 0x61 I_DIV_1: DIV R, R
- 0x70 I_TST_0: TST R
- 0x80 I_JMP_0: JMP imm
- 0x81 I_JMP_1: JMP R
- 0x82 I_JEZ_0: JEZ imm
- 0x83 I_JEZ_1: JEZ R
- 0x84 I_JLZ_0: JLZ imm
- 0x85 I_JLZ_1: JLZ R
- 0x86 I_JGZ_0: JGZ imm
- 0x87 I_JGZ_1: JGZ R

## 简短示例（汇编片段与说明）

1. 将常数 42 加到 R2：

- 汇编：ADD R2, 42
- 机器字节：30 00 02 00 2A 00 00 00

2. 把内存地址 200 处的 32-bit 值载入 R3：

- 汇编：LOD R3, (200)
- 机器字节：13 00 03 00 C8 00 00 00

3. 条件跳转示例（if R5 == 0 goto label）

- 汇编序列：
  TST R5
  JEZ label
- 机器：
  TST -> 70 00 05 00 00 00 00 00
  JEZ label -> 82 00 00 00 <label_addr 4 bytes>

4. 写字符串到数据段并输出（概念）

- 数据段（使用 DBS）: label_str: DBS 'H','i',0
- 代码:
  LOD R15, label_str ; reg[15] = addr
  OTS
- 汇编会将 label_str 地址放入 constant
