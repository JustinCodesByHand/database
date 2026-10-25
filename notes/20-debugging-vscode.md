# 20 — Debugging (VS Code)

Part of the [notes tree](../ainotes.md). Read when setting up or fixing debugging in VS Code.

Binary targets (see [10-project-and-build](10-project-and-build.md)): `build/tests`,
`build/minidb`, `build/scratch`.

## What is configured

`.vscode/` is **gitignored** (`.gitignore:11`), so these files are local and will **not**
survive a fresh clone — recreate them from this note on a new machine.

| File | Contents |
|---|---|
| `launch.json` | 3 `cppdbg` launch configs, `MIMode: gdb` (gdb is at `/usr/bin/gdb`; codelldb/lldb are not installed), each with a matching `preLaunchTask` |
| `tasks.json` | `make build/tests`, `make build/minidb`, `make build/scratch`, `make test`. Every task declares `"problemMatcher": ["$gcc"]` |
| `c_cpp_properties.json` | IntelliSense config. No `compile_commands.json` exists, so that field is deliberately omitted. |

**Two rules that make or break it:**

1. A `preLaunchTask` must use the **file** target (`make build/tests`), never `make test`,
   or the binary runs once during the build and again under the debugger.
2. Every `preLaunchTask` needs a `problemMatcher` (`$gcc`), or VS Code can **hang before
   launching** the debugger.

## The original failure (why it didn't work)

Three extensions had each written a launch config, and the active one pointed at:

```json
"program": "/home/jcmac/code/database/tests/build/Debug/outDebug"
```

That is the **C/C++ Runner** extension's build convention, and the file never existed.
The Runner never reads the Makefile, and `settings.json` was entirely `C_Cpp_Runner.*`.
The Runner targets single-file projects; for a Makefile project use plain `cppdbg` + tasks.

Separately, `c_cpp_properties.json` was **0 bytes** — invalid JSON, which makes the C/C++
extension error out before debugging even starts.

## `tests/scratch.c` — the best first debug target

It was broken by the field rename and is now fixed (`TokenType`→`tokentype`,
`TokenList.token_list`→`token_list_buffer`, `Token.position`→`start_index`,
`TOKEN_ERROR`→`token_error`). It tokenizes one string and prints every token:

```sh
make build/scratch && ./build/scratch SELECT
```

It is the fastest way to see what the lexer actually produced.
