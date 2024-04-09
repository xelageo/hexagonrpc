valgrind = find_program('valgrind', required : false)

if valgrind.found()
  add_test_setup('valgrind', exe_wrapper : valgrind)
endif

test_iobuffer = executable('test_iobuffer',
  'test_iobuffer.c',
  '../hexagonrpcd/iobuffer.c',
  c_args : ['-Wall', '-Wextra', '-Wpedantic', '-Wno-unused-parameter'],
  include_directories : include,
)

test('iobuffer', test_iobuffer)
