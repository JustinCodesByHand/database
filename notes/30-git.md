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
| `dev` vs `origin/dev` | **ahead 4**, behind 0 (unpushed) |
| Recent `dev` | `86db7e9` scripts, `e2b07de` notes, `d8e8592` scratch, `1813707` SELECT token |
| Remote | `origin` = `https://github.com/JustinCodesByHand/database.git` |
| Remote branches | `origin/main`, `origin/dev`, `origin/parser` |
| `rescue-a1ce250` | still present (safety branch; safe to delete once confirmed) |

`main` is **not** where the real work is — work happens on `dev`.

## Stashes — READ THIS BEFORE `git stash`

```
stash@{0}: On dev: Auto-stashing changes for checking out main
stash@{1}: On dev: Auto-stashing changes for checking out main
```

Both were created automatically when a `git checkout main` auto-stashed a dirty tree.

- **`stash@{0}` is valuable — it is the prefix-match FIX** for `tokenize_word`, i.e. the next
  work item. It adds the missing length check:
  ```c
  if (newToken.word_length == strlen("SELECT") &&
      (strncasecmp(newToken.input_string, "select", newToken.word_length)) == 0) {
  ```
  **Do not drop it.** Apply with `git stash apply stash@{0}` (or `pop` to apply + delete).
- **`stash@{1}` is superseded** — it deletes two lines of `src/tokenizer.h` (a mangled
  `token_type_name` comment). **Do not pop.** The header has since been repaired properly.

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
- **Auth was never configured**, so auto-push cannot actually push yet. Needs `gh auth login`
  then `gh auth setup-git`. Until then `dev` accumulates unpushed commits.

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
