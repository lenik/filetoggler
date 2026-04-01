project('filetoggler', ['cpp', 'c'],
    version : '1.0.1',
    default_options : ['cpp_std=c++20', 'warning_level=2', 'optimization=0'])

bas_c_dep = dependency('bas-c', required : true)
glib_dep = dependency('glib-2.0', required : true)
wx_dep = dependency('wxwidgets', modules : ['base', 'core'], required : true)

# Generate config.h with version
conf_data = configuration_data()
conf_data.set_quoted('FILETOGGLER_VERSION', meson.project_version())
configure_file(
    output : 'config.h',
    configuration : conf_data,
    install : false)

executable('filetoggler',
    [
        'src/main.cpp',
        'src/core.cpp',
        'src/cli.cpp',
        'src/gui.cpp',
    ],
    dependencies : [bas_c_dep, wx_dep, glib_dep],
    include_directories : include_directories('.'),
    install : true)

# Create ft symlink to filetoggler
meson.add_install_script('sh', '-c',
    'ln -sf filetoggler "$MESON_INSTALL_DESTDIR_PREFIX/@0@/ft"'.format(get_option('bindir')))

install_data('completions/filetoggler.bash',
    install_dir : join_paths(get_option('datadir'), 'bash-completion', 'completions'))

install_data('completions/ft.bash',
    install_dir : join_paths(get_option('datadir'), 'bash-completion', 'completions'))

install_man('filetoggler.1')

# Create ft.1 symlink to filetoggler.1
meson.add_install_script('sh', '-c',
    'ln -sf filetoggler.1 "$MESON_INSTALL_DESTDIR_PREFIX/@0@/man1/ft.1"'.format(get_option('mandir')))

subdir('tests')
