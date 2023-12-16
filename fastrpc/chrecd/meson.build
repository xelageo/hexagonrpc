executable('chrecd',
  'interfaces.c',
  'main.c',
  c_args : ['-Wall', '-Wextra', '-Wpedantic'],
  include_directories : include,
  install : true,
  link_with : libhexagonrpc,
  install_dir : client_target,
)
