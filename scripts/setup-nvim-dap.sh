#!/usr/bin/env bash
# Reproducible LazyVim debugging setup for the MiniDB C project.
#
# Run on any machine that already has Neovim + LazyVim:
#
#     scripts/setup-nvim-dap.sh
#
# It is idempotent -- safe to run as many times as you like. Anything it would
# overwrite is backed up next to the original first.
#
# What it does:
#   1. enables the LazyVim "dap.core" extra in lua/config/lazy.lua
#   2. writes lua/plugins/dap.lua, which points codelldb at build/tests,
#      build/minidb and build/scratch (the same binaries VS Code debugs)
#   3. moves aside any LEGACY codelldb adapter (type = "executable") found at
#      ~/.config/nvim/dap.lua, lua/dap.lua or after/plugin/dap.lua. Such a file
#      hands codelldb the literal "${port}" (nvim-dap only substitutes it for
#      type = "server"), which fails with a "port error".
#
# After running it, inside Neovim:
#   :MasonInstall codelldb     (skip if already installed)
#   :Lazy sync
# then restart Neovim. See ainotes.md, "Debugging (Neovim / LazyVim)".
set -euo pipefail

NVIM="${XDG_CONFIG_HOME:-$HOME/.config}/nvim"
LAZY="$NVIM/lua/config/lazy.lua"
DAP="$NVIM/lua/plugins/dap.lua"
EXTRA='lazyvim.plugins.extras.dap.core'
STAMP="$(date +%Y%m%d%H%M%S)"

if [[ ! -f "$LAZY" ]]; then
  echo "error: $LAZY not found." >&2
  echo "This script expects an existing LazyVim install (it only adds the DAP bits)." >&2
  exit 1
fi

# 1. Enable the DAP extra, immediately after the LazyVim import so it is applied
#    before the user's own plugins (the user's opts must win on load order).
if grep -q "$EXTRA" "$LAZY"; then
  echo "ok:   dap.core already enabled in $LAZY"
else
  cp "$LAZY" "$LAZY.bak.$STAMP"
  awk '
    { print }
    /import = "lazyvim.plugins"/ && !done {
      print "    { import = \"lazyvim.plugins.extras.dap.core\" },"
      done = 1
    }
  ' "$LAZY" > "$LAZY.tmp"
  mv "$LAZY.tmp" "$LAZY"
  echo "ok:   enabled dap.core in $LAZY (backup: $LAZY.bak.$STAMP)"
fi

# 2. Write the launch config (only if it differs from what is already there).
tmp="$(mktemp)"
cat > "$tmp" <<'LUA'
-- Debugging for the MiniDB C project (nvim-dap + codelldb).
--
-- Requires the LazyVim "dap.core" extra, enabled in lua/config/lazy.lua:
--     { import = "lazyvim.plugins.extras.dap.core" },
--
-- These are the SAME binaries the VS Code configs debug (see .vscode/launch.json),
-- so both editors stop in the same places:
--     make build/tests     ->  build/tests    (the test suite)
--     make build/minidb    ->  build/minidb
--     make build/scratch   ->  build/scratch  (tokenizer harness)
--
-- Note the Makefile trap: `make test` builds AND RUNS the suite. A launch config
-- must use the *file* target `build/tests`, otherwise the binary runs once during
-- the build and a second time under the debugger.
return {
  {
    "mfussenegger/nvim-dap",
    opts = function()
      local dap = require("dap")

      -- codelldb bundles its own LLDB, so no system lldb is needed. Mason puts
      -- its bin/ on PATH inside nvim; fall back to the mason path just in case.
      local codelldb = vim.fn.exepath("codelldb")
      if codelldb == "" then
        codelldb = vim.fn.stdpath("data") .. "/mason/bin/codelldb"
      end

      if not dap.adapters.codelldb then
        dap.adapters.codelldb = {
          type = "server",
          host = "localhost",
          port = "${port}",
          executable = {
            command = codelldb,
            args = { "--port", "${port}" },
          },
        }
      end

      -- Project root: walk up from the current file until a Makefile is found.
      -- Falls back to the nvim cwd if that fails.
      local root = (function()
        local dir = vim.fn.fnamemodify(vim.api.nvim_buf_get_name(0), ":p:h")
        if dir == "" then
          dir = vim.fn.getcwd()
        end
        local mk = vim.fn.findfile("Makefile", dir .. ";")
        if mk ~= "" then
          return vim.fn.fnamemodify(mk, ":p:h")
        end
        return vim.fn.getcwd()
      end)()

      -- Rebuild the target, then hand nvim-dap the fresh binary path.
      local function target(make_target, bin)
        return function()
          vim.fn.system({ "make", "-C", root, make_target })
          if vim.v.shell_error ~= 0 then
            vim.notify("make " .. make_target .. " failed (see :messages)", vim.log.levels.ERROR)
          end
          return root .. "/build/" .. bin
        end
      end

      local configs = {
        {
          type = "codelldb",
          request = "launch",
          name = "minidb: build + debug tests",
          program = target("build/tests", "tests"),
          args = {},
          cwd = root,
          stopOnEntry = false,
        },
        {
          type = "codelldb",
          request = "launch",
          name = "minidb: build + debug minidb",
          program = target("build/minidb", "minidb"),
          args = { "mini.db" },
          cwd = root,
          stopOnEntry = false,
        },
        {
          type = "codelldb",
          request = "launch",
          name = "minidb: build + debug scratch",
          program = target("build/scratch", "scratch"),
          -- Change this to whatever string you want to tokenize.
          args = { "SELECT" },
          cwd = root,
          stopOnEntry = false,
        },
      }

      dap.configurations.c = configs
      dap.configurations.cpp = configs
    end,
  },
}
LUA

mkdir -p "$(dirname "$DAP")"
if [[ -f "$DAP" ]] && cmp -s "$tmp" "$DAP"; then
  echo "ok:   $DAP already up to date"
  rm -f "$tmp"
else
  if [[ -f "$DAP" ]]; then
    cp "$DAP" "$DAP.bak.$STAMP"
    echo "ok:   backed up old config to $DAP.bak.$STAMP"
  fi
  mv "$tmp" "$DAP"
  chmod 0644 "$DAP" # mktemp creates 0600; normalise so it matches a normal config file
  echo "ok:   wrote $DAP"
fi

# 3. Neutralise legacy/conflicting codelldb adapters. There must be exactly ONE
#    `dap.adapters.codelldb`, and it must be `type = "server"`. Two ways a second
#    one appears and breaks debugging:
#      a. an old `type = "executable"` adapter that still passes ${port}: nvim-dap
#         only substitutes ${port} for `type = "server"`, so codelldb receives the
#         literal string and dies -> "port error";
#      b. a duplicate spec elsewhere under lua/plugins/ (e.g. dap-c.lua) that wins
#         the load race and clobbers the good adapter -- often one with no mason
#         fallback, so `exepath("codelldb")` is "" and the adapter can't start.
#    Move ANY other file that defines the adapter aside, so the one written in
#    step 2 is the only one in play.
while IFS= read -r f; do
  [[ -z "$f" || "$f" == "$DAP" ]] && continue
  mv "$f" "$f.legacy.$STAMP"
  echo "ok:   moved conflicting codelldb adapter aside: $f -> $f.legacy.$STAMP"
done < <(grep -rlE "dap\.adapters\.codelldb" "$NVIM" --include='*.lua' 2>/dev/null | sort)

cat <<'NEXT'

Next, inside Neovim:
    :MasonInstall codelldb      (skip if codelldb is already installed)
    :Lazy sync
then restart Neovim.

To use: open a file in the repo, <leader>db to set a breakpoint,
then <leader>dc and pick "minidb: build + debug tests".
NEXT
