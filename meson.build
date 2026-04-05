project(
  'weld',
  'cpp',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

srcs = [
  'src/main.cc',
  'src/input-file.cc',
  'src/mapped-file.cc',
  'src/elf.cc',
  'src/error.cc',
  'src/cli_parser.cc',
  'src/arch.cc',
]

if target_machine.system() == 'windows'
  error('Windows is not supported')
else
  srcs += ['src/mapped-file-unix.cc']
endif

incdir = include_directories('include')
exe = executable(
  'weld',
  srcs,
  install : true,
  include_directories: incdir,
)


subdir('tests')
