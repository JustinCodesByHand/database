# Neovim DAP debugging (MiniDB) — step-by-step fix list

If DAP won't work in Neovim on your second laptop, do these in order. Don't skip anything. Share the outputs if it still fails.

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
