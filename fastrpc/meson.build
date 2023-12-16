project('fastrpc', 'c')

include = include_directories('include')
client_target = get_option('libexecdir') / 'hexagonrpc'

subdir('libhexagonrpc')
subdir('hexagonrpcd')
subdir('chrecd')

subdir('tests')
