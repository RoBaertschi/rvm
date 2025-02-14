project('rvm', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

rvm_lib = library('rvm', ['rvm.cpp'], install: true)
rvm_inc = include_directories('.')
rvm_dep = declare_dependency(link_with : [rvm_lib], include_directories : [rvm_inc])

executable('rvm',
           ['main.cpp'],
           install : true, dependencies : [rvm_dep])

tests = executable('rvm_tests', ['tests.cpp'], dependencies : [rvm_dep])

test('rvm tests', tests)

ftxui_screen_dep = dependency('ftxui-screen')
ftxui_dom_dep = dependency('ftxui-dom')
ftxui_component_dep = dependency('ftxui-component')

rvm_create = executable('rvm_create', ['rvm_create.cpp'], install : true, dependencies : [rvm_dep, ftxui_screen_dep, ftxui_dom_dep, ftxui_component_dep])
