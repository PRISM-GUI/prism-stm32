# Host (native) build of the board-independent apps (app/) on PRISM: their doctest tests,
# scope_bench (the audio scope as fast as it goes, timed like on the board) and alloc_sites
# (where one snapshot build allocates).
#
# PRISM is the subproject subprojects/prism (a wrap-git of PRISM's main, or a symlink to a
# checkout), built without SDL. The app/ and third_party/ files live one level up -- this is a
# separate meson project so the board build's cross file never applies to it; host/subprojects
# is a symlink to ../subprojects.
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
prism_core_dep = prism.get_variable('prism_core_dep')
prism_raster_dep = prism.get_variable('prism_raster_dep')
prism_platform_dep = prism.get_variable('prism_platform_dep')
# libprism is built with -freflection when the compiler has it and with the debug tools outside
# release builds, but PRISM adds both flags to its own project only. Code including its headers
# must see the same ones: they change what the headers declare (WidgetTree's and Node's members),
# so a mismatch crashes.
# simplify: until prism_headers_dep carries these flags itself; drop this then.
if prism.get_variable('has_reflection')
  add_project_arguments('-freflection', language : 'cpp')
endif
if prism.get_variable('debug_tools_enabled')
  add_project_arguments('-DPRISM_DEBUG_TOOLS_ENABLED', language : 'cpp')
endif
doctest_dep = dependency('doctest', fallback : ['doctest', 'doctest_dep'])

# CMSIS-DSP's real FFT (third_party/CMSIS-DSP, tag v1.16.2), portable C on the host: the same FFT
# as on the board. __GNUC_PYTHON__ is CMSIS-DSP's own switch for a non-Arm GCC build (it skips
# cmsis_compiler.h).
cmsis_dsp_root = repo / 'third_party' / 'CMSIS-DSP'
cmsis_dsp_inc = include_directories(cmsis_dsp_root / 'Include', cmsis_dsp_root / 'PrivateInclude')
cmsis_dsp_host_args = ['-D__GNUC_PYTHON__']
cmsis_dsp = static_library('cmsis_dsp', files(
    cmsis_dsp_root / 'Source' / 'TransformFunctions' / 'arm_rfft_fast_f32.c',
    cmsis_dsp_root / 'Source' / 'TransformFunctions' / 'arm_rfft_fast_init_f32.c',
    cmsis_dsp_root / 'Source' / 'TransformFunctions' / 'arm_cfft_f32.c',
    cmsis_dsp_root / 'Source' / 'TransformFunctions' / 'arm_cfft_init_f32.c',
    cmsis_dsp_root / 'Source' / 'TransformFunctions' / 'arm_cfft_radix8_f32.c',
    cmsis_dsp_root / 'Source' / 'TransformFunctions' / 'arm_bitreversal2.c',
    cmsis_dsp_root / 'Source' / 'CommonTables' / 'arm_common_tables.c',
    cmsis_dsp_root / 'Source' / 'CommonTables' / 'arm_const_structs.c',
  ),
  include_directories : cmsis_dsp_inc,
  c_args : cmsis_dsp_host_args + ['-w'],
)
cmsis_dsp_dep = declare_dependency(
  link_with : cmsis_dsp,
  include_directories : cmsis_dsp_inc,
  compile_args : cmsis_dsp_host_args,
)

# The board-independent app code (app/), linked into its tests and scope_bench.
scope_lib = static_library('scope', files(
    repo / 'app' / 'spectrum.cpp',
    repo / 'app' / 'synthetic_source.cpp',
    repo / 'app' / 'audio_scope.cpp',
  ),
  include_directories : repo_inc,
  dependencies : [prism_core_dep, cmsis_dsp_dep],
)
scope_dep = declare_dependency(
  link_with : scope_lib,
  include_directories : repo_inc,
  dependencies : [prism_core_dep, cmsis_dsp_dep],
)

foreach t : ['test_spectrum', 'test_synthetic_source', 'test_adc_sample']
  test(t, executable(t, files('tests' / (t + '.cpp')), dependencies : [scope_dep, doctest_dep]))
endforeach

foreach t : ['test_audio_scope', 'test_widgets_app']
  test(t, executable(t, files('tests' / (t + '.cpp')),
    dependencies : [scope_dep, prism_raster_dep, doctest_dep]))
endforeach

# Its FrameStats comes from PRISM's platform library, which calls the two hooks a program
# linking it defines: alloc_count_host.cpp has them.
executable('scope_bench', files('scope_bench.cpp', 'alloc_count_host.cpp'),
  dependencies : [scope_dep, prism_platform_dep],
)

# Diagnostic: where one snapshot build allocates (host/alloc_sites.cpp). Linked -no-pie so
# addr2line can name the recorded return addresses.
executable('alloc_sites', files('alloc_sites.cpp'),
  link_args : ['-no-pie'],
  dependencies : [scope_dep, prism_raster_dep],
)
