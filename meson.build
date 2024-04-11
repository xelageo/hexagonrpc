project('fastrpc', 'c')

include = include_directories('include')
client_target = get_option('libexecdir') / 'hexagonrpc'

cflags = ['-Wall', '-Wextra', '-Wpedantic', '-Wno-unused-parameter']

subdir('libhexagonrpc')
subdir('hexagonrpcd')
subdir('chrecd')

subdir('tests')
