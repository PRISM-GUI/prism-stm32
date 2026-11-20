# PRISM on the STM32H7B3I-DK. PRISM (the subproject subprojects/prism) gives the library, the
# rasterizer, the frame runtime (platform/) and, on a bare-metal host, the FreeRTOS kernel and
# POSIX layer files, which this board compiles against its own FreeRTOSConfig.h.
project('prism-stm32', 'c', 'cpp',
  version : '0.0.1',
  default_options : ['cpp_std=c++26', 'warning_level=2', 'optimization=s', 'b_staticpic=false',
                     'libunibreak:werror=false', 'libunibreak:warning_level=0'],
  meson_version : '>= 1.5.0',
)

# PRISM debug tools off: the board has no keyboard for their hotkey. simplify: PRISM also builds
# its QEMU test firmwares (tests/embedded) here, two links over the libraries this build makes
# anyway; add a PRISM option to skip them if that starts to matter.
prism = subproject('prism', default_options : ['prism_debug_tools=disabled'])
prism_platform_dep = prism.get_variable('prism_platform_dep')

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

# ---- STM32H7B3I-DK board support (boards/stm32h7b3i_dk/) + ST's HAL/BSP + the demo app ----
# The board's defines come from ST's .cproject for this board:
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

# PRISM's FreeRTOS kernel and POSIX layer (pthreads, newlib locks, TLS, the allocation counter,
# _sbrk, the timing line's counters), compiled against the board's FreeRTOSConfig.h.
freertos_board_inc = [prism.get_variable('freertos_kernel_inc'),
                      prism.get_variable('freertos_cm7_port_inc'), include_directories(board_dir)]
freertos_board = static_library('freertos_stm32h7b3i_dk',
  prism.get_variable('freertos_kernel_files'), prism.get_variable('freertos_cm7_port_files'),
  include_directories : freertos_board_inc,
)
posix_board = static_library('posix_stm32h7b3i_dk', prism.get_variable('freertos_posix_files'),
  include_directories : [freertos_board_inc, prism.get_variable('freertos_posix_inc')],
  dependencies : prism_platform_dep.partial_dependency(includes : true),
  c_args : get_option('alloc_guard') ? ['-DPRISM_ALLOC_GUARD_TRAP'] : [],
)

# The board's own objects, linked whole into each firmware: the CMSIS startup .s (meson
# assembles .s/.S with the C driver, -x assembler-with-cpp), system_stm32h7xx.c (from
# STM32CubeH7's STM32H7B3I-DK template, as-is), the trimmed stm32h7xx_it.c, the TIM6 HAL
# timebase and the FreeRTOS hooks.
stm32_board = static_library('stm32h7b3i_dk_board', files(
    cmsis_device / 'Source' / 'Templates' / 'gcc' / 'startup_stm32h7b3xxq.s',
    board_dir / 'system_stm32h7xx.c',
    board_dir / 'stm32h7xx_it.c',
    board_dir / 'stm32h7xx_hal_timebase_tim.c',
    board_dir / 'freertos_hooks.c',
  ),
  include_directories : [board_inc, freertos_board_inc],
  c_args : board_defines,
)

board_ld = meson.current_source_dir() / board_dir / 'stm32h7b3.ld'
# Per-target sources: these see the target's defines (-DPRISM_RENODE), unlike the board library.
board_sources = files(
  board_dir / 'main.cpp',
  board_dir / 'display.cpp',
  board_dir / 'touch.cpp',
  'app' / 'audio_scope.cpp',
  'app' / 'spectrum.cpp',
  'app' / 'synthetic_source.cpp',
  board_dir / 'adc_source.cpp',
)
board_incs = [board_inc, freertos_board_inc, include_directories('.'), cmsis_dsp_inc]
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
  'dependencies' : prism_platform_dep,
  'link_with' : [cmsis_dsp, hal_lib, bsp_lib, freertos_board],
  'link_whole' : [posix_board, stm32_board],
  'link_depends' : files(board_dir / 'stm32h7b3.ld'),
  'link_args' : ['-T', board_ld, prism.get_variable('alloc_count_link_arg')],
  'c_args' : board_defines,
}

# The firmware for the real board.
stm32h7b3i_dk = executable('stm32h7b3i_dk.elf', board_sources,
  kwargs : board_firmware_args,
  cpp_args : board_defines + example_defines,
)

# The same firmware for Renode (-DPRISM_RENODE: main.cpp skips SystemClock_Config).
stm32h7b3i_dk_renode = executable('stm32h7b3i_dk_renode.elf', board_sources,
  kwargs : board_firmware_args,
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
