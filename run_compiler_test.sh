#!/bin/sh

# 立即因错误而退出
set -e

if [ "$#" -ne 6 ]; then
    echo "Usage: $0 <compiler> <asm> <machine> <m-file> <in-file> <expected-out-file>"
    exit 1
fi

COMPILER_PATH=$1
ASM_PATH=$2
MACHINE_PATH=$3
M_FILE_PATH=$4
IN_FILE_PATH=$5
EXPECTED_OUT_FILE_PATH=$6

# 设置临时工作区
WORK_DIR=$(mktemp -d -t cyrilcc_test)

# --- 3. 定义独特的文件名 ---
ASM_FILE="$WORK_DIR/test_output.s"
OBJ_FILE="$WORK_DIR/test_output.o"
ACTUAL_RAW_OUT_FILE="$WORK_DIR/actual_raw.out"
ACTUAL_PARSED_OUT_FILE="$WORK_DIR/actual_parsed.out"

# --- 4. 执行编译-汇编-运行链 ---

echo "--- Compiling: $M_FILE_PATH ---"
$COMPILER_PATH $M_FILE_PATH -o $ASM_FILE

echo "--- Assembling: $ASM_FILE ---"
$ASM_PATH $ASM_FILE


echo "--- Running: $OBJ_FILE (Input: $IN_FILE_PATH) ---"
$MACHINE_PATH $OBJ_FILE < "$IN_FILE_PATH" > "$ACTUAL_RAW_OUT_FILE"

echo "--- Parsing Output ---"
# 使用 awk 打印所有行，直到遇到包含 "---" 的行
awk '/---/{exit} {print}' "$ACTUAL_RAW_OUT_FILE" > "$ACTUAL_PARSED_OUT_FILE"

echo "--- Comparing Results ---"
if diff -u -B "$EXPECTED_OUT_FILE_PATH" "$ACTUAL_PARSED_OUT_FILE"; then
    echo "SUCCESS: $M_FILE_PATH"
    exit 0 # 通过
else
    echo "FAILURE: $M_FILE_PATH"
    echo "--- Raw machine output was: ---"
    cat "$ACTUAL_RAW_OUT_FILE"
    echo "-------------------------------"
    exit 1 # 失败
fi
