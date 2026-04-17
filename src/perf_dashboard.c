/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Performance dashboard instrumentation implementation.
 */
#include <stddef.h>
#include <stdatomic.h>

#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/perf_dashboard.h>
#include <86box/timer.h>
#include <86box/video.h>
#include "cpu.h"

extern int fps;

#ifdef _WIN32
#    include <windows.h>
#else
#    include <time.h>
#endif

#define PERFDASH_NS_PER_SEC 1000000000ULL

volatile int perfdash_enabled = 0;

perf_counter_t perf_counters[PERF_DOMAIN__COUNT] = { 0 };

volatile uint64_t perf_instructions_executed   = 0;
volatile uint64_t perf_dynarec_blocks_compiled = 0;
volatile uint64_t perf_exceptions_count        = 0;
volatile uint64_t perf_memory_bytes_rw         = 0;
volatile uint64_t perf_audio_underruns         = 0;

#ifdef _WIN32
static LARGE_INTEGER perfdash_qpc_frequency = { 0 };
static ATOMIC_INT    perfdash_qpc_ready     = 0;
#endif

static uint64_t
perfdash_atomic_load_u64(volatile uint64_t *value)
{
#ifdef _WIN32
    return (uint64_t) InterlockedCompareExchange64((volatile LONG64 *) value, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n((uint64_t *) value, __ATOMIC_RELAXED);
#else
    return *value;
#endif
}

static void
perfdash_atomic_store_u64(volatile uint64_t *value, uint64_t next_value)
{
#ifdef _WIN32
    InterlockedExchange64((volatile LONG64 *) value, (LONG64) next_value);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n((uint64_t *) value, next_value, __ATOMIC_RELAXED);
#else
    *value = next_value;
#endif
}

static void
perfdash_atomic_add_u64(volatile uint64_t *value, uint64_t delta)
{
#ifdef _WIN32
    InterlockedExchangeAdd64((volatile LONG64 *) value, (LONG64) delta);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_fetch_add((uint64_t *) value, delta, __ATOMIC_RELAXED);
#else
    *value += delta;
#endif
}

#ifdef _WIN32
static void
perfdash_init_clock(void)
{
    if (!ATOMIC_LOAD(perfdash_qpc_ready)) {
        QueryPerformanceFrequency(&perfdash_qpc_frequency);
        ATOMIC_STORE(perfdash_qpc_ready, 1);
    }
}
#endif

void
perfdash_init(void)
{
#ifdef _WIN32
    perfdash_init_clock();
#endif
    ATOMIC_STORE(perfdash_enabled, 0);
    perfdash_reset();
}

void
perfdash_start(void)
{
    ATOMIC_STORE(perfdash_enabled, 0);
    perfdash_reset();
    ATOMIC_STORE(perfdash_enabled, 1);
}

void
perfdash_stop(void)
{
    ATOMIC_STORE(perfdash_enabled, 0);
}

void
perfdash_reset(void)
{
    const int was_enabled = ATOMIC_LOAD(perfdash_enabled);

    ATOMIC_STORE(perfdash_enabled, 0);

    for (size_t i = 0; i < PERF_DOMAIN__COUNT; i++) {
        perfdash_atomic_store_u64(&perf_counters[i].ns_accum, 0);
        perfdash_atomic_store_u64(&perf_counters[i].call_count, 0);
    }

    perfdash_atomic_store_u64(&perf_instructions_executed, 0);
    perfdash_atomic_store_u64(&perf_dynarec_blocks_compiled, 0);
    perfdash_atomic_store_u64(&perf_exceptions_count, 0);
    perfdash_atomic_store_u64(&perf_memory_bytes_rw, 0);
    perfdash_atomic_store_u64(&perf_audio_underruns, 0);

    if (was_enabled)
        ATOMIC_STORE(perfdash_enabled, 1);
}

uint64_t
perfdash_now_ns(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter;
    uint64_t      seconds;
    uint64_t      remainder;
    uint64_t      frequency;

    perfdash_init_clock();

    QueryPerformanceCounter(&counter);

    frequency = (uint64_t) perfdash_qpc_frequency.QuadPart;
    seconds   = (uint64_t) counter.QuadPart / frequency;
    remainder = (uint64_t) counter.QuadPart % frequency;

    return (seconds * PERFDASH_NS_PER_SEC) + ((remainder * PERFDASH_NS_PER_SEC) / frequency);
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t) ts.tv_sec * PERFDASH_NS_PER_SEC) + (uint64_t) ts.tv_nsec;
#endif
}

void
perfdash_record_scope(perf_domain_t domain, uint64_t delta_ns)
{
    if ((domain < 0) || (domain >= PERF_DOMAIN__COUNT))
        return;

    perfdash_atomic_add_u64(&perf_counters[domain].ns_accum, delta_ns);
    perfdash_atomic_add_u64(&perf_counters[domain].call_count, 1);
}

void
perfdash_counter_inc(volatile uint64_t *counter)
{
    if (counter == NULL)
        return;

    perfdash_atomic_add_u64(counter, 1);
}

void
perfdash_counter_add(volatile uint64_t *counter, uint64_t delta)
{
    if ((counter == NULL) || (delta == 0))
        return;

    perfdash_atomic_add_u64(counter, delta);
}

void
perfdash_take_snapshot(perf_snapshot_t *out)
{
    if (out == NULL)
        return;

    for (size_t i = 0; i < PERF_DOMAIN__COUNT; i++) {
        out->ns_accum[i]   = perfdash_atomic_load_u64(&perf_counters[i].ns_accum);
        out->call_count[i] = perfdash_atomic_load_u64(&perf_counters[i].call_count);
    }

    out->instructions_executed = perfdash_atomic_load_u64(&perf_instructions_executed);
    out->dynarec_blocks        = perfdash_atomic_load_u64(&perf_dynarec_blocks_compiled);
    out->exceptions            = perfdash_atomic_load_u64(&perf_exceptions_count);
    out->memory_bytes_rw       = perfdash_atomic_load_u64(&perf_memory_bytes_rw);
    out->audio_underruns       = perfdash_atomic_load_u64(&perf_audio_underruns);
    out->wall_ns               = perfdash_now_ns();
    out->guest_tsc             = tsc;
    out->guest_cycles_per_sec  = cpu_s ? (uint64_t) cpu_s->rspeed : 0;
    out->speed_percent         = (uint32_t) ((fps > 0) ? (fps / (force_10ms ? 1 : 10)) : 0);
    out->render_fps            = (uint32_t) atomic_load(&monitors[0].mon_actualrenderedframes);
    out->using_dynarec         = (uint8_t) !!cpu_use_dynarec;
}
