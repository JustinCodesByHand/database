# 22 — Reproduce debugging on another machine

Part of the [notes tree](../ainotes.md). Read when setting up a new machine.

## Neovim / LazyVim (automated)

Run once per machine, in the repo, on a host that already has Neovim + LazyVim:

```sh
scripts/setup-nvim-dap.sh
```

It is **idempotent** and backs up anything it would overwrite. It:

1. enables the `dap.core` extra in `lua/config/lazy.lua` (inserted right after the
   `lazyvim.plugins` import, so it precedes `plugins/`), and
2. writes `lua/plugins/dap.lua`.

Then, inside Neovim: `:MasonInstall codelldb`, `:Lazy sync`, and restart.

Verified against a sandbox `XDG_CONFIG_HOME` before being trusted on the real config: the
insert position was correct and a second run reported everything `already`.

Assumption: it only *adds* the DAP bits to an existing LazyVim config — it will abort if
`~/.config/nvim/lua/config/lazy.lua` is missing.

## VS Code (manual)

`.vscode/` is **gitignored**, so it does not travel with the repo. On a new machine, recreate
`launch.json`, `tasks.json`, and `c_cpp_properties.json` from
[20-debugging-vscode](20-debugging-vscode.md). Key points: `cppdbg` + `MIMode: gdb`, `program`
= `${workspaceFolder}/build/tests`, `preLaunchTask` = `make build/tests` (the file target, not
`make test`), and every task needs `"problemMatcher": ["$gcc"]`.

## Dependencies to check

| Need | How to check | Install |
|---|---|---|
| `gcc` | `gcc --version` | system package |
| `gdb` (VS Code) | `gdb --version` | system package |
| `codelldb` (Neovim) | `ls ~/.local/share/nvim/mason/bin/codelldb` | `:MasonInstall codelldb` |
| repo built | `make build/tests` | — |

Note: mason installs live in `~/.local/share/nvim/mason/bin/`, which is on **Neovim's** PATH
but usually **not** the shell's — `command -v codelldb` can lie.
