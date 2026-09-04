#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "memory.h"
#include "newcpu.h"
#include "readcpu.h"
#include "uae/m68k_embed.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

struct uae_m68k_context {
    struct uae_m68k_memory memory;
    void *memory_user;
    struct uae_m68k_diagnostics diagnostics;
    uint64_t dropped_log_count;
    jmp_buf fault_jump;
    enum uae_m68k_memory_status memory_status;
    uaecptr fault_address;
    uae_u8 exception_vector;
    uae_u32 exception_cycles;
};

_Thread_local struct regstruct regs;
_Thread_local struct flag_struct regflags;

struct uae_prefs currprefs;
struct uae_prefs changed_prefs;

const int areg_byteinc[] = {1, 1, 1, 1, 1, 1, 1, 2};
const int imm8_table[] = {8, 1, 2, 3, 4, 5, 6, 7};
int movem_index1[256];
int movem_index2[256];
int movem_next[256];
cpuop_func *loop_mode_table[65536];

int hardware_bus_error;
bool debugmem_trace;

static _Thread_local struct uae_m68k_context *active_context;
static cpuop_func *opcode_table[65536];
static atomic_int table_state;
static uint32_t legal_opcode_count;

extern const struct cputbl op_smalltbl_12[];

static void abort_memory(enum uae_m68k_memory_status status, uaecptr address) {
    active_context->memory_status = status;
    active_context->fault_address = address;
    longjmp(active_context->fault_jump, 1);
}

static uae_u32 read_memory(uaecptr address, uae_u8 width, uae_u8 instruction) {
    uae_u32 value = 0;
    if (width > 1 && (address & 1))
        abort_memory(UAE_M68K_MEMORY_MISALIGNED, address);
    const struct uae_m68k_read_request request = {
        .address = address, .width = width, .instruction = instruction};
    enum uae_m68k_memory_status status =
        active_context->memory.read(active_context->memory_user, &request, &value);
    if (status != UAE_M68K_MEMORY_OK)
        abort_memory(status, address);
    return value;
}

static void write_memory(uaecptr address, uae_u8 width, uae_u32 value) {
    if (width > 1 && (address & 1))
        abort_memory(UAE_M68K_MEMORY_MISALIGNED, address);
    const struct uae_m68k_write_request request = {
        .address = address, .value = value, .width = width};
    enum uae_m68k_memory_status status =
        active_context->memory.write(active_context->memory_user, &request);
    if (status != UAE_M68K_MEMORY_OK)
        abort_memory(status, address);
}

uae_u32 memory_get_byte(uaecptr address) { return read_memory(address, 1, 0); }
uae_u32 memory_get_word(uaecptr address) { return read_memory(address, 2, 0); }
uae_u32 memory_get_long(uaecptr address) {
    return (memory_get_word(address) << 16) | memory_get_word(address + 2);
}
uae_u32 memory_get_wordi(uaecptr address) { return read_memory(address, 2, 1); }
uae_u32 memory_get_longi(uaecptr address) {
    return (memory_get_wordi(address) << 16) | memory_get_wordi(address + 2);
}
void memory_put_byte(uaecptr address, uae_u32 value) { write_memory(address, 1, value); }
void memory_put_word(uaecptr address, uae_u32 value) { write_memory(address, 2, value); }
void memory_put_long(uaecptr address, uae_u32 value) {
    memory_put_word(address, value >> 16);
    memory_put_word(address + 2, value);
}

static uae_u16 compose_sr(void) {
    return (uae_u16)((regs.t1 << 15) | (regs.s << 13) | (regs.intmask << 8) | (GET_XFLG() << 4) |
                     (GET_NFLG() << 3) | (GET_ZFLG() << 2) | (GET_VFLG() << 1) | GET_CFLG());
}

void REGPARAM2 MakeSR(void) { regs.sr = compose_sr(); }

static void import_sr(uae_u16 sr) {
    int old_supervisor = regs.s;
    regs.sr = sr;
    regs.t1 = (sr >> 15) & 1;
    regs.t0 = 0;
    regs.s = (sr >> 13) & 1;
    regs.m = 0;
    regs.intmask = (sr >> 8) & 7;
    SET_XFLG((sr >> 4) & 1);
    SET_NFLG((sr >> 3) & 1);
    SET_ZFLG((sr >> 2) & 1);
    SET_VFLG((sr >> 1) & 1);
    SET_CFLG(sr & 1);
    if (old_supervisor != regs.s) {
        if (old_supervisor) {
            regs.isp = m68k_areg(regs, 7);
            m68k_areg(regs, 7) = regs.usp;
        } else {
            regs.usp = m68k_areg(regs, 7);
            m68k_areg(regs, 7) = regs.isp;
        }
    }
}

void REGPARAM2 MakeFromSR(void) { import_sr(regs.sr); }
void REGPARAM2 MakeFromSR_T0(void) { import_sr(regs.sr); }
void REGPARAM2 MakeFromSR_STOP(void) { import_sr(regs.sr); }

static void prime_prefetch(uaecptr pc) {
    regs.pc = pc;
    regs.instruction_pc = pc;
    regs.ir = memory_get_wordi(pc);
    regs.irc = memory_get_wordi(pc + 2);
}

static void enter_exception(int vector, uaecptr saved_pc) {
    uae_u16 saved_sr = compose_sr();
    if (!regs.s) {
        regs.usp = m68k_areg(regs, 7);
        m68k_areg(regs, 7) = regs.isp;
    }
    regs.s = 1;
    regs.t1 = regs.t0 = 0;
    m68k_areg(regs, 7) -= 4;
    memory_put_long(m68k_areg(regs, 7), saved_pc);
    m68k_areg(regs, 7) -= 2;
    memory_put_word(m68k_areg(regs, 7), saved_sr);
    regs.isp = m68k_areg(regs, 7);
    active_context->exception_vector = (uae_u8)vector;
    active_context->exception_cycles = 34;
    prime_prefetch(memory_get_longi((uaecptr)vector * 4));
}

void REGPARAM2 Exception_cpu_oldpc(int vector, uaecptr oldpc) {
    uaecptr pc = oldpc == 0xffffffff ? m68k_getpci() : oldpc;
    enter_exception(vector, pc);
}
void REGPARAM2 Exception_cpu(int vector) { Exception_cpu_oldpc(vector, 0xffffffff); }
void REGPARAM2 Exception(int vector) {
    uaecptr pc =
        vector == 2 || vector == 3 || vector == 4 || vector == 8 || vector == 10 || vector == 11
            ? regs.instruction_pc
            : m68k_getpci();
    enter_exception(vector, pc);
}

uae_u32 REGPARAM2 op_illg(uae_u32 opcode) {
    int vector = (opcode & 0xf000) == 0xa000 ? 10 : (opcode & 0xf000) == 0xf000 ? 11 : 4;
    Exception(vector);
    return 0;
}
void REGPARAM2 op_unimpl(uae_u32 opcode) { (void)op_illg(opcode); }

static void fault_from_exception(uaecptr address) {
    abort_memory(UAE_M68K_MEMORY_MISALIGNED, address);
}

void exception3_read_access(uae_u32 opcode, uaecptr address, int size, int fc) {
    (void)opcode;
    (void)size;
    (void)fc;
    fault_from_exception(address);
}
void exception3_read_access2(uae_u32 opcode, uaecptr address, int size, int fc) {
    exception3_read_access(opcode, address, size, fc);
}
void exception3_read_prefetch(uae_u32 opcode, uaecptr address) {
    (void)opcode;
    fault_from_exception(address);
}
void exception3_read_prefetch_only(uae_u32 opcode, uaecptr address) {
    exception3_read_prefetch(opcode, address);
}
void exception3_write_access(uae_u32 opcode, uaecptr address, int size, uae_u32 value, int fc) {
    (void)opcode;
    (void)size;
    (void)value;
    (void)fc;
    fault_from_exception(address);
}
void exception3_write(uae_u32 opcode, uaecptr address, int size, uae_u32 value, int fc) {
    exception3_write_access(opcode, address, size, value, fc);
}
void exception2_read(uae_u32 opcode, uaecptr address, int size, int fc) {
    (void)opcode;
    (void)size;
    (void)fc;
    abort_memory(UAE_M68K_MEMORY_UNMAPPED, address);
}
void exception2_write(uae_u32 opcode, uaecptr address, int size, uae_u32 value, int fc) {
    (void)opcode;
    (void)size;
    (void)value;
    (void)fc;
    abort_memory(UAE_M68K_MEMORY_UNMAPPED, address);
}
void exception2_fetch(uae_u32 opcode, int offset, int pcoffset) {
    (void)opcode;
    (void)pcoffset;
    abort_memory(UAE_M68K_MEMORY_UNMAPPED, m68k_getpci() + offset);
}
void exception2_fetch_opcode(uae_u32 opcode, int offset, int pcoffset) {
    exception2_fetch(opcode, offset, pcoffset);
}

void checkint(void) {}
void m68k_setstopped(int stopped) { regs.stopped = (flagtype)stopped; }
void do_cycles_slow(int cycles) { (void)cycles; }
void do_cycles_stop(int cycles) { (void)cycles; }
bool cpureset(void) {
    if (active_context->memory.reset_devices)
        active_context->memory.reset_devices(active_context->memory_user);
    return false;
}
void branch_stack_push(uaecptr oldpc, uaecptr newpc) {
    (void)oldpc;
    (void)newpc;
}
void branch_stack_pop_rte(uaecptr pc) { (void)pc; }
void branch_stack_pop_rts(uaecptr pc) { (void)pc; }

static void emit_log(struct uae_m68k_context *context, const char *message, uint32_t length,
                     uae_u8 truncated) {
    if (context->diagnostics.write) {
        const struct uae_m68k_log_event event = {.kind = UAE_M68K_LOG_UPSTREAM_DIAGNOSTIC,
                                                 .message = message,
                                                 .length = length,
                                                 .truncated = truncated};
        context->diagnostics.write(context->diagnostics.user, &event);
    } else if (context->dropped_log_count != UINT64_MAX) {
        ++context->dropped_log_count;
    }
}

void write_log(const TCHAR *format, ...) {
    if (!active_context)
        return;
    char message[512];
    va_list arguments;
    va_start(arguments, format);
    int required = vsnprintf(message, sizeof message, format, arguments);
    va_end(arguments);
    if (required < 0) {
        static const char formatting_error[] = "PUAE diagnostic formatting failed";
        emit_log(active_context, formatting_error, sizeof formatting_error - 1, 0);
        return;
    }
    uint32_t length =
        required < (int)sizeof message ? (uint32_t)required : (uint32_t)sizeof message - 1;
    emit_log(active_context, message, length, required >= (int)sizeof message);
}

static void initialize_movem_tables(void) {
    for (int value = 0; value < 256; ++value) {
        int bit = 0;
        while (bit < 8 && !(value & (1 << bit)))
            ++bit;
        movem_index1[value] = bit;
        movem_index2[value] = 7 - bit;
        movem_next[value] = bit == 8 ? 0 : value & ~(1 << bit);
    }
}

static void build_opcode_table(void) {
    init_table68k();
    initialize_movem_tables();
    for (uint32_t opcode = 0; opcode < 65536; ++opcode)
        opcode_table[opcode] = NULL;
    for (int index = 0; op_smalltbl_12[index].handler_ff; ++index)
        opcode_table[op_smalltbl_12[index].opcode] = op_smalltbl_12[index].handler_ff;
    for (uint32_t opcode = 0; opcode < 65536; ++opcode) {
        if (table68k[opcode].mnemo == i_ILLG || table68k[opcode].clev > 0)
            continue;
        if (table68k[opcode].handler >= 0)
            opcode_table[opcode] = opcode_table[table68k[opcode].handler];
        if (opcode_table[opcode])
            ++legal_opcode_count;
    }
    exit_table68k();
}

static void ensure_opcode_table(void) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&table_state, &expected, 1)) {
        build_opcode_table();
        atomic_store(&table_state, 2);
        return;
    }
    while (atomic_load(&table_state) != 2) {
    }
}

struct uae_m68k_context *uae_m68k_context_create(const struct uae_m68k_memory *memory,
                                                 void *memory_user,
                                                 const struct uae_m68k_diagnostics *diagnostics) {
    if (!memory || !memory->read || !memory->write)
        return NULL;
    struct uae_m68k_context *context = calloc(1, sizeof *context);
    if (!context)
        return NULL;
    context->memory = *memory;
    context->memory_user = memory_user;
    if (diagnostics)
        context->diagnostics = *diagnostics;
    struct uae_m68k_context *saved_context = active_context;
    active_context = context;
    ensure_opcode_table();
    active_context = saved_context;
    return context;
}

void uae_m68k_context_destroy(struct uae_m68k_context *context) { free(context); }

static void import_state(const struct uae_m68k_state *state) {
    regs = (struct regstruct){0};
    regflags = (struct flag_struct){0};
    for (int index = 0; index < 8; ++index) {
        m68k_dreg(regs, index) = state->data[index];
        m68k_areg(regs, index) = state->address[index];
    }
    regs.pc = state->pc;
    regs.instruction_pc = state->pc;
    regs.usp = state->usp;
    regs.isp = state->ssp;
    regs.ir = state->ir;
    regs.irc = state->irc;
    regs.stopped = state->stopped;
    regs.halted = state->halted;
    regs.s = (state->sr >> 13) & 1;
    import_sr(state->sr);
}

static void export_state(struct uae_m68k_state *state) {
    for (int index = 0; index < 8; ++index) {
        state->data[index] = m68k_dreg(regs, index);
        state->address[index] = m68k_areg(regs, index);
    }
    state->pc = m68k_getpci();
    state->sr = compose_sr();
    state->usp = regs.usp;
    state->ssp = regs.isp;
    if (regs.s)
        state->ssp = m68k_areg(regs, 7);
    else
        state->usp = m68k_areg(regs, 7);
    state->ir = regs.ir;
    state->irc = regs.irc;
    state->prefetch_address = state->pc;
    state->prefetch_valid = 1;
    state->stopped = regs.stopped != 0;
    state->halted = regs.halted != 0;
}

struct uae_m68k_step_result uae_m68k_step(struct uae_m68k_context *context,
                                          struct uae_m68k_state *state) {
    struct uae_m68k_step_result result = {0};
    if (!context || !state) {
        result.status = UAE_M68K_STEP_HALTED;
        return result;
    }

    struct regstruct saved_regs = regs;
    struct flag_struct saved_flags = regflags;
    struct uae_m68k_context *saved_context = active_context;
    active_context = context;
    context->memory_status = UAE_M68K_MEMORY_OK;
    context->exception_vector = 0;
    context->exception_cycles = 0;
    import_state(state);

    if (setjmp(context->fault_jump)) {
        result.status = UAE_M68K_STEP_MEMORY_FAULT;
        result.memory_status = context->memory_status;
        result.fault_address = context->fault_address;
        export_state(state);
        goto restore;
    }

    if (regs.halted) {
        result.status = UAE_M68K_STEP_HALTED;
        goto export_and_restore;
    }
    if (state->pending_interrupt_level > regs.intmask) {
        uae_u8 vector = context->memory.acknowledge_interrupt
                            ? context->memory.acknowledge_interrupt(context->memory_user,
                                                                    state->pending_interrupt_level)
                            : (uae_u8)(24 + state->pending_interrupt_level);
        regs.stopped = 0;
        regs.intmask = state->pending_interrupt_level;
        enter_exception(vector, m68k_getpci());
        result.status = UAE_M68K_STEP_EXCEPTION;
        result.exception_vector = vector;
        result.cycles = 44;
        goto export_and_restore;
    }
    if (regs.stopped) {
        result.status = UAE_M68K_STEP_HALTED;
        goto export_and_restore;
    }
    if (!state->prefetch_valid || state->prefetch_address != state->pc)
        prime_prefetch(state->pc);
    regs.instruction_pc = regs.pc;
    int trace_after_instruction = regs.t1;
    result.opcode = regs.ir;
    cpuop_func *handler = opcode_table[result.opcode];
    uae_u32 packed_cycles = handler ? handler(result.opcode) : op_illg(result.opcode);
    result.instruction_executed = 1;
    if (!context->exception_vector && trace_after_instruction)
        enter_exception(9, m68k_getpci());
    result.cycles = (packed_cycles & 0xffff) / (CYCLE_UNIT / 2) + context->exception_cycles;
    result.exception_vector = context->exception_vector;
    result.status = context->exception_vector ? UAE_M68K_STEP_EXCEPTION : UAE_M68K_STEP_OK;

export_and_restore:
    export_state(state);
restore:
    active_context = saved_context;
    regs = saved_regs;
    regflags = saved_flags;
    return result;
}

uint32_t uae_m68k_legal_opcode_count(void) {
    ensure_opcode_table();
    return legal_opcode_count;
}

uint64_t uae_m68k_dropped_log_count(const struct uae_m68k_context *context) {
    return context ? context->dropped_log_count : 0;
}
