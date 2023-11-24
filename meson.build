project('nora', 'c')

little_endian = target_machine.endian() == 'little'
big_endian = target_machine.endian() == 'big'


add_project_arguments([
	'-DWLR_USE_UNSTABLE',
	'-DWLR_LITTLE_ENDIAN=@0@'.format(little_endian.to_int()),
	'-DWLR_BIG_ENDIAN=@0@'.format(big_endian.to_int()),
], language: 'c')

wlroots_proj = subproject('wlroots')
wlroots_dep = wlroots_proj.get_variable('wlroots')


dependencies = [wlroots_dep, dependency('wayland-server')]

common_files = []

subdir('protocol')
subdir('proxies')

executable(
  'nora', 
  [
    'nora/main.c', 
    'nora/server.c', 
    'nora/view.c', 
    'nora/output.c', 
    'nora/input.c', 
    'nora/desktop.c', 
    'nora/desktop/manager.c',
    common_files,
  ], 
  dependencies: dependencies,
)
