project(
    'cyrilcc',
    ['cpp', 'c'],
    version: '1.0',
    default_options: ['buildtype=debug', 'cpp_std=c++23'],
)

parser_dir = meson.current_build_dir()

flex_prog = find_program('flex')
bison_prog = find_program('bison')

# bison -d -o parser.cpp --defines=parser.h parser.y
gen_bison = generator(
    bison_prog,
    output: ['@BASENAME@.cpp', '@BASENAME@.h'],
    arguments: [
        '-d',
        '-o', '@OUTPUT0@',
        '--defines=@OUTPUT1@',
        '@INPUT@',
    ],
)

parser_files = gen_bison.process('src/parser.y')

# flex -o lexer.cpp --header-file=lexer.h lexer.l
gen_flex = generator(
    flex_prog,
    output: ['@BASENAME@.cpp', '@BASENAME@.h'],
    arguments: ['-o', '@OUTPUT0@', '--header-file=@OUTPUT1@', '@INPUT@'],
)

lexer_files = gen_flex.process('src/lexer.l')

inc_dirs = include_directories('src', '.')

cyrilcc = executable(
    'cyrilcc',
    'src/main.cpp',
    'src/ast.cpp',
    parser_files,
    lexer_files,
    include_directories: inc_dirs,
    link_args: ['-L/opt/homebrew/opt/llvm/lib/c++'],
)

asm_inc_dirs = include_directories('asm-machine')
asm_gen_bison = generator(
    bison_prog,
    output: ['@BASENAME@.y.c', '@BASENAME@.y.h'],
    arguments: [
        '-d',
        '-o', '@OUTPUT0@',
        '--defines=@OUTPUT1@',
        '@INPUT@',
    ],
)
asm_parser_files = asm_gen_bison.process('asm-machine/asm.y')
asm_gen_flex = generator(
    flex_prog,
    output: '@BASENAME@.l.c',
    arguments: ['-o', '@OUTPUT0@', '@INPUT@'],
)
asm_lexer_files = asm_gen_flex.process('asm-machine/asm.l')
asm_exe = executable(
    'asm',
    asm_parser_files, # 包含 asm.y.c 和 asm.y.h
    asm_lexer_files, # 包含 asm.l.c
    include_directories: asm_inc_dirs, # 查找 inst.h
    install: true,
)

machine_exe = executable(
    'machine',
    'asm-machine/machine.c',
    include_directories: asm_inc_dirs, # 查找 inst.h
    install: true,
)

test_runner_script = find_program('run_compiler_test.sh')
test_case_dir = '../test/testcase'

m_files = [
    'struct-arr.m',
    'while.m',
    'char.m',
    'func-int.m',
    'switch.m',
    'for-bc.m',
    'arr-struct.m',
    'if.m',
    'while-bc.m',
    'char-int.m',
    'for.m',
    'arr.m',
    'ptr-char-int.m',
    'ptr-char.m',
    'func-char.m',
    'ptr-int.m',
    'struct-ptr.m',
    'struct.m',
    'int.m',
    'arr-while.m',
]

foreach m_file : m_files
    base_name = m_file.split('.').get(0)
    test_name = base_name
    in_file = test_case_dir + '/' + base_name + '.in'
    out_file = test_case_dir + '/' + base_name + '.out'

    test(
        test_name,
        test_runner_script,
        args: [
            cyrilcc,
            asm_exe,
            machine_exe,
            test_case_dir + '/' + m_file,
            in_file,
            out_file,
        ],
        depends: [cyrilcc, asm_exe, machine_exe],
        timeout: 1,
    )
endforeach