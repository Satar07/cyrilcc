project(
    'cyrilcc',
    'cpp',
    version: '1.0',
    default_options: ['buildtype=debug', 'cpp_std=c++23'],
)

parser_dir = meson.current_build_dir()

flex_prog = find_program('flex', dirs: '/opt/homebrew/opt/flex/bin')
bison_prog = find_program('bison', dirs: '/opt/homebrew/opt/bison/bin')

# bison -d -o parser.cpp --defines=parser.h parser.y
gen_bison = generator(
    bison_prog,
    output: ['@BASENAME@.cpp', '@BASENAME@.h'],
    arguments: [
        '-Wcounterexamples',
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

executable(
    'cyrilcc',
    'src/main.cpp',
    'src/ast.cpp',
    'src/ir.cpp',
    'src/ir_dumper.cpp',
    'src/asm_gen.cpp',
    parser_files,
    lexer_files,
    include_directories: inc_dirs,
    link_args: ['-L/opt/homebrew/opt/llvm/lib/c++'],
)