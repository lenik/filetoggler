wx_dep = dependency('wxwidgets', modules : ['base', 'core'], required : true)

executable('filetoggler',
    [
        'main.cpp',
        'core.cpp',
        'cli.cpp',
        'gui.cpp',
    ],
    dependencies : [wx_dep],
    install : true)

# Create ft symlink to filetoggler
meson.add_install_script('sh', '-c',
    'ln -sf filetoggler "$MESON_INSTALL_DESTDIR_PREFIX/@0@/ft"'.format(get_option('bindir')))
