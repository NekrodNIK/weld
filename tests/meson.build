incdirs = get_variable('incdirs')
weld_lib = get_variable('weld_lib')

tests = {
  'hashmap-test': ['hashmap-test.cc'],
  'thread-pool-test': ['thread-pool-test.cc'],
}

gtest_dep = dependency('gtest', main : true)

foreach name, srcs : tests 
  test_exe = executable(
    name,
    srcs,
    dependencies : [gtest_dep],
    build_by_default : false,
    include_directories : incdirs
  )
  test(name, test_exe)
endforeach
