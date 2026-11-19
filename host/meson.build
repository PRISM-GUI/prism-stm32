# Host (native) build of the rasterizer and its tests: the raster library, PRISM's core from a
# PRISM checkout (prism_dir; the same source list as the root meson.build's prism_sources
# option), the doctest tests, and render_demo (writes a demo.ppm of a laid-out snapshot).
#
# The raster/, scripts/ and assets/ files live one level up -- this is a separate meson project
# so the board build's cross file never applies to it. Meson accepts files() outside the project
# root as absolute paths, exactly as the root build does for prism_dir.
#
# Build & run:
#   meson setup host/build -Dprism_dir=/path/to/PRSIM
#   ninja -C host/build && meson test -C host/build
#   host/build/render_demo demo.ppm
#
# NOTE on subprojects: host/subprojects holds copies of the root project's .wrap files and
# packagefiles plus doctest.wrap (from PRISM's subprojects/). It can instead be a symlink to
# ../subprojects -- the root subprojects/ carries doctest.wrap too -- if that is preferred.
project('prism-stm32-host', 'cpp',
  version : '0.0.1',
  default_options : ['cpp_std=c++26', 'warning_level=2', 'optimization=2',
                     'libunibreak:werror=false', 'libunibreak:warning_level=0'],
  meson_version : '>= 1.5.0',
)

# The prism-stm32 checkout this host/ lives in; normalized, since Meson takes 'host/..' for
# a directory inside this project and refuses it as an include directory.
repo = import('fs').parent(meson.current_source_dir())
prism_root = get_option('prism_dir')
prism_inc = include_directories(prism_root / 'include')
repo_inc = include_directories(repo)

deps = [
  dependency('stdexec', fallback : ['stdexec', 'stdexec_dep']),
  dependency('fmt', fallback : ['fmt', 'fmt_header_only_dep'], default_options : ['header-only=true']),
  dependency('libunibreak', fallback : ['libunibreak', 'unibreak_dep'], default_options : ['default_library=static']),
  dependency('sheenbidi', fallback : ['sheenbidi', 'sheenbidi_dep'], default_options : ['default_library=static']),
  dependency('glaze', fallback : ['glaze', 'glaze_dep']),
  dependency('magic_enum', fallback : ['magic_enum', 'magic_enum_dep'], default_options : ['test=false']),
]
doctest_dep = dependency('doctest', fallback : ['doctest', 'doctest_dep'])

prism_sources = []
foreach f : get_option('prism_sources')
  prism_sources += files(prism_root / 'src' / f)
endforeach

prism_core = static_library('prism_core', prism_sources,
  include_directories : prism_inc,
  dependencies : deps,
)

# The raster library, as the board build makes it (embedded fonts via scripts/embed.py).
python3 = find_program('python3')
embedded_font_sources = [
  custom_target('embedded_noto_sans_latin',
    input : files(repo / 'assets' / 'NotoSans-Latin.ttf'),
    output : 'embedded_noto_sans_latin.cpp',
    command : [python3, files(repo / 'scripts' / 'embed.py'), '@INPUT@', '@OUTPUT@', 'noto_sans_latin'],
  ),
  custom_target('embedded_noto_sans_bold_latin',
    input : files(repo / 'assets' / 'NotoSans-Bold-Latin.ttf'),
    output : 'embedded_noto_sans_bold_latin.cpp',
    command : [python3, files(repo / 'scripts' / 'embed.py'), '@INPUT@', '@OUTPUT@', 'noto_sans_bold_latin'],
  ),
]

raster_lib = static_library('raster',
  files(repo / 'raster' / 'canvas.cpp', repo / 'raster' / 'draw_snapshot.cpp', repo / 'raster' / 'fonts.cpp'),
  embedded_font_sources,
  include_directories : [repo_inc, prism_inc],
  dependencies : deps,
)

foreach t : ['test_canvas', 'test_fonts', 'test_snapshot']
  e = executable(t, files('tests' / (t + '.cpp')),
    include_directories : [repo_inc, prism_inc],
    link_with : [raster_lib, prism_core],
    dependencies : deps + [doctest_dep],
  )
  test(t, e)
endforeach

executable('render_demo', files('render_demo.cpp'),
  include_directories : [repo_inc, prism_inc],
  link_with : [raster_lib, prism_core],
  dependencies : deps,
)
