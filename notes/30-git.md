# 30 — Git

Part of the [notes tree](../ainotes.md). Read when touching branches/remotes, recovering a
commit, or checking what is stashed.

> **These notes have been wrong before.** A prior cross-setup brief claimed a `parser` branch,
> `main` at `5ae9ff9`, and a merged `ch01-repl`. All wrong. **Run `git log --all --decorate`
> and `git status` before trusting any table here.**

## Current state (verified 2026-10-16)

| Thing | Reality |
|---|---|
| Branch | `dev` |
| `dev` vs `origin/dev` | **ahead 1 / behind 1 — DIVERGED** (see below) |
| Local `dev` HEAD | `8b266f9` (notes + the prefix-match fix, squashed) |
| `origin/dev` | `a9074d0` (notes only) |
| Recent `dev` | `8b266f9`, `af5d522` notes tree, `86db7e9` scripts, `e2b07de` notes, `d8e8592` scratch |
| Remote | `origin` = `https://github.com/JustinCodesByHand/database.git` |
| Remote branches | `origin/main`, `origin/dev`, `origin/parser` |
| `rescue-a1ce250` | still present (safety branch; safe to delete once confirmed) |

`main` is **not** where the real work is — work happens on `dev`.

### ⚠️ `dev` diverged from `origin/dev` (2026-10-16)

A `git rebase -i` squashed the prefix-match fix (`6d54fb4 "fix word stringcmp"`) into the
already-pushed notes commit `a9074d0`, producing `8b266f9`. Rewriting a pushed commit means
local `dev` no longer contains `a9074d0`, so it is **ahead 1 / behind 1** and the auto-push
hook cannot fast-forward — **pushes will fail** until this is resolved. It prints a warning
and keeps the commit; it never forces.

Options:
- `git push --force-with-lease` — remote `dev` moves to `8b266f9`, which contains everything
  `a9074d0` had plus the fix. Safe content-wise; use `--force-with-lease`, never bare
  `--force`. A second machine with the old `dev` then needs `git pull --rebase` (or reset).
- `git reset --hard origin/dev` then re-commit the fix — avoids a force-push but throws away
  the squash.

## Stash

```
stash@{0}: On dev: Auto-stashing changes for checking out main
```

One stash left (2026-10-16). It is the **superseded** 2-line `src/tokenizer.h` deletion (a
mangled `token_type_name` comment). **Do not pop.** Drop it with `git stash drop` when ready.
The other auto-stash held the prefix-match fix and has been applied and committed.

## Auto-push (set up 2026-10-03 22:52)

Goal: every commit goes to `origin` automatically.

- `.git/hooks/post-commit` — installed, executable, tested. Pushes the branch's upstream after
  each commit. **Local only:** `.git/hooks/` is not tracked, so it does **not** exist on
  another clone. To share it, copy the file or point `core.hooksPath` at a dotfiles repo.
  Skips when: rebase/merge/cherry-pick in progress; detached HEAD; no upstream; nothing new;
  `GIT_AUTO_PUSH=0`. On failure it warns and **keeps the commit**; it never `--force`.
  Per-commit opt-out: `GIT_AUTO_PUSH=0 git commit -m "wip"`.
  Dry run: `GIT_AUTO_PUSH_DRY_RUN=1 sh .git/hooks/post-commit`.
- `push.autoSetupRemote=true` set globally, so new branches auto-track.
- `gh` **2.102.0** at `~/.local/bin/gh` (standalone binary, *not* pacman — the pacman sync DB
  was 6 days stale with 172 pending upgrades; installing via pacman would have forced a
  whole-system `-Syu`). **Consequence:** pacman won't update `gh`; re-download manually.
- **Authenticated — auto-push works (confirmed 2026-10-16).** `gh auth status` shows logged in
  to `github.com` as **JustinCodesByHand** (token in the keyring; scopes `gist, read:org, repo,
  workflow`). However, the 2026-10-16 rebase rewrote a pushed commit, so `dev` is now
  **diverged** from `origin/dev` (see state table). The post-commit hook refuses to
  non-fast-forward push (warns and keeps the commit), so you must `git push --force-with-lease`
  to sync after resolving divergence.

## Lesson: detached HEAD silently orphans commits (learned 2026-10-03 the hard way)

A commit can vanish from every branch and still be perfectly intact:

1. `git checkout <raw-sha>` moves to that commit **without moving any branch** → detached HEAD.
2. You commit. It is created **on the detached HEAD only**; no branch points at it. No warning.
3. `git checkout <branch>` returns to the branch at its *old* commit → your commit is now
   **unreachable**. `git log` won't show it; the work looks gone.

**Spot it:** `git reflog --date=iso`. If it shows `commit: <msg>` then later
`checkout: moving from <that-sha>… to <branch>`, that is exactly this.

**Prevention:**
- `git checkout <branch>` (or `git switch <branch>`) before committing. To keep committing
  from a SHA, `git checkout -b <new-branch> <sha>` first.
- `git switch` is safer than `git checkout` — it refuses to leave a branch with uncommitted work.
- `git status`'s first line says it immediately: `On branch dev` vs `HEAD detached at …`.
- Do **not** set `advice.detachedHead false`. That warning is the only guard you have.

**Recovery ladder** (least destructive first):
1. `git reflog` → find the SHA. It survives until `git gc` prunes unreachable objects
   (default grace: 2 weeks).
2. `git branch rescue-<sha> <sha>` — pins it so gc can never take it. **Do this first.**
3. `git cherry-pick rescue-<sha>` (or `git merge`) onto your branch.
4. Last resort: `git diff <sha>~1 <sha> > /tmp/work.patch`, apply by hand.

**Never `git reset --hard` a branch to "get the commit back"** — that discards whatever else
the branch gained. Recover *forward*, not backward. A dangling commit is not a lost commit.
