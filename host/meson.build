# Host (native) build on PRISM of what stays in this repository: test_adc_sample (app/
# adc_sample.hpp, the board's ADC reading as an audio sample), plus scope_bench (the audio scope
# as fast as it goes, timed like on the board) and alloc_sites (where one snapshot build
# allocates). The apps themselves -- the widget demo and the audio scope -- live in PRISM's
# examples/embedded/ now and come through prism_embedded_apps_dep, CMSIS-DSP included.
#
# PRISM is the subproject subprojects/prism (a wrap-git of PRISM's main, or a symlink to a
# checkout), built without SDL. app/ lives one level up -- this is a separate meson project so
# the board build's cross file never applies to it; host/subprojects is a symlink to
# ../subprojects.
#
# Build & run:
#   meson setup host/build
#   ninja -C host/build && meson test -C host/build
#   host/build/scope_bench scope.ppm
project('prism-stm32-host', 'c', 'cpp',
  version : '0.0.1',
  # Static libraries, as on the board: alloc_sites names the frames of one executable.
  default_options : ['cpp_std=c++26', 'warning_level=2', 'optimization=2', 'default_library=static'],
  meson_version : '>= 1.5.0',
)

# The prism-stm32 checkout this host/ lives in; normalized, since Meson takes 'host/..' for
# a directory inside this project and refuses it as an include directory.
repo = import('fs').parent(meson.current_source_dir())
repo_inc = include_directories(repo)

# No network, serial or Markdown code here: those would only fetch more wraps.
prism = subproject('prism', default_options : ['sdl=disabled', 'http=disabled', 'io=disabled',
                                               'markdown=disabled'])
prism_raster_dep = prism.get_variable('prism_raster_dep')
prism_platform_dep = prism.get_variable('prism_platform_dep')
# The shared board apps (PRISM's examples/embedded/): the scope's sources and headers
# (<embedded/x.hpp>), its static library, PRISM's core and CMSIS-DSP for the FFT.
prism_embedded_apps_dep = prism.get_variable('prism_embedded_apps_dep')
doctest_dep = dependency('doctest', fallback : ['doctest', 'doctest_dep'])

# The one app/ file that stayed here: the board's ADC reading as an audio sample.
test('test_adc_sample', executable('test_adc_sample', files('tests' / 'test_adc_sample.cpp'),
  include_directories : repo_inc,
  dependencies : doctest_dep))

# Its FrameStats comes from PRISM's platform library, which calls the two hooks a program
# linking it defines: alloc_count_host.cpp has them.
executable('scope_bench', files('scope_bench.cpp', 'alloc_count_host.cpp'),
  dependencies : [prism_embedded_apps_dep, prism_platform_dep],
)

# Diagnostic: where one snapshot build allocates (host/alloc_sites.cpp). Linked -no-pie so
# addr2line can name the recorded return addresses.
executable('alloc_sites', files('alloc_sites.cpp'),
  link_args : ['-no-pie'],
  dependencies : [prism_embedded_apps_dep, prism_raster_dep],
)
