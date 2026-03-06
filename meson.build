project(
  'weld',
  'cpp',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

srcs = [
  'src/main.cc'
]
tests = [
  'tests/test_elf.cc'
]

gtest_dep = dependency('gtest', main : true)

exe = executable(
  'weld',
  'src/main.cc',
  install : true,
)

test_exe = executable(
  'tests',
  tests,
  dependencies : [gtest_dep]
)
test('gtest test', test_exe)
