project('filetoggler', ['cpp', 'c'],
    version : '1.0.0',
    default_options : ['cpp_std=c++20', 'warning_level=2', 'optimization=0'])

subdir('src')
subdir('tests')

install_data('completions/filetoggler.bash',
    install_dir : join_paths(get_option('datadir'), 'bash-completion', 'completions'))

install_data('completions/ft.bash',
    install_dir : join_paths(get_option('datadir'), 'bash-completion', 'completions'))

install_man('filetoggler.1')

# Create ft.1 symlink to filetoggler.1
meson.add_install_script('sh', '-c',
    'ln -sf filetoggler.1 "$MESON_INSTALL_DESTDIR_PREFIX/@0@/man1/ft.1"'.format(get_option('mandir')))
