proxy_dependencies = [dependency('wayland-client'), dependency('libsystemd'), dependency('threads')]

executable(
  'nora-proxies',
  [
    'main.c',
    'proxy.c',

    'desktop-management.c',
    'desktop-management-getters.c',
    'desktop-management-methods.c',

    'output-management.c',
    'output-management-getters.c',
    common_files,
  ],
  dependencies: proxy_dependencies,
)
