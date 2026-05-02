tests = ['hashmap-test.cc', 'thread-pool-test.cc']
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
  dependencies : [gtest_dep]
)
test('gtest test', test_exe)
