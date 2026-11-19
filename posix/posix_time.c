/* POSIX time on the FreeRTOS tick count.
   simplify: CLOCK_REALTIME == CLOCK_MONOTONIC == time since boot (there is no
   wall clock); sub-tick precision is not provided. */
#include "posix_internal.h"
#include <sys/time.h>
#include <unistd.h>

static void now_ms(uint64_t *ms) {
    *ms = (uint64_t)xTaskGetTickCount() * 1000u / (uint64_t)configTICK_RATE_HZ;
}

int clock_gettime(clockid_t clock_id, struct timespec *tp) {
    if (!tp) return EINVAL;
    if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC) return EINVAL;
    uint64_t ms;
    now_ms(&ms);
    tp->tv_sec = (time_t)(ms / 1000u);
    tp->tv_nsec = (long)((ms % 1000u) * 1000000u);
    return 0;
}

/* Absolute CLOCK_REALTIME deadline -> relative tick timeout (0 if already past,
   rounded up so a 1 ms request never returns early). */
TickType_t posix_abs_to_ticks(const struct timespec *abs) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    int64_t now_ms = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
    int64_t abs_ms = (int64_t)abs->tv_sec * 1000 + abs->tv_nsec / 1000000;
    int64_t delta = abs_ms - now_ms;
    if (delta <= 0) return 0;
    return (TickType_t)((delta * (int64_t)configTICK_RATE_HZ + 999) / 1000);
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
    (void)rmtp; /* no signals exist, so nanosleep is never interrupted */
    if (!rqtp || rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000L)
        return EINVAL;
    if (!posix_sched_running()) return 0; /* cannot sleep before the scheduler runs */
    int64_t ms = (int64_t)rqtp->tv_sec * 1000 + rqtp->tv_nsec / 1000000;
    TickType_t ticks = (TickType_t)((ms * (int64_t)configTICK_RATE_HZ + 999) / 1000);
    if (ms > 0 && ticks == 0) ticks = 1;
    vTaskDelay(ticks);
    return 0;
}

unsigned int sleep(unsigned int seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = 0;
    nanosleep(&ts, NULL);
    return 0;
}

int usleep(useconds_t useconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)(useconds / 1000000u);
    ts.tv_nsec = (long)(useconds % 1000000u) * 1000L;
    return nanosleep(&ts, NULL);
}

/* newlib's gettimeofday() calls this; libnosys.a provides a failing one, but our
   strong definition in an object linked before libnosys wins. */
int _gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv) {
        uint64_t ms;
        now_ms(&ms);
        tv->tv_sec = (time_t)(ms / 1000u);
        tv->tv_usec = (suseconds_t)((ms % 1000u) * 1000u);
    }
    return 0;
}

int gettimeofday(struct timeval *tv, void *tz) { return _gettimeofday(tv, tz); }

time_t time(time_t *timer) {
    uint64_t ms;
    now_ms(&ms);
    time_t s = (time_t)(ms / 1000u);
    if (timer) *timer = s;
    return s;
}
