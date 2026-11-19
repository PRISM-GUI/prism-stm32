# PRISM on the STM32H7B3I-DK. Spike stage: does PRISM's core cross-compile for a Cortex-M7, and
# how big is it? PRISM's own meson.build always builds its SDL backend, so this builds PRISM's
# core sources itself, from a PRISM checkout (option prism_dir; Meson forbids compiling files
# that sit under subprojects/ from the parent project).
# simplify: the source list is copied from PRISM's src/meson.build; replace with
# subproject('prism') (subprojects/prism.wrap) once PRISM has an option to build without SDL.
project('prism-stm32', 'c', 'cpp',
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

# ---- FreeRTOS + posix/ layer + QEMU test firmware (mps2-an500, Cortex-M7) ----
# The kernel is built for QEMU's MPS2-AN500 board; FreeRTOSConfig.h lives with
# the firmware in tests/qemu/ (a different board gets its own config dir).
freertos_root = 'third_party/FreeRTOS-Kernel'
freertos_inc = include_directories(
  freertos_root / 'include',
  freertos_root / 'portable' / 'GCC' / 'ARM_CM7' / 'r0p1',
  'tests' / 'qemu',
)

freertos = static_library('freertos', files(
    freertos_root / 'tasks.c',
    freertos_root / 'queue.c',
    freertos_root / 'list.c',
    freertos_root / 'timers.c',
    freertos_root / 'event_groups.c',
    freertos_root / 'portable' / 'GCC' / 'ARM_CM7' / 'r0p1' / 'port.c',
    freertos_root / 'portable' / 'MemMang' / 'heap_4.c',
  ),
  include_directories : freertos_inc,
)

posix = static_library('posix', files(
    'posix' / 'freertos_posix.c',
    'posix' / 'posix_mutex.c',
    'posix' / 'posix_cond.c',
    'posix' / 'posix_rwlock.c',
    'posix' / 'posix_time.c',
    'posix' / 'tls.c',
    'posix' / 'tls_support.S',
  ),
  include_directories : [freertos_inc, include_directories('posix')],
)

# --specs=nosys.specs and -Wl,--gc-sections come from cross/stm32h7.ini (the
# driver refuses nosys.specs twice). The posix objects are linked whole (gthr-posix references the pthread_* functions
# weakly, so a plain archive would never be pulled in) and therefore land on
# the link line before the -lnosys that nosys.specs appends at the end.
qemu_tests = executable('qemu_tests.elf', files(
    'tests' / 'qemu' / 'startup.c',
    'tests' / 'qemu' / 'semihost.c',
    'tests' / 'qemu' / 'freertos_hooks.c',
    'tests' / 'qemu' / 'main.cpp',
  ),
  include_directories : freertos_inc,
  link_whole : posix,
  link_depends : files('tests' / 'qemu' / 'mps2_an500.ld'),
  link_with : freertos,
  link_args : [
    '-T', meson.current_source_dir() / 'tests' / 'qemu' / 'mps2_an500.ld',
  ],
)
