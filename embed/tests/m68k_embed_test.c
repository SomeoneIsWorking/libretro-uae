#include "uae/log.h"
#include "uae/m68k_embed.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

struct memory_fixture {
    uint8_t bytes[8];
    int emit_diagnostic;
};

struct log_fixture {
    uint32_t count;
    enum uae_m68k_log_kind kind;
    uint8_t truncated;
    char message[64];
};

static enum uae_m68k_memory_status
read_memory(void *user, const struct uae_m68k_read_request *request, uint32_t *value) {
    struct memory_fixture *memory = user;
    if (memory->emit_diagnostic) {
        memory->emit_diagnostic = 0;
        write_log("embed diagnostic %u", 7U);
    }
    if (request->width != 2 || request->address > sizeof memory->bytes - 2)
        return UAE_M68K_MEMORY_UNMAPPED;
    *value = ((uint32_t)memory->bytes[request->address] << 8) | memory->bytes[request->address + 1];
    return UAE_M68K_MEMORY_OK;
}

static enum uae_m68k_memory_status write_memory(void *user,
                                                const struct uae_m68k_write_request *request) {
    (void)user;
    (void)request;
    return UAE_M68K_MEMORY_READ_ONLY;
}

static void record_log(void *user, const struct uae_m68k_log_event *event) {
    struct log_fixture *log = user;
    assert(event->length < sizeof log->message);
    ++log->count;
    log->kind = event->kind;
    log->truncated = event->truncated;
    for (uint32_t index = 0; index < event->length; ++index)
        log->message[index] = event->message[index];
    log->message[event->length] = '\0';
}

static void run_logging_case(int install_sink) {
    struct memory_fixture memory = {
        .bytes = {0x70, 0x07, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71},
        .emit_diagnostic = 1,
    };
    struct log_fixture log = {0};
    const struct uae_m68k_memory callbacks = {
        .read = read_memory,
        .write = write_memory,
    };
    const struct uae_m68k_diagnostics diagnostics = {
        .write = record_log,
        .user = &log,
    };
    struct uae_m68k_context *context =
        uae_m68k_context_create(&callbacks, &memory, install_sink ? &diagnostics : NULL);
    assert(context != NULL);
    struct uae_m68k_state state = {.sr = 0x2700};
    const struct uae_m68k_step_result result = uae_m68k_step(context, &state);
    assert(result.status == UAE_M68K_STEP_OK);
    assert(state.data[0] == 7);
    if (install_sink) {
        assert(log.count == 1);
        assert(log.kind == UAE_M68K_LOG_UPSTREAM_DIAGNOSTIC);
        assert(log.truncated == 0);
        assert(strcmp(log.message, "embed diagnostic 7") == 0);
        assert(uae_m68k_dropped_log_count(context) == 0);
    } else {
        assert(log.count == 0);
        assert(uae_m68k_dropped_log_count(context) == 1);
    }
    uae_m68k_context_destroy(context);
}

int main(void) {
    run_logging_case(1);
    run_logging_case(0);
    assert(uae_m68k_legal_opcode_count() == 45815U);
    return 0;
}
