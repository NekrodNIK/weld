incdirs = get_variable('incdirs')
weld_lib = get_variable('weld_lib')

tests = ['hashmap-test.cc', 'thread-pool-test.cc']
fuzz_targets = {
  'archive-fuzz': ['fuzz/archive-fuzz.cc']
}
unix_tests = []

if target_machine.system() == 'windows' 
  error('Windows is not supported')
else
  srcs += unix_tests
endif

gtest_dep = dependency('gtest', main : true)
test_exe = executable(
  'tests',
  tests,
  dependencies : [gtest_dep],
  build_by_default : false,
  include_directories : incdirs
)
test('gtest test', test_exe)

fuzzing = get_option('fuzzing')
compiler_is_clang = meson.get_compiler('cpp').get_id() == 'clang'

if fuzzing and compiler_is_clang
  foreach name, srcs : fuzz_targets
    fuzz_exe = executable(
      name,
      srcs,
      link_with: weld_lib,
      dependencies : [],
      override_options : ['b_lundef=false'],
      cpp_args : ['-fsanitize=fuzzer,address,undefined'],
      link_args : ['-fsanitize=fuzzer,address,undefined'],
      build_by_default : false,
      include_directories : incdirs
    )
  endforeach
endif
