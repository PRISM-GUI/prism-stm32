# PRISM on a single-cortex-M7 ST board: the STM32H7B3I-DK or the STM32F746G-DISCO (meson option
# `board`, with the matching cross file). PRISM (the subproject subprojects/prism) gives the
# library, the rasterizer, the frame runtime (platform/), the demo apps (examples/embedded/) and,
# on a bare-metal host, the FreeRTOS kernel and POSIX layer files, which each board compiles
# against its own FreeRTOSConfig.h.
project('prism-stm32', 'c', 'cpp',
  version : '0.0.1',
  default_options : ['cpp_std=c++26', 'warning_level=2', 'optimization=s',
                     'libunibreak:werror=false', 'libunibreak:warning_level=0'],
  meson_version : '>= 1.5.0',
)

is_f7 = get_option('board') == 'stm32f746g_disco'

# PRISM debug tools off: the board has no keyboard for their hotkey. PRISM's own tests
# (QEMU firmwares) stay off here. Profiling on (doc/design/profiling.md in PRISM): the
# PRISM_PROFILE_* marks plus the global recorder, dumped over the ST-LINK's serial port on a
# button (boards/*/main.cpp). The recorder is sized to the board's RAM: 1024 events x 6 rings
# (~144 KiB) in the H7's 1 MiB AXI SRAM, 256 events x 4 rings (~24 KiB) in the F7's 256 KiB.
prism = subproject('prism', default_options : ['prism_debug_tools=disabled', 'tests=disabled',
                                               'profiling=enabled',
                                               'profiling_events=' + (is_f7 ? '128' : '1024'),
                                               'profiling_threads=' + (is_f7 ? '2' : '6'),
                                               # {fmt}'s localized formatting drags libstdc++'s
                                               # locale facets in (~184 KiB): affordable in the
                                               # H7's 2 MiB of flash, not in the F7's 1 MiB.
                                               'fmt_locale=' + (is_f7 ? 'false' : 'true'),
                                               # libunibreak's UAX #14 + grapheme tables cost
                                               # ~100 KiB; the F7 uses the table-free rule set
                                               # of ui/text_segmentation.hpp instead (breaks at
                                               # spaces, hyphens and CJK ideographs).
                                               'line_breaking=' + (is_f7 ? 'simple' : 'unicode'),
                                               # The second embedded face is 20 KiB of bold
                                               # Noto Sans; off, bold draws with the regular one.
                                               'embedded_bold_face=' + (is_f7 ? 'false' : 'true')])
prism_platform_dep = prism.get_variable('prism_platform_dep')
# The shared board apps (the widget demo and the audio scope) plus CMSIS-DSP for the scope's
# FFT come from PRISM (examples/embedded/): prism_embedded_apps_dep carries the include
# directory (the apps include each other as <embedded/x.hpp>), the apps' static library, PRISM's
# core and CMSIS-DSP.
prism_embedded_apps_dep = prism.get_variable('prism_embedded_apps_dep')

# ---- Per-board: sources, defines and include paths (boards/<board>/) ----
st_drivers = 'third_party' / 'st' / 'Drivers'
cmsis_core_inc = 'third_party' / 'cmsis-core' / 'CMSIS' / 'Core' / 'Include'
board_dir = 'boards' / get_option('board')

if is_f7
  # ---- STM32F746G-DISCO ----
  hal_dir = 'third_party' / 'stm32f7xx-hal-driver'
  cmsis_device = 'third_party' / 'cmsis-device-f7'
  bsp_dir = st_drivers / 'BSP' / 'STM32F746G-Discovery'
  # STM32F746xx from ST's .cproject for this board, plus the HAL's own switch.
  board_defines = ['-DSTM32F746xx', '-DUSE_HAL_DRIVER']
  # The board's own conf headers (stm32f7xx_hal_conf.h, ft5336_conf.h, mt48lc4m32b2_conf.h,
  # FreeRTOSConfig.h) come first, so they win over any template.
  board_inc = include_directories(
    board_dir,
    hal_dir / 'Inc',
    bsp_dir,
    st_drivers / 'BSP',   # so the BSP headers' "../Components/..." includes resolve
    st_drivers / 'BSP' / 'Components' / 'mt48lc4m32b2',
    st_drivers / 'BSP' / 'Components' / 'ft5336',
    st_drivers / 'BSP' / 'Components' / 'rk043fn48h',
    cmsis_device / 'Include',
    cmsis_core_inc,
  )
  # Only the HAL modules the board conf enables (boards/stm32f746g_disco/stm32f7xx_hal_conf.h).
  hal_sources = files(
    hal_dir / 'Src' / 'stm32f7xx_hal.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_rcc.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_rcc_ex.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_flash.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_flash_ex.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_pwr.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_pwr_ex.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_cortex.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_gpio.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_exti.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_dma.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_dma_ex.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_dma2d.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_ltdc.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_tim.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_tim_ex.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_i2c.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_i2c_ex.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_sdram.c',
    hal_dir / 'Src' / 'stm32f7xx_ll_fmc.c',      # FMC_SDRAM_* used by the SDRAM HAL
    hal_dir / 'Src' / 'stm32f7xx_hal_adc.c',     # the scope's input: Arduino A0
    hal_dir / 'Src' / 'stm32f7xx_hal_adc_ex.c',
    hal_dir / 'Src' / 'stm32f7xx_hal_uart.c',    # COM1 (USART1, the ST-LINK's virtual COM port):
    hal_dir / 'Src' / 'stm32f7xx_hal_uart_ex.c', # the profiling dump's transport
  )
  # The BSP paths the firmware uses: LEDs, buttons, COM1, I2C and the SDRAM. Its LCD and touch
  # drivers are left out: they need the font utilities (not vendored) and the old FT5336
  # component API, so display.cpp and touch.cpp drive the LTDC and the FT5336 themselves.
  bsp_sources = files(
    bsp_dir / 'stm32746g_discovery.c',
    bsp_dir / 'stm32746g_discovery_sdram.c',
    st_drivers / 'BSP' / 'Components' / 'mt48lc4m32b2' / 'mt48lc4m32b2.c',
    st_drivers / 'BSP' / 'Components' / 'ft5336' / 'ft5336.c',
    st_drivers / 'BSP' / 'Components' / 'ft5336' / 'ft5336_reg.c',
  )
  startup = cmsis_device / 'Source' / 'Templates' / 'gcc' / 'startup_stm32f746xx.s'
  system_file = 'system_stm32f7xx.c'
  it_file = 'stm32f7xx_it.c'
  timebase_file = 'stm32f7xx_hal_timebase_tim.c'
  ld_name = 'stm32f746g.ld'
  # The F7 board logs its boot progress and faults on COM1 (see boot_log.h).
  board_extra_sources = files(board_dir / 'boot_log.cpp')
else
  # ---- STM32H7B3I-DK ----
  hal_dir = 'third_party' / 'stm32h7xx-hal-driver'
  cmsis_device = 'third_party' / 'cmsis-device-h7'
  bsp_dir = st_drivers / 'BSP' / 'STM32H7B3I-DK'
  # The board's defines come from ST's .cproject for this board:
  # STM32H7B3xxQ, USE_HAL_DRIVER, and USE_PWR_DIRECT_SMPS_SUPPLY — the power supply MUST be
  # direct SMPS (a wrong mode can lock the chip; ExitRun0Mode in the CMSIS startup relies on it).
  board_defines = ['-DSTM32H7B3xxQ', '-DUSE_HAL_DRIVER', '-DUSE_PWR_DIRECT_SMPS_SUPPLY']
  # The board's own conf headers (stm32h7xx_hal_conf.h, stm32h7b3i_discovery_conf.h,
  # ft5336_conf.h, gt911_conf.h, is42s16800j_conf.h, FreeRTOSConfig.h) come first, so they
  # win over any template.
  board_inc = include_directories(
    board_dir,
    hal_dir / 'Inc',
    bsp_dir,
    st_drivers / 'BSP',   # so the BSP headers' "../Components/..." includes resolve
    st_drivers / 'BSP' / 'Components' / 'Common',
    st_drivers / 'BSP' / 'Components' / 'ft5336',
    st_drivers / 'BSP' / 'Components' / 'gt911',
    st_drivers / 'BSP' / 'Components' / 'is42s16800j',
    st_drivers / 'BSP' / 'Components' / 'rk043fn48h',
    cmsis_device / 'Include',
    cmsis_core_inc,
  )
  # Only the HAL modules the board conf enables (boards/stm32h7b3i_dk/stm32h7xx_hal_conf.h).
  hal_sources = files(
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
    hal_dir / 'Src' / 'stm32h7xx_hal_uart.c',    # COM1 (USART1, the ST-LINK's virtual COM
    hal_dir / 'Src' / 'stm32h7xx_hal_uart_ex.c', # port): the profiling dump's transport
  )
  # The BSP paths the firmware uses: LEDs, the I2C4 bus, LCD (LTDC/DMA2D/TIM2), touch, SDRAM,
  # not audio/SD/camera/EEPROM/OSPI. Plus the component drivers they pull in.
  bsp_sources = files(
    bsp_dir / 'stm32h7b3i_discovery.c',
    bsp_dir / 'stm32h7b3i_discovery_bus.c',
    bsp_dir / 'stm32h7b3i_discovery_lcd.c',
    bsp_dir / 'stm32h7b3i_discovery_ts.c',
    bsp_dir / 'stm32h7b3i_discovery_sdram.c',
    st_drivers / 'BSP' / 'Components' / 'ft5336' / 'ft5336.c',
    st_drivers / 'BSP' / 'Components' / 'ft5336' / 'ft5336_reg.c',
    st_drivers / 'BSP' / 'Components' / 'gt911' / 'gt911.c',
    st_drivers / 'BSP' / 'Components' / 'gt911' / 'gt911_reg.c',
    st_drivers / 'BSP' / 'Components' / 'is42s16800j' / 'is42s16800j.c',
  )
  startup = cmsis_device / 'Source' / 'Templates' / 'gcc' / 'startup_stm32h7b3xxq.s'
  system_file = 'system_stm32h7xx.c'
  it_file = 'stm32h7xx_it.c'
  timebase_file = 'stm32h7xx_hal_timebase_tim.c'
  ld_name = 'stm32h7b3.ld'
  board_extra_sources = []
endif

# ---- Shared by the boards: HAL, BSP, FreeRTOS, board objects, firmware targets ----
hal_lib = static_library('stm32_hal', hal_sources,
  include_directories : board_inc,
  c_args : board_defines,
)

bsp_lib = static_library('board_bsp', bsp_sources,
  include_directories : board_inc,
  c_args : board_defines,
)

# PRISM's FreeRTOS kernel and POSIX layer (pthreads, newlib locks, TLS, the allocation counter,
# _sbrk, the timing line's counters), compiled against the board's FreeRTOSConfig.h.
freertos_board_inc = [prism.get_variable('freertos_kernel_inc'),
                      prism.get_variable('freertos_cm7_port_inc'), include_directories(board_dir)]
freertos_board = static_library('freertos_board',
  prism.get_variable('freertos_kernel_files'), prism.get_variable('freertos_cm7_port_files'),
  include_directories : freertos_board_inc,
)
posix_board = static_library('posix_board', prism.get_variable('freertos_posix_files'),
  include_directories : [freertos_board_inc, prism.get_variable('freertos_posix_inc')],
  dependencies : prism_platform_dep.partial_dependency(includes : true),
  c_args : get_option('alloc_guard') ? ['-DPRISM_ALLOC_GUARD_TRAP'] : [],
)

# The board's own objects, linked whole into each firmware: the CMSIS startup .s (meson
# assembles .s/.S with the C driver, -x assembler-with-cpp), the system file (from the CMSIS
# device template, as-is), the trimmed IRQ handlers, the TIM6 HAL timebase and the FreeRTOS hooks.
board_objects = static_library('board_objects', files(
    startup,
    board_dir / system_file,
    board_dir / it_file,
    board_dir / timebase_file,
    board_dir / 'freertos_hooks.c',
  ) + board_extra_sources,
  include_directories : [board_inc, freertos_board_inc],
  c_args : board_defines,
  cpp_args : board_defines,
)

board_ld = meson.current_source_dir() / board_dir / ld_name
# Per-target sources: these see the target's defines (-DPRISM_RENODE), unlike the board library.
board_sources = files(
  board_dir / 'main.cpp',
  board_dir / 'display.cpp',
  board_dir / 'touch.cpp',
  board_dir / 'uart_stream.cpp',
  board_dir / 'adc_source.cpp',
)
# include_directories('.') stays for "app/adc_sample.hpp" (the one app/ file that did not move
# to PRISM; the H7 board's ADC uses it).
board_incs = [board_inc, freertos_board_inc, include_directories('.')]
# The example the firmware runs (meson option `example`).
example_defines = get_option('example') == 'scope' ? ['-DPRISM_EXAMPLE_SCOPE'] : []
# The allocation guard (meson option `alloc_guard`): main.cpp arms it at the first timing line.
if get_option('alloc_guard')
  example_defines += ['-DPRISM_ALLOC_GUARD']
endif

# The posix and board objects are linked whole: libstdc++'s gthr-posix references the pthread_*
# functions weakly, newlib's retargetable locks must be defined before libc.a is scanned, and
# the board library overrides weak HAL symbols (HAL_InitTick on TIM6) and the startup file's
# weak IRQ handlers — a plain archive member is never pulled in for any of those.
board_firmware_args = {
  'include_directories' : board_incs,
  'dependencies' : [prism_platform_dep, prism_embedded_apps_dep],
  'link_with' : [hal_lib, bsp_lib, freertos_board],
  'link_whole' : [posix_board, board_objects],
  'link_depends' : files(board_ld),
  'link_args' : ['-T', board_ld, prism.get_variable('alloc_count_link_arg')],
  'c_args' : board_defines,
}

# The firmware for the real board.
board_firmware = executable(get_option('board') + '.elf', board_sources,
  kwargs : board_firmware_args,
  cpp_args : board_defines + example_defines,
)

# The same firmware for Renode (-DPRISM_RENODE: the boards' main.cpp skips SystemClock_Config,
# their display/touch drivers drop the parts Renode does not model).
board_firmware_renode = executable(get_option('board') + '_renode.elf', board_sources,
  kwargs : board_firmware_args,
  cpp_args : board_defines + example_defines + ['-DPRISM_RENODE'],
)

# Flashable image and size report for the real board.
objcopy = find_program('arm-none-eabi-objcopy')
arm_size = find_program('arm-none-eabi-size')
custom_target(get_option('board') + '.bin',
  input : board_firmware,
  output : get_option('board') + '.bin',
  command : [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
  build_by_default : true,
)
custom_target(get_option('board') + '.size',
  input : board_firmware,
  output : get_option('board') + '.size.txt',
  command : [arm_size, '@INPUT@'],
  capture : true,
  build_by_default : true,
)
