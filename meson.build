# PRISM on the STM32H7B3I-DK. Spike stage: does PRISM's core cross-compile for a Cortex-M7, and
# how big is it? PRISM's own meson.build always builds its SDL backend, so this builds PRISM's
# core sources itself, from a PRISM checkout (option prism_dir; Meson forbids compiling files
# that sit under subprojects/ from the parent project).
# simplify: the source list is copied from PRISM's src/meson.build; replace with
# subproject('prism') (subprojects/prism.wrap) once PRISM has an option to build without SDL.
project('prism-stm32', 'cpp',
  version : '0.0.1',
  default_options : ['cpp_std=c++26', 'warning_level=2', 'optimization=s', 'b_staticpic=false',
                     'libunibreak:werror=false', 'libunibreak:warning_level=0'],
  meson_version : '>= 1.5.0',
)

prism_root = get_option('prism_dir')
prism_inc = include_directories(prism_root / 'include')

deps = [
  dependency('stdexec', fallback : ['stdexec', 'stdexec_dep']),
  dependency('fmt', fallback : ['fmt', 'fmt_header_only_dep'], default_options : ['header-only=true']),
  dependency('libunibreak', fallback : ['libunibreak', 'unibreak_dep'], default_options : ['default_library=static']),
  dependency('sheenbidi', fallback : ['sheenbidi', 'sheenbidi_dep'], default_options : ['default_library=static']),
  dependency('glaze', fallback : ['glaze', 'glaze_dep']),
  dependency('magic_enum', fallback : ['magic_enum', 'magic_enum_dep']),
]

prism_sources = []
foreach f : get_option('prism_sources')
  prism_sources += files(prism_root / 'src' / f)
endforeach

prism_core = static_library('prism_core', prism_sources,
  include_directories : prism_inc,
  dependencies : deps,
)

executable('spike.elf', 'spike/main.cpp',
  include_directories : prism_inc,
  link_with : prism_core,
  dependencies : deps,
)
