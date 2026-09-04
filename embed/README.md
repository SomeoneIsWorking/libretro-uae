# PUAE 68000 embed target

`PUAE::M68kEmbed` is the maintained title-neutral 68000 CPU boundary. It owns opaque contexts,
complete 68000 opcode dispatch, callback memory, prefetch, exception and interrupt entry, and
bounded single-instruction stepping. It does not link the libretro frontend or Amiga device owners.

Callers may install `uae_m68k_diagnostics` when creating a context. Every upstream `write_log` call
made inside that context is delivered as `UAE_M68K_LOG_UPSTREAM_DIAGNOSTIC`; the message buffer is
valid only for the callback. A context without a sink never writes to process streams. Instead it
saturating-counts discarded events, observable through `uae_m68k_dropped_log_count`, so absence of a
sink cannot masquerade as evidence that no diagnostic occurred.

Run the standalone synthetic gate from the fork root:

```sh
python embed/tools/verify.py
```

Build outputs stay in the fork-local `build/` root. Remove that generated root and the verifier's
Python bytecode with `python embed/tools/clean.py`; the cleaner refuses every other target.

The fork workflow runs that gate on Linux x86-64, Windows x86-64, and Apple Silicon macOS with full
history and pinned actions. Android compile and device execution are owned by the consuming
`amigaport` workflow because it supplies the pinned `shared/android-port` contract.

The focused C lint profile excludes the easily-swappable-parameter rule for fixed PUAE ABI
signatures and the analyzer's blanket unsafe-buffer warning for bounded `vsnprintf`; Annex-K
formatting functions are not portable across the supported hosts, and truncation is surfaced in the
typed log event.
