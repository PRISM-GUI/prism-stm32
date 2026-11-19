// QEMU firmware tests for the posix/ layer on FreeRTOS (mps2-an500, Cortex-M7).
// One FreeRTOS task with a big stack runs the tests, a watchdog task exits with
// code 99 after 20 s, and the exit code is the number of failed tests.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include "FreeRTOS.h"
#include "task.h"

extern "C" {
#include "qemu_log.h"
size_t xPortGetFreeHeapSize(void);
}

using namespace std::chrono_literals;

static int failures = 0;

static void check(const char *name, bool ok) {
    char line[96];
    std::snprintf(line, sizeof line, "%-40s %s\n", name, ok ? "PASS" : "FAIL");
    log(line);   // semihosting directly: printf through newlib's stdio prints nothing yet
    if (!ok) ++failures;
}

// 1. std::thread runs a lambda and join() returns; the lambda saw the captured value.
static void test_thread_join() {
    int captured = 0;
    std::thread t([&captured] { captured = 42; });
    t.join();
    check("1 std::thread join", captured == 42 && !t.joinable());
}

// 2. 8 threads each increment a shared counter 10000 times under std::mutex.
static void test_mutex_counter() {
    std::mutex m;
    long counter = 0;
    constexpr int kThreads = 8, kIters = 10000;
    std::thread ts[kThreads];
    for (int i = 0; i < kThreads; i++)
        ts[i] = std::thread([&] {
            for (int j = 0; j < kIters; j++) {
                std::lock_guard<std::mutex> l(m);
                ++counter;
            }
        });
    for (auto &t : ts) t.join();
    check("2 8x10000 increments under std::mutex", counter == kThreads * kIters);
}

// 3. condition_variable ping-pong between two threads, 1000 round trips.
static void test_condvar_pingpong() {
    std::mutex m;
    std::condition_variable cv;
    int turn = 0;
    long trips = 0;
    constexpr int kRounds = 1000;
    std::thread pong([&] {
        std::unique_lock<std::mutex> l(m);
        for (int i = 0; i < kRounds; i++) {
            cv.wait(l, [&] { return turn == 1; });
            turn = 0;
            ++trips;
            cv.notify_one();
        }
    });
    {
        std::unique_lock<std::mutex> l(m);
        for (int i = 0; i < kRounds; i++) {
            turn = 1;
            cv.notify_one();
            cv.wait(l, [&] { return turn == 0; });
        }
    }
    pong.join();
    check("3 condition_variable ping-pong x1000", trips == kRounds);
}

// 4. wait_for with nothing notifying times out after ~50 ms.
static void test_wait_for_timeout() {
    std::mutex m;
    std::condition_variable cv;
    std::unique_lock<std::mutex> l(m);
    auto t0 = std::chrono::steady_clock::now();
    bool got = cv.wait_for(l, 50ms, [] { return false; });
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    check("4 wait_for(50ms) times out in 40..200ms", !got && ms >= 40 && ms <= 200);
}

// 5. sleep_for(20ms) sleeps at least 20 ms.
static void test_sleep_for() {
    auto t0 = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(20ms);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    check("5 sleep_for(20ms) >= 20ms", ms >= 20);
}

// 6. thread_local: initializer per thread, independence, non-trivial ctor/dtor.
struct Counted {
    static std::atomic<int> ctored;
    static std::atomic<int> destroyed;
    Counted() { ++ctored; }
    ~Counted() { ++destroyed; }
};
std::atomic<int> Counted::ctored{0};
std::atomic<int> Counted::destroyed{0};
thread_local int tl_value = 1;
thread_local Counted tl_counted;

static void test_thread_local() {
    tl_value = 5;
    (void)tl_counted; // construct in this thread
    int seen = -1, other = -1;
    std::thread t([&] {
        seen = tl_value; // a fresh thread sees the initializer, not our 5
        tl_value = 7;
        (void)tl_counted;
        other = tl_value;
    });
    t.join();
    bool values_ok = (seen == 1 && other == 7 && tl_value == 5);
    bool ctor_ok = (Counted::ctored.load() == 2); // once per thread that touches it
    bool dtor_ok = (Counted::destroyed.load() >= 1); // the joined thread's copy ran
    check("6 thread_local values per thread", values_ok);
    check("6 thread_local ctor once/thread, dtor at exit", ctor_ok && dtor_ok);
}

// 7. shared_ptr copied and destroyed 100000 times across 4 threads.
static void test_shared_ptr() {
    auto p = std::make_shared<int>(42);
    constexpr int kThreads = 4, kIters = 25000;
    std::thread ts[kThreads];
    for (int i = 0; i < kThreads; i++)
        ts[i] = std::thread([&p] {
            for (int j = 0; j < kIters; j++) {
                auto q = p;
                (void)q;
            }
        });
    for (auto &t : ts) t.join();
    check("7 shared_ptr 100000 copies, use_count==1", p.use_count() == 1);
}

// 8. recursive_mutex locked twice by one thread; timed_mutex::try_lock_for fails
//    after ~30 ms when held by another thread.
static void test_recursive_and_timed_mutex() {
    std::recursive_mutex rm;
    rm.lock();
    rm.lock(); // must not deadlock
    rm.unlock();
    rm.unlock();
    check("8a std::recursive_mutex locks twice", true); // reaching here = no deadlock

    std::timed_mutex tm;
    tm.lock();
    std::atomic<bool> got{true};
    std::atomic<long> ms{0};
    std::thread t([&] {
        auto t0 = std::chrono::steady_clock::now();
        got = tm.try_lock_for(30ms);
        ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - t0).count();
    });
    t.join();
    tm.unlock();
    check("8b timed_mutex try_lock_for fails ~30ms", !got && ms >= 25 && ms <= 150);
}

// 9. A detached thread runs and its control block is freed (heap free bytes back
//    to within 64 bytes of before).
static void test_detached_thread_frees() {
    // Earlier tests' threads may still await the idle task's cleanup: let it run first, so
    // their memory does not come back in the middle of the measure.
    vTaskDelay(pdMS_TO_TICKS(200));
    size_t before = xPortGetFreeHeapSize();
    std::atomic<bool> done{false};
    {
        std::thread t([&done] { done = true; });
        t.detach();
    }
    while (!done.load()) std::this_thread::sleep_for(1ms);
    vTaskDelay(pdMS_TO_TICKS(200)); // the idle task frees the deleted task's stack
    long lost = (long)before - (long)xPortGetFreeHeapSize();
    char line[64];
    std::snprintf(line, sizeof line, "  heap lost: %ld bytes\n", lost);
    log(line);
    check("9 detached thread frees its block (<=64B)", lost <= 64);
}

// 10. shared_mutex: two readers hold it together; a writer waits for them.
static void test_shared_mutex() {
    std::shared_mutex sm;
    std::atomic<bool> reader_in{false}, release{false}, writer_got{false};
    sm.lock_shared(); // reader 1: this thread
    std::thread reader([&] {
        sm.lock_shared(); // reader 2
        reader_in = true;
        while (!release.load()) std::this_thread::sleep_for(1ms);
        sm.unlock_shared();
    });
    while (!reader_in.load()) std::this_thread::sleep_for(1ms);
    std::thread writer([&] {
        sm.lock(); // must wait until both readers are done
        writer_got = true;
        sm.unlock();
    });
    std::this_thread::sleep_for(50ms); // give the writer time to (not) acquire
    bool writer_blocked = !writer_got.load();
    release = true;
    sm.unlock_shared();
    reader.join();
    writer.join();
    check("10 shared_mutex: 2 readers, writer waits", writer_blocked && writer_got.load());
}

static void test_task(void *) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    log("posix-on-FreeRTOS tests (mps2-an500)\n");
    test_thread_join();
    test_mutex_counter();
    test_condvar_pingpong();
    test_wait_for_timeout();
    test_sleep_for();
    test_thread_local();
    test_shared_ptr();
    test_recursive_and_timed_mutex();
    test_detached_thread_frees();
    test_shared_mutex();
    char line[32];
    std::snprintf(line, sizeof line, "failures: %d\n", failures);
    log(line);
    exit_qemu(failures); // exit code = number of failures
}

static void watchdog_task(void *) {
    vTaskDelay(pdMS_TO_TICKS(20000));
    log("WATCHDOG: tests did not finish in 20 s\n");
    exit_qemu(99);
}

extern "C" int main(void) {
    xTaskCreate(test_task, "tests", 16384, nullptr, 2, nullptr); // 64 KiB stack
    xTaskCreate(watchdog_task, "watchdog", 512, nullptr, configMAX_PRIORITIES - 1, nullptr);
    vTaskStartScheduler();
    for (;;) { }
}
