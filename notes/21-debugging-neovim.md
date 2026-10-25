# 21 — Debugging (Neovim / LazyVim)

Part of the [notes tree](../ainotes.md). Read when setting up nvim-dap, or when you need the
keymaps or dap-ui window behaviour.

## Why it wasn't working

**The LazyVim `dap.core` extra was never enabled.** The spec in
`~/.config/nvim/lua/config/lazy.lua` imported only `lazyvim.plugins` and `plugins`, so
`nvim-dap` was not even installed. No launch config could have fixed that. (`:LazyExtras`
toggles the same thing.)

Two changes, both **outside the repo**, under `~/.config/nvim`:

1. `lua/config/lazy.lua` — added `{ import = "lazyvim.plugins.extras.dap.core" }`
   **before** `{ import = "plugins" }`. Order matters: extras apply first, so user `opts` win.
2. `lua/plugins/dap.lua` — new file; points codelldb at the three Makefile targets.

`codelldb` is installed by mason (`~/.local/share/nvim/mason/bin/codelldb`). An earlier
`command -v codelldb` said "missing" only because mason's `bin/` is on **Neovim's** PATH, not
the shell's. codelldb bundles its own LLDB, so the missing system `lldb` was never a blocker.

Use: open a file in the repo, `<leader>db` to set a breakpoint, then `<leader>dc` and pick
**"minidb: build + debug tests"**. Each config rebuilds via `make -C <root> <file-target>`
inside its `program()` function — the nvim equivalent of VS Code's `preLaunchTask`.

## How the LazyVim mechanisms compose (verified against lazy.nvim source)

- `lazy/core/plugin.lua` `M._values` runs **every** `opts` function for a plugin in spec order
  (super first), so a later `opts` in `plugins/` overrides an earlier extra's `opts`.
- `lazy/core/loader.lua` `M.config` resolves `opts` **then** runs `config`, so
  `mason-nvim-dap` (called from `dap.core`'s `config`) appends four `LLDB: Launch*` configs
  *after* the custom three. Harmless — just pick the `minidb:` entries.

Verify a load-order claim by reading `lazy/core/{plugin,loader}.lua`, not from memory.

## Keymaps (LazyVim defaults)

| Key | Action | | Key | Action |
|---|---|---|---|---|
| `<leader>db` | toggle breakpoint | | `<leader>di` | step **i**nto |
| `<leader>dB` | conditional breakpoint | | `<leader>do` | step **o**ut |
| `<leader>dc` | run / continue | | `<leader>dO` | step **O**ver |
| `<leader>da` | run with args | | `<leader>dC` | run to cursor |
| `<leader>dl` | run last | | `<leader>dg` | goto line (no execute) |
| `<leader>dj` | frame **down** | | `<leader>dk` | frame **up** |
| `<leader>dt` | terminate | | `<leader>dP` | pause |
| `<leader>dr` | toggle REPL | | `<leader>ds` | session |
| `<leader>du` | toggle the whole UI | | `<leader>de` | **eval** expression under cursor (n/x) |
| `<leader>dw` | hover a value | | | |

Inside a dap-ui panel (buffer-local, per element; defaults from
`nvim-dap-ui/lua/dapui/config/init.lua:83-92`):

| Key | Action |
|---|---|
| `<CR>` (or double-click) | **expand/collapse** a variable's children |
| `o` | **open** — jump to that stack frame / breakpoint location |
| `e` | **edit a variable's value at runtime**, or edit a watch expression |
| `r` | send the line to the REPL |
| `d` | remove a watch expression |
| `t` | toggle (breakpoint enabled/disabled; subtle frames in stacks) |
| `w` | add a watch |

`e` is the sleeper feature: at a breakpoint you can change a variable and continue, no
recompile. The REPL panel shows a **winbar with clickable play / pause / step icons**
(`controls.enabled`, on because Neovim has `+winbar`). `G` in the `console` panel jumps to the
bottom.

## dap-ui windows: navigate / expand / resize

They are ordinary windows: navigate with `<C-h/j/k/l>` (LazyVim) or `<C-w>w`. `<leader>du`
hides/shows them; they auto-open on `event_initialized`.

**Why `<C-w>>` does nothing:** the docked panels set `winfixwidth`/`winfixheight`
(`nvim-dap-ui/lua/dapui/windows/layout.lua:179-180`), so resize keys are silently ignored.

- **Temporary** (per session):
  `:lua vim.wo.winfixwidth=false vim.wo.winfixheight=false`, then
  `<C-w><` / `<C-w>>` / `<C-w>-` / `<C-w>+`.
- **Permanent** — override `layouts` on the `rcarriga/nvim-dap-ui` spec. Note
  `dapui/config/init.lua:194-196`: supplying `layouts` **replaces** the default outright, so
  every element you want must be listed.

```lua
{
  "rcarriga/nvim-dap-ui",
  opts = {
    layouts = {
      {
        elements = {
          { id = "scopes", size = 0.4 },
          { id = "breakpoints", size = 0.2 },
          { id = "stacks", size = 0.2 },
          { id = "watches", size = 0.2 },
        },
        size = 50, -- width, in columns, of the left panel
        position = "left",
      },
      {
        elements = { "repl", "console" },
        size = 15, -- height, in rows, of the bottom panel
        position = "bottom",
      },
    },
  },
},
```

Within an area, element `size` values are fractions of that area (0.25 each by default);
`render.indent = 1` controls child-variable indentation.

## Verified headlessly

`nvim --headless` + `require("lazy").load({ plugins = { "nvim-dap" } })` confirmed:
config files compile; `nvim-dap`/`nvim-dap-ui` load; `dap.adapters.codelldb` resolves to the
mason binary; all three `program()` calls build and return executable paths; and a real
`dap.run` fires `DAP_EVENT_INITIALIZED` → `DAP_EVENT_EXITED`. That is the pattern for
checking any "is nvim actually configured" question without a GUI.

## Not done (offered)

`lang.clangd` is **not** enabled, so there is no C LSP (hover/definition/completion). clangd
is already installed by mason. Enabling `lazyvim.plugins.extras.lang.clangd` adds it; its own
`opts` defines the codelldb adapter, and the guard in `plugins/dap.lua` lets them coexist.
