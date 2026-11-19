/* Semihosting-based logging/exit for the QEMU firmware (tests/qemu). */
#ifndef QEMU_LOG_H
#define QEMU_LOG_H

void log(const char *s);
void exit_qemu(int code) __attribute__((noreturn));

#endif /* QEMU_LOG_H */
