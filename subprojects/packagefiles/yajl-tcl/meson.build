project('yajltcl', 'c',
  version:          '1.8.1',
  meson_version:    '>=0.64.0',
  default_options:  ['warning_level=1', 'werror=false'],
)

fs = import('fs')

tcl_dep  = dependency('tcl',  version: '>=8.5')
yajl_dep = dependency('yajl', version: '>=2.0', default_options: ['warning_level=1', 'werror=false'])

deps = []

# config.h required by the C sources
conf = configuration_data()
conf.set_quoted('PACKAGE_NAME',     meson.project_name())
conf.set_quoted('PACKAGE_VERSION',  meson.project_version())
conf.set('USE_TCL_STUBS', 1)
conf.set('MODULE_SCOPE',  'extern')
configure_file(output: 'config.h', configuration: conf)

yajltcl_mod = shared_module(meson.project_name(),
  'generic/yajltcl.c',
  'generic/tclyajltcl.c',
  'generic/yajltcllex.c',
  dependencies:         [tcl_dep, yajl_dep],
  include_directories:  include_directories('generic'),
  #override_options:     ['warning_level=1', 'werror=false'],
)

# Place pkgIndex.tcl and yajl.tcl alongside the built module
pkgindex_conf = configuration_data()
pkgindex_conf.set('PACKAGE_VERSION',  meson.project_version())
pkgindex_conf.set('PKG_LIB_FILE',     fs.name(yajltcl_mod.full_path()))
configure_file(input: 'pkgIndex.tcl.in',  output: 'pkgIndex.tcl', configuration: pkgindex_conf)
configure_file(input: 'yajl.tcl',         output: 'yajl.tcl',     copy: true)

# Wrap fallback dependency
yajl_tcl_dep = declare_dependency(dependencies: deps)
meson.override_dependency('yajl-tcl', yajl_tcl_dep)

# Extend devenv so 'package require yajltcl' works during build
# TCLLIBPATH is a Tcl list (space-separated)
devenv = environment()
devenv.prepend('TCLLIBPATH', meson.current_build_dir(), separator: ' ')
meson.add_devenv(devenv)
