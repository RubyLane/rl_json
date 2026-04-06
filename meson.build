project('rl_json', 'c',
  version:            '0.16',
  default_options:    ['c_std=c23,c2x,c17,c11','warning_level=3','werror=true'],
  meson_version:      '>=1.3.0',
)

fs = import('fs')
cc = meson.get_compiler('c')

if get_option('debugmode')
  add_project_arguments(cc.get_supported_arguments(['-ggdb3']), language: 'c')
endif

add_project_arguments('-Wno-missing-field-initializers', language: 'c')
add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

pkg_sources = []
conf = configuration_data()        # For C code (config.h)

# Options (see meson.options) <<<
conf.set10('DEBUG',    get_option('debugmode'))
conf.set10('DEDUP',    get_option('dedup'))
conf.set10('ENSEMBLE', get_option('ensemble'))
conf.set10('TESTMODE', get_option('testmode'))
conf.set10('UNLOAD',   get_option('unload'))
# Options >>>

tea_want_shims = [
  'tip445',
  'Tcl_GetBytesFromObj',
]

deps = []
inc = []

subdir('teabase/init')

# Feature detection (must come after teabase/init which provides tcl_dep) <<<
conf.set10('HAVE___BUILTIN_FFSLL',      cc.has_function('__builtin_ffsll'))
conf.set10('HAVE_FFSLL',                cc.has_function('ffsll', prefix: '#include <strings.h>'))
conf.set10('HAVE_TCL_GETNUMBERFROMOBJ', cc.has_function('Tcl_GetNumberFromObj', dependencies: tcl_dep))
conf.set10('HAVE_UNISTD_H',             cc.has_header('unistd.h'))
# Feature detection >>>

# Package config values compiled into binary
conf.set_quoted('RL_JSON_INCLUDE_PATH_INSTALL', pkg_dir)
conf.set_quoted('RL_JSON_LIBRARY_PATH_INSTALL', pkg_dir)
conf.set_quoted('RL_JSON_PACKAGE_PATH_INSTALL', pkg_dir)
conf.set_quoted('RL_JSON_LIBRARY',              meson.project_name())
conf.set_quoted('RL_JSON_STUBLIB',              meson.project_name() + 'stub')
conf.set_quoted('RL_JSON_HEADER',               'rl_json.h')
conf.set_quoted('RL_JSON_INCLUDE_SPEC',         '-I' + pkg_dir)
conf.set_quoted('RL_JSON_BUILD_INCLUDE_SPEC',   '-I' + meson.project_build_root())
conf.set_quoted('RL_JSON_LIB_SPEC',             '-L' + pkg_dir)
conf.set_quoted('RL_JSON_BUILD_LIB_SPEC',       '-L' + meson.project_build_root() / 'teabase')
conf.set_quoted('RL_JSON_STUB_LIB_SPEC',        '-L' + pkg_dir)
conf.set_quoted('RL_JSON_BUILD_STUB_LIB_SPEC',  '-L' + meson.project_build_root() / 'teabase')

# Stubs generation
deps += declare_dependency(
  sources: custom_target('genstubs',
    output: ['rl_jsonStubInit.c', 'rl_jsonDecls.h'],
    command: [
      build_tclsh,
      meson.project_source_root() / 'teabase/tools/genStubs.tcl',
      meson.project_build_root(),
      meson.project_source_root() / 'generic/rl_json.decls',
    ],
    install: true,
    install_dir: [false, pkg_dir],
  ),
)

subdir('generic')

if get_option('debugmode')
  pkg_sources += files('teabase/names.c')
endif

# Generate version UUID via configure_file <<<
git = find_program('git', native: true, required: false)

uuid_val = 'unknown'
if git.found()
  git_hash = run_command(git, '-C', meson.project_source_root(),
    'rev-parse', 'HEAD', check: false)
  if git_hash.returncode() == 0
    uuid_val = 'git-' + git_hash.stdout().strip()
  endif
endif

uuid_conf = configuration_data()
uuid_conf.set('RL_JSON_VERSION_UUID', uuid_val)

configure_file(
  output: 'rl_jsonUuid.h',
  configuration: uuid_conf,
  input: 'rl_jsonUuid.h.in',
)

# Build root has generated rl_jsonUuid.h and config.h
inc += include_directories('.')
# Generate version UUID >>>

# Install public header to versioned package directory
install_data(
    'generic/rl_json.h',
    install_dir: pkg_dir,
)

# teabase builds the shared module and generates config.h
subdir('teabase')

# Build and install stubs library for dependent extensions (after config.h)
stubs_lib = static_library(meson.project_name() + 'stub',
  'generic/rl_jsonStubLib.c',
  dependencies:         deps,
  include_directories:  inc,
  install:              true,
  install_dir:          pkg_dir,
)

# Generate relocatable pkg-config file for stub library
pc_conf = configuration_data()
pc_conf.set('PACKAGE_NAME',     meson.project_name())
pc_conf.set('PACKAGE_VERSION',  meson.project_version())
pc_conf.set('PKG_DIR_BASENAME', meson.project_name() + '-' + meson.project_version())
pc_conf.set('STUBLIB',          meson.project_name() + 'stub')
configure_file(
  input:          meson.project_name() + '.pc.in',
  output:         meson.project_name() + '.pc',
  configuration:  pc_conf,
  install_dir:    tcl_dep.get_variable('libdir') / 'pkgconfig',
  install:        true,
)

subdir('doc')

# vim: foldmethod=marker foldmarker=<<<,>>> shiftwidth=2 expandtab
