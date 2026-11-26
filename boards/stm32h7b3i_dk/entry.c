/* The C entry the CMSIS startup reaches, and the way back into the app's main().
 *
 * The reset handler is in a third_party submodule (startup_stm32h7b3xxq.s, `bl main`), so the
 * board does not edit it: -Wl,--wrap=main (meson.build) turns that call into __wrap_main, which
 * runs the board's own entry; the app's main() stays a plain main(), reached here through
 * __real_main.
 * CHECK: the linker's --wrap is what keeps the two names apart; nothing else in the image calls
 * main (the startup's `bl main` is the only reference).
 */

void board_main(void);   /* boards/stm32h7b3i_dk/main.cpp: bring-up, the app task, the scheduler */
int __real_main(void);   /* the app: PRISM's examples/embedded/<example>_main.cpp, unchanged */

int board_app_main(void)   /* called on the app task, once the port is registered */
{
    return __real_main();
}

void __wrap_main(void)
{
    board_main();
}
