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

# CMSIS-DSP's real FFT (third_party/CMSIS-DSP, tag v1.16.2) for the audio scope; the host build
# (host/meson.build) compiles the same files. On Arm it needs the CMSIS core headers.
cmsis_dsp_root = 'third_party' / 'CMSIS-DSP'
cmsis_dsp_inc = include_directories(cmsis_dsp_root / 'Include', cmsis_dsp_root / 'PrivateInclude',
  'third_party' / 'cmsis-core' / 'CMSIS' / 'Core' / 'Include')
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
  c_args : ['-w'],
)

# Heap allocations go through posix/alloc_count.c first, which counts them (the timing line).
alloc_count_link_arg = '-Wl,--wrap=_malloc_r'

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
    'posix' / 'alloc_count.c',
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
  link_args : ['-T', qemu_ld, alloc_count_link_arg],
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
  link_args : ['-T', qemu_ld, alloc_count_link_arg],
)

# ---- STM32H7B3I-DK board support (boards/stm32h7b3i_dk/) + ST's HAL/BSP + the demo app ----
# The board has its own FreeRTOSConfig.h, so the kernel and the posix/ layer are built a
# second time against the board's config (one freertos/posix archive per board; the QEMU
# pair above stays untouched). The board's defines come from ST's .cproject for this board:
# STM32H7B3xxQ, USE_HAL_DRIVER, and USE_PWR_DIRECT_SMPS_SUPPLY — the power supply MUST be
# direct SMPS (a wrong mode can lock the chip; ExitRun0Mode in the CMSIS startup relies on it).
board_dir = 'boards' / 'stm32h7b3i_dk'
st_drivers = 'third_party' / 'st' / 'Drivers'
hal_dir = 'third_party' / 'stm32h7xx-hal-driver'
cmsis_device = 'third_party' / 'cmsis-device-h7'
cmsis_core_inc = 'third_party' / 'cmsis-core' / 'CMSIS' / 'Core' / 'Include'

board_defines = ['-DSTM32H7B3xxQ', '-DUSE_HAL_DRIVER', '-DUSE_PWR_DIRECT_SMPS_SUPPLY']

# The board's own conf headers (stm32h7xx_hal_conf.h, stm32h7b3i_discovery_conf.h,
# ft5336_conf.h, gt911_conf.h, is42s16800j_conf.h, FreeRTOSConfig.h) come first, so they
# win over any template.
board_inc = include_directories(
  board_dir,
  hal_dir / 'Inc',
  st_drivers / 'BSP' / 'STM32H7B3I-DK',
  st_drivers / 'BSP',   # so the BSP headers' "../Components/..." includes resolve
  st_drivers / 'BSP' / 'Components' / 'Common',
  st_drivers / 'BSP' / 'Components' / 'ft5336',
  st_drivers / 'BSP' / 'Components' / 'gt911',
  st_drivers / 'BSP' / 'Components' / 'is42s16800j',
  st_drivers / 'BSP' / 'Components' / 'rk043fn48h',
  cmsis_device / 'Include',
  cmsis_core_inc,
)

# Only the HAL modules the board conf enables (see boards/stm32h7b3i_dk/stm32h7xx_hal_conf.h).
hal_lib = static_library('stm32h7_hal', files(
    hal_dir / 'Src' / 'stm32h7xx_hal.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_rcc.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_rcc_ex.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_flash.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_flash_ex.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_pwr.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_pwr_ex.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_cortex.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_gpio.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_exti.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_dma.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_dma_ex.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_dma2d.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_ltdc.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_ltdc_ex.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_tim.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_tim_ex.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_i2c.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_i2c_ex.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_sdram.c',
    hal_dir / 'Src' / 'stm32h7xx_ll_fmc.c',      # FMC_SDRAM_* used by the SDRAM HAL
    hal_dir / 'Src' / 'stm32h7xx_hal_mdma.c',
    hal_dir / 'Src' / 'stm32h7xx_hal_adc.c',     # the scope's input: Arduino A0
    hal_dir / 'Src' / 'stm32h7xx_hal_adc_ex.c',
  ),
  include_directories : board_inc,
  c_args : board_defines,
)

# The BSP paths the firmware uses: LEDs, the I2C4 bus, LCD (LTDC/DMA2D/TIM2), touch, SDRAM,
# not audio/SD/camera/EEPROM/OSPI. Plus the component drivers they pull in.
bsp_lib = static_library('stm32h7b3i_dk_bsp', files(
    st_drivers / 'BSP' / 'STM32H7B3I-DK' / 'stm32h7b3i_discovery.c',
    st_drivers / 'BSP' / 'STM32H7B3I-DK' / 'stm32h7b3i_discovery_bus.c',
    st_drivers / 'BSP' / 'STM32H7B3I-DK' / 'stm32h7b3i_discovery_lcd.c',
    st_drivers / 'BSP' / 'STM32H7B3I-DK' / 'stm32h7b3i_discovery_ts.c',
    st_drivers / 'BSP' / 'STM32H7B3I-DK' / 'stm32h7b3i_discovery_sdram.c',
    st_drivers / 'BSP' / 'Components' / 'ft5336' / 'ft5336.c',
    st_drivers / 'BSP' / 'Components' / 'ft5336' / 'ft5336_reg.c',
    st_drivers / 'BSP' / 'Components' / 'gt911' / 'gt911.c',
    st_drivers / 'BSP' / 'Components' / 'gt911' / 'gt911_reg.c',
    st_drivers / 'BSP' / 'Components' / 'is42s16800j' / 'is42s16800j.c',
  ),
  include_directories : board_inc,
  c_args : board_defines,
)

# FreeRTOS + posix/ built against the board's FreeRTOSConfig.h (the QEMU pair above uses
# boards/qemu_mps2's). Same sources, separate archives so the two configs never mix.
freertos_board_inc = include_directories(
  freertos_root / 'include',
  freertos_root / 'portable' / 'GCC' / 'ARM_CM7' / 'r0p1',
  board_dir,
)
freertos_board = static_library('freertos_stm32h7b3i_dk', files(
    freertos_root / 'tasks.c',
    freertos_root / 'queue.c',
    freertos_root / 'list.c',
    freertos_root / 'timers.c',
    freertos_root / 'event_groups.c',
    freertos_root / 'portable' / 'GCC' / 'ARM_CM7' / 'r0p1' / 'port.c',
    freertos_root / 'portable' / 'MemMang' / 'heap_4.c',
  ),
  include_directories : freertos_board_inc,
)
posix_board = static_library('posix_stm32h7b3i_dk', files(
    'posix' / 'freertos_posix.c',
    'posix' / 'posix_mutex.c',
    'posix' / 'posix_cond.c',
    'posix' / 'posix_rwlock.c',
    'posix' / 'posix_time.c',
    'posix' / 'newlib_locks.c',
    'posix' / 'alloc_count.c',
    'posix' / 'tls.c',
    'posix' / 'tls_support.S',
  ),
  include_directories : [freertos_board_inc, include_directories('posix')],
  c_args : get_option('alloc_guard') ? ['-DPRISM_ALLOC_GUARD_TRAP'] : [],
)

# The board's own objects, linked whole into each firmware: the CMSIS startup .s (meson
# assembles .s/.S with the C driver, -x assembler-with-cpp, like posix/tls_support.S),
# system_stm32h7xx.c (from STM32CubeH7's STM32H7B3I-DK template, as-is), the trimmed stm32h7xx_it.c, the TIM6 HAL
# timebase, _sbrk, the FreeRTOS hooks, the display and touch drivers.
stm32_board = static_library('stm32h7b3i_dk_board', files(
    cmsis_device / 'Source' / 'Templates' / 'gcc' / 'startup_stm32h7b3xxq.s',
    board_dir / 'system_stm32h7xx.c',
    board_dir / 'stm32h7xx_it.c',
    board_dir / 'stm32h7xx_hal_timebase_tim.c',
    board_dir / 'sbrk.c',
    board_dir / 'freertos_hooks.c',
  ),
  include_directories : [board_inc, freertos_board_inc, prism_inc, include_directories('.')],
  dependencies : deps,
  c_args : board_defines,
  cpp_args : board_defines,
)

board_ld = meson.current_source_dir() / board_dir / 'stm32h7b3.ld'
# Per-target sources: these see the target's defines (-DPRISM_RENODE), unlike the board library.
board_sources = files(
  board_dir / 'main.cpp',
  board_dir / 'display.cpp',
  board_dir / 'touch.cpp',
  'app' / 'runtime.cpp',
  'app' / 'frame_stats.cpp',
  'app' / 'audio_scope.cpp',
  'app' / 'spectrum.cpp',
  'app' / 'synthetic_source.cpp',
  board_dir / 'adc_source.cpp',
  board_dir / 'system_counters.cpp',
)
board_incs = [board_inc, freertos_board_inc, prism_inc, include_directories('.'), cmsis_dsp_inc]
# The example the firmware runs (meson option `example`).
example_defines = get_option('example') == 'scope' ? ['-DPRISM_EXAMPLE_SCOPE'] : []
# The allocation guard (meson option `alloc_guard`): the runtime arms it once startup is done.
if get_option('alloc_guard')
  example_defines += ['-DPRISM_ALLOC_GUARD']
endif

# The firmware for the real board.
stm32h7b3i_dk = executable('stm32h7b3i_dk.elf', board_sources,
  include_directories : board_incs,
  dependencies : deps,
  link_with : [prism_core, raster_lib, cmsis_dsp, hal_lib, bsp_lib, freertos_board],
  # The board library is linked whole: it overrides weak HAL symbols (HAL_InitTick on TIM6)
  # and the startup file's weak IRQ handlers, and a plain archive member is never pulled
  # in to replace a weak definition that is already there.
  link_whole : [posix_board, stm32_board],
  link_depends : files(board_dir / 'stm32h7b3.ld'),
  link_args : ['-T', board_ld, alloc_count_link_arg],
  c_args : board_defines,
  cpp_args : board_defines + example_defines,
)

# The same firmware for Renode (-DPRISM_RENODE: main.cpp skips SystemClock_Config).
stm32h7b3i_dk_renode = executable('stm32h7b3i_dk_renode.elf', board_sources,
  include_directories : board_incs,
  dependencies : deps,
  link_with : [prism_core, raster_lib, cmsis_dsp, hal_lib, bsp_lib, freertos_board],
  # The board library is linked whole: it overrides weak HAL symbols (HAL_InitTick on TIM6)
  # and the startup file's weak IRQ handlers, and a plain archive member is never pulled
  # in to replace a weak definition that is already there.
  link_whole : [posix_board, stm32_board],
  link_depends : files(board_dir / 'stm32h7b3.ld'),
  link_args : ['-T', board_ld, alloc_count_link_arg],
  c_args : board_defines,
  cpp_args : board_defines + example_defines + ['-DPRISM_RENODE'],
)

# Flashable image and size report for the real board.
objcopy = find_program('arm-none-eabi-objcopy')
arm_size = find_program('arm-none-eabi-size')
custom_target('stm32h7b3i_dk.bin',
  input : stm32h7b3i_dk,
  output : 'stm32h7b3i_dk.bin',
  command : [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
  build_by_default : true,
)
custom_target('stm32h7b3i_dk.size',
  input : stm32h7b3i_dk,
  output : 'stm32h7b3i_dk.size.txt',
  command : [arm_size, '@INPUT@'],
  capture : true,
  build_by_default : true,
)
