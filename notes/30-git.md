# 30 — Git

Part of the [notes tree](../ainotes.md). Read when touching branches/remotes, recovering a
commit, or checking what is stashed.

> **These notes have been wrong before.** A prior cross-setup brief claimed a `parser` branch,
> `main` at `5ae9ff9`, and a merged `ch01-repl`. All wrong. **Run `git log --all --decorate`
> and `git status` before trusting any table here.**

## Current state (verified 2026-10-25)

| Thing | Reality |
|---|---|
| Branch | **`main` — the working branch since 2026-10-25** (the learner moved; `dev` merged in) |
| `main` vs `origin/main` | in sync; **the post-commit hook pushes `main` too** |
| `dev` | merged into `main` by `90c73f9`, then retired; `origin/dev` keeps the pre-merge history |
| Remote | `origin` = `https://github.com/JustinCodesByHand/database.git` |
| Old Oct-16 divergence | RESOLVED long before the merge |

### The merge incident (2026-10-25) — what "a merge that didn't work" actually is

The learner merged `dev` into `main` in lazygit; the conflict picks took the **stale Oct-3
side** in five files — `src/tokenizer.c/h`, `tests/test_tokenizer.c`, `tests/scratch.c`,
`ainotes.md` — importing pre-rename identifiers (`TokenType`, `lex_tkn_list`) and the
pre-tree 491-line `ainotes.md`, and the broken merge was pushed. The signature to look for:
build errors naming identifiers that exist nowhere on the good branch. **A merge picks
nothing itself — every hunk pick IS the resolution; `b` (pick both) interleaves stale and
current content** (that produced the franken test file: old bodies, new registrations).
Repaired in `dd55245` by taking dev's five files from the merge's own second parent.

> [!IMPORTANT]
> **After any merge, `make test` gates the push.** The broken merge reached `origin/main`
> because nothing gated it.

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
