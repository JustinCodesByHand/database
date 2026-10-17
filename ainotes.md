# AI Notes — MiniDB (C database project)

**Purpose:** handoff notes for AI-assisted sessions on MiniDB, split into a tree so a
session reads only what its task needs instead of the whole log.

**Last updated:** 2026-10-16
**Working directory:** `/home/jcmac/code/database`

---

## How to use these notes (read this first)

This file is the **root**, and the only file you always read in full.

1. Find your task in the routing table below.
2. Open **only** the branch(es) for that task. Do not read the whole `notes/` tree.
3. Open `notes/90-log.md` (append-only session history) only when you need the *why* — it is
   historical, and old entries can contradict the current code.
4. **Trust the repo over these notes** when they disagree. These notes have been wrong before
   — see `notes/30-git.md` (a whole commit was thought lost) and `notes/42-token-type-name.md`
   (a function thought unrecoverable). Re-verify against disk before relying on a claim.

### Routing table

| If your task is… | Read |
|---|---|
| Build / run / toolchain / where files live | `notes/10-project-and-build.md` |
| Debug in **VS Code** | `notes/20-debugging-vscode.md` |
| Debug in **Neovim / LazyVim**; dap-ui keys & resizing | `notes/21-debugging-neovim.md` |
| Set up debugging on a **new machine** | `notes/22-reproduce-debugging.md` |
| DAP **won't work** — step-by-step checks to find out why | `notes/23-debug-lazyvim-troubleshoot.md` |
| Git state, auto-push, recover a lost commit | `notes/30-git.md` |
| Tokenizer — header contract & invariants | `notes/40-tokenizer-contract.md` |
| Tokenizer — current code & open defects | `notes/41-tokenizer-state.md` |
| `token_type_name` | `notes/42-token-type-name.md` |
| `list_push` growth logic | `notes/43-list-push.md` |
| Recall / learn C concepts (switch, loops, `realloc`, `static`) | `notes/50-concepts-c.md` |
| Decide what to do next | `notes/60-open-items.md` |
| Understand *why* something was decided (history, may be stale) | `notes/90-log.md` |

**Most sessions:** start with `notes/60-open-items.md`, then the one topic file it points at.

### Current focus (2026-10-16)

Chapter 2 (tokenizer). Build is green — **7 tests, 0 failed**. The lexer now handles
whitespace, alphabetic words, `*`, and EOF. Unknown characters are consumed and skipped
(stopgap). Next step: keyword table (so `FROM` becomes `TOKEN_FROM`) and punctuation/operators
as real tokens. Details: [41-tokenizer-state](notes/41-tokenizer-state.md),
[60-open-items](notes/60-open-items.md).

**Also open (debugging):** a DAP **"port error"** on the *second laptop* only. Root cause is
almost certainly a legacy `type = "executable"` codelldb adapter there (nvim-dap only substitutes
`${port}` for `type = "server"`). Diagnosis, checks, and the fix:
[23-debug-lazyvim-troubleshoot](notes/23-debug-lazyvim-troubleshoot.md). Verified **no** port
problem on the primary machine.

### How to keep these notes

**STANDING INSTRUCTION (from the learner, 2026-10-03):** update the notes after *every*
meaningful question, discovery, or piece of progress — not in a batch at the end.

- Put the fact in the **topic file** it belongs to (and fix any file it contradicts).
- Put the *narrative* of a session in `notes/90-log.md` (append-only).
- If something was learned the hard way, it goes in the relevant topic file so the next
  session does not rediscover it.
- If a change spans concerns, add a row to the routing table rather than growing this file.

### Layout of the tree

```
ainotes.md                     <- you are here (root: always read)
notes/
  10-project-and-build.md      toolchain, make targets, file layout
  20-debugging-vscode.md       VS Code launch/tasks config
  21-debugging-neovim.md       nvim-dap setup, keymaps, dap-ui usage
  22-reproduce-debugging.md    scripts/setup-nvim-dap.sh + recreation steps
  23-debug-lazyvim-troubleshoot.md  step-by-step checks when DAP won't work
  30-git.md                    branch state, auto-push, detached-HEAD incident
  40-tokenizer-contract.md     the header contract and invariants
  41-tokenizer-state.md        what the code does now + open defects
  42-token-type-name.md        the 27-case type-name function
  43-list-push.md              the growth pattern
  50-concepts-c.md             C concepts asked about / learned
  60-open-items.md             next steps + pending cleanup
  90-log.md                    session history (append-only, historical)
```
