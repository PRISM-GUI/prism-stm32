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
  dependency('magic_enum', fallback : ['magic_enum', 'magic_enum_dep'], default_options : ['test=false']),
]

prism_sources = []
foreach f : get_option('prism_sources')
  prism_sources += files(prism_root / 'src' / f)
endforeach

prism_core = static_library('prism_core', prism_sources,
  include_directories : prism_inc,
  dependencies : deps,
)

# ---- The software rasterizer for the board's framebuffer (raster/) ----
# Plain C++ over a pixel span, no SDL: the same sources build for the host (host/) and the
# Cortex-M7. The TTF fonts are compiled in: scripts/embed.py turns each assets/*.ttf into a .cpp
# byte array (GCC 15 has #embed in C only, not in C++, so a Python custom_target emits them).
python3 = find_program('python3')
embedded_font_sources = [
  custom_target('embedded_noto_sans_latin',
    input : files('assets' / 'NotoSans-Latin.ttf'),
    output : 'embedded_noto_sans_latin.cpp',
    command : [python3, files('scripts' / 'embed.py'), '@INPUT@', '@OUTPUT@', 'noto_sans_latin'],
  ),
  custom_target('embedded_noto_sans_bold_latin',
    input : files('assets' / 'NotoSans-Bold-Latin.ttf'),
    output : 'embedded_noto_sans_bold_latin.cpp',
    command : [python3, files('scripts' / 'embed.py'), '@INPUT@', '@OUTPUT@', 'noto_sans_bold_latin'],
  ),
]

raster_lib = static_library('raster',
  files('raster' / 'canvas.cpp', 'raster' / 'draw_snapshot.cpp', 'raster' / 'fonts.cpp'),
  embedded_font_sources,
  include_directories : [include_directories('.'), prism_inc],
  dependencies : deps,
)

# ---- QEMU board support (mps2-an500) + FreeRTOS + posix/ layer + test firmwares ----
# The board support (startup, semihosting, hooks, FreeRTOSConfig.h, linker script) lives in
# boards/qemu_mps2/ and is shared by every firmware for that board; a different board gets its
# own directory (and its own FreeRTOSConfig.h).
qemu_board_dir = 'boards' / 'qemu_mps2'
freertos_root = 'third_party/FreeRTOS-Kernel'
freertos_inc = include_directories(
  freertos_root / 'include',
  freertos_root / 'portable' / 'GCC' / 'ARM_CM7' / 'r0p1',
  qemu_board_dir,
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

# The board's own objects: vectors/Reset_Handler, semihosting log/_write/exit, the FreeRTOS
# hooks. Linked whole into each firmware (see the comment at the executables below).
qemu_board = static_library('qemu_board', files(
    qemu_board_dir / 'startup.c',
    qemu_board_dir / 'semihost.c',
    qemu_board_dir / 'freertos_hooks.c',
  ),
  include_directories : freertos_inc,
)

posix = static_library('posix', files(
    'posix' / 'freertos_posix.c',
    'posix' / 'posix_mutex.c',
    'posix' / 'posix_cond.c',
    'posix' / 'posix_rwlock.c',
    'posix' / 'posix_time.c',
    'posix' / 'newlib_locks.c',
    'posix' / 'tls.c',
    'posix' / 'tls_support.S',
  ),
  include_directories : [freertos_inc, include_directories('posix')],
)

# --specs=nosys.specs and -Wl,--gc-sections come from cross/stm32h7.ini (the
# driver refuses nosys.specs twice). The posix and board objects are linked whole
# (gthr-posix references the pthread_* functions weakly, so a plain archive would
# never be pulled in; and newlib's retarget locks in posix/newlib_locks.c must be
# defined before libc.a is scanned, or the linker would extract the no-op
# libc_a-lock.o instead) and therefore land on the link line before the -lnosys
# that nosys.specs appends at the end.
qemu_ld = meson.current_source_dir() / qemu_board_dir / 'mps2_an500.ld'

posix_tests = executable('posix_tests.elf', files('tests' / 'posix_qemu' / 'main.cpp'),
  include_directories : freertos_inc,
  link_whole : [posix, qemu_board],
  link_depends : files(qemu_board_dir / 'mps2_an500.ld'),
  link_with : freertos,
  link_args : ['-T', qemu_ld],
)

# PRISM's core (prism_core) as a firmware: a model, a WidgetTree, snapshots —
# same board support, posix layer and linker script as the posix tests. Links the rasterizer too:
# the firmware measures text with raster::Fonts and draws a snapshot into an in-RAM 480x272
# framebuffer (see tests/prism_qemu/main.cpp).
prism_tests = executable('prism_tests.elf', files('tests' / 'prism_qemu' / 'main.cpp'),
  include_directories : [prism_inc, freertos_inc, include_directories('.')],
  dependencies : deps,
  link_with : [prism_core, raster_lib, freertos],
  link_whole : [posix, qemu_board],
  link_depends : files(qemu_board_dir / 'mps2_an500.ld'),
  link_args : ['-T', qemu_ld],
)
