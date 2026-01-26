test_sources = [
    'test_core.cpp',
]

wx_dep = dependency('wxwidgets', modules : ['base', 'core'], required : true)

test_exe = executable('filetoggler_tests',
    test_sources + [
        '../src/core.cpp',
    ],
    include_directories : include_directories('..', '../src'),
    dependencies : [wx_dep],
    install : false)

test('filetoggler_tests', test_exe)
