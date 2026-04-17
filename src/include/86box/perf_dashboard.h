/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Performance dashboard instrumentation API.
 */
#ifndef EMU_PERF_DASHBOARD_H
#define EMU_PERF_DASHBOARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum perf_domain_t {
    PERF_DOMAIN_CPU_INTERP = 0,
    PERF_DOMAIN_CPU_DYNAREC,
    PERF_DOMAIN_CPU_MEM_FAST,
    PERF_DOMAIN_CPU_MEM_SLOW,
    PERF_DOMAIN_CPU_TLB,
    PERF_DOMAIN_CPU_BRANCH_ABORT,
    PERF_DOMAIN_VIDEO_S3,
    PERF_DOMAIN_VIDEO_MATROX,
    PERF_DOMAIN_VIDEO_VOODOO_FIFO,
    PERF_DOMAIN_VIDEO_VOODOO_RENDER,
    PERF_DOMAIN_VIDEO_BLIT,
    PERF_DOMAIN_VIDEO_PAGEFLIP,
    PERF_DOMAIN_SOUND_MIX,
    PERF_DOMAIN_SOUND_EMU8K,
    PERF_DOMAIN_SOUND_OPL,
    PERF_DOMAIN_SOUND_SPK,
    PERF_DOMAIN_TIMING_PIT,
    PERF_DOMAIN_TIMING_IRQ,
    PERF_DOMAIN_TIMING_DMA,
    PERF_DOMAIN_IO_HDD,
    PERF_DOMAIN_IO_CDROM,
    PERF_DOMAIN_IO_FLOPPY,
    PERF_DOMAIN_MAIN_LOOP,
    PERF_DOMAIN_QT_UI,
    PERF_DOMAIN__COUNT
} perf_domain_t;

typedef struct perf_counter_t {
    volatile uint64_t ns_accum;
    volatile uint64_t call_count;
} perf_counter_t;

typedef struct perf_snapshot_t {
    uint64_t ns_accum[PERF_DOMAIN__COUNT];
    uint64_t call_count[PERF_DOMAIN__COUNT];
    uint64_t instructions_executed;
    uint64_t dynarec_blocks;
    uint64_t exceptions;
    uint64_t memory_bytes_rw;
    uint64_t audio_underruns;
    uint64_t wall_ns;
    uint64_t guest_tsc;
    uint64_t guest_cycles_per_sec;
    uint32_t speed_percent;
    uint32_t render_fps;
    uint8_t  using_dynarec;
    uint8_t  reserved[3];
} perf_snapshot_t;

extern volatile int perfdash_enabled;

extern perf_counter_t perf_counters[PERF_DOMAIN__COUNT];

extern volatile uint64_t perf_instructions_executed;
extern volatile uint64_t perf_dynarec_blocks_compiled;
extern volatile uint64_t perf_exceptions_count;
extern volatile uint64_t perf_memory_bytes_rw;
extern volatile uint64_t perf_audio_underruns;

extern void     perfdash_init(void);
extern void     perfdash_start(void);
extern void     perfdash_stop(void);
extern void     perfdash_reset(void);
extern uint64_t perfdash_now_ns(void);
extern void     perfdash_take_snapshot(perf_snapshot_t *out);
extern void     perfdash_record_scope(perf_domain_t domain, uint64_t delta_ns);
extern void     perfdash_counter_inc(volatile uint64_t *counter);
extern void     perfdash_counter_add(volatile uint64_t *counter, uint64_t delta);

#if defined(__GNUC__) || defined(__clang__)
#    define PERFDASH_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#    define PERFDASH_UNLIKELY(x) (x)
#endif

#ifdef PERFDASH_ENABLE
#    define PERF_SCOPE_BEGIN(domain) \
        uint64_t _perf_t0_##domain = PERFDASH_UNLIKELY(perfdash_enabled) ? perfdash_now_ns() : 0

#    define PERF_SCOPE_END(domain) do { \
        if (PERFDASH_UNLIKELY(perfdash_enabled)) \
            perfdash_record_scope((domain), perfdash_now_ns() - _perf_t0_##domain); \
    } while (0)

#    define PERF_INC(counter) do { \
        if (PERFDASH_UNLIKELY(perfdash_enabled)) \
            perfdash_counter_inc(&(counter)); \
    } while (0)

#    define PERF_ADD(counter, amount) do { \
        if (PERFDASH_UNLIKELY(perfdash_enabled)) \
            perfdash_counter_add(&(counter), (uint64_t) (amount)); \
    } while (0)
#else
#    define PERF_SCOPE_BEGIN(domain) ((void) 0)
#    define PERF_SCOPE_END(domain)   ((void) 0)
#    define PERF_INC(counter)        ((void) 0)
#    define PERF_ADD(counter, n)     ((void) 0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* EMU_PERF_DASHBOARD_H */
