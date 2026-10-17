# 23 — LazyVim DAP troubleshooting (step-by-step)

Part of the [notes tree](../ainotes.md). Read when DAP **won't work** and you need to find out
why. For the happy-path setup (the automated script), see
[22-reproduce-debugging](22-reproduce-debugging.md); for the config/keymaps, see
[21-debugging-neovim](21-debugging-neovim.md).

If DAP won't work in Neovim on your second laptop, do these in order. Don't skip anything. Share the outputs if it still fails.

## Known issue: "port error" when starting the debugger

**Symptom:** pressing `<leader>dc` produces something like
`invalid value '${port}' for '--port <PORT>'`, or `Could not connect to adapter on port …`, or
codelldb exits immediately.

**Why it happens.** nvim-dap substitutes the `${port}` placeholder **only** for adapters declared
with `type = "server"`. An older declaration uses `type = "executable"` (or `"pipe"`) while still
passing `args = { "--port", "${port}" }`. Then the literal string `${port}` is handed to
codelldb, which rejects it — a **port error before any debugging happens**.

**There must be exactly ONE codelldb adapter, and it must be `type = "server"`.** Find them all:

```sh
grep -rn "codelldb" ~/.config/nvim --include='*.lua'
```

**Good** definition (from `lua/plugins/dap.lua`, written by `scripts/setup-nvim-dap.sh`):

```lua
dap.adapters.codelldb = {
  type = "server",
  host = "localhost",
  port = "${port}",
  executable = { command = codelldb, args = { "--port", "${port}" } },
}
```

**Bad / legacy** definition — this is what throws the port error (note `type = "executable"`):

```lua
dap.adapters.codelldb = {
  type = "executable",
  command = "codelldb",
  args = { "--port", "${port}", "--non-interactive" },
}
```

**Files that can smuggle in the bad version** (any of these existing is a problem):

| Path | Loaded how |
|---|---|
| `~/.config/nvim/dap.lua` | NOT auto-loaded from here on the primary machine (outside `lua/`), but easy to copy in and confuse |
| `~/.config/nvim/lua/dap.lua` | **`require`-able — dangerous** |
| `~/.config/nvim/after/plugin/dap.lua` | **auto-loaded after plugins — most dangerous** |

**Fix:** move any legacy file aside and restart Neovim:

```sh
mv ~/.config/nvim/dap.lua            ~/.config/nvim/dap.lua.legacy 2>/dev/null
mv ~/.config/nvim/lua/dap.lua        ~/.config/nvim/lua/dap.lua.legacy 2>/dev/null
mv ~/.config/nvim/after/plugin/dap.lua ~/.config/nvim/after/plugin/dap.lua.legacy 2>/dev/null
```

`scripts/setup-nvim-dap.sh` now does this automatically (backs up, does not delete) as its step 3.
Re-run it on the second laptop, then `:Lazy sync` and restart.

**Confirm which adapter is actually registered** (run in a repo dir):

```sh
nvim --headless -c "lua require('lazy').load({plugins={'nvim-dap'}}); print(vim.inspect(require('dap').adapters.codelldb))" -c "qa!" 2>&1 | tail -20
```

It must print `type = "server"` and `port = "${port}"`. If it shows `type = "executable"`, some
other file won the race — find it with the grep above.

**Verify codelldb itself** (independent of Neovim):

```sh
~/.local/share/nvim/mason/bin/codelldb --help              # must list --port
~/.local/share/nvim/mason/bin/codelldb --port 55123 &      # should LISTEN
ss -ltnp | grep 55123                                      # 127.0.0.1:55123
kill %1
```

### What we verified on the PRIMARY machine (2026-10-16)

- The registered adapter **is** `type = "server"`, `port = "${port}"`, codelldb at the mason path.
- `codelldb --port 55123` **listens** on `127.0.0.1:55123`; `--help` lists `--port`.
  (`codelldb --version` is **not** supported — it errors `unexpected argument found`, so a
  "version check" failing is expected, not a bug.)
- A headless launch of `build/tests` completed the whole handshake: `~/.local/state/nvim/dap.log`
  shows codelldb connecting and the debuggee launching via
  `codelldb-launch --connect=127.0.0.1:43933`.
- Therefore the primary machine has **no** port problem. The error is specific to the **second
  laptop**, and the difference is almost certainly a stray/legacy adapter file there.

**Capture the real error on the second laptop** — this is the fastest way to a definitive fix:

```vim
:lua require('dap').set_log_level('DEBUG')
" reproduce the failure, then:
:messages
:edit ~/.local/state/nvim/dap.log
```

## 1) Check Neovim and LazyVim are there
```sh
nvim --version | head -2
# Does ~/.config/nvim exist and look like LazyVim?
ls -la ~/.config/nvim/lua/config/lazy.lua
```
- If `lazy.lua` is missing → this laptop doesn't have LazyVim. Install LazyVim first.
- If it exists → continue.

## 2) Pull the repo (get current files)
```sh
cd /home/jcmac/code/database
git pull
scripts/setup-nvim-dap.sh --help 2>&1 | head -1  # should exist
ls -la build/tests build/minidb build/scratch 2>&1
```
- If `scripts/setup-nvim-dap.sh` is missing or not executable → `chmod +x scripts/setup-nvim-dap.sh`
- If the three binaries are missing → `make build/tests build/minidb build/scratch`

## 3) Back up (just in case)
```sh
TS=$(date +%Y%m%d%H%M%S)
[ -d ~/.config/nvim ] && cp -r ~/.config/nvim ~/.config/nvim.bak.$TS && echo "backed up to ~/.config/nvim.bak.$TS"
```

## 4) Run the setup script
```sh
cd /home/jcmac/code/database
scripts/setup-nvim-dap.sh
```
**Expected:**
- `ok:   dap.core ...` (enabled or already there)
- `ok:   wrote .../lua/plugins/dap.lua` or `ok:   ... already up to date`

**If it aborts:**
- `error: .../lua/config/lazy.lua not found` → LazyVim not installed here. Fix that first.
- Anything else → copy the **entire** output of that command.

## 5) Verify the edits actually landed
```sh
# Must show the extra import (after lazyvim.plugins import)
grep -n 'dap.core' ~/.config/nvim/lua/config/lazy.lua
# Plugin file must exist
ls -la ~/.config/nvim/lua/plugins/dap.lua
# Show the first 40 lines of dap.lua to spot obvious issues
sed -n '1,40p' ~/.config/nvim/lua/plugins/dap.lua
```
**Check:** the `dap.core` import appears **before** `{ import = "plugins" }` (the script does this). If it's after, DAP won't be loaded correctly.

## 6) Install codelldb and sync LazyVim (INSIDE Neovim)
```sh
cd /home/jcmac/code/database
nvim .
```
From inside Neovim, run these **exact** commands (colon commands):
- `:MasonInstall codelldb`
- `:Lazy sync`
- `:q!` (or quit and reopen)

**Check codelldb binary exists (outside Neovim):**
```sh
test -x ~/.local/share/nvim/mason/bin/codelldb && echo "codelldb: OK" || echo "codelldb: MISSING"
```
(If "MISSING", repeat step 6 — Mason may have failed.)

## 7) Headless sanity check
```sh
cd /home/jcmac/code/database
nvim --headless -c "
lua << 'LUA'
-- try to load nvim-dap
local ok, dap = pcall(require, 'dap')
if not ok then
  print('ERR: cannot require dap'); os.exit(1)
end
print('OK: dap loaded')
-- try to load lazy plugin state for nvim-dap
local lazy_ok = require('lazy').has('nvim-dap')
print('lazy has nvim-dap:', lazy_ok)
-- codelldb
local c = vim.fn.exepath('codelldb')
if c == '' then c = vim.fn.stdpath('data')..'/mason/bin/codelldb' end
print('codelldb path:', c, vim.fn.executable(c)==1 and '(executable)' or '(not executable)')
-- program paths
local root = vim.fn.getcwd()
for _,p in ipairs{'build/tests','build/minidb','build/scratch'} do
  local full = root..'/'..p
  print('prog', p, vim.fn.filereadable(full)==1 and '(exists)' or '(MISSING)')
end
os.exit(0)
LUA
" +q 2>&1 | tail -40
```

**Expected:** `OK: dap loaded`, `lazy has nvim-dap: true`, `codelldb path: ... (executable)`, all three progs `(exists)`. If any say `ERR`/`MISSING`, that tells you the specific thing.

## 8) Actually debug (and capture errors)
1. Open Neovim in the repo: `cd /home/jcmac/code/database && nvim .`
2. Open `src/tokenizer.c` (or any C file)
3. Set breakpoint: `<leader>db` on some line
4. Run: `<leader>dc` → pick **`minidb: build + debug tests`**
5. Watch for errors. If it fails immediately, look at `:messages` (in Neovim: `:messages`)

**If it doesn't stop at breakpoint or exits instantly:**
- Check `:messages` first (copy the last ~30 lines)
- Check `:LspInfo`? Only relevant if clangd is involved; dap itself doesn't require LSP. But worth noting if enabled.

## 9) Quick answers to "what's wrong"
- `dap.core` import not present in lazy.lua → step 5 fails check
- `dap.lua` missing or wrong → step 5 fails
- codelldb missing → step 6/7
- build binaries missing → step 2/5
- configs don't show in picker → step 6 (`:Lazy sync` + restart) or extra load order

**Copy ALL outputs from steps 4–7 AND the last ~30 lines of `:messages` from step 8. That’s usually enough to pinpoint it.**
