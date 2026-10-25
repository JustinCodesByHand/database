# 10 — Project & build

Part of the [notes tree](../ainotes.md). Read when you need the toolchain, the file layout,
or the make targets.

## Environment

The project was previously on a Windows host driven through WSL. It now lives on a native
**CachyOS Linux** box.

| | |
|---|---|
| OS | CachyOS (Arch-based), kernel `7.2.8-1-cachyos`, x86_64 |
| Compiler | gcc 16.2.1 |
| Path | `/home/jcmac/code/database` |
| Windows path | `/mnt/c/...` **does not exist here** |

The old command is obsolete:

```sh
wsl bash -lc "cd /mnt/c/Users/jcmac/database/database && make test"   # OBSOLETE on this box
```

Replaced by a plain native build:

```sh
cd /home/jcmac/code/database && make test
```

> Why WSL was needed once: Git Bash's gcc is MinGW and lacks POSIX
> `fmemopen`/`open_memstream`. On native Linux that constraint is gone.

## Compiler flags

```
CC    := gcc
CSTD  := -std=c17
WARN  := -Wall -Wextra -Werror -Wshadow -Wconversion -Wpointer-arith
DEBUG := -g3 -O0
SAN   := -fsanitize=address,undefined -fno-omit-frame-pointer
```

**Every warning is fatal** (`-Werror`). Treat a warning as a stopped build, not noise.

## Make targets

| Command | Does |
|---|---|
| `make` / `make all` | build `build/minidb` |
| `make test` | build **and run** `build/tests` |
| `make build/tests` | build the test binary **only** (no run) |
| `make build/minidb` | build `build/minidb` **only** |
| `make build/scratch` | build the tokenizer scratch harness **only** |
| `make scratch [ARG='...']` | build **and run** the scratch harness (5s timeout) |
| `make run` | run `build/minidb mini.db` |
| `make clean` | remove `build/`, `*.db`, `*.wal` |

> **The trap that bites every debugger setup:** `make test` builds **and runs**. To rebuild
> without running (e.g. from a debugger's `preLaunchTask`) use the *file* target
> `make build/tests`. Otherwise the program runs once during the build and again under the
> debugger.

## File layout

```
src/    main.c  repl.c repl.h  tokenizer.c tokenizer.h  util/hex.c util/hex.h
        minidb-c-guide.md            <- the guide; line numbers referenced throughout
tests/  test.h test_main.c test_repl.c test_tokenizer.c
        scratch.c                    <- standalone tokenizer harness (own main())
Makefile
scripts/setup-nvim-dap.sh            <- Neovim debug setup (see notes/22-…)
```

Everything is built together. `tests/test_tokenizer.c` is where tokenizer work is verified.
`tests/scratch.c` is filtered out of the test build (it has its own `main()`), so it never
collides with `test_main.c`.

## Chapter 1 (REPL) — DONE

- `repl.c` uses `getline`, `strcspn` to strip the newline, `trim`, then dispatch.
- `.exit` → `__EXIT`; anything else → "Cant parse sql." → `__CANTPARSE`.
- `__EXIT` / `__CONTINUE` / `__CANTPARSE` are **reserved identifiers** (leading double
  underscore). Pending rename — see `notes/60-open-items.md`.

## Editor tooling — clangd (the LSP)

**One server for C and C++.** clangd is it; there is no separate "C LSP". It picks the language
per file: `.c` → C; `.h` is *ambiguous* and both Vim's filetype detection and clangd default
it to **C++** — that was the source of C++ completions while editing `tokenizer.h` / `test.h`
(2026-10-23).

| Piece | Where | What |
|---|---|---|
| `.clangd` | repo root (committed) | `Add: [-xc, -std=c17]` for `.*\.(c|h)$` — pins headers to C. No `-I` flags here: clangd's *fallback* compiles each file with its own directory as cwd, so relative `-I` paths silently misresolve. |
| `make compile_commands` | Makefile (committed) | generates `compile_commands.json` (gitignored — absolute paths, machine-specific) from the same `$(SRC)`/`$(TEST_SRC)`/`$(CFLAGS)` as the build. Re-run after adding/renaming sources. |
| `vim.g.c_syntax_for_h = true` | `~/.config/nvim/init.lua` (outside repo) | Vim's documented switch (`:h filetype`) for `.h` → `c` filetype. Verified: `.h` buffers now `ft=c`. Needs copying to the other laptop. |

The clangd extra (`lazyvim.plugins.extras.lang.clangd`) is enabled via `lazyvim.json`
(`:LazyExtras`) — not in `lazy.lua`'s imports; don't go looking for it there.

DAP needs nothing: codelldb debugs the compiled ELF and has no C/C++ opinion.

Verification tool: `clangd --check=<file>` (NOT `--check-path`) parses one file outside the
editor; its `tweak: … FAIL` lines are internal noise, not diagnostics.
