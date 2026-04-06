inc += include_directories('.')

pkg_sources += files(
  'api.c',
  'dedup.c',
  'json_types.c',
  'parser.c',
  'rl_jsonBuildInfo.c',
  'rl_json.c',
)

if get_option('cbor')
  pkg_sources += files(
    'cbor.c',
    'bignum_ops.c'
  )
endif

