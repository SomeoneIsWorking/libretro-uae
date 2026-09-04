#ifndef UAE_M68K_EMBED_H
#define UAE_M68K_EMBED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum uae_m68k_memory_status {
    UAE_M68K_MEMORY_OK = 0,
    UAE_M68K_MEMORY_UNMAPPED = 1,
    UAE_M68K_MEMORY_READ_ONLY = 2,
    UAE_M68K_MEMORY_MISALIGNED = 3
};

enum uae_m68k_step_status {
    UAE_M68K_STEP_OK = 0,
    UAE_M68K_STEP_MEMORY_FAULT = 1,
    UAE_M68K_STEP_HALTED = 2,
    UAE_M68K_STEP_EXCEPTION = 3
};

struct uae_m68k_read_request {
    uint32_t address;
    uint8_t width;
    uint8_t instruction;
};

struct uae_m68k_write_request {
    uint32_t address;
    uint32_t value;
    uint8_t width;
};

struct uae_m68k_memory {
    enum uae_m68k_memory_status (*read)(void *user, const struct uae_m68k_read_request *request,
                                        uint32_t *value);
    enum uae_m68k_memory_status (*write)(void *user, const struct uae_m68k_write_request *request);
    uint8_t (*acknowledge_interrupt)(void *user, uint8_t level);
    void (*reset_devices)(void *user);
};

enum uae_m68k_log_kind { UAE_M68K_LOG_UPSTREAM_DIAGNOSTIC = 0 };

struct uae_m68k_log_event {
    enum uae_m68k_log_kind kind;
    /* The message bytes remain valid only for the duration of the callback. */
    const char *message;
    uint32_t length;
    uint8_t truncated;
};

struct uae_m68k_diagnostics {
    void (*write)(void *user, const struct uae_m68k_log_event *event);
    void *user;
};

struct uae_m68k_state {
    uint32_t data[8];
    uint32_t address[8];
    uint32_t pc;
    uint16_t sr;
    uint32_t usp;
    uint32_t ssp;
    uint16_t ir;
    uint16_t irc;
    uint32_t prefetch_address;
    uint8_t prefetch_valid;
    uint8_t pending_interrupt_level;
    uint8_t stopped;
    uint8_t halted;
};

struct uae_m68k_step_result {
    enum uae_m68k_step_status status;
    enum uae_m68k_memory_status memory_status;
    uint32_t cycles;
    uint32_t fault_address;
    uint16_t opcode;
    uint8_t exception_vector;
    uint8_t instruction_executed;
};

struct uae_m68k_context;

struct uae_m68k_context *uae_m68k_context_create(const struct uae_m68k_memory *memory,
                                                 void *memory_user,
                                                 const struct uae_m68k_diagnostics *diagnostics);
void uae_m68k_context_destroy(struct uae_m68k_context *context);

struct uae_m68k_step_result uae_m68k_step(struct uae_m68k_context *context,
                                          struct uae_m68k_state *state);

uint32_t uae_m68k_legal_opcode_count(void);
/* Missing sinks never print; this saturating counter makes every discarded event observable. */
uint64_t uae_m68k_dropped_log_count(const struct uae_m68k_context *context);

#ifdef __cplusplus
}
#endif

#endif
