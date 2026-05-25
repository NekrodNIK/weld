project(
  'weld',
  'cpp',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : [
    'warning_level=3', 'cpp_std=c++23',
    'optimization=0', 
    'debug=false',
  ],
)

srcs = [
  'src/cli.cc',
  'src/input-file.cc',
  'src/mapped-file.cc',
  'src/elf.cc',
  'src/arch.cc',
  'src/output-file.cc',
  'src/archive.cc',
]
incdirs = include_directories('include', '.')

if target_machine.system() == 'windows'
  error('Windows is not supported')
else
  srcs += ['src/mapped-file-unix.cc']
endif

weld_lib = static_library('weld_lib', srcs, include_directories : incdirs)
exe = executable(
  'weld',
  ['src/main.cc'],
  link_with: weld_lib,
  install : true,
  include_directories: incdirs,
)

subdir('tests')
