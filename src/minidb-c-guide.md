# MiniDB — Build a SQL Database in C

**A self-contained, test-driven build guide.**

This document is written so you can work through the entire project with only this
file, the documentation linked inside it, and your own head. Every chapter tells you
*what* you're building, *why* it's built that way, which data structures to reach for,
and gives you skeletons and a test list. It does not give you finished implementations,
because typing someone else's implementation teaches you nothing.

---

## Table of contents

**Part 0 — [How to use this guide](#part-0--how-to-use-this-guide)**
**Part 1 — [The reference shelf](#part-1--the-reference-shelf)**
**Part 2 — [Environment and project setup](#part-2--environment-and-project-setup)**
**Part 3 — [Git from the command line](#part-3--git-from-the-command-line)**
**Part 4 — [GitHub Actions](#part-4--github-actions)**
**Part 5 — [TDD and refactoring reference](#part-5--tdd-and-refactoring-reference)**
**Part 6 — [The C toolkit for this project](#part-6--the-c-toolkit-for-this-project)**
**Part 6B — [C syntax refresher](#part-6b--c-syntax-refresher-foobar-edition)** (foo/bar examples, every gotcha)
**Part 7 — [The chapters](#part-7--the-chapters)**
**Part 8 — [Appendices](#part-8--appendices)** (crash decoder, byte layouts, glossary)
**Part 9 — [Questions you're going to have](#part-9--questions-youre-going-to-have)** (predicted stuck points, by chapter)

> **Stuck right now?** Jump to [Part 9](#part-9--questions-youre-going-to-have).
> **C feeling rusty?** [Part 6B](#part-6b--c-syntax-refresher-foobar-edition) is a syntax
> refresher with foo/bar examples and every gotcha that bites in this project.
> **Something crashed?** [Part 8A](#a-crash-and-error-decoder) decodes segfaults,
> sanitizer output, and valgrind reports.

---

## A word about C, since you're choosing it deliberately

C is the right language for this project. A database is fundamentally about controlling
bytes on disk, and C lets you say exactly what you mean with no runtime, no object
headers, and no serialization layer between you and the file. The best existing tutorial
on this exact project is in C. SQLite is in C. You will be reading real database source
code by Chapter 6 and it will look like your own code.

C is also less forgiving than most languages. There is no bounds checking, no garbage
collector, and no exception to catch when something goes wrong — a mistake produces
either a crash or, worse, silently wrong behavior that shows up three functions later.

**Two things make this completely manageable, and this guide leans on both hard:**

1. **Sanitizers.** `-fsanitize=address,undefined` turns silent memory corruption into a
   loud, precise error message with a stack trace and a line number. This one compiler
   flag converts C's worst property into something close to a helpful exception. Every
   build in this guide uses it. It is not optional and it is not advanced.
2. **Tests.** A test suite you run every two minutes means a memory bug is at most two
   minutes old and lives in the twenty lines you just wrote. Finding a bug in twenty
   lines is easy. Finding it in four thousand is what makes people hate C.

Nobody debugs C by being careful enough. They debug it with tooling and short feedback
loops. Set both up in Part 2 and the rest of this is a normal project.

---

# Part 0 — How to use this guide

## The architecture you're building

A single-file embedded database in the spirit of SQLite. One `.db` file on disk, a REPL
you type SQL into, and a real B+ tree underneath. Not a server. No networking. No
threads.

```
  REPL / CLI          you type: SELECT * FROM users WHERE id = 3;
     |
  Tokenizer           [SELECT] [STAR] [FROM] [IDENT users] [WHERE] [IDENT id] [EQ] [NUM 3]
     |
  Parser              AST: Select{table="users", where=Binary(Column(id), EQ, Literal(3))}
     |
  Planner             "use the primary key index, seek to key 3"
     |
  Executor            pulls rows through a cursor, one at a time
     |
  Access method       B+ tree: search, insert, split
     |
  Pager               reads/writes 4096-byte pages, caches them in memory
     |
  File                users.db
```

**Each layer only talks to the layer directly below it.** That is the single most
important design decision in the project, and in C it's enforced by which headers a
`.c` file includes. `parser.c` must not `#include "pager.h"`. When you're tempted to
break that, stop — the temptation is the thing you're here to learn to resist.

## Chapter roadmap

| #  | Chapter                      | Core skill you're actually learning                    |
|----|------------------------------|--------------------------------------------------------|
| 1  | REPL and meta-commands       | TDD rhythm, `FILE*` injection, `getline`, ownership     |
| 2  | Tokenizer                    | State machines, string views, pointer arithmetic        |
| 3  | Parser to AST                | Recursive descent, tagged unions, arena allocation      |
| 4  | Row serialization            | Struct padding, `memcpy`, endianness, alignment         |
| 5  | Pager and file persistence   | `malloc` lifecycle, `fseek`/`fread`, buffer pool        |
| 6  | B+ tree leaf nodes           | On-disk layout, binary search over raw bytes            |
| 7  | Splits and internal nodes    | Tree rebalancing, root promotion, cursors               |
| 8  | Catalog and `CREATE TABLE`   | Bootstrapping, dynamic schemas, variable-size records   |
| 9  | `WHERE` and expression eval  | Tree-walking interpreters, three-valued logic           |
| 10 | `DELETE` and `UPDATE`        | Free space, tombstones, `memmove`                       |
| 11 | Write-ahead log              | Durability, `fsync`, crash recovery, checksums          |
| 12 | Planner, `ORDER BY`, joins   | Function pointers, vtables, the iterator model           |

Chapters 1–5 give you a working (if dumb) database that persists data. That is a
genuine milestone and you should stop and enjoy it. Everything after makes it fast and
real.

**Expect this to take a semester.** A chapter is a week of evenings, not an evening.
Chapters 6 and 7 alone may take three weeks. That is normal and is not a sign you're
doing it wrong.

## The unstuck protocol

Being stuck is not the problem; being stuck *without a procedure* is the problem. Work
down this list in order. Do not skip steps, and do not jump to the bottom.

**1. Did it crash? Read the sanitizer output, not the crash.**
If you built with `-fsanitize=address,undefined` (you did, see Part 2), a segfault
comes with a detailed report naming the exact line, the kind of error
(heap-buffer-overflow, use-after-free, stack-buffer-overflow), and where the memory was
allocated and freed. That report is usually the whole answer. Part 8A decodes the
common ones.

**2. If there's no sanitizer output, run it under the debugger.**
```bash
gdb --args ./build/minidb_tests
(gdb) run
(gdb) bt          # backtrace: the call stack at the moment of death
(gdb) frame 1     # move to the caller
(gdb) print foo   # inspect a variable
```
On macOS use `lldb` with the same idea (`run`, `bt`, `frame select 1`, `p foo`).
A segfault with no other information means the crash is 30 seconds from being solved,
because `bt` will tell you exactly where you were.

**3. Read the actual error, out loud, slowly.**
Compiler warnings in particular. `-Wall -Wextra` catches an enormous fraction of real
bugs before you run anything, and this project treats warnings as errors so you can't
ignore them.

**4. Restate what you expected vs. what happened, in writing.**
One sentence each. "I expected `row_deserialize(row_serialize(r))` to give back the same
row. I got a row with an empty username." Half the time, writing this sentence solves
the problem, because it forces you to check whether your expectation was even correct.

**5. Print the bytes.** Not the struct. The bytes.
Serialization bugs are invisible at the struct level and obvious at the byte level.
Part 6 has a `hex_dump` helper. Use it constantly from Chapter 4 onward.

**6. Shrink the test.**
If a test with 20 rows fails, write one with 2 rows. If that passes, try 3. The smallest
failing input is nearly always self-explaining. This has a name — **minimal
reproduction** — and it is a real professional technique.

**7. Check the three C-specific suspects.**
Before anything else, when behavior is *weird* rather than *wrong*:
- Did you `free` something and then use it? (ASan says use-after-free.)
- Is a pointer pointing at a local variable that went out of scope? (ASan says
  stack-use-after-return.)
- Did you write one byte past the end of a buffer? (ASan says heap-buffer-overflow.)

Those three account for most "impossible" behavior in C, and all three are caught
instantly by the sanitizer you already turned on.

**8. Explain it to an inanimate object.**
Genuinely. Say the code out loud, line by line, to a wall. It works because speech is
slower than reading, so you can't skim.

**9. Go to the primary source.**
Not a blog. The man page (`man 3 fread`), or cppreference, or the SQLite file format
spec. Part 1 has direct links. Reading primary documentation compounds; reading forum
answers does not.

**10. Leave it.**
Commit your failing test with a `// FIXME` and go to bed. If you have been on one
problem for more than 90 minutes, you are no longer debugging, you are staring.

**11. Now go find a human.** Office hours, a classmate, a study group. Bring: the
minimal reproduction, what you expected, what happened, what you've tried. That's the
format of a good bug report, and practicing it is directly practicing a professional
skill.

## How to search effectively

- Search the **function name plus the problem**: `fread short read return value`, not
  your whole error with your own function names in it.
- For any standard library function, just read the man page first: `man 3 memcpy`. It's
  faster than a search and it's authoritative.
- Prefix with `site:en.cppreference.com` to force the reference to the top.
- If you're searching for more than 10 minutes, the answer probably isn't a search
  result. Go back to step 5 (print the bytes) or step 6 (shrink the test).

---

# Part 1 — The reference shelf

Bookmark all of these now. When a chapter says "docs for this chapter," it means these.

## C language and standard library

| Resource | Use it for | Link |
|---|---|---|
| **cppreference — C library** | The best C reference online. Every function, with examples. Live here. | <https://en.cppreference.com/w/c> |
| **man pages** | `man 3 fread`, `man 2 fsync`. Authoritative, offline, instant. | `man 3 <function>` |
| **Beej's Guide to C Programming** | Free, friendly, complete. The best "relearn C" book. | <https://beej.us/guide/bgc/> |
| **Modern C**, Jens Gustedt (free PDF) | The best modern C book. Chapters on pointers and memory are excellent. | <https://gustedt.gitlabpages.inria.fr/modern-c/> |
| `<stdint.h>` fixed-width types | Chapter 4 onward. `uint32_t`, `uint8_t`. Non-negotiable for file formats. | <https://en.cppreference.com/w/c/types/integer> |
| `<string.h>` (`memcpy`, `memmove`, `memcmp`) | Chapters 4–7. | <https://en.cppreference.com/w/c/string/byte> |
| `<stdio.h>` file I/O | Chapter 5. `fopen`, `fseek`, `fread`, `fwrite`. | <https://en.cppreference.com/w/c/io> |
| `<stdlib.h>` memory | `malloc`, `calloc`, `realloc`, `free`. | <https://en.cppreference.com/w/c/memory> |
| **C17 draft standard (N2176)** | When you need to know what's actually guaranteed. Dense but definitive. | <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2176.pdf> |
| **SEI CERT C Coding Standard** | Named rules for the traps. Searchable when you want to know "is this safe?" | <https://wiki.sei.cmu.edu/confluence/display/c> |

**How to read cppreference:** start with the signature, then the "Parameters" and
"Return value" sections, then — most importantly — **"Notes"**, which is where the traps
live. For `strncpy`, the Notes section is the entire reason not to use it.

## Tooling

| Resource | Use it for | Link |
|---|---|---|
| **GCC warning options** | What `-Wall -Wextra` actually enable, and what else exists. | <https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html> |
| **AddressSanitizer** | Catches buffer overflows, use-after-free, leaks. Your most important tool. | <https://github.com/google/sanitizers/wiki/AddressSanitizer> |
| **UndefinedBehaviorSanitizer** | Catches signed overflow, misaligned access, bad shifts. | <https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html> |
| **Valgrind** | Slower than ASan but catches uninitialized reads ASan misses. | <https://valgrind.org/docs/manual/quick-start.html> |
| **GDB cheat sheet** | The debugger. Learn `run`, `bt`, `break`, `print`, `step`. | <https://sourceware.org/gdb/current/onlinedocs/gdb.html/> |
| **GNU Make manual** | Your build system. Read §2 "An Introduction to Makefiles". | <https://www.gnu.org/software/make/manual/make.html> |

## Git and GitHub

| Resource | Use it for | Link |
|---|---|---|
| **Pro Git (free book)** | Chapters 1–3 cover everything you need. Genuinely excellent. | <https://git-scm.com/book/en/v2> |
| Git reference manual | `git help <command>` in prose form. | <https://git-scm.com/docs> |
| GitHub Docs: Actions | The CI system. | <https://docs.github.com/en/actions> |
| Actions workflow syntax | Every key in the YAML file. | <https://docs.github.com/en/actions/reference/workflow-syntax-for-github-actions> |

## Databases and language implementation

| Resource | Use it for | Link |
|---|---|---|
| **"Let's Build a Simple Database"** (cstack) | A SQLite clone **in C**, built in nearly this order. Your single best companion. Read its part *after* you finish yours. | <https://cstack.github.io/db_tutorial/> |
| **SQLite file format spec** | The real thing you're imitating. Read §1.6 "B-tree Pages" before Chapter 6. | <https://www.sqlite.org/fileformat2.html> |
| SQLite architecture overview | The layer diagram, from the source. | <https://www.sqlite.org/arch.html> |
| **Crafting Interpreters** | Part III is a bytecode VM **in C** — Chapter 16 (Scanning) and 17 (Compiling) map onto your Chapters 2 and 3, in C, with the memory management shown. | <https://craftinginterpreters.com/> |
| **Database Internals**, Alex Petrov | Part I is B-trees and storage. Worth buying. | <https://www.databass.dev/> |
| CMU 15-445 (free lectures) | Lectures 3–5 (storage, buffer pool), 7–8 (tree indexes). Projects are in C++. | <https://15445.courses.cs.cmu.edu/> |
| **B+ tree visualizer** | Insert keys, watch splits happen. Do this before Chapter 7. | <https://www.cs.usfca.edu/~galles/visualization/BPlusTree.html> |
| **Refactoring catalog** | Named refactorings. Language-agnostic; the ideas transfer to C directly. | <https://refactoring.guru/refactoring/catalog> |

**cstack's tutorial being in C is a bigger deal now than it would have been in Java.**
You can read its code directly rather than translating. Use it the right way: struggle
with your chapter first, then read theirs. Reading it first converts a project you'd
remember into an afternoon you'd forget.

---

# Part 2 — Environment and project setup

## Install the toolchain

**Linux (Ubuntu/Debian):**
```bash
sudo apt update
sudo apt install build-essential gdb valgrind git
```

**macOS:**
```bash
xcode-select --install          # gives you clang, make, lldb
brew install gdb                # optional; lldb is fine and preinstalled
```
Valgrind support on Apple Silicon is poor. You don't need it — AddressSanitizer works
well on macOS and covers most of the same ground.

**Windows:** use **WSL2** with Ubuntu, then follow the Linux instructions. This guide
uses POSIX functions (`getline`, `fmemopen`, `fsync`) that don't exist in MSVC. WSL is
not a compromise here; it's the normal way to do this.

**Verify before writing a line of code:**
```bash
gcc --version        # or clang --version
make --version
gdb --version
git --version
```

## Which C standard

Use **`-std=c17`**. It's supported by every compiler you'll meet, and it was GCC's
default from version 8 through 14. C23 exists (it is GCC's default from version 15, and Clang accepts `-std=c23` from Clang 18), but pinning `c17` explicitly means your laptop and CI agree, which is the whole point of pinning. Nothing in this guide needs C23.

## Project layout

```
minidb/
├── .github/
│   └── workflows/
│       └── ci.yml
├── .gitignore
├── Makefile
├── src/
│   ├── main.c
│   ├── repl.c        repl.h
│   ├── tokenizer.c   tokenizer.h
│   ├── parser.c      parser.h      ast.h
│   ├── row.c         row.h
│   ├── pager.c       pager.h
│   ├── btree.c       btree.h
│   └── util/
│       ├── hex.c     hex.h
│       └── arena.c   arena.h
├── tests/
│   ├── test.h                  the test harness (given below)
│   ├── test_main.c             runs every suite
│   ├── test_repl.c
│   ├── test_tokenizer.c
│   ├── test_row.c
│   ├── test_pager.c
│   └── test_btree.c
└── build/                      generated; gitignored
```

**Every `.c` gets a matching `.h`.** The header is the *public interface*; the `.c` is
the implementation. Anything not in the header should be `static` in the `.c`, which
makes it invisible outside the file. That's C's version of `private`, and it's how you
enforce the layering from Part 0.

## The Makefile

Plain `make`, not CMake. It's about 30 lines, you'll understand every one of them, and
knowing what a build actually does is worth more here than the convenience.

```makefile
CC      := gcc
CSTD    := -std=c17
WARN    := -Wall -Wextra -Werror -Wshadow -Wconversion -Wpointer-arith
DEBUG   := -g3 -O0
SAN     := -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS  := $(CSTD) $(WARN) $(DEBUG) $(SAN) -Isrc
LDFLAGS := $(SAN)

SRC       := $(shell find src -name '*.c')
SRC_NOMAIN:= $(filter-out src/main.c,$(SRC))
TEST_SRC  := $(shell find tests -name '*.c')

BUILD := build

.PHONY: all test run clean fmt

all: $(BUILD)/minidb

$(BUILD)/minidb: $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(BUILD)/tests: $(SRC_NOMAIN) $(TEST_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -Itests $(SRC_NOMAIN) $(TEST_SRC) -o $@ $(LDFLAGS)

# THE command you will run a thousand times
test: $(BUILD)/tests
	./$(BUILD)/tests

run: $(BUILD)/minidb
	./$(BUILD)/minidb mini.db

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) *.db *.wal
```

**Makefile gotcha that will get you once:** recipe lines must be indented with a **real
tab character**, not spaces. If you see `Makefile:12: *** missing separator. Stop.`,
that's what happened. Configure your editor to keep literal tabs in Makefiles.

Notes on the flags, because they're doing a lot of work:

- **`-Wall -Wextra -Werror`** — turn on the useful warnings and make them fatal. This
  feels harsh and is the single best decision in the file. A C warning is usually a bug.
- **`-Wshadow`** catches a local variable hiding an outer one, which produces
  "impossible" behavior.
- **`-Wconversion`** catches silent narrowing like `int` → `uint8_t`. Noisy at first;
  every complaint is a place you should have written an explicit cast and thought about
  it.
- **`-g3 -O0`** — full debug info, no optimization, so the debugger shows you real line
  numbers and real variables.
- **`-fsanitize=address,undefined`** — the big one. Catches buffer overflows,
  use-after-free, leaks, signed overflow, misaligned access, and bad shifts, at runtime,
  with a precise report. Costs ~2x speed, which is irrelevant here.
- **`-fno-omit-frame-pointer`** — makes the sanitizer's stack traces readable.

**Release build** (for when you want to benchmark, much later):
```bash
make clean && make CFLAGS="-std=c17 -Wall -Wextra -O2 -Isrc" LDFLAGS=""
```

## The test harness

You're going to write your own, because in C it's ~60 lines, it has zero dependencies,
and you'll understand every line of your own failure output. This is normal practice in
real C projects.

`tests/test.h` — copy this in whole, it's infrastructure, not the learning content:

```c
#ifndef MINIDB_TEST_H
#define MINIDB_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int tests_run;
extern int tests_failed;
extern const char *current_test;

#define FAIL(fmt, ...)                                                        \
    do {                                                                      \
        printf("  FAIL %s\n    %s:%d: " fmt "\n",                             \
               current_test, __FILE__, __LINE__, ##__VA_ARGS__);              \
        tests_failed++;                                                       \
        return;                                                               \
    } while (0)

#define ASSERT(cond)                                                          \
    do { if (!(cond)) FAIL("assertion failed: %s", #cond); } while (0)

#define ASSERT_EQ_INT(expected, actual)                                       \
    do {                                                                      \
        long long _e = (long long)(expected), _a = (long long)(actual);       \
        if (_e != _a) FAIL("expected %lld, got %lld", _e, _a);                \
    } while (0)

#define ASSERT_EQ_STR(expected, actual)                                       \
    do {                                                                      \
        const char *_e = (expected), *_a = (actual);                          \
        if (_a == NULL) FAIL("expected \"%s\", got NULL", _e);                 \
        if (strcmp(_e, _a) != 0)                                              \
            FAIL("expected \"%s\", got \"%s\"", _e, _a);                       \
    } while (0)

#define ASSERT_EQ_MEM(expected, actual, n)                                    \
    do {                                                                      \
        if (memcmp((expected), (actual), (n)) != 0)                           \
            FAIL("memory differs over %zu bytes", (size_t)(n));               \
    } while (0)

#define ASSERT_STR_CONTAINS(haystack, needle)                                 \
    do {                                                                      \
        const char *_h = (haystack);                                          \
        if (_h == NULL || strstr(_h, (needle)) == NULL)                       \
            FAIL("expected to find \"%s\" in \"%s\"", (needle),                \
                 _h ? _h : "(null)");                                          \
    } while (0)

#define ASSERT_NULL(ptr)     ASSERT((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT((ptr) != NULL)

/* Runs one test function. */
#define RUN_TEST(fn)                                                          \
    do {                                                                      \
        current_test = #fn;                                                   \
        int before = tests_failed;                                            \
        tests_run++;                                                          \
        fn();                                                                 \
        if (tests_failed == before) printf("  ok   %s\n", #fn);               \
    } while (0)

#define SUITE(name) printf("\n%s\n", name)

#endif /* MINIDB_TEST_H */
```

`tests/test_main.c`:

```c
#include "test.h"

int tests_run = 0;
int tests_failed = 0;
const char *current_test = "";

/* one declaration per test file */
void suite_repl(void);
void suite_tokenizer(void);
void suite_row(void);
void suite_pager(void);
void suite_btree(void);

int main(void) {
    suite_repl();
    suite_tokenizer();
    suite_row();
    suite_pager();
    suite_btree();

    printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

And a test file looks like this:

```c
#include "test.h"
#include "row.h"

static void serialized_row_is_exactly_row_size(void) {
    uint8_t buf[ROW_SIZE];
    Row foo = {.id = 1, .username = "bob", .email = "b@x.com"};
    ASSERT_EQ_INT(ROW_SIZE, row_serialize(&foo, buf));
}

void suite_row(void) {
    SUITE("row");
    RUN_TEST(serialized_row_is_exactly_row_size);
}
```

**Returning a non-zero exit code on failure is what makes CI work** — `make test` fails
the build automatically.

If you'd rather use an off-the-shelf framework, [Unity](https://github.com/ThrowTheSwitch/Unity)
(three files) and [greatest](https://github.com/silentbicycle/greatest) (one header) are
both good and widely used. Your own harness is genuinely fine and I'd start there.

## `.gitignore`

```gitignore
build/
*.o
*.db
*.wal
*.dSYM/
a.out
core
vgcore.*
.vscode/
.idea/
compile_commands.json
```

## Commands you'll run constantly

```bash
make test          # compile + run all tests        <- your main loop
make run           # run the REPL
make clean         # nuke build/ when things get weird
make -j8 test      # parallel build; faster

# run under the debugger when something crashes
gdb --args ./build/tests

# valgrind, for uninitialized-memory bugs ASan can miss (Linux)
valgrind --leak-check=full --track-origins=yes ./build/tests
```

**Editor setup worth ten minutes:** generate a `compile_commands.json` so your editor
knows your include paths and shows errors inline.

```bash
sudo apt install bear          # or: brew install bear
bear -- make clean all
```

Then clangd (VS Code "clangd" extension, or built into CLion/Neovim) gives you real
autocomplete, jump-to-definition, and live errors. In C this is a much bigger quality
of life improvement than in Java.

---

# Part 3 — Git from the command line

## The mental model

Git has four places your code can be. Almost every confusing Git moment comes from not
knowing which one you're talking about.

```
  working tree  --git add-->  index (staging)  --git commit-->  local repo  --git push-->  remote
   (your files)                (what's next)                    (history)                 (GitHub)
```

- **Working tree** — the actual files on disk right now.
- **Index / staging area** — the changes that will go into the next commit. It exists so
  you can commit *some* of your changes and not others.
- **Local repository** — the commit history in `.git/`. Fully functional offline.
- **Remote** — GitHub. Just another copy of the history.

`HEAD` points at the commit you have checked out. A **branch** is a movable pointer to a
commit — that's genuinely all it is, which is why creating one is instant.

## First-time setup

```bash
git config --global user.name  "Your Name"
git config --global user.email "you@example.com"
git config --global init.defaultBranch main
git config --global pull.rebase true      # keeps history linear
```

## Starting the repo

```bash
cd minidb
git init
git add .
git commit -m "chore: project skeleton with makefile, sanitizers, and test harness"

# create an empty repo on github.com first (no README, no .gitignore), then:
git remote add origin https://github.com/<you>/minidb.git
git push -u origin main
```

`-u` sets the upstream so plain `git push` and `git pull` work afterwards.

## The daily loop

```bash
git switch -c ch01-repl        # create + switch to a new branch
# ... write a failing test ...
git add tests/test_repl.c
git commit -m "test: repl exits on .exit"
# ... make it pass ...
git add -A
git commit -m "feat: repl loop with .exit meta-command"
# ... refactor ...
git commit -am "refactor: extract meta_command_parse"

git push -u origin ch01-repl
# open a pull request on GitHub, watch CI, merge

git switch main
git pull
git branch -d ch01-repl
```

**Commit at every green bar.** Small commits are the free version of undo. A commit
containing one passing test and the code to make it pass is a perfect commit.

**Work in branches and pull requests even though you're solo.** The PR page shows you
your own diff in a review context before it lands on `main`. Reading your own diff
catches an astonishing amount — a leftover `printf`, a `free` you meant to add, a test
you commented out. It also means `main` is always green, which is the habit every
professional team runs on.

## Inspecting

```bash
git status                     # what's changed, what's staged  <- run constantly
git diff                       # unstaged changes, line by line
git diff --staged              # what's about to be committed
git log --oneline --graph --all --decorate
git log -p src/pager.c         # history of one file, with diffs
git show <sha>                 # everything about one commit
git blame src/pager.c          # who/when/why for each line
```

Worth an alias:
```bash
git config --global alias.lg "log --oneline --graph --all --decorate"
```

## Recovery recipes

None of these are dangerous if you know the command. **Almost nothing in Git is
unrecoverable once committed.**

| Situation | Command |
|---|---|
| Discard changes to one file (not committed) | `git restore <file>` |
| Unstage a file but keep the changes | `git restore --staged <file>` |
| Fix the message of the last commit | `git commit --amend -m "better message"` |
| Add a forgotten file to the last commit | `git add <file>` then `git commit --amend --no-edit` |
| Undo the last commit, keep changes staged | `git reset --soft HEAD~1` |
| Undo the last commit, keep changes unstaged | `git reset HEAD~1` |
| Undo the last commit and discard the work | `git reset --hard HEAD~1` (destructive) |
| Undo a commit that's already pushed | `git revert <sha>` (makes a new, inverse commit) |
| Stash work to switch branches quickly | `git stash` then `git stash pop` |
| Get back to a commit you "lost" | `git reflog`, find the sha, `git reset --hard <sha>` |

`git reflog` is the safety net: it records every position `HEAD` has held, including ones
no branch points at any more. If you ever think you destroyed work, run it first.

**Warning:** `--amend` and `reset` rewrite history. Only use them on commits you have
not pushed. For anything already on GitHub, use `git revert`.

## Merge conflicts

Git writes markers into the file:

```
<<<<<<< HEAD
#define LEAF_MAX_CELLS 13
=======
#define LEAF_MAX_CELLS (LEAF_SPACE / LEAF_CELL_SIZE)
>>>>>>> ch06-leaf-nodes
```

Above `=======` is your side; below is theirs. Edit to what you actually want, **delete
all three marker lines**, then `git add <file>` and `git commit`. `git merge --abort`
backs out entirely.

## Commit message convention

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat:     a new capability            feat: insert rows into leaf nodes
fix:      a bug fix                   fix: off-by-one in cell offset calculation
test:     adding or fixing tests      test: leaf node rejects duplicate keys
refactor: no behavior change          refactor: extract node_header helpers
docs:     documentation               docs: explain page header layout
chore:    build, deps, config         chore: add ubsan to the makefile
```

Subject under 72 characters, imperative mood ("add", not "added"). If you need to
explain *why*, leave a blank line and write a paragraph — that body is where the value
is, because the diff already shows *what* changed.

---

# Part 4 — GitHub Actions

## Concepts, in order

- **Workflow** — a YAML file in `.github/workflows/`.
- **Event** — what triggers it (`push`, `pull_request`).
- **Job** — a unit that runs on one fresh VM. Jobs run in parallel and are isolated.
- **Runner** — the VM. `ubuntu-latest` is free for public repos.
- **Step** — one thing in a job: `uses:` a prebuilt action, or `run:` a shell command.

The key mental model: **the runner starts completely empty.** No copy of your code, no
compiler beyond the defaults. That's why the first step is always checkout.

## `.github/workflows/ci.yml`

For C, CI earns its keep in a way it doesn't in a memory-safe language: **it compiles
your code with a second compiler.** GCC and Clang disagree about which sloppy code to
warn on, and code that's accidentally relying on undefined behavior often works on one
and breaks on the other. Two compilers is a free second opinion.

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:

jobs:
  build-and-test:
    name: ${{ matrix.cc }} with sanitizers
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        cc: [gcc, clang]

    steps:
      - name: Check out the repository
        uses: actions/checkout@v4

      - name: Build and run tests
        run: make CC=${{ matrix.cc }} test

  valgrind:
    name: Valgrind (no sanitizers)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install valgrind
        run: sudo apt-get update && sudo apt-get install -y valgrind

      # ASan and valgrind conflict — build without sanitizers for this job
      - name: Build without sanitizers
        run: make SAN= test

      - name: Run tests under valgrind
        run: |
          valgrind --error-exitcode=1 --leak-check=full \
                   --errors-for-leak-kinds=definite \
                   ./build/tests
```

Line by line:

- **`matrix: cc: [gcc, clang]`** runs the whole job twice, once per compiler.
  `fail-fast: false` means a gcc failure doesn't cancel the clang run — you want both
  results.
- **`make CC=${{ matrix.cc }} test`** — make variables can be overridden on the command
  line, which is exactly why the Makefile has `CC := gcc` rather than hardcoding it.
- **The separate valgrind job** builds with `SAN=` (empty), because AddressSanitizer and
  valgrind both intercept memory allocation and will fight. Valgrind catches
  **uninitialized reads** that ASan misses, which in a database that reads
  partially-written pages is a genuinely relevant class of bug.
- **`--error-exitcode=1`** makes valgrind failures fail the build. Without it, valgrind
  prints a report and exits 0, and CI stays green while leaking memory.

## Reading a failed run

GitHub **Actions** tab, or the red X next to your commit. Click the run, click the job,
expand the failed step. For a compile error, search the log for `error:`. For a test
failure, your harness prints `FAIL` and the file:line.

## Common CI failures

| Symptom | Cause | Fix |
|---|---|---|
| Passes with gcc, fails with clang | Different warning sets, or you relied on UB | Read the warning — it's usually a real bug |
| `implicit declaration of function 'getline'` | Missing `#define _POSIX_C_SOURCE 200809L` before includes | See Part 6; this bites everyone once |
| Passes locally, fails in CI | You have a stale `build/` dir locally | `make clean && make test` locally to reproduce |
| Valgrind reports leaks, ASan didn't | You never called `free`, and ASan's leak check needs a clean exit | Free everything on the normal exit path |
| Test passes alone, fails in the suite | Shared global state, or a leftover `.db` file | Unique temp filenames per test; reset globals |
| `missing separator` in the Makefile | Spaces instead of a tab in a recipe line | Convert to a real tab |

That first row is the whole reason for the compiler matrix. A second compiler is the
cheapest code review you will ever get.

## Optional: a status badge

In `README.md`:
```markdown
![CI](https://github.com/<you>/minidb/actions/workflows/ci.yml/badge.svg)
```

A green badge on a repo you're showing an interviewer is real signal — especially one
that says the tests pass under two compilers and valgrind.


---

# Part 5 — TDD and refactoring reference

## The loop

**1. RED.** Write one test for behavior that doesn't exist. Run `make test`. Watch it
fail.

> It must fail *for the right reason*. If it fails to compile because the function
> doesn't exist yet, that's a legitimate C red — write the stub returning a dummy value,
> then watch the assertion fail. You haven't proven the test can detect the behavior
> until you've seen it fail on the assertion.

**2. GREEN.** Write the *least* code that makes it pass. Genuinely the least.
Hardcoding a return value is legitimate. It feels like cheating; it isn't. It proves the
test is wired up, and the next test forces the hardcoding out.

**3. REFACTOR.** Now that a test protects you, clean it up. Rename. Extract. Remove
duplication. Run `make test` after every small change.

**4. COMMIT.** Every green bar is a commit point.

**The step everyone skips is 3, and it's the one that teaches you the most.** Green
without refactor is just writing code with extra ceremony. The refactoring step is where
design happens, and design judgment is what separates a senior engineer from a junior
one.

## What to test, and what not to

**Test:**
- Public behavior of a unit — given this input, this output.
- **Boundaries.** Empty, one element, exactly full, one past full. In C, off-by-one
  errors aren't just wrong answers, they're memory corruption. This is where they live.
- Error paths. Bad input should produce a specific, useful error.
- **Round trips.** `deserialize(serialize(x)) == x` is the highest-value test in
  Chapter 4.
- **Invariants.** After a B+ tree split, is every key still findable? One test, a
  hundred bugs caught.
- **Ownership.** Does the caller free what it should? ASan's leak detector turns this
  into an automatic assertion on every test run.

**Don't test:**
- Trivial accessors. You'd be testing the compiler.
- `static` functions directly. Test them through the public function that uses them. If
  a `static` function badly wants its own test, that's a signal it wants to be its own
  translation unit.
- Implementation details you intend to change.

## Test structure: Arrange, Act, Assert

```c
static void splits_when_leaf_is_full(void) {
    /* Arrange */
    uint8_t page[PAGE_SIZE] = {0};
    leaf_init(page, true);
    for (uint32_t i = 0; i < LEAF_MAX_CELLS; i++) {
        Row foo = row_with_id(i);
        leaf_insert(page, i, &foo);
    }

    /* Act */
    Row bar = row_with_id(99);
    int result = leaf_insert(page, 99, &bar);

    /* Assert */
    ASSERT_EQ_INT(DB_FULL, result);
}
```

One blank line between the three sections, and **one logical assertion per test** — one
*reason to fail*, not literally one `ASSERT`.

## Test naming

Name the behavior, not the function. `test_insert` tells you nothing when it goes red
six weeks later.

```c
/* bad */
static void test_insert(void) { }
static void test2(void) { }

/* good */
static void insert_rejects_duplicate_key(void) { }
static void serialized_row_is_exactly_row_size_bytes(void) { }
static void reading_a_page_beyond_eof_returns_a_zero_filled_page(void) { }
```

When CI fails, the test name is the entire error message you get. Make it a sentence.

## Testing with temporary files

Chapters 5 onward touch the disk. Never write to a fixed filename — tests will collide
and pass or fail depending on order. Two workable approaches:

```c
/* Approach 1: mkstemp — portable and safe */
static char *temp_db_path(char *buf, size_t n) {
    snprintf(buf, n, "/tmp/minidb_test_XXXXXX");
    int fd = mkstemp(buf);       /* creates the file, returns a unique name */
    if (fd >= 0) close(fd);
    return buf;
}

static void pages_persist_across_reopen(void) {
    char path[64];
    temp_db_path(path, sizeof path);

    /* ... use it ... */

    remove(path);                /* clean up at the end of EVERY test */
}
```

```c
/* Approach 2: tmpfile() — returns a FILE* that's deleted automatically */
FILE *foo = tmpfile();           /* no name, auto-removed on close */
```

`tmpfile()` is simpler but gives you no path, so it doesn't work for tests that need to
close and *reopen* the database — which is exactly the test that proves persistence.
Use `mkstemp` for those.

**Cleanup is manual in C.** Every test that creates a file must `remove()` it, and every
test that `malloc`s must `free`. AddressSanitizer's leak check will fail your build if
you forget, which is exactly the enforcement you want.

## In-memory `FILE*` for testing I/O without files

This is the C equivalent of injecting a fake stream, and it makes Chapter 1 testable:

```c
#include <stdio.h>

/* Read from a string as though it were a file */
FILE *in = fmemopen("SELECT * FROM foo;\n.exit\n", 25, "r");

/* Capture writes into a growing buffer */
char *out_buf = NULL;
size_t out_len = 0;
FILE *out = open_memstream(&out_buf, &out_len);

repl_run(in, out);

fclose(out);                  /* MUST fclose before reading out_buf */
ASSERT_STR_CONTAINS(out_buf, "minidb> ");
free(out_buf);                /* open_memstream's buffer is yours to free */
fclose(in);
```

Both are POSIX (Linux and macOS, not MSVC — another reason for WSL on Windows).
**`open_memstream`'s buffer is only valid after `fclose` or `fflush`**, and you own it,
so you must `free` it. Forgetting either is a classic and ASan will tell you.

## Code smells to watch for

When you hit one during the refactor step, that's your cue.

| Smell | What it looks like | The fix |
|---|---|---|
| **Long function** | Can't see it on one screen | Extract a `static` helper |
| **Duplicated code** | Third time writing the same 4 lines | Extract a function |
| **Magic number** | `buf + 6` — what's 6? | `#define` or `enum` constant |
| **Long parameter list** | More than ~4 parameters | Pass a `struct` (by pointer) |
| **Out-parameter soup** | `f(a, &b, &c, &d)` | Return a small struct instead |
| **Boolean parameter** | `node_init(page, true)` — true what? | Two functions, or an enum |
| **`void*` everywhere** | Type safety thrown away | A tagged union or a real type |
| **Header includes headers it doesn't need** | `#include` chain nobody understands | Forward-declare; include in the `.c` |
| **Manual `free` scattered everywhere** | Ownership is unclear | One owner, or an arena (Part 6) |
| **Hard-to-write test** | 40 lines of setup | Too many responsibilities — split it |

That last row is the most important line here. **When a test is hard to write, that's
information about your design, not about your testing ability.** Reading test pain as a
design signal is most of what "thinking like a SWE" means in practice.

## The refactorings you'll actually use

**Extract Function.** The workhorse. Select lines, name the concept, make it `static`.

**Extract Constant.** Turn `4096` into `PAGE_SIZE`. Chapters 4–7 are full of byte
offsets and *every one must be a named constant*. A file with bare numbers is
unmaintainable within a week.

**Introduce Parameter Object.** When `read(int page, int offset, int len)` appears in six
signatures, that triple wants to be a struct:
```c
typedef struct { uint32_t page; uint16_t offset; uint16_t len; } Slice;
```

**Replace Conditional with Function Pointers.** Chapter 12's operators. When you have a
`switch (op->type)` in `open`, `next`, and `close`, that's three switches over the same
tag — a vtable struct replaces all three. Part 6 shows the pattern.

**Rename.** Do it constantly. If you can't name a thing, you don't understand it yet.

Full catalog: <https://refactoring.guru/refactoring/catalog>

---

# Part 6 — The C toolkit for this project

Everything here is something a specific chapter needs. Skim now so you know it exists;
come back when the chapter calls for it.

## Feature test macros (read this before you write any code)

`getline`, `fmemopen`, `open_memstream`, `fsync`, and `mkstemp` are POSIX, not ISO C.
With `-std=c17` the compiler hides them unless you ask. Put this at the **very top** of
any `.c` file that uses them — before every `#include`:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
```

**`implicit declaration of function 'getline'` means you forgot this.** It's the single
most common setup error in C projects like this one, and the error message doesn't hint
at the cause at all.

Cleaner alternative — put it in the Makefile so it applies everywhere:
```makefile
CFLAGS := $(CSTD) -D_POSIX_C_SOURCE=200809L $(WARN) ...
```
That's what I'd do. Then you never think about it again.

## Fixed-width integer types — non-negotiable for a file format

```c
#include <stdint.h>

uint8_t  foo;    /* exactly 8 bits, unsigned  — a byte */
uint16_t bar;    /* exactly 16 bits */
uint32_t baz;    /* exactly 32 bits — your page numbers and keys */
uint64_t qux;    /* exactly 64 bits — your LSNs */
int32_t  quux;   /* exactly 32 bits, signed */
size_t   n;      /* big enough for any object size; unsigned; printf with %zu */
```

**`int` is not a fixed size.** It's *at least* 16 bits and is usually 32, but "usually"
is not good enough for a byte layout that must be readable on another machine. Every
field in your on-disk format is a `uint8_t`, `uint16_t`, `uint32_t`, or `uint64_t`. No
exceptions.

Printing them needs the macros from `<inttypes.h>`, because the right format specifier
varies by platform:
```c
#include <inttypes.h>
printf("page %" PRIu32 ", size %zu\n", page_num, byte_count);
```
`%u` for a `uint32_t` usually works and is technically wrong; `-Wformat` will tell you.

## `char` is a mess; use `uint8_t` for bytes

```c
char foo;             /* signed or unsigned — IMPLEMENTATION DEFINED. Avoid for bytes */
signed char bar;      /* definitely signed, -128..127 */
unsigned char baz;    /* definitely unsigned, 0..255 */
uint8_t qux;          /* same as unsigned char in practice; says "byte" to the reader */
```

**Rule for this project: `char*` for text, `uint8_t*` for raw bytes.** Pages are
`uint8_t*`. SQL source is `char*`. Mixing them is where sign-extension bugs come from:

```c
char foo = (char)200;
int bad  = foo;               /* -56 if char is signed. Silently wrong. */
int good = (unsigned char)foo; /* 200. Always. */
```

## Pointers, the short version

```c
int foo = 42;
int *bar = &foo;      /* bar holds the ADDRESS of foo */
*bar = 43;            /* write through the pointer; foo is now 43 */
int baz = *bar;       /* read through it */

int *qux = NULL;      /* points at nothing. Dereferencing it segfaults. */
if (qux != NULL) { }  /* always check before dereferencing */
```

Pointer arithmetic moves in **units of the pointed-to type**:

```c
uint8_t buf[100];
uint8_t *foo = buf;
foo + 1;              /* 1 byte forward  — sizeof(uint8_t) == 1 */

uint32_t nums[100];
uint32_t *bar = nums;
bar + 1;              /* 4 bytes forward — sizeof(uint32_t) == 4 */
```

`buf[i]` is exactly `*(buf + i)`. Arrays decay to pointers when passed to functions,
which is why `sizeof` breaks there — see the gotcha in Part 6B.

`const` on pointers, which you should use everywhere it applies:

```c
const uint8_t *foo;        /* pointer to const data: can't write *foo */
uint8_t *const bar;        /* const pointer: can't change bar itself */
const uint8_t *const baz;  /* both */
```

**Use `const uint8_t *` for any function that reads a page but shouldn't modify it.** The
compiler then enforces your intent, and a reader can tell at a glance whether a function
mutates its input. This is free documentation and free bug prevention.

## Memory: `malloc`, `free`, and ownership

```c
#include <stdlib.h>

uint8_t *foo = malloc(PAGE_SIZE);         /* uninitialized — contains GARBAGE */
uint8_t *bar = calloc(1, PAGE_SIZE);      /* zero-filled. Prefer this for pages. */
if (foo == NULL) { /* out of memory */ }  /* always check */

bar = realloc(bar, PAGE_SIZE * 2);        /* grow; may MOVE the block */
free(foo);
foo = NULL;                               /* prevents accidental reuse */
```

**`calloc` for pages, always.** A page read from beyond end-of-file must be zeros, and
`malloc` gives you whatever was in that memory before. Uninitialized reads are the
nastiest class of C bug because the behavior changes between runs and between debug and
release builds.

**The realloc trap:**
```c
foo = realloc(foo, n);    /* if realloc fails, it returns NULL and you LEAKED foo */

/* correct: */
uint8_t *tmp = realloc(foo, n);
if (tmp == NULL) { /* handle; foo is still valid */ }
foo = tmp;
```

### The ownership rule that makes C manageable

**For every allocation, exactly one piece of code is responsible for freeing it, and you
write down which.** Not a language feature — a discipline, expressed in comments and
naming.

```c
/* Returns a newly allocated Token list. CALLER OWNS IT — call token_list_free(). */
TokenList *tokenize(const char *source);

/* Borrows `page`. Does NOT take ownership; does not free it. */
void leaf_insert(uint8_t *page, uint32_t key, const Row *row);
```

Two conventions that make ownership obvious at the call site:
- A function named `*_create` / `*_new` returns owned memory; there's a matching
  `*_free` / `*_destroy`.
- A function taking `const T *` is borrowing and will not keep or free it.

Follow those two and most memory bugs never happen.

### Arena allocation — use this for the AST

Chapter 3 builds a tree of small nodes. Freeing a tree node-by-node is fiddly, easy to
get wrong, and pure overhead. Instead, allocate every node from one big block and free
the whole block at once:

```c
typedef struct {
    uint8_t *buf;
    size_t   capacity;
    size_t   used;
} Arena;

Arena *arena_create(size_t capacity);
void  *arena_alloc(Arena *a, size_t size);   /* never individually freed */
void   arena_free(Arena *a);                  /* frees EVERYTHING at once */
```

Then parsing is:
```c
Arena *foo = arena_create(64 * 1024);
Statement *stmt = parse(tokens, foo);
/* ... use stmt ... */
arena_free(foo);                  /* every node gone, one call, no leaks possible */
```

**This is a real technique used in real compilers and databases**, not a shortcut. When
a group of allocations all die at the same time, an arena is both faster and safer than
individual `free` calls. Implementing one is about 30 lines and it's a genuinely
valuable thing to have written once.

(Watch alignment: `arena_alloc` should round `used` up to a multiple of
`_Alignof(max_align_t)` before returning a pointer. See the alignment section below.)

## Structs, padding, and the single biggest trap in this project

```c
typedef struct {
    uint8_t  foo;
    uint32_t bar;
} Bad;

sizeof(Bad)      /* 8, NOT 5! */
```

The compiler inserts **3 bytes of padding** after `foo` so that `bar` lands on a 4-byte
boundary, because many CPUs require or prefer aligned access.

### Therefore: never `fwrite` a struct to disk

```c
/* CATASTROPHICALLY WRONG for a file format */
fwrite(&my_row, sizeof my_row, 1, file);
```

Three separate reasons:
1. **Padding bytes are uninitialized** — you write garbage to disk, and the same logical
   row produces different bytes on different runs.
2. **Padding differs between compilers and architectures** — a file written by gcc on
   x86 may not be readable by clang on ARM.
3. **Endianness differs between architectures** — a `uint32_t` written on a
   little-endian machine reads back byte-reversed on a big-endian one.

**Always serialize field by field, explicitly**, which is what Chapter 4 is about:

```c
size_t row_serialize(const Row *foo, uint8_t *out) {
    write_u32_be(out + ID_OFFSET, foo->id);
    write_fixed_str(out + USERNAME_OFFSET, foo->username, USERNAME_SIZE);
    write_fixed_str(out + EMAIL_OFFSET,    foo->email,    EMAIL_SIZE);
    return ROW_SIZE;
}
```

Verbose, explicit, portable, and correct. Every real database does exactly this.

### Lock your layout with `static_assert`

```c
#include <assert.h>

static_assert(ROW_SIZE == 291, "row layout changed — bump the file format version");
static_assert(LEAF_HEADER_SIZE == 14, "leaf header layout changed");
```

This is a **compile-time** check. If someone adds a field and the size shifts, the build
fails immediately with your message, instead of silently corrupting every database file
in existence. Put one of these next to every layout definition.

`offsetof` tells you where a field actually landed, which is useful when you're
debugging padding:
```c
#include <stddef.h>
printf("%zu\n", offsetof(Bad, bar));   /* 4, not 1 */
```

## Endianness: write it explicitly

Use big-endian on disk. It's what SQLite does, and a hex dump reads left-to-right in the
order you'd write the number, so `00 00 00 2a` is visibly 42.

```c
static void write_u32_be(uint8_t *out, uint32_t foo) {
    out[0] = (uint8_t)(foo >> 24);
    out[1] = (uint8_t)(foo >> 16);
    out[2] = (uint8_t)(foo >>  8);
    out[3] = (uint8_t)(foo      );
}

static uint32_t read_u32_be(const uint8_t *in) {
    return ((uint32_t)in[0] << 24)
         | ((uint32_t)in[1] << 16)
         | ((uint32_t)in[2] <<  8)
         | ((uint32_t)in[3]      );
}
```

Eight lines, no headers, works identically on every machine. Note the casts: without
`(uint32_t)`, `in[0] << 24` promotes to `int` and shifting into the sign bit is
undefined behavior. UBSan will catch it; the casts prevent it.

You could use `htonl`/`ntohl` from `<arpa/inet.h>`, but they're POSIX-only and the names
say "network" rather than "file format." The explicit version is clearer and more
portable.

## Alignment: don't cast pointers into the middle of a buffer

```c
uint8_t page[4096];

/* UNDEFINED BEHAVIOR — page+6 may not be 4-byte aligned */
uint32_t foo = *(uint32_t *)(page + 6);

/* CORRECT — memcpy has no alignment requirement */
uint32_t bar;
memcpy(&bar, page + 6, sizeof bar);
```

On x86 the bad version usually works, which is worse than if it always crashed — it
works on your laptop and traps on ARM, and UBSan flags it. Your header layouts put a
`uint32_t` at offset 6, which is not 4-byte aligned, so **this matters in this project
specifically.** Use `memcpy`, or better, use the `read_u32_be` helper above, which reads
byte-at-a-time and sidesteps alignment entirely.

## Strings

C strings are NUL-terminated `char` arrays. The `\0` is part of the storage but not the
length.

```c
#include <string.h>

char foo[32];
strlen(foo);                      /* length WITHOUT the NUL */
strcmp(foo, bar);                 /* 0 if equal (not 1! not true!) */
strncmp(foo, bar, n);             /* compare at most n bytes */
memcpy(dst, src, n);              /* raw bytes, regions must not overlap */
memmove(dst, src, n);             /* same but OVERLAP IS SAFE — use in B+ tree shifts */
memset(foo, 0, sizeof foo);       /* zero it */
memcmp(a, b, n);                  /* 0 if equal */
```

**`strcmp` returns 0 for equal.** `if (strcmp(a, b))` reads like "if equal" and means the
opposite. Always write `if (strcmp(a, b) == 0)`.

**Never use `strcpy` or `strcat`.** They have no length limit and are the classic buffer
overflow. **Also avoid `strncpy`** — it's the trap that looks like the fix: if the source
is at least `n` bytes it does *not* NUL-terminate the destination, leaving you with an
unterminated string that reads off the end.

Safe options:
```c
snprintf(foo, sizeof foo, "%s", bar);       /* always NUL-terminates. Truncates safely. */

/* Or, since your fields are fixed-width and zero-padded anyway: */
memset(foo, 0, size);
memcpy(foo, bar, len < size ? len : size);
```

`snprintf` is the general-purpose right answer and returns how many bytes it *would*
have written, so you can detect truncation.

## String views — for the tokenizer

Chapter 2 produces tokens that are slices of the source. Copying each one with `strdup`
means an allocation and a free per token. Instead, point into the source:

```c
typedef struct {
    TokenType   type;
    const char *start;   /* points INTO the source string; not owned */
    size_t      length;  /* not NUL-terminated! */
    size_t      position;
} Token;
```

Printing one needs the precision specifier, since there's no NUL:
```c
printf("%.*s\n", (int)foo.length, foo.start);   /* %.*s takes length then pointer */
```

**The constraint this creates:** tokens are only valid while the source string is alive.
Write that in a comment above the struct. This is a real trade — zero allocations for a
lifetime dependency — and it's exactly the kind of decision C makes you think about
explicitly. Chapter 2 discusses when to take it and when not to.

## File I/O

```c
#include <stdio.h>

FILE *foo = fopen("mini.db", "r+b");   /* read+write, binary, must exist */
FILE *bar = fopen("mini.db", "w+b");   /* read+write, TRUNCATES to empty */
if (foo == NULL) { perror("fopen"); }  /* perror prints your message + the errno text */

fseek(foo, (long)page_num * PAGE_SIZE, SEEK_SET);   /* note the cast */
size_t n = fread(buf, 1, PAGE_SIZE, foo);           /* returns ITEMS read */
fwrite(buf, 1, PAGE_SIZE, foo);
fflush(foo);                                        /* push libc buffer to the OS */
fclose(foo);
```

**Always check `fread`'s return value.** A short read is not an error — it's how you
learn you hit end of file, which happens constantly in a growing database:

```c
size_t got = fread(buf, 1, PAGE_SIZE, foo);
if (got < PAGE_SIZE) {
    if (feof(foo))  { /* past EOF: zero-fill the rest. NORMAL. */ }
    if (ferror(foo)) { /* a real I/O error */ }
}
```

**`fflush` is not `fsync`.** `fflush` moves data from the C library's buffer into the
operating system. `fsync` moves it from the OS onto the physical device. Only the second
survives a power cut, and it's Chapter 11's whole subject:

```c
#include <unistd.h>
fflush(foo);
fsync(fileno(foo));      /* fileno() gets the raw fd from a FILE* */
```

The `mode` string matters more than it looks: `"w"` truncates the file to zero
immediately. Opening an existing database with `"w+b"` deletes all the data before you
read a byte. Use `"r+b"` for existing files, and create with `"w+b"` only when you know
it's new.

## Error handling without exceptions

C has no exceptions. Pick one convention and use it everywhere. For this project:

```c
typedef enum {
    DB_OK = 0,
    DB_NOT_FOUND,
    DB_DUPLICATE_KEY,
    DB_FULL,
    DB_IO_ERROR,
    DB_PARSE_ERROR,
    DB_OUT_OF_MEMORY,
} DbResult;

/* Return the status; produce the value through an out-parameter. */
DbResult btree_find(BTree *tree, uint32_t key, Row *out_row);
```

Call sites become:
```c
Row foo;
DbResult rc = btree_find(tree, 3, &foo);
if (rc != DB_OK) {
    return rc;               /* propagate */
}
```

Two rules that keep this from becoming a mess:
- **Return `DbResult` from anything that can fail.** Values come back through out-params.
- **Never ignore a returned `DbResult`.** Adding `-Wunused-result` and marking functions
  `__attribute__((warn_unused_result))` makes the compiler enforce it, which is worth
  doing for the handful of functions where ignoring the result is catastrophic.

Add a string converter early — you'll use it in every error message and test failure:
```c
const char *db_result_str(DbResult rc);   /* "DB_DUPLICATE_KEY" */
```

For the `goto cleanup` pattern that handles multi-step failure cleanly, see Part 6B.

## Headers

```c
/* row.h */
#ifndef MINIDB_ROW_H
#define MINIDB_ROW_H

#include <stdint.h>
#include <stddef.h>

#define USERNAME_SIZE 32

typedef struct { uint32_t id; char username[USERNAME_SIZE + 1]; } Row;

size_t row_serialize(const Row *foo, uint8_t *out);

#endif /* MINIDB_ROW_H */
```

- **Include guards** (`#ifndef` / `#define` / `#endif`) prevent double inclusion, which
  otherwise causes "redefinition of struct" errors. `#pragma once` works on every
  compiler you'll use and is one line, but guards are universal — either is fine, just
  be consistent.
- **Headers declare; `.c` files define.** A function body in a header, included twice,
  is a duplicate symbol at link time.
- **Include what you use.** If `row.h` mentions `uint32_t`, it includes `<stdint.h>`
  itself rather than hoping its includer did.
- **`static` in a `.c` file means "private to this file."** Every helper that isn't in
  the header should be `static`. This is your access control, and it also lets the
  compiler optimize better.

`undefined reference to 'foo'` at *link* time means you declared `foo` in a header and
never wrote the body, or the `.c` isn't in the build. That error comes from the linker,
not the compiler, and it always means "the declaration exists but the definition
doesn't."

## Function pointers — for Chapter 12

```c
/* a variable holding a function */
int (*foo)(int, int) = add;
foo(2, 3);

/* readable version with typedef */
typedef int (*BinaryOp)(int, int);
BinaryOp bar = add;
```

The vtable pattern, which replaces the interface you'd write in Java:

```c
typedef struct Operator Operator;

typedef struct {
    DbResult (*open)(Operator *self);
    DbResult (*next)(Operator *self, Row *out);   /* DB_NOT_FOUND when exhausted */
    void     (*close)(Operator *self);
} OperatorVTable;

struct Operator {
    const OperatorVTable *vtable;
    /* subtypes embed this struct as their FIRST member */
};

/* calling through it */
op->vtable->next(op, &row);
```

A concrete operator embeds `Operator` first, so a pointer to it can be cast to
`Operator*` safely (the first member's address equals the struct's address — that's
guaranteed by the standard):

```c
typedef struct {
    Operator base;        /* MUST be first */
    Cursor  *cursor;
} SeqScan;
```

This is how C does polymorphism, and it's exactly how SQLite's virtual table interface
works. Seeing that the "objects" in object-oriented code are a struct plus a function
table is genuinely clarifying.

## The hex dump helper — write this today

You'll use it constantly from Chapter 4 onward. It turns invisible serialization bugs
into visible ones.

`src/util/hex.c`:
```c
#include "hex.h"
#include <stdio.h>
#include <ctype.h>

void hex_dump(const void *data, size_t len) {
    const uint8_t *bytes = data;
    for (size_t i = 0; i < len; i += 16) {
        printf("%08zx  ", i);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) printf("%02x ", bytes[i + j]);
            else             printf("   ");
            if (j == 7) putchar(' ');
        }
        printf(" |");
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            int c = bytes[i + j];
            putchar(isprint(c) ? c : '.');
        }
        printf("|\n");
    }
}
```

Output, with the bug usually visible at a glance:
```
00000000  00 00 00 2a 62 6f 62 00  00 00 00 00 00 00 00 00  |...*bob.........|
```
(There's `42` in the first four bytes, big-endian, then `bob` and zero padding.)

## Debugging tools, concretely

**AddressSanitizer** is already on. When it fires, you get something like:
```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x60300000eff4
WRITE of size 4 at 0x60300000eff4 thread T0
    #0 0x4f2a1b in leaf_insert src/btree.c:87
    #1 0x4f3c22 in test_insert tests/test_btree.c:41
0x60300000eff4 is located 0 bytes to the right of 4096-byte region
allocated by thread T0 here:
    #0 0x7f... in calloc
    #1 0x4f1a05 in pager_get_page src/pager.c:52
```
Read it as: **what** (heap-buffer-overflow), **where** (btree.c:87), **which memory**
(0 bytes past a 4096-byte block allocated in pager.c:52). That's a page overrun, and
you now know the exact line. This report is worth more than an hour of `printf`.

**GDB**, the minimum you need:
```bash
gdb --args ./build/tests
(gdb) run
(gdb) bt                 # where did it die
(gdb) frame 1            # go up one caller
(gdb) print foo          # inspect a variable
(gdb) print *bar         # dereference a pointer
(gdb) print buf[0]@16    # print 16 array elements starting at 0
(gdb) break btree.c:87   # set a breakpoint
(gdb) continue
(gdb) next               # step over
(gdb) step               # step into
```
`print buf[0]@16` is the one nobody knows and everybody needs — it dumps a chunk of an
array in one command.

**Valgrind**, for uninitialized reads ASan misses:
```bash
make SAN= clean test          # rebuild WITHOUT sanitizers
valgrind --leak-check=full --track-origins=yes ./build/tests
```
`--track-origins=yes` tells you where the uninitialized value came from, not just where
it was used. That's usually the whole answer.

**They conflict.** Never run valgrind on an ASan-instrumented binary — rebuild without
sanitizers first. That's why CI has a separate valgrind job.


---

# Part 6B — C syntax refresher (foo/bar edition)

Everything here uses `foo`, `bar`, and `baz` so nothing is entangled with the project.
This is a lookup section — skim once, then come back when something looks unfamiliar.
Traps are marked **Gotcha**.

## The smallest complete program

```c
#include <stdio.h>

int main(void) {
    printf("hello\n");
    return 0;                  /* 0 = success. Non-zero = failure. */
}
```

```bash
gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined foo.c -o foo && ./foo
```

Keep a `scratch.c` and that command in your shell history. **When you're unsure how
something behaves, don't reason about it and don't search — write five lines and watch
it.** That habit is worth more than any amount of reading.

## Types and declarations

```c
int      foo = 42;
unsigned bar = 42u;
long     baz = 42L;
double   qux = 3.14;
float    quux = 3.14f;
char     c = 'a';              /* single quotes = char. "a" = a 2-byte string! */
_Bool    d = 1;

#include <stdbool.h>
bool foo2 = true;              /* bool/true/false need this header in C17 */

#include <stdint.h>
uint32_t bar2 = 42;            /* exactly 32 bits — use these for file formats */

const int baz2 = 5;            /* can't be reassigned */
static int qux2 = 0;           /* file scope: private to this .c; in a function: persists */
```

**Gotcha:** `'a'` is a `char` (one byte); `"a"` is a `char[2]` — the letter plus a NUL.
Mixing them produces confusing type errors.

**Gotcha:** declarations bind the `*` to the variable, not the type:
```c
int *foo, bar;      /* foo is int*, bar is plain int! */
int *foo, *bar;     /* both pointers — what you probably meant */
```

## Operators and precedence

```c
foo + bar   foo - bar   foo * bar   foo / bar   foo % bar
foo++       ++foo       foo--       --foo
foo == bar  foo != bar  foo < bar   foo >= bar
foo && bar  foo || bar  !foo
foo & bar   foo | bar   foo ^ bar   ~foo   foo << 2   foo >> 2
foo ? bar : baz
```

**Gotchas that actually bite:**

```c
7 / 2                 /* 3 — integer division truncates */
7 % 2                 /* 1 */
7.0 / 2               /* 3.5 — one floating operand makes it floating */

if (foo = 5)          /* ASSIGNS 5, then tests it (always true). Wanted ==. */
                      /* -Wall catches this; write if (5 == foo) if you're worried */

foo & 1 == 0          /* == binds TIGHTER than &. This is foo & (1 == 0). */
(foo & 1) == 0        /* what you meant. Parenthesize bitwise ops. Always. */

int foo = INT_MAX;
foo + 1;              /* UNDEFINED BEHAVIOR for signed overflow. UBSan catches it. */

unsigned bar = 0;
bar - 1;              /* 4294967295 — unsigned wraps, defined but surprising */

foo << 32             /* UB if foo is 32 bits. Shift count must be < width. */
```

**The unsigned comparison trap**, which will get you in a loop:
```c
size_t foo = 0;
for (size_t i = 0; i <= foo - 1; i++) { }   /* foo-1 is HUGE. Infinite loop. */

/* also: */
int bar = -1;
unsigned baz = 1;
if (bar < baz) { }    /* FALSE! bar converts to unsigned = 4294967295 */
```
`-Wconversion` and `-Wsign-compare` (in `-Wextra`) flag these, which is why the Makefile
turns them on.

## Control flow

```c
if (foo > 0) { } else if (foo < 0) { } else { }

for (int i = 0; i < 10; i++) { }
while (foo) { }
do { } while (foo);                 /* body runs at least once */

switch (foo) {
    case 1:
        bar();
        break;                      /* FORGETTING THIS falls through to case 2 */
    case 2:
    case 3:                         /* deliberate fall-through: 2 and 3 both do this */
        baz();
        break;
    default:
        qux();
        break;
}

continue;   /* next iteration */
break;      /* leave loop or switch */
```

**Gotcha:** C has no `switch` expression and no automatic `break`. Fall-through is the
default and is a real bug source. If you deliberately fall through, say so:
```c
case 1:
    foo();
    /* fall through */             /* GCC recognizes this comment and stops warning */
case 2:
```

**Gotcha:** always use braces, even for one statement.
```c
if (foo)
    bar();
    baz();            /* baz() runs UNCONDITIONALLY. Indentation lies. */
```

## Functions

```c
int foo(int bar, const char *baz) {
    return bar * 2;
}

void qux(void) { }             /* (void) means NO parameters. () means unspecified! */

static int helper(int foo) { } /* static = private to this .c file */
```

**Gotcha:** `int foo()` and `int foo(void)` are different in C. Empty parens mean
"unspecified arguments" and disable type checking. **Always write `(void)`** for a
no-argument function.

**C passes everything by value**, including structs. To let a function modify something,
pass a pointer:

```c
void bad(int foo)  { foo = 99; }          /* modifies a copy; caller sees nothing */
void good(int *foo){ *foo = 99; }         /* modifies the caller's variable */

int bar = 1;
bad(bar);        /* bar is still 1 */
good(&bar);      /* bar is now 99 */
```

For structs, pass `const T *` to read and `T *` to modify — copying a 300-byte struct on
every call is waste, and the pointer says what you mean.

## Arrays

```c
int foo[5] = {1, 2, 3, 4, 5};
int bar[5] = {0};              /* all zeros — the idiom for zero-init */
int baz[]  = {1, 2, 3};        /* size inferred: 3 */
uint8_t page[4096] = {0};

foo[0] = 10;
size_t n = sizeof foo / sizeof foo[0];    /* 5 — the array-length idiom */
```

**Gotcha — the big one.** Arrays *decay* to pointers when passed to a function, and
`sizeof` then measures the pointer:

```c
void qux(int bar[]) {
    sizeof bar;         /* 8 (a pointer), NOT 20. The length is GONE. */
}
```

**Therefore: always pass the length alongside the array.**
```c
void qux(const int *bar, size_t n);
```
Every function in this project that takes a buffer takes its length too. No exceptions.

**Gotcha:** C does not bounds-check. `foo[10]` on a 5-element array compiles fine and
corrupts memory. ASan catches it at runtime, which is why it's always on.

## Strings

```c
#include <string.h>

char foo[32] = "hello";        /* 5 chars + NUL, rest zeroed */
const char *bar = "hello";     /* points at read-only memory — DON'T modify */

strlen(foo);                   /* 5 — does not count the NUL */
sizeof foo;                    /* 32 — the array size */

strcmp(foo, "hello") == 0;     /* equal. Note the == 0. */
strncmp(foo, bar, 3) == 0;     /* first 3 bytes equal */

snprintf(foo, sizeof foo, "%d items", 3);     /* safe formatting */
```

**Gotcha:** `bar[0] = 'H'` on a string literal is undefined behavior — literals live in
read-only memory. Use an array if you need to modify.

**Gotcha:** `strcmp` returns 0 for equal. `if (strcmp(a, b))` means "if DIFFERENT."

**Never use** `strcpy`, `strcat`, `sprintf`, `gets` — all unbounded. **Also avoid**
`strncpy`, which doesn't NUL-terminate when the source fills the buffer. Use `snprintf`
or explicit `memcpy` with your own bounds.

## Pointers

```c
int foo = 42;
int *bar = &foo;        /* & = address-of */
*bar;                   /* 42 — * = dereference */
*bar = 43;              /* foo is now 43 */

int *baz = NULL;
if (baz != NULL) *baz;  /* always check before dereferencing */

/* pointer arithmetic moves by sizeof(pointee) */
uint8_t  qux[10];  qux + 1;    /* +1 byte  */
uint32_t quux[10]; quux + 1;   /* +4 bytes */

foo[i];                 /* is exactly *(foo + i) */

/* pointer to pointer — for functions that allocate for you */
void alloc_foo(char **out) { *out = malloc(10); }
char *bar2;
alloc_foo(&bar2);
```

**Gotcha:** returning a pointer to a local variable is a use-after-return bug:
```c
const char *bad(void) {
    char foo[32] = "hi";
    return foo;          /* foo dies when the function returns. Garbage. */
}
```
ASan catches this as `stack-use-after-return`. Return `malloc`'d memory (and document
ownership), or take an output buffer as a parameter.

## Structs

```c
struct Foo {
    int   bar;
    char  baz[32];
};

struct Foo foo1 = {.bar = 1, .baz = "hi"};   /* designated initializers — use these */
struct Foo foo2 = {0};                        /* all zero */

foo1.bar;                                     /* dot on a value */

struct Foo *ptr = &foo1;
ptr->bar;                                     /* arrow on a pointer */
(*ptr).bar;                                   /* the same thing, uglier */

/* typedef so you can drop the `struct` keyword */
typedef struct { int bar; } Baz;
Baz qux = {.bar = 1};
```

**Gotcha:** `.` on a value, `->` on a pointer. Mixing them is a compile error, which is
merciful.

**Gotcha:** `sizeof(struct Foo)` is not the sum of its members — see padding in Part 6.
This is why you never write a struct straight to disk.

Struct assignment copies the whole thing:
```c
Baz foo3 = qux;         /* a full copy, including arrays inside */
```

## Unions and tagged unions — how you build the AST

A union holds *one* of its members at a time, in overlapping memory:

```c
union Foo {
    int   bar;
    float baz;
};
sizeof(union Foo);      /* size of the LARGEST member, not the sum */
```

A union alone doesn't tell you which member is live, so you pair it with a tag. This is
the **tagged union**, and it's what replaces sealed interfaces from other languages:

```c
typedef enum { EXPR_LITERAL, EXPR_COLUMN, EXPR_BINARY } ExprType;

typedef struct Expr Expr;
struct Expr {
    ExprType type;                       /* the TAG — says which member is valid */
    union {
        struct { int value; }                          literal;
        struct { const char *name; }                   column;
        struct { Expr *left; int op; Expr *right; }    binary;
    } as;                                /* the payload */
};
```

Using it:
```c
switch (foo->type) {
    case EXPR_LITERAL: return foo->as.literal.value;
    case EXPR_COLUMN:  return lookup(foo->as.column.name);
    case EXPR_BINARY:  return apply(foo->as.binary.op,
                                    eval(foo->as.binary.left),
                                    eval(foo->as.binary.right));
}
```

**Gotcha:** reading a union member you didn't write is undefined behavior (with a narrow
exception for common initial sequences). The tag exists to stop you. Always switch on it.

**Tip:** omit `default:` from a switch over an enum. With `-Wall`, the compiler then
warns when you add a new enum value and forget to handle it — the closest C gets to
exhaustiveness checking, and genuinely valuable for an AST. Adding `default:` throws
that away.

## Enums

```c
typedef enum { FOO_A, FOO_B, FOO_C } Foo;      /* 0, 1, 2 */
typedef enum { BAR_X = 1, BAR_Y = 5 } Bar;     /* explicit values */

Foo foo = FOO_A;
switch (foo) { case FOO_A: break; case FOO_B: break; case FOO_C: break; }
```

**Gotcha:** C enums are just ints. There's no namespacing and no type safety — you can
assign 47 to a `Foo`. Prefix the members (`FOO_A`, not `A`) to avoid collisions, since
they all live in one global namespace.

**Gotcha:** enums can't be printed by name. Write a `const char *foo_str(Foo)` function
early; you'll want it in every error message and test failure.

## `typedef`

```c
typedef unsigned char  byte;
typedef struct Foo     Foo;              /* so you can write Foo instead of struct Foo */
typedef int (*BarFn)(int, int);          /* function pointer type */
```

Forward declaration for a self-referential struct:
```c
typedef struct Node Node;                /* declare the name first */
struct Node {
    int   foo;
    Node *next;                          /* now this works */
};
```

## Dynamic memory

```c
#include <stdlib.h>

int *foo = malloc(10 * sizeof *foo);     /* sizeof *foo, not sizeof(int) — survives
                                            a type change on the declaration */
if (foo == NULL) { /* handle */ }

int *bar = calloc(10, sizeof *bar);      /* zero-filled */

int *tmp = realloc(foo, 20 * sizeof *foo);
if (tmp == NULL) { /* foo is STILL VALID — don't lose it */ }
foo = tmp;

free(foo);
foo = NULL;                              /* prevents accidental reuse */
```

**The four classic bugs, all caught by ASan:**
```c
free(foo); free(foo);       /* double free */
free(foo); *foo = 1;        /* use after free */
foo = malloc(10);           /* leak — never freed */
free(foo + 1);              /* freeing a non-start pointer */
```

**Gotcha:** `malloc` does *not* zero the memory. Reading it before writing is
undefined behavior with values that change between runs. Use `calloc` when you want
zeros — and for pages, you always do.

## The `goto cleanup` pattern

The one legitimate `goto` in C, and it's genuinely the idiomatic solution to multi-step
failure cleanup:

```c
DbResult foo(void) {
    DbResult rc = DB_OK;
    char *bar = NULL;
    FILE *baz = NULL;

    bar = malloc(100);
    if (bar == NULL) { rc = DB_OUT_OF_MEMORY; goto cleanup; }

    baz = fopen("x", "rb");
    if (baz == NULL) { rc = DB_IO_ERROR; goto cleanup; }

    /* ... real work; any failure can `goto cleanup` ... */

cleanup:
    if (baz) fclose(baz);
    free(bar);                  /* free(NULL) is safe and does nothing */
    return rc;
}
```

Every exit path runs the same cleanup, once, in one place. Without this you either
duplicate cleanup at every `return` or leak. `free(NULL)` being a safe no-op is what
makes the pattern clean.

## The preprocessor

```c
#include <stdio.h>          /* system header: searches system paths */
#include "foo.h"            /* your header: searches relative first */

#define FOO 42
#define BAR(x) ((x) * 2)    /* PARENTHESIZE EVERYTHING */

#ifdef FOO
#endif

#ifndef MINIDB_FOO_H        /* include guard */
#define MINIDB_FOO_H
#endif

#if defined(__linux__)
#endif
```

**Gotcha — always parenthesize macro parameters and the whole body:**
```c
#define BAD(x)  x * 2
BAD(1 + 1)              /* expands to 1 + 1 * 2 = 3, not 4 */

#define GOOD(x) ((x) * 2)
GOOD(1 + 1)             /* ((1+1) * 2) = 4 */
```

**Gotcha — macro arguments evaluate more than once:**
```c
#define MAX(a, b) ((a) > (b) ? (a) : (b))
MAX(foo++, bar)         /* foo++ happens TWICE */
```

**Prefer `static inline` functions and `enum` constants to macros** wherever you can.
Functions type-check and evaluate arguments once; enums show up in the debugger.

The `do { } while (0)` wrapper in the test harness exists so a multi-statement macro
behaves like one statement inside an `if` without braces. It's the standard idiom.

## `printf` format specifiers

```c
printf("%d\n",  foo);       /* int */
printf("%u\n",  foo);       /* unsigned int */
printf("%ld\n", foo);       /* long */
printf("%zu\n", foo);       /* size_t  <- NOT %d */
printf("%f\n",  foo);       /* double */
printf("%.2f\n",foo);       /* 2 decimal places */
printf("%s\n",  foo);       /* char* — must be NUL-terminated */
printf("%.*s\n", (int)len, ptr);   /* first `len` bytes; no NUL needed */
printf("%c\n",  foo);       /* char */
printf("%p\n",  (void*)foo);/* pointer */
printf("%02x ", foo);       /* hex, zero-padded to 2 — for hex dumps */
printf("%%\n");             /* a literal % */

#include <inttypes.h>
printf("%" PRIu32 "\n", foo);      /* uint32_t, portably */
```

**Gotcha:** a wrong specifier is undefined behavior, not a cast. `printf("%d", my_size_t)`
can print garbage or crash. `-Wall` catches these — another reason warnings are errors.

**Debugging tip:** `printf` to `stdout` is buffered and may be lost when you crash. Use
`fprintf(stderr, ...)` for debug output — `stderr` is unbuffered, so it always appears.

## `sizeof`

```c
sizeof(int);              /* 4 on typical platforms */
sizeof foo;               /* no parens needed for an expression */
sizeof *foo;              /* size of what foo points AT */
sizeof foo / sizeof foo[0];   /* array length — only where the array hasn't decayed */
```

`sizeof` yields `size_t` (unsigned), so print it with `%zu`, and be careful subtracting
from it — see the unsigned trap above.

## Common undefined behavior — the list worth knowing

UB means the compiler may do anything, including working fine today and breaking after
you add an unrelated line. UBSan catches most of these at runtime.

- Reading uninitialized memory
- Dereferencing `NULL` or a freed pointer
- Reading or writing past the end of an array
- Signed integer overflow (`INT_MAX + 1`)
- Shifting by ≥ the type's width, or shifting a negative value
- Dereferencing a misaligned pointer (your `page + 6` cast!)
- Modifying a string literal
- Returning a pointer to a local
- `free`ing something twice, or something not from `malloc`
- Reading a union member you didn't write

**You do not avoid these by being careful.** You avoid them by turning on sanitizers and
running tests constantly. That's the whole strategy.

## Reading a compile error

| Message | What it means |
|---|---|
| `implicit declaration of function 'foo'` | Missing `#include`, or missing `_POSIX_C_SOURCE` |
| `undefined reference to 'foo'` (at link) | Declared but never defined, or the `.c` isn't in the build |
| `redefinition of 'struct Foo'` | Missing include guard, or defined in a header included twice |
| `expected ';' before ...` | Missing semicolon, often on the *previous* line — look up |
| `dereferencing pointer to incomplete type` | Only a forward declaration is visible; include the real header |
| `assignment discards 'const' qualifier` | Assigning a `const T*` to a `T*`; either add const or copy |
| `comparison of integer expressions of different signedness` | Mixing `int` and `size_t` — cast deliberately |
| `control reaches end of non-void function` | A path with no `return` |
| `unused parameter 'foo'` | With `-Wextra`; silence deliberately with `(void)foo;` |
| `missing separator` (Makefile) | Spaces instead of a tab in a recipe line |

**Always fix the first error first.** One error cascades into twenty that vanish when
the real one is fixed.


---

# Part 7 — The chapters

Every chapter has the same shape:

- **Goal** — what works when you're done.
- **Why** — the design decision being taught, and the alternatives you're rejecting.
- **Data structure hints** — what to reach for and what trap is waiting.
- **Skeletons** — signatures and TODOs. No implementations.
- **Test list** — write these one at a time, red then green.
- **Refactor to look for** — the specific smell this chapter produces.
- **Docs for this chapter** — what to read.
- **Done when** — the checklist.

The test lists are the real content. Treat each line as a task; write the test first.

---

## Chapter 1 — The REPL

### Goal

A read-eval-print loop. Prints `minidb> `, reads a line, and either handles a
**meta-command** (starts with `.`, like `.exit` and `.help`) or reports that it can't
parse SQL yet. Exits cleanly on `.exit` and on end-of-input.

### Why this first, and the decision it forces

It's the smallest runnable thing, and it forces the first real design decision
immediately.

**The decision: `repl_run` takes its input and output as `FILE*` parameters.** If it
uses `stdin` and `stdout` directly, you cannot test it — you'd have to run the program,
type into it, and read the screen with your eyes. That's not a test, it's a chore, and
you won't do it consistently.

Instead, production passes `stdin`/`stdout`; tests pass `fmemopen` and `open_memstream`
streams. Same technique the pager will use in Chapter 5, when it takes a path so tests
can hand it a temp file.

The rule generalizes: **push I/O to the edges, keep the middle pure.** You'll meet this
idea four more times.

### Data structure hints

- **`getline` is the right way to read a line.** It allocates and grows the buffer for
  you, so there's no fixed line-length limit:
  ```c
  char  *line = NULL;      /* getline allocates this */
  size_t cap  = 0;         /* getline manages this */
  ssize_t len = getline(&line, &cap, in);
  if (len == -1) { /* EOF or error */ }
  ...
  free(line);              /* YOU free it, once, at the end */
  ```
  **Reuse the same buffer across iterations** — pass the same `&line` and `&cap` each
  time and `getline` grows it only when needed. Freeing and re-allocating per line is
  waste. Free once when the loop ends.
- **`getline` keeps the trailing newline.** You must strip it, or `.exit\n` won't match
  `.exit`. This is the first bug you'll hit.
- **`fflush(out)` after printing the prompt.** Output is buffered, so the prompt sits in
  the buffer while you block on input, making the REPL look frozen. This is the single
  most common Chapter 1 bug.
- **Return an enum, not a bool**, from the line handler. A bool means... continue?
  success? handled? By Chapter 8 you'll want a third outcome and a bool can't express
  it.
- **Ownership:** `repl_run` does not own `in` or `out`. It must not `fclose` them. Write
  that in a comment above the function — it's the kind of thing that's obvious now and
  isn't in six weeks.

### Skeletons

`src/repl.h`:
```c
#ifndef MINIDB_REPL_H
#define MINIDB_REPL_H

#include <stdio.h>

/*
 * Reads lines from `in`, writes results to `out`, until .exit or EOF.
 * Does NOT take ownership of either stream — the caller closes them.
 */
void repl_run(FILE *in, FILE *out);

#endif
```

`src/repl.c`:
```c
#define _POSIX_C_SOURCE 200809L
#include "repl.h"

#include <stdlib.h>
#include <string.h>

#define PROMPT "minidb> "

typedef enum { OUTCOME_CONTINUE, OUTCOME_EXIT } Outcome;

/* Handles one non-blank, trimmed line. */
static Outcome handle_line(const char *line, FILE *out) {
    /* TODO: leading '.' -> meta-command
     *       otherwise    -> we can't parse SQL yet; say so
     */
    (void)line; (void)out;
    return OUTCOME_CONTINUE;
}

/* Removes a trailing '\n' (and '\r' if present) in place. */
static void strip_newline(char *line) {
    /* TODO */
    (void)line;
}

/* Returns a pointer to the first non-space char; trims trailing space in place. */
static char *trim(char *line) {
    /* TODO */
    return line;
}

void repl_run(FILE *in, FILE *out) {
    /* TODO:
     *   char *line = NULL; size_t cap = 0;
     *   loop:
     *     fputs(PROMPT, out); fflush(out);
     *     if (getline(&line, &cap, in) == -1) break;     <- EOF
     *     strip_newline(line); trimmed = trim(line);
     *     if (*trimmed == '\0') continue;                 <- blank
     *     if (handle_line(trimmed, out) == OUTCOME_EXIT) break;
     *   free(line);                                       <- exactly once
     */
    (void)in; (void)out;
}
```

`src/main.c`:
```c
#include "repl.h"
#include <stdio.h>

int main(void) {
    repl_run(stdin, stdout);
    return 0;
}
```

Notice how thin `main` is. That's the goal — there's nothing in it worth testing.

### Test list

`tests/test_repl.c`. Start with this harness — it's the pattern every test uses:

```c
#define _POSIX_C_SOURCE 200809L
#include "test.h"
#include "repl.h"

#include <stdlib.h>
#include <string.h>

/*
 * Runs the repl over `input` and returns everything it printed.
 * CALLER OWNS the returned string — free() it.
 */
static char *run_with(const char *input) {
    FILE *in = fmemopen((void *)input, strlen(input), "r");

    char  *out_buf = NULL;
    size_t out_len = 0;
    FILE  *out = open_memstream(&out_buf, &out_len);

    repl_run(in, out);

    fclose(out);            /* MUST close before out_buf is valid */
    fclose(in);
    return out_buf;         /* caller frees */
}

static void prints_prompt_before_reading(void) {
    char *foo = run_with(".exit\n");
    ASSERT_STR_CONTAINS(foo, "minidb> ");
    free(foo);
}

static void exits_on_exit_command(void) {
    /* if .exit didn't stop the loop, the second line would also be processed */
    char *foo = run_with(".exit\nnonsense\n");
    ASSERT(strstr(foo, "nonsense") == NULL);
    free(foo);
}

static void exits_cleanly_at_end_of_input(void) {
    /* no .exit at all — getline returns -1. Must not hang or crash. */
    char *foo = run_with("");
    ASSERT_EQ_STR("minidb> ", foo);
    free(foo);
}

void suite_repl(void) {
    SUITE("repl");
    RUN_TEST(prints_prompt_before_reading);
    RUN_TEST(exits_on_exit_command);
    RUN_TEST(exits_cleanly_at_end_of_input);
}
```

Then, on your own:

- [ ] An unknown meta-command reports `Unrecognized command '.bogus'` and keeps going.
- [ ] After an unknown command, a second prompt is printed (count occurrences).
- [ ] A SQL-looking line reports `Unrecognized keyword` and keeps going.
- [ ] Blank lines and whitespace-only lines are ignored, no error printed.
- [ ] `"  .exit  "` with surrounding whitespace still exits.
- [ ] `.help` lists the available commands.
- [ ] A line with no trailing newline (last line of a file) is still handled.
- [ ] **A very long line (5000 chars) doesn't crash** — proves you're using `getline`
      and not a fixed `char buf[256]`.

That last one matters. It's the test that catches the buffer overflow you'd have written
with `fgets` and a fixed buffer.

### Refactor to look for

Write `handle_line` naively first: a chain of `strcmp`s. Then notice that when you add
the third meta-command, the function does two unrelated jobs — deciding *whether* a line
is a meta-command, and deciding *which one*. That's the smell. Extract:

```c
typedef enum { META_EXIT, META_HELP, META_UNKNOWN } MetaCommand;

/* @param line a trimmed line known to start with '.' */
static MetaCommand meta_parse(const char *line);
```

Then `handle_line` is a small switch. **Do not write this version first.** Write the ugly
version, feel the friction, then refactor. Refactoring toward a shape you can already
see teaches you nothing; refactoring because the code told you to is the skill.

### Docs for this chapter

- `getline` — read the whole man page, especially the ownership paragraph:
  `man 3 getline` or <https://www.man7.org/linux/man-pages/man3/getline.3.html>
- `fmemopen`: <https://www.man7.org/linux/man-pages/man3/fmemopen.3.html>
- `open_memstream`: <https://www.man7.org/linux/man-pages/man3/open_memstream.3.html>
- Beej's Guide, the `stdio` chapter: <https://beej.us/guide/bgc/html/split/file-inputoutput.html>

### Done when

- [ ] All tests pass under both gcc and clang in CI, with no leaks reported.
- [ ] `make run` gives a working prompt you can type into.
- [ ] `repl.c` contains no reference to `stdin` or `stdout`.
- [ ] `main.c` is under 10 lines.
- [ ] Every `getline` buffer is freed exactly once. (ASan verifies this for you.)

---

## Chapter 2 — The tokenizer

### Goal

Turn `"SELECT * FROM users WHERE id = 3;"` into a list of tokens:

```
[SELECT] [STAR] [FROM] [IDENT "users"] [WHERE] [IDENT "id"] [EQ] [NUMBER 3] [SEMI] [EOF]
```

### Why a separate tokenizing pass

You could parse straight from the character stream. Nobody does, and the reason is worth
internalizing: **separating lexing from parsing means the parser never thinks about
whitespace, comments, or how many characters a keyword has.** The parser gets to reason
about `SELECT` as one indivisible thing. That split makes the parser roughly three times
smaller.

Same principle as the layering in Part 0, at a smaller scale.

### Data structure hints

- **The key C design decision: tokens point into the source, they don't copy it.**
  ```c
  typedef struct {
      TokenType   type;
      const char *start;    /* points INTO the source; NOT owned, NOT NUL-terminated */
      size_t      length;
      size_t      position; /* index in the source, for error messages */
  } Token;
  ```
  Zero allocations, zero frees, no leaks possible. The cost is a **lifetime dependency**:
  tokens are only valid while the source string is alive. Write that in a comment above
  the struct.

  The alternative is `strdup`ing each lexeme, which is simpler to reason about and costs
  an allocation and a free per token. **Take the string-view version** — it's what real
  tokenizers do and the constraint is easy to respect here, since the parser runs
  immediately after. But make the choice deliberately and document it.
- **Comparing a token to a keyword** needs length-aware comparison, since there's no
  NUL:
  ```c
  static bool token_is(const Token *foo, const char *word) {
      size_t n = strlen(word);
      return foo->length == n && strncasecmp(foo->start, word, n) == 0;
  }
  ```
  `strncasecmp` is POSIX and gives you case-insensitive keywords for free.
- **Keyword lookup:** a static table, scanned after you've consumed a full identifier.
  ```c
  static const struct { const char *word; TokenType type; } KEYWORDS[] = {
      {"select", TOKEN_SELECT}, {"from", TOKEN_FROM}, /* ... */
  };
  ```
  Scan the full identifier *first*, then look it up. Never match keywords
  character-by-character in the main loop — that's how `selection` becomes `SELECT` +
  `ion`.
- **The token list needs to grow.** A dynamic array: `Token *tokens; size_t count, cap;`
  with `realloc` doubling when full. Write that once, carefully, and test it — growing
  arrays is a classic source of off-by-one memory bugs. (Remember the realloc trap: use
  a temp variable.)
- **Cursor state:** `const char *source; size_t current;` plus tiny helpers `peek()`,
  `advance()`, `is_at_end()`, `match(char)`. Write those four first; the rest is short
  once they exist.
- **`peek()` past the end returns `'\0'`.** The source is NUL-terminated, so this is
  free — and it means `peek()` never needs a bounds check.
- **Always emit an EOF token.** The parser can then always `peek()` without a bounds
  check, removing an entire class of bug from Chapter 3.

### Skeletons

`src/tokenizer.h`:
```c
#ifndef MINIDB_TOKENIZER_H
#define MINIDB_TOKENIZER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TOKEN_SELECT, TOKEN_FROM, TOKEN_WHERE, TOKEN_INSERT, TOKEN_INTO,
    TOKEN_VALUES, TOKEN_CREATE, TOKEN_TABLE, TOKEN_DELETE, TOKEN_UPDATE, TOKEN_SET,
    TOKEN_STAR, TOKEN_COMMA, TOKEN_SEMICOLON, TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_EQ, TOKEN_NEQ, TOKEN_LT, TOKEN_LTE, TOKEN_GT, TOKEN_GTE,
    TOKEN_NUMBER, TOKEN_STRING, TOKEN_IDENT,
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType   type;
    const char *start;     /* points into the source. NOT owned. NOT NUL-terminated. */
    size_t      length;
    size_t      position;
} Token;

typedef struct {
    Token *tokens;
    size_t count;
    size_t capacity;
    /* on failure: */
    bool        had_error;
    const char *error_msg;
    size_t      error_pos;
} TokenList;

/*
 * Tokenizes `source`. The returned list points INTO `source`,
 * which must outlive the list.
 * CALLER OWNS the result — call token_list_free().
 */
TokenList *tokenize(const char *source);
void token_list_free(TokenList *foo);

const char *token_type_name(TokenType type);   /* for error messages and tests */

#endif
```

`src/tokenizer.c` (structure only):
```c
typedef struct {
    const char *source;
    size_t      current;
    TokenList  *out;
} Lexer;

/* --- cursor helpers: WRITE THESE FIRST --- */
static bool is_at_end(const Lexer *foo);
static char peek(const Lexer *foo);            /* '\0' at end */
static char peek_next(const Lexer *foo);
static char advance(Lexer *foo);
static bool match(Lexer *foo, char expected);  /* consume if it matches */

/* --- one-token scanners --- */
static void scan_number(Lexer *foo);
static void scan_string(Lexer *foo);           /* 'quoted' */
static void scan_identifier(Lexer *foo);       /* then keyword lookup */

/* --- list management --- */
static bool list_push(TokenList *foo, Token t);   /* realloc-doubling; false on OOM */
```

### Test list

- [ ] Empty input produces exactly one token: `TOKEN_EOF`.
- [ ] Whitespace-only input produces one `TOKEN_EOF`.
- [ ] `"SELECT"` produces `[SELECT, EOF]`.
- [ ] Keywords are case-insensitive: `select`, `SELECT`, `SeLeCt` all give `TOKEN_SELECT`.
- [ ] `"users"` produces `TOKEN_IDENT` with `length == 5`.
- [ ] **`"selection"` produces one `TOKEN_IDENT`**, not `SELECT` followed by `ion`.
- [ ] `"42"` produces `TOKEN_NUMBER` with the right `start`/`length`.
- [ ] `"'hello'"` produces `TOKEN_STRING` whose `start`/`length` **exclude the quotes**.
- [ ] An unterminated string sets `had_error` with the opening position.
- [ ] `"<="` is one `TOKEN_LTE`, not `LT` then `EQ`.
- [ ] `"<"` followed by a space is a single `TOKEN_LT`.
- [ ] `position` is right: in `"SELECT * FROM"`, `FROM` has position 9.
- [ ] A full statement tokenizes to the expected sequence.
- [ ] `"SELECT@"` reports an error naming the offending character.
- [ ] Identifiers may contain digits and underscores after the first char (`user_id2`),
      but may not start with a digit.
- [ ] **`token_list_free(NULL)` is safe** — a no-op, not a crash. (Mirrors `free`.)
- [ ] **A 10,000-token input works** — proves your `realloc` growth is correct. This is
      the test that catches dynamic-array bugs, and ASan will catch the overflow if the
      growth is wrong.

### Refactor to look for

Two things:

1. **Duplicated "consume while a predicate holds"** in `scan_number` and
   `scan_identifier`. Extract:
   ```c
   static void consume_while(Lexer *foo, int (*pred)(int));
   ```
   Then `scan_number` is `consume_while(lex, isdigit)` and identifiers use a small
   custom predicate. That's a function pointer doing real work, and a good first taste of
   Chapter 12's vtables.
2. **The `list_push` growth logic.** Write it once, in one function, with the temp-var
   realloc pattern. If growth logic appears in two places, one of them is wrong.

### Docs for this chapter

- **Crafting Interpreters, Chapter 16 "Scanning on Demand"** — the same design, in C,
  with the string-view technique: <https://craftinginterpreters.com/scanning-on-demand.html>
- `<ctype.h>` (`isdigit`, `isalpha`, `isalnum`, `isspace`):
  <https://en.cppreference.com/w/c/string/byte#Character_classification>
  (**Gotcha:** these take an `int` that must be representable as `unsigned char` or
  `EOF`. Passing a negative `char` is UB. Cast: `isdigit((unsigned char)foo)`.)
- `strncasecmp`: <https://www.man7.org/linux/man-pages/man3/strcasecmp.3.html>

### Done when

- [ ] Every statement form you plan to support tokenizes correctly.
- [ ] Every error case sets `had_error` with a usable position.
- [ ] `tokenize` has no knowledge of SQL *grammar* — only of SQL *spelling*. It should
      happily tokenize `FROM SELECT , , ;` without complaint. That's the parser's job.
- [ ] No leaks: every `tokenize` in a test has a matching `token_list_free`.

---

## Chapter 3 — The parser and AST

### Goal

Turn a token list into an **abstract syntax tree**. Support
`SELECT ... FROM ... [WHERE ...]` and `INSERT INTO ... VALUES (...)`.

### Why recursive descent, and why an AST

**Why an AST rather than executing as you parse?** Because a tree is a value you can
inspect, test, transform, and optimize. Chapter 12's planner will rewrite it. Every test
in this chapter checks the shape of the tree, which is only possible because it's plain
data.

**Why recursive descent?** Because the code mirrors the grammar, one function per rule.
Write the grammar down first:

```
statement  -> selectStmt | insertStmt
selectStmt -> "SELECT" columnList "FROM" IDENT ( "WHERE" expression )? ";"?
insertStmt -> "INSERT" "INTO" IDENT "VALUES" "(" literalList ")" ";"?
columnList -> "*" | IDENT ( "," IDENT )*
expression -> comparison
comparison -> primary ( ( "=" | "!=" | "<" | "<=" | ">" | ">=" ) primary )?
primary    -> NUMBER | STRING | IDENT
```

Then write one function per rule with the same name. If the grammar is right, the parser
nearly writes itself — and when it doesn't, the grammar was wrong, which is a much more
useful thing to discover.

### Data structure hints

- **The AST is a tagged union.** This is the C replacement for a sealed type hierarchy.
  See Part 6B for the pattern.
- **Allocate every node from an arena.** This is the chapter's most important C
  decision. A tree of small nodes freed individually means a recursive `free` walk,
  which is fiddly, easy to get wrong (free the children before the parent!), and pure
  overhead. With an arena, parsing allocates from one block and `arena_free` releases
  everything at once. **Leaks become impossible by construction.** This is what real
  compilers do.
- **`parse` takes the arena as a parameter**, so the caller controls the lifetime:
  ```c
  Statement *parse(const TokenList *tokens, Arena *arena);
  ```
  Ownership is then trivially clear: the caller owns the arena, the AST lives exactly as
  long as it, and no one ever calls `free` on a node.
- **The parser is a cursor over the token list**, exactly like the tokenizer was a cursor
  over characters. Same helpers, one level up: `peek()`, `advance()`,
  `check(TokenType)`, `match(TokenType)`, and the important one:
  ```c
  static bool expect(Parser *foo, TokenType type, const char *message);
  ```
  Nearly every line of your parser calls `expect`. Writing it first makes everything
  after it two lines long.
- **`match` vs `expect`:** `match` consumes *if* the type is there and returns a bool
  (for optional parts like `WHERE`); `expect` consumes or records an error (for required
  parts). Getting these confused means malformed input silently parses into a wrong tree
  instead of erroring — much worse than a crash, because it fails later somewhere
  unrelated.
- **Error handling without exceptions:** set a `had_error` flag on the parser and return
  `NULL` from the failing rule. Every caller checks for `NULL` and propagates. Verbose,
  but explicit. (A `setjmp`/`longjmp` panic mode is the alternative real parsers use —
  interesting to read about, not worth it here.)
- **The EOF token from Chapter 2 pays off here** — `peek()` never needs a bounds check.

### Skeletons

`src/ast.h`:
```c
#ifndef MINIDB_AST_H
#define MINIDB_AST_H

#include <stdint.h>
#include <stddef.h>

typedef enum { EXPR_LITERAL_INT, EXPR_LITERAL_STR, EXPR_COLUMN, EXPR_BINARY } ExprType;
typedef enum { OP_EQ, OP_NEQ, OP_LT, OP_LTE, OP_GT, OP_GTE } BinaryOp;

typedef struct Expr Expr;
struct Expr {
    ExprType type;
    union {
        struct { int64_t value; }                            literal_int;
        struct { const char *start; size_t length; }         literal_str;
        struct { const char *start; size_t length; }         column;
        struct { Expr *left; BinaryOp op; Expr *right; }     binary;
    } as;
};

typedef enum { STMT_SELECT, STMT_INSERT } StatementType;

typedef struct {
    StatementType type;
    union {
        struct {
            const char *table; size_t table_len;
            /* column_count == 0 means SELECT * */
            const char **columns; size_t *column_lens; size_t column_count;
            Expr *where;                     /* NULL if no WHERE clause */
        } select;
        struct {
            const char *table; size_t table_len;
            Expr **values; size_t value_count;
        } insert;
    } as;
} Statement;

#endif
```

Note that every string in the AST is still a `(pointer, length)` view into the original
SQL source. The lifetime chain is: **source string outlives the token list outlives the
AST arena.** Write that down in a comment at the top of `ast.h`. Getting a lifetime
chain straight and stated is exactly the kind of thinking C forces and other languages
let you skip.

`src/parser.h`:
```c
#ifndef MINIDB_PARSER_H
#define MINIDB_PARSER_H

#include "ast.h"
#include "tokenizer.h"
#include "util/arena.h"

typedef struct {
    bool        had_error;
    const char *error_msg;
    size_t      error_pos;
} ParseError;

/*
 * Parses `tokens` into an AST allocated from `arena`.
 * Returns NULL on error, with details in `out_err`.
 * The AST borrows string data from the original source: it must outlive both.
 */
Statement *parse(const TokenList *tokens, Arena *arena, ParseError *out_err);

#endif
```

`src/parser.c` (structure only):
```c
typedef struct {
    const TokenList *tokens;
    size_t           current;
    Arena           *arena;
    ParseError      *err;
} Parser;

/* --- one function per grammar rule --- */
static Statement *select_statement(Parser *foo);
static Statement *insert_statement(Parser *foo);
static bool       column_list(Parser *foo, /* out params */ ...);
static Expr      *expression(Parser *foo);
static Expr      *comparison(Parser *foo);
static Expr      *primary(Parser *foo);

/* --- cursor helpers: WRITE THESE FIRST --- */
static const Token *peek(const Parser *foo);
static const Token *advance(Parser *foo);
static bool         check(const Parser *foo, TokenType type);
static bool         match(Parser *foo, TokenType type);
static bool         expect(Parser *foo, TokenType type, const char *message);

/* --- node constructors, all arena-allocated --- */
static Expr *expr_binary(Parser *foo, Expr *left, BinaryOp op, Expr *right);
static Expr *expr_column(Parser *foo, const Token *t);
```

### Test list

- [ ] `SELECT * FROM users;` gives `STMT_SELECT`, table `users`, `column_count == 0`,
      `where == NULL`.
- [ ] `SELECT id, name FROM users;` gives two columns, in order.
- [ ] The trailing semicolon is optional.
- [ ] `SELECT * FROM users WHERE id = 3;` produces `EXPR_BINARY` with `OP_EQ`, an
      `EXPR_COLUMN` left, and an `EXPR_LITERAL_INT` right.
- [ ] Each comparison operator maps to the right `BinaryOp`.
- [ ] String literals in `WHERE` become `EXPR_LITERAL_STR`, not `EXPR_COLUMN`.
- [ ] `INSERT INTO users VALUES (1, 'bob', 'b@x.com');` gives three values, in order,
      with the right types.
- [ ] `SELECT FROM users;` errors, mentioning the column list.
- [ ] `SELECT * users;` errors, mentioning `FROM`.
- [ ] `SELECT * FROM;` errors, mentioning a table name.
- [ ] `INSERT INTO users VALUES (1, 'bob';` errors — unclosed paren.
- [ ] An error reports the position of the **offending token**, not the end of input.
- [ ] **Parsing a statement, then `arena_free`, leaks nothing.** ASan's leak check
      verifies this automatically on every run.
- [ ] **A parse error still leaks nothing** — the arena is freed regardless. This is the
      test that proves the arena design is earning its keep.

### Refactor to look for

1. **Duplicated `expect` calls with hand-written messages.** After the fifth
   `expect(foo, TOKEN_FROM, "expected FROM")`, the message is redundant with the type.
   Derive a default from `token_type_name()`, keeping the explicit string as an override
   for cases needing context.
2. **`select_statement` growing past a screen.** Extract `where_clause()` returning
   `Expr*` or `NULL`. One function per grammar rule — when a function handles two rules,
   split it.
3. **`if (foo == NULL) return NULL;` after every sub-call.** Verbose but correct; this is
   the price of no exceptions. Don't try to be clever with macros here — explicit
   propagation is readable and debuggable, and a macro that hides a `return` is a real
   readability cost.

Also: if you find yourself wanting to know "does this table exist?" inside the parser,
stop. That's catalog knowledge and it belongs in Chapter 8's planner. The parser's job is
*shape*, not *meaning*. This line is genuinely tempting to cross and keeping it clean is
one of the main things the project teaches.

### Docs for this chapter

- **Crafting Interpreters, Chapter 17 "Compiling Expressions"** — recursive descent in C:
  <https://craftinginterpreters.com/compiling-expressions.html>
- **Crafting Interpreters, Chapter 5 "Representing Code"** — why an AST and how to shape
  it: <https://craftinginterpreters.com/representing-code.html>
- Arena allocation, a good short write-up:
  <https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator>
- SQLite's SQL grammar diagrams, if you want to see how deep it goes:
  <https://www.sqlite.org/lang.html>

### Done when

- [ ] Every supported statement parses to the correct tree.
- [ ] Every malformed statement reports an error with an accurate position.
- [ ] `parser.c` includes nothing from the storage layer. Check the `#include` list
      literally — if `pager.h` has crept in, find out how and undo it.
- [ ] Zero leaks, on both the success and the error path.

---

## Chapter 4 — Row serialization

### Goal

Convert a `Row` struct to a fixed-size byte array and back, exactly. This is where the
project stops being a language exercise and becomes a database.

### Why fixed-width, and why manual serialization

**Why not just `fwrite(&row, sizeof row, 1, f)`?** Because of struct padding, and this is
the single most important thing in this chapter. Read the padding section in Part 6 if
you haven't. Three fatal problems: padding bytes are uninitialized garbage, padding
differs between compilers and architectures, and integer byte order differs between
architectures. A file format is a contract with the future — you must be able to say
precisely what byte 137 means, on every machine, forever.

**Why fixed-width rows to start?** Because then row *N* lives at offset *N × ROW_SIZE*
and finding it is arithmetic instead of a search. That's an enormous simplification while
you get the storage layer right. The cost is wasted space (a 3-character username still
burns 32 bytes) and a hard length limit. Chapter 8 revisits it.

**This is a deliberate, temporary simplification, and knowing which simplifications to
take first — and writing down why — is a core engineering skill.** Put that reasoning in
a comment.

The layout:

| Field      | Type          | Offset | Size |
|------------|---------------|--------|------|
| `id`       | `uint32_t` BE | 0      | 4    |
| `username` | UTF-8, padded | 4      | 32   |
| `email`    | UTF-8, padded | 36     | 255  |
|            |               |        | **291 total** |

### Data structure hints

- **Named constants for every offset and size**, each defined in terms of the previous
  one:
  ```c
  enum {
      ID_OFFSET       = 0,
      ID_SIZE         = 4,
      USERNAME_OFFSET = ID_OFFSET + ID_SIZE,
      USERNAME_SIZE   = 32,
      EMAIL_OFFSET    = USERNAME_OFFSET + USERNAME_SIZE,
      EMAIL_SIZE      = 255,
      ROW_SIZE        = EMAIL_OFFSET + EMAIL_SIZE
  };
  ```
  Use an `enum`, not `#define` — enum constants are typed, scoped, and visible in the
  debugger. Defining each offset from the previous one means inserting a field later is
  a one-line change. Every real storage engine does this.
- **`static_assert(ROW_SIZE == 291, "...")`** right below. Compile-time protection
  against an accidental layout change.
- **The in-memory struct is separate from the on-disk layout.** In memory, keep
  NUL-terminated strings with room for the terminator:
  ```c
  typedef struct {
      uint32_t id;
      char     username[USERNAME_SIZE + 1];   /* +1 for the NUL */
      char     email[EMAIL_SIZE + 1];
  } Row;
  ```
  The `+1` matters: on disk the field is exactly 32 bytes with no terminator, but in
  memory you want a normal C string. **Keeping these two representations distinct is the
  whole point of the chapter.**
- **Use `read_u32_be`/`write_u32_be`** from Part 6. No `memcpy` of a `uint32_t`, no
  pointer casting — byte at a time, portable, alignment-safe.
- **Padding on write:** `memset` the field region to zero, then `memcpy` the bytes in.
  `calloc`ed buffers are already zero, but don't rely on the caller's buffer being fresh.
- **Trimming on read is the part people get wrong.** Scan for the first zero byte to
  find the real length, then copy that many bytes and add your own NUL. Otherwise your
  string is 29 trailing NULs long and `strcmp` fails in a way that looks identical in the
  debugger. When this happens — and it will — hex-dump the bytes.
- **UTF-8 is multi-byte.** `"café"` is 4 characters but 5 bytes. Validate the **byte**
  length, not a character count. In C this is actually easier than in most languages,
  because `strlen` already gives you bytes.
- **Validate on construction**, in a `row_init` that returns `DbResult`, so an invalid
  `Row` can never exist. "Make illegal states unrepresentable" is one of the
  highest-leverage habits in this document.

### Skeletons

`src/row.h`:
```c
#ifndef MINIDB_ROW_H
#define MINIDB_ROW_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "result.h"

enum {
    ID_OFFSET       = 0,
    ID_SIZE         = 4,
    USERNAME_OFFSET = ID_OFFSET + ID_SIZE,
    USERNAME_SIZE   = 32,
    EMAIL_OFFSET    = USERNAME_OFFSET + USERNAME_SIZE,
    EMAIL_SIZE      = 255,
    ROW_SIZE        = EMAIL_OFFSET + EMAIL_SIZE
};

static_assert(ROW_SIZE == 291, "row layout changed — bump the file format version");

typedef struct {
    uint32_t id;
    char     username[USERNAME_SIZE + 1];   /* +1 for the NUL terminator */
    char     email[EMAIL_SIZE + 1];
} Row;

/* Validates and fills `out`. Returns DB_INVALID if a field is too long. */
DbResult row_init(Row *out, uint32_t id, const char *username, const char *email);

/* Writes exactly ROW_SIZE bytes into `out`. */
void row_serialize(const Row *foo, uint8_t *out);

/* Reads ROW_SIZE bytes from `in` into `out`. Always succeeds. */
void row_deserialize(const uint8_t *in, Row *out);

bool row_equals(const Row *foo, const Row *bar);   /* for tests */

#endif
```

`src/row.c` (helpers you'll want):
```c
/* Writes `src` into `out`, zero-padded to exactly `field_size` bytes. */
static void write_fixed_str(uint8_t *out, const char *src, size_t field_size) {
    /* TODO: memset to 0; memcpy min(strlen(src), field_size) bytes */
}

/* Reads a zero-padded string of at most `field_size` bytes; NUL-terminates `out`. */
static void read_fixed_str(const uint8_t *in, size_t field_size, char *out) {
    /* TODO: scan for the first 0 byte to find the real length (or field_size if none);
     *       memcpy that many; out[len] = '\0';
     */
}
```

### Test list

The first is the most valuable test in the entire project:

- [ ] **Round trip:** `row_deserialize(row_serialize(foo))` equals the original.
- [ ] Round-trips with empty strings.
- [ ] Round-trips with **maximum-length** strings (exactly 32 and exactly 255 bytes).
- [ ] Round-trips with `id == 0` and `id == UINT32_MAX`.
- [ ] Round-trips with non-ASCII (`"café"`, `"日本"`) — proves UTF-8 handling is real
      and not accidentally ASCII-only.
- [ ] A 33-byte username makes `row_init` return `DB_INVALID`.
- [ ] A 17-character two-byte-per-char username (34 bytes) **also** returns `DB_INVALID`
      — proves you're measuring bytes, not characters.
- [ ] Padding is zeros: for a 3-char username, bytes 7 through 35 are all `0`.
- [ ] `id` is stored **big-endian**: for `id == 1`, the first four bytes are
      `00 00 00 01`. Assert on the actual bytes, not just the round trip.
- [ ] **Deserializing an all-zero buffer gives `{0, "", ""}` and does not crash.**
- [ ] `sizeof(Row) != ROW_SIZE` — write this as a *comment*, not a test, explaining
      why. It's the padding lesson, permanently recorded where the next reader sees it.

That "all-zero buffer" test seems pointless until Chapter 5, when reading a fresh page
returns zeros and you need it to be harmless.

### Refactor to look for

`row_serialize` and `row_deserialize` both grow near-identical string-handling blocks,
one per field. That duplication is what `write_fixed_str` and `read_fixed_str` are for.
Write it duplicated first if you want to feel it, then extract.

Second, once those exist, notice that `(offset, size)` travel together everywhere.
That's primitive obsession, and the fix is a small struct:
```c
typedef struct { size_t offset, size; } FieldSpec;
```
Whether that's worth it at three fields is a genuine judgment call, and thinking it
through is worth more than either answer. Write down which way you went and why.

### Docs for this chapter

- **Struct padding and alignment** — the definitive explanation:
  <https://en.cppreference.com/w/c/language/object#Alignment>
- `memcpy` / `memset` / `memmove`:
  <https://en.cppreference.com/w/c/string/byte>
- `static_assert`: <https://en.cppreference.com/w/c/language/_Static_assert>
- SQLite's record format, to see the mature version — §2.1:
  <https://www.sqlite.org/fileformat2.html#record_format>
- cstack Part 3, "An In-Memory, Append-Only, Single-Table Database":
  <https://cstack.github.io/db_tutorial/parts/part3.html>

### Done when

- [ ] Round-trip tests pass for every boundary case above.
- [ ] Not a single bare numeric literal in `row.c` outside the enum block.
- [ ] You have used `hex_dump` at least once and understood what you saw.
- [ ] You can state, from memory, what byte 36 of a serialized row contains.
- [ ] You can explain to someone else why `fwrite(&row, sizeof row, 1, f)` is wrong.

---

## Chapter 5 — The pager

### Goal

Read and write **pages** — fixed 4096-byte blocks — from a file, with an in-memory cache.
After this chapter, data survives restarting the program. Insert rows, `.exit`, start
again, `SELECT` them back.

### Why pages, and why a cache

**Why pages?** Because disks and operating systems work in blocks. Reading 4 bytes from a
file costs the same as reading 4096, because the OS reads a whole block either way. So
the unit of I/O should be a block, and 4096 matches the typical OS page. Every real
database does this.

**Why a cache?** Because the same pages get touched over and over — a B+ tree root is
read on *every single query*. Keeping pages in memory turns thousands of disk reads into
thousands of array accesses. This is the **buffer pool**, the biggest performance
structure in any database.

**Why does the pager own the file and nothing else?** So everything above it can say
"give me page 3" without knowing whether that came from disk, memory, or a network. That
indirection is what later lets you add eviction, or write-ahead logging, or compression,
without touching the B+ tree at all. **This is the same "one job, clean interface" move
as the tokenizer/parser split and the `FILE*` injection in Chapter 1 — third time,
different scale.** That's the lesson, not a coincidence.

### Data structure hints

- **`PAGE_SIZE = 4096`**, a named constant.
- **A page is a `uint8_t *`** pointing at a `calloc`ed 4096-byte block. The pager owns
  every one of them and frees them in `pager_close`.
- **The cache: start with a fixed array**, not a hash map.
  ```c
  #define MAX_PAGES 1024
  typedef struct {
      FILE    *file;
      size_t   file_length;
      uint32_t page_count;
      uint8_t *pages[MAX_PAGES];      /* NULL = not cached */
      bool     dirty[MAX_PAGES];
  } Pager;
  ```
  Direct indexing by page number, no hashing, no collisions, trivially debuggable. It
  caps you at a 4MB database, which is fine for now. **Note the limit in a comment**;
  removing it (an LRU cache with eviction) is a natural stretch goal and a good one.
- **Lazy loading:** `pager_get_page(n)` returns the cached page; on a miss, `calloc`s and
  reads from disk; **past end-of-file, returns the zeroed page without reading.** A
  database file grows by writing new pages, so "past the end" is normal, not an error.
  (This is why Chapter 4's all-zero test mattered.)
- **`calloc`, never `malloc`.** A page past EOF must be zeros, and `malloc` gives you
  whatever was there before. Uninitialized reads are the nastiest C bugs because behavior
  changes between runs.
- **Dirty tracking:** a `bool` per page. `pager_flush` writes only dirty pages and clears
  the flags. Four lines, and it's a genuine performance structure.
- **Ownership is the crux of this chapter.** `pager_get_page` returns a **borrowed**
  pointer — the caller may read and write through it but must never `free` it. Write that
  in the header comment, in capitals if you like. Getting this wrong produces a double
  free, which ASan catches instantly but which is confusing if you haven't decided who
  owns what.
- **`fseek` cast:** `fseek(f, (long)page_num * PAGE_SIZE, SEEK_SET)`. Without the cast,
  `uint32_t * int` arithmetic can overflow. `fseek` takes a `long`, which is 32 bits on
  some platforms — for files over 2GB you'd need `fseeko` and `off_t`. Note that limit
  in a comment; you won't hit it, but knowing it exists is the point.
- **Check `fread`'s return.** A short read at EOF is normal — zero-fill the remainder.

### Skeletons

`src/pager.h`:
```c
#ifndef MINIDB_PAGER_H
#define MINIDB_PAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "result.h"

#define PAGE_SIZE 4096
#define MAX_PAGES 1024        /* caps the database at 4MB — see pager.c */

typedef struct Pager Pager;

/* Opens or creates the database file. CALLER OWNS the result: pager_close(). */
DbResult pager_open(const char *path, Pager **out);

/*
 * Returns a BORROWED pointer to the page. Valid until pager_close().
 * The caller must NOT free it.
 * Reading beyond the current end of file yields a fresh zero-filled page.
 */
DbResult pager_get_page(Pager *foo, uint32_t page_num, uint8_t **out);

void     pager_mark_dirty(Pager *foo, uint32_t page_num);
uint32_t pager_page_count(const Pager *foo);
uint32_t pager_allocate_page(Pager *foo);      /* next page number; no disk I/O */
DbResult pager_flush(Pager *foo);              /* writes every dirty page */
DbResult pager_close(Pager *foo);              /* flushes, frees everything */

#endif
```

`src/pager.c` (TODOs):
```c
DbResult pager_open(const char *path, Pager **out) {
    /* TODO: fopen "r+b"; if that fails because it doesn't exist, fopen "w+b"
     *       NEVER just use "w+b" — it TRUNCATES an existing database to empty
     *       fseek to end, ftell for the length, fseek back
     *       calloc the Pager; pages[] all NULL
     *       decide and DOCUMENT: what if the length isn't a multiple of PAGE_SIZE?
     */
}

DbResult pager_get_page(Pager *foo, uint32_t page_num, uint8_t **out) {
    /* TODO: bounds check page_num < MAX_PAGES
     *       cache hit -> return it
     *       calloc a page
     *       if page_num < pages_on_disk: fseek + fread (short read at EOF is FINE)
     *       cache it, grow page_count if needed, return borrowed pointer
     */
}
```

### Test list

Every test gets its own temp file and removes it at the end.

- [ ] A brand-new file has a page count of 0.
- [ ] `pager_get_page(0)` on a new file returns a **zero-filled** page of exactly
      `PAGE_SIZE`.
- [ ] Writing to a page, flushing, and reading it back **in a new `Pager` instance**
      returns the written bytes. (A new instance — otherwise you're only testing the
      cache.)
- [ ] `pager_get_page(n)` twice returns the **same pointer** — proves the cache works and
      that mutations are visible.
- [ ] `pager_flush` with no dirty pages doesn't grow the file.
- [ ] A page written, modified, then flushed once has the **final** contents.
- [ ] `pager_close` flushes automatically — write, close without flushing, reopen,
      verify.
- [ ] `pager_page_count` grows as pages are allocated.
- [ ] **Opening an existing file does not truncate it.** Write data, close, reopen,
      verify it's still there. This catches the `"w+b"` mistake, which silently destroys
      every database you open.
- [ ] `pager_get_page(MAX_PAGES)` returns an error rather than corrupting memory.
- [ ] Opening a file whose length isn't a multiple of `PAGE_SIZE` behaves the way you
      documented.
- [ ] **`pager_close` frees every cached page.** ASan's leak check does this
      automatically — if you forget one, the test suite fails.

That last one is free enforcement you'd have to write by hand in most languages. The
truncation test is the one that saves you from an evening of "where did my data go."

### Wiring it up

This is where the layers first connect end to end. Add a thin `Table` that sits between
the executor and the pager, storing rows at:
```c
page   = row_num / ROWS_PER_PAGE;
offset = (row_num % ROWS_PER_PAGE) * ROW_SIZE;
```
with `ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE` = 14, wasting 22 bytes per page.

**Do not "fix" that waste by letting rows straddle page boundaries.** A row split across
two pages means two disk reads and a partial-write hazard on crash. Every real database
wastes that tail. Wasting bytes to keep an invariant simple is a trade real engineers
make constantly, and recognizing when to make it is the skill.

Now `INSERT` and `SELECT` work end to end and data survives restart. **Stop and
appreciate this. You have written a database.**

### Refactor to look for

The offset arithmetic appears in both `table_read` and `table_write`. Extract it:
```c
typedef struct { uint32_t page; uint16_t offset; } RowLocation;
static RowLocation row_location(uint32_t row_num);
```
Chapter 7 needs a real `Cursor` anyway, so this is a natural place to grow one.

### Docs for this chapter

- `fopen` modes — read the table carefully, `"w"` truncates:
  <https://en.cppreference.com/w/c/io/fopen>
- `fread` / `fseek` / `ftell`: <https://en.cppreference.com/w/c/io>
- `mkstemp` for test files: <https://www.man7.org/linux/man-pages/man3/mkstemp.3.html>
- cstack Part 5, persistence to disk:
  <https://cstack.github.io/db_tutorial/parts/part5.html>
- CMU 15-445 Lecture 3 "Database Storage" and Lecture 6 "Buffer Pools":
  <https://15445.courses.cs.cmu.edu/>

### Done when

- [ ] Data survives a restart. Insert, `.exit`, restart, `SELECT`, see your rows.
- [ ] Every test uses a unique temp file and removes it; no `.db` in your repo.
- [ ] `pager.c` has no knowledge of rows, tables, or SQL. Check the `#include` list.
- [ ] Zero leaks reported by ASan and valgrind.
- [ ] You can explain why the page size is 4096 and why rows don't straddle pages.

---

## Chapter 6 — B+ tree leaf nodes

### Goal

Replace the flat array of rows with a **B+ tree**, starting with a single leaf node.
Rows become key-value pairs sorted by key, stored inside one page, with binary search.

### Why a B+ tree

Right now `SELECT * FROM users WHERE id = 3` scans every row: O(n). With a sorted
structure you get O(log n). But *which* sorted structure?

- **Sorted array in a file?** Fast lookup, but inserting in the middle means moving every
  subsequent row on disk. Unacceptable.
- **In-memory BST?** Doesn't survive restart, and doesn't fit in RAM at scale.
- **B+ tree.** A tree whose nodes are exactly one page. Height stays tiny because each
  node has hundreds of children — a 3-level tree holds millions of rows, so any lookup is
  at most 3 disk reads. It stays balanced through splits. All data lives in the leaves,
  and leaves are chained, so a full scan is a sequential walk with no tree traversal.

That last property is why it's a B+ tree and not a B-tree, and why every relational
database on earth uses one.

This chapter builds only the leaf. Chapter 7 makes it a tree.

### The page layout

Precision matters. Write these constants before any code.

**Common node header** (every node):

| Field            | Offset | Size |
|------------------|--------|------|
| `node_type`      | 0      | 1    |
| `is_root`        | 1      | 1    |
| `parent`         | 2      | 4    |
|                  |        | **6** |

**Leaf header** (follows the common header):

| Field       | Offset | Size |
|-------------|--------|------|
| `num_cells` | 6      | 4    |
| `next_leaf` | 10     | 4    |
|             |        | **8** (14 total) |

**Leaf body** — cells of key + serialized row:

| Field   | Size |
|---------|------|
| `key`   | 4    |
| `value` | 291  |
|         | **295 per cell** |

The arithmetic: `(4096 − 14) / 295 = 4082 / 295 = 13.83`, so **`LEAF_MAX_CELLS = 13`**,
with 187 bytes of slack. **Compute it in code from the constants** so it self-corrects if
the row format changes:
```c
enum {
    LEAF_SPACE     = PAGE_SIZE - LEAF_HEADER_SIZE,      /* 4082 */
    LEAF_CELL_SIZE = LEAF_KEY_SIZE + ROW_SIZE,          /* 295 */
    LEAF_MAX_CELLS = LEAF_SPACE / LEAF_CELL_SIZE        /* 13 */
};
static_assert(LEAF_MAX_CELLS == 13, "leaf capacity changed unexpectedly");
```

`next_leaf` is the page number of the next leaf, or 0 for "none" — page 0 is always the
root, so it can never legitimately be a next-leaf pointer. That's a neat trick real
formats use.

### Data specifics for C

- **A "leaf node" is not a struct; it's a set of functions over a `uint8_t *page`.**
  ```c
  uint32_t leaf_num_cells(const uint8_t *page);
  void     leaf_set_num_cells(uint8_t *page, uint32_t n);
  uint32_t leaf_key_at(const uint8_t *page, uint32_t index);
  ```
  There is no `LeafNode` type to deserialize into. **The page bytes are the truth.** If
  you parse a page into a struct and write it back later, you have two sources of truth
  and they will diverge after a flush. This is the most important design decision in the
  chapter, and C makes it natural in a way that struct-oriented languages don't.
- **Note the `const` on the readers.** The compiler then enforces which functions mutate,
  which is free documentation and free bug prevention.
- **Use `read_u32_be` / `write_u32_be`**, never pointer casts. Your `num_cells` lives at
  offset 6, which is **not 4-byte aligned** — casting `(uint32_t*)(page + 6)` is
  undefined behavior that works on x86 and traps on ARM. UBSan will flag it. The
  byte-at-a-time helpers sidestep it entirely.
- **Cell offset arithmetic** in one function, never inlined:
  ```c
  static size_t leaf_cell_offset(uint32_t index) {
      return LEAF_HEADER_SIZE + (size_t)index * LEAF_CELL_SIZE;
  }
  ```
  Note the `(size_t)` cast — `uint32_t * int` can overflow before it widens.
- **Binary search, not linear scan.** You're building a database; the log is the point.
  The loop should return the index of the first cell with `key >= target` — which is the
  insertion position when absent and the location when present. One function serving
  find, insert, and delete means one off-by-one bug instead of three.
- **Inserting in the middle shifts cells right — use `memmove`, not `memcpy`.** The
  source and destination overlap, and `memcpy` with overlapping regions is undefined
  behavior. It will usually appear to work, which is worse. This is a real, common,
  hard-to-find C bug and it is exactly what `memmove` exists for.
- **Reject duplicate keys** with `DB_DUPLICATE_KEY`. Primary keys are unique; enforcing it
  here is one line and saves confusion later.

### Skeletons

`src/node.h`:
```c
#ifndef MINIDB_NODE_H
#define MINIDB_NODE_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "pager.h"
#include "row.h"
#include "result.h"

typedef enum { NODE_INTERNAL = 0, NODE_LEAF = 1 } NodeType;

enum {
    /* common header */
    NODE_TYPE_OFFSET   = 0,  NODE_TYPE_SIZE   = 1,
    IS_ROOT_OFFSET     = 1,  IS_ROOT_SIZE     = 1,
    PARENT_OFFSET      = 2,  PARENT_SIZE      = 4,
    COMMON_HEADER_SIZE = PARENT_OFFSET + PARENT_SIZE,          /* 6 */

    /* leaf header */
    LEAF_NUM_CELLS_OFFSET = COMMON_HEADER_SIZE,  LEAF_NUM_CELLS_SIZE = 4,
    LEAF_NEXT_OFFSET      = LEAF_NUM_CELLS_OFFSET + LEAF_NUM_CELLS_SIZE,
    LEAF_NEXT_SIZE        = 4,
    LEAF_HEADER_SIZE      = LEAF_NEXT_OFFSET + LEAF_NEXT_SIZE, /* 14 */

    /* leaf body */
    LEAF_KEY_SIZE  = 4,
    LEAF_CELL_SIZE = LEAF_KEY_SIZE + ROW_SIZE,                 /* 295 */
    LEAF_SPACE     = PAGE_SIZE - LEAF_HEADER_SIZE,             /* 4082 */
    LEAF_MAX_CELLS = LEAF_SPACE / LEAF_CELL_SIZE               /* 13 */
};

static_assert(COMMON_HEADER_SIZE == 6,  "common header layout changed");
static_assert(LEAF_HEADER_SIZE   == 14, "leaf header layout changed");
static_assert(LEAF_MAX_CELLS     == 13, "leaf capacity changed");

/* --- common header --- */
NodeType node_type(const uint8_t *page);
void     node_set_type(uint8_t *page, NodeType type);
bool     node_is_root(const uint8_t *page);
void     node_set_root(uint8_t *page, bool is_root);
uint32_t node_parent(const uint8_t *page);
void     node_set_parent(uint8_t *page, uint32_t parent);

/* --- leaf --- */
void     leaf_init(uint8_t *page, bool is_root);
uint32_t leaf_num_cells(const uint8_t *page);
uint32_t leaf_next(const uint8_t *page);
void     leaf_set_next(uint8_t *page, uint32_t page_num);
uint32_t leaf_key_at(const uint8_t *page, uint32_t index);
void     leaf_value_at(const uint8_t *page, uint32_t index, Row *out);

/* Index of the first cell whose key >= `key`. Equals num_cells if all are smaller. */
uint32_t leaf_find_index(const uint8_t *page, uint32_t key);

/* DB_DUPLICATE_KEY if present, DB_FULL if no room (chapter 7 replaces this). */
DbResult leaf_insert(uint8_t *page, uint32_t key, const Row *value);

bool leaf_is_full(const uint8_t *page);

#endif
```

### Test list

- [ ] `LEAF_MAX_CELLS == 13` (a guard test — loudly reports a size change).
- [ ] An initialized leaf has 0 cells and `node_type == NODE_LEAF`.
- [ ] Insert one, read it back at index 0.
- [ ] Inserting keys 3, 1, 2 leaves them stored in order 1, 2, 3.
- [ ] Inserting at the front shifts everything right without corruption.
- [ ] Inserting at the very end appends.
- [ ] `leaf_find_index` on an empty node returns 0.
- [ ] `leaf_find_index` for a key smaller than all keys returns 0.
- [ ] `leaf_find_index` for a key larger than all returns `num_cells`.
- [ ] `leaf_find_index` for a present key returns its exact index.
- [ ] `leaf_find_index` for an absent key between two present keys returns the position
      it would occupy.
- [ ] A duplicate key returns `DB_DUPLICATE_KEY` and leaves `num_cells` unchanged.
- [ ] Filling to exactly `LEAF_MAX_CELLS` works; one more returns `DB_FULL`.
- [ ] Every value round-trips: insert 13 distinct rows, read all 13 back correctly.
- [ ] **Inserting 13 cells writes nothing past `PAGE_SIZE`.** Put the page in a
      `calloc`ed 4096-byte block so ASan detects any overrun — a stack array works too
      but heap gives better ASan reports.
- [ ] A node written through the pager, flushed, and reopened has all its cells intact.

That last one catches the "I parsed into a struct instead of using a view" mistake.
Write it early.

### Refactor to look for

1. **The single-byte bool accessors** (`node_type`, `is_root`) duplicate a read-a-byte /
   write-a-byte pattern. Extract `read_u8` / `write_u8` helpers.
2. **`leaf_*` and the coming `internal_*` share the common header.** You'll be tempted to
   copy those six accessors. Don't — they're already in `node.h` in the skeleton above,
   shared by both. Doing this *now*, while there's one node type, is much easier than
   later. Noticing duplication before it exists is the thing you're practicing.

### Docs for this chapter

- **SQLite file format §1.6, "B-tree Pages"** — read before coding. It's the real version
  of what you're building: <https://www.sqlite.org/fileformat2.html#b_tree_pages>
- cstack Part 8, "B-Tree Leaf Node Format" — same layout, in C, with diagrams:
  <https://cstack.github.io/db_tutorial/parts/part8.html>
- `memmove` vs `memcpy` — read the Notes section:
  <https://en.cppreference.com/w/c/string/byte/memmove>
- CMU 15-445 Lecture 8, "Tree Indexes": <https://15445.courses.cs.cmu.edu/>

### Done when

- [ ] `INSERT` and `SELECT` go through the leaf, and rows come back sorted by key.
- [ ] Inserting out of order still reads back in order.
- [ ] The 14th insert returns `DB_FULL` rather than corrupting the page.
- [ ] No pointer casts to `uint32_t*` anywhere — all access goes through the BE helpers.
- [ ] You can hex-dump a page and point at the header, the first key, and the third
      cell's email field.

---

## Chapter 7 — Splits, internal nodes, and cursors

**This is the hardest chapter. Budget three weeks of evenings. It is normal to spend a
whole session on one failing test. Getting through it is the single biggest thing you can
put on a résumé from this project.**

### Goal

When a leaf fills, **split** it into two and create a parent. Support a multi-level tree
with correct search, and a `Cursor` that walks all rows in key order.

### Why splitting is done this way

When a leaf with 13 cells receives a 14th, you have 14 cells to distribute across two
nodes:

- **Split evenly** (7 and 7). Uneven splits degrade toward a linked list, and balance is
  the entire value of the tree.
- **The new right node's smallest key is promoted** into the parent as a separator.
  Everything strictly less goes left; everything greater-or-equal goes right. Pick that
  convention and never deviate — inconsistency produces bugs that only appear at specific
  tree shapes, which are agony to find.
- **The leaf keeps its data.** In a B+ tree, promoting a key *copies* it; the row stays
  in the leaf. (In a plain B-tree it would move. That difference is exactly what makes
  leaf-chained scans possible.)
- **Splitting the root is special.** Page 0 must always be the root, because everything
  else holds page numbers pointing at it. So: copy the root's contents into a *new* page,
  create a second new page for the other half, and re-initialize page 0 as an internal
  node pointing at both. The tree grows in height at the root, which is why B+ trees stay
  balanced without any rebalancing logic.

That root-split trick is genuinely elegant and worth sitting with until it clicks.

### Internal node layout

| Field         | Offset | Size |
|---------------|--------|------|
| common header | 0      | 6    |
| `num_keys`    | 6      | 4    |
| `right_child` | 10     | 4    |
|               |        | **14 header** |

Body: (child pointer, key) pairs, 8 bytes each. `(4096 − 14) / 8 = 510` keys max.

With *n* keys there are *n+1* children, so the last child lives in the header as
`right_child` — the standard trick to keep the cell array uniform.

**For testing, make the max overridable.** In C the clean way is a non-`const` global
with a default:
```c
/* node.c */
uint32_t internal_max_keys = INTERNAL_MAX_KEYS_DEFAULT;   /* 510 */

/* node.h */
extern uint32_t internal_max_keys;    /* tests set this to 3 to force splits */
```
Then tests set it to 3 and force internal splits with a dozen inserts instead of half a
million. **Reset it in every test that changes it** — a leaked global is exactly the
shared state that makes tests order-dependent. Making a rare code path cheap to reach is
a legitimate technique, not a hack.

### Data structure hints

- **A split has to return two things**: the promoted key and the new page number. With no
  tuples, use out-parameters and a status:
  ```c
  typedef struct { uint32_t promoted_key; uint32_t new_page; } Split;

  /* DB_OK = inserted, no split. DB_SPLIT = split occurred, *out_split is filled. */
  static DbResult insert_into(BTree *t, uint32_t page_num, uint32_t key,
                              const Row *row, Split *out_split);
  ```
  Using a distinct `DB_SPLIT` status rather than a bool means the caller can't ignore it
  by accident, and the code reads as a state machine.
- **Search is recursive**: at an internal node, binary-search the keys to pick a child,
  descend, repeat until a leaf.
- **Update the parent pointers of moved children.** When an internal node splits, children
  that moved to the new node still record the *old* parent. Forgetting this produces a
  tree that works for lookups and breaks on the next split — a brutal bug to chase.
  **Write the test for it before you write the code.**
- **The temp-buffer split.** You have 14 cells to place and a 13-cell page. Copy all
  cells into a temporary array, insert the new one in sorted position, then deal 7 and 7.
  ```c
  uint8_t temp[(LEAF_MAX_CELLS + 1) * LEAF_CELL_SIZE];   /* ~4KB on the stack */
  ```
  4KB on the stack is fine (default stack is ~8MB). If you'd rather, `calloc` it — but
  then every early return needs a `free`, so use the `goto cleanup` pattern. **Splitting
  in place without a temp buffer is possible and is a nightmare.** Take the buffer.
- **The `Cursor`** holds `(pager, page_num, cell_index, at_end)` and knows how to
  advance: increment the index; if it runs off the leaf, jump to `next_leaf` and reset to
  0; if `next_leaf` is 0, you're at the end. This is the **iterator model**, how every
  database executes queries, and Chapter 12 builds directly on it.
- **Cursor invalidation:** a cursor holds a page number and index. If the tree splits
  while a cursor is live, that position may now be wrong. Real databases have elaborate
  machinery for this. **You should simply document that mutating the tree invalidates all
  cursors**, and never do both at once. Recognizing the hazard and constraining your way
  around it is the right engineering call here.

### Skeletons

```c
/* src/btree.h */
typedef struct BTree BTree;
typedef struct { uint32_t promoted_key; uint32_t new_page; } Split;

DbResult btree_open(Pager *pager, BTree **out);
DbResult btree_insert(BTree *foo, uint32_t key, const Row *row);
DbResult btree_find(BTree *foo, uint32_t key, Row *out);      /* DB_NOT_FOUND if absent */

typedef struct {
    BTree   *tree;
    uint32_t page_num;
    uint32_t cell_index;
    bool     at_end;
} Cursor;

DbResult cursor_start(BTree *foo, Cursor *out);      /* leftmost cell of leftmost leaf */
DbResult cursor_next(Cursor *foo, Row *out);         /* DB_NOT_FOUND when exhausted */

/* TEST ONLY. Prints the tree structure to `out`. */
void btree_dump(BTree *foo, FILE *out);
```

```c
/* src/btree.c — the shape of the recursion */
static DbResult insert_into(BTree *t, uint32_t page_num, uint32_t key,
                            const Row *row, Split *out_split) {
    /* TODO: leaf  -> leaf_insert; if DB_FULL -> split_leaf, return DB_SPLIT
     *       inner -> pick child, recurse;
     *                if child returned DB_SPLIT, insert the promoted key here,
     *                which MAY ITSELF SPLIT -> return DB_SPLIT again
     */
}

static DbResult split_leaf(BTree *t, uint32_t page_num, uint32_t key,
                           const Row *row, Split *out_split) {
    /* TODO: allocate a new page; copy 13 cells + the new one into a temp buffer
     *       in sorted order; deal 7 to old, 7 to new
     *       fix chaining: new->next = old->next; old->next = new_page
     *       out_split = { smallest key in new, new_page }
     */
}

static DbResult split_root(BTree *t, const Split *split) {
    /* TODO: page 0 must STAY the root.
     *       copy page 0's contents to a fresh page A
     *       re-init page 0 as an internal node: left child A, right child split->new_page
     *       set is_root on page 0, clear it on A
     *       set the parent pointer of A and of split->new_page to 0
     */
}
```

### Test list

Build these in order. Do not skip ahead — each is a prerequisite for debugging the next.

**Leaf splitting**
- [ ] Inserting `LEAF_MAX_CELLS + 1` rows makes the root an internal node.
- [ ] After that split, the tree has exactly 3 pages: root, left leaf, right leaf.
- [ ] The two leaves have 7 and 7 cells.
- [ ] Every one of the 14 keys is still findable.
- [ ] Keys are in ascending order across both leaves.
- [ ] `next_leaf` chaining: left points at right; right points at 0.
- [ ] The promoted key equals the smallest key in the right leaf.
- [ ] Splitting while inserting the **smallest** key works (new key goes left).
- [ ] Splitting while inserting the **largest** key works (new key goes right).
- [ ] Splitting while inserting a key landing exactly at the split point works.

**Multi-level**
- [ ] With `internal_max_keys = 3`, inserting enough rows splits an internal node.
- [ ] After an internal split, all keys are still findable.
- [ ] After an internal split, the **parent pointers of moved children are correct**.
- [ ] Tree height grows only when the root splits.
- [ ] Insert 1,000 rows in **random** order; all findable; a cursor walk returns them in
      ascending key order.
- [ ] Insert 1,000 rows in **ascending** order (worst case for many trees); same
      assertions.
- [ ] Insert 1,000 rows in **descending** order; same assertions.

**Cursor**
- [ ] A cursor over an empty tree immediately returns `DB_NOT_FOUND`.
- [ ] A cursor over a single leaf returns every row in order.
- [ ] A cursor over a multi-leaf tree crosses leaf boundaries correctly.
- [ ] A cursor over a 3-level tree still returns everything in order.

**Persistence and memory**
- [ ] Build a multi-level tree, close, reopen, everything still findable.
- [ ] The 1,000-row test reports **zero leaks** and zero ASan errors.

Those three 1,000-row tests are your safety net for the rest of the project. They're
slow-ish and boring and they will catch nearly everything.

### Write the dump helper before you debug

Before you start on splits, write this. It will save you many hours:

```c
void btree_dump(BTree *foo, FILE *out);
```
```
internal 0 (1 key): keys [7]
  - leaf 1 (7 cells, next=2): keys [1 2 3 4 5 6 7]
  - leaf 2 (7 cells, next=0): keys [8 9 10 11 12 13 14]
```

When a split test fails, dump before and after. The structural bug is visible
immediately in a way it never is in a debugger. **Building a tool to see your own data
structure is what an experienced engineer does first, not last.**

### Refactor to look for

`leaf_*` and `internal_*` now have real parallel structure — both have "is this full,"
"find the index for this key," "shift cells to make room." You could unify them behind a
vtable.

**Be honest with yourself about whether it helps.** Leaf and internal nodes genuinely
differ (different cell sizes, different meanings), and forcing them behind one interface
can produce a worse design than two clear families of functions sharing the common-header
helpers. My read: **share the common header, keep the bodies separate.**

But run the experiment — `git switch -c try-node-vtable`, do it, look at the result, keep
or revert. Using a branch to try a design you might throw away is itself a professional
habit worth building.

### Docs for this chapter

- **B+ tree visualizer** — insert keys, watch splits. **Do this before writing code.**
  Twenty minutes here makes the code make sense in a way reading doesn't:
  <https://www.cs.usfca.edu/~galles/visualization/BPlusTree.html>
- cstack Parts 10–13 — splitting, internal nodes, recursive search, in C. Work through
  yours first: <https://cstack.github.io/db_tutorial/parts/part10.html>
- **Database Internals**, Petrov, Chapters 2–4. The clearest prose on splits anywhere:
  <https://www.databass.dev/>
- SQLite file format §1.6 again, now with more context:
  <https://www.sqlite.org/fileformat2.html#b_tree_pages>

### Done when

- [ ] 1,000 rows inserted in random order are all findable, and a cursor returns them
      sorted.
- [ ] A multi-level tree survives close and reopen.
- [ ] You have a `btree_dump` and you used it.
- [ ] Zero ASan errors and zero leaks across the whole suite.
- [ ] You can explain, out loud and without notes, why the root split allocates two new
      pages instead of one.


---

## Chapter 8 — The catalog and `CREATE TABLE`

### Goal

Stop hardcoding the `users` schema. Support `CREATE TABLE t (id INT, name TEXT)`, store
the schema in the database file itself, and serialize rows according to it.

### Why the catalog lives in the database

The schema must persist alongside the data it describes. The elegant solution — the one
SQLite uses — is **the catalog is just another table**. A tree at a known page holds one
row per user table with its name, root page, and column definitions.

This is a bootstrapping problem and it's genuinely delightful once it clicks: to read any
table you need the catalog, and the catalog is a table. It resolves because the catalog's
*own* schema is hardcoded in your source at a known root page. One hardcoded fact
bootstraps everything.

### Data structure hints

- **The schema is dynamic, so `Row` can no longer be a fixed struct.** Options, in
  increasing order of C-ness:
  1. `void *values[]` plus the schema to interpret them — flexible, type-unsafe.
  2. A tagged union `Value` — `{ type, union { int64_t i; struct { char *s; size_t n; } } }`.
     **Take this one.** It's the same pattern as your AST, it's type-safe at runtime,
     and it makes the evaluator in Chapter 9 straightforward.
- **A `Row` becomes `{ const Schema *schema; Value *values; size_t count; }`.**
  Who owns `values`? Decide and document. Simplest workable answer: the row owns them,
  with `row_free`; or the caller passes a buffer sized from the schema. The second avoids
  per-row allocation in a scan loop, which matters — but start with the first and
  optimize when a test tells you to.
- **This will break most of your existing tests, and that's fine — it's what they're
  for.** Change them one file at a time, committing after each goes green. Add a
  test-only helper `Row *test_row(uint32_t id, const char *name, const char *email)` so
  most tests need a one-word change instead of a rewrite. Surviving a change like this
  with tests as your guide is precisely the experience you're here for.
- **Schema offsets are computed at runtime** by running totals — the same pattern as
  Chapter 4's enum, but now in a loop.
- **Variable-length rows are the natural next step and you may reasonably skip them.**
  With `TEXT` of arbitrary length, a cell is no longer fixed size, so a leaf can't compute
  `cell_offset(i)` by multiplication. The real solution is a **slot array** at the top of
  the page pointing at cell offsets, with cells growing from the bottom up — that's the
  actual SQLite design.

  **This is a large change with a lot of new bug surface, and the learning-per-hour is
  lower than in Chapters 6, 7, and 11.** Keeping fixed widths and just making them
  schema-driven is a legitimate stopping point. Decide deliberately, write the reasoning
  in your README, and move on. Knowing when to stop scope-creeping is a skill too.

### Skeletons

```c
/* src/catalog.h */
typedef enum { COL_INT, COL_TEXT, COL_BOOL } ColumnType;

typedef struct {
    char       name[64];
    ColumnType type;
    uint32_t   size;        /* bytes on disk */
} Column;

typedef struct {
    Column  *columns;
    size_t   count;
    uint32_t row_size;      /* computed sum */
} Schema;

typedef struct {
    ColumnType type;
    union {
        int64_t as_int;
        struct { const char *ptr; size_t len; } as_text;
        bool    as_bool;
    };
    bool is_null;
} Value;

uint32_t schema_offset_of(const Schema *foo, const char *column_name);  /* UINT32_MAX if absent */
const Column *schema_column(const Schema *foo, const char *name);

typedef struct { char name[64]; uint32_t root_page; Schema schema; } TableInfo;

DbResult catalog_create_table(Catalog *foo, const char *name, const Schema *schema);
DbResult catalog_lookup(Catalog *foo, const char *name, TableInfo *out);
```

### Test list

- [ ] `schema_offset_of` returns running totals: first column 0, second = size of first.
- [ ] `schema->row_size` is the sum of column sizes.
- [ ] `CREATE TABLE users (id INT, name TEXT)` parses to a `STMT_CREATE_TABLE` node.
- [ ] Creating a table adds an entry to the catalog.
- [ ] Creating a table that already exists returns a clear error.
- [ ] Selecting from a nonexistent table returns a clear error **naming the table**.
- [ ] The catalog survives close and reopen — the real persistence test.
- [ ] `INSERT` with the wrong number of values errors.
- [ ] `INSERT` with a type mismatch (`'abc'` into an `INT`) errors.
- [ ] Two tables with different schemas coexist and don't interfere.
- [ ] `.tables` (a new meta-command) lists them — an easy win that makes it feel real.
- [ ] No leaks when a `CREATE TABLE` fails partway.

### Refactor to look for

As `Row` changes, watch for code reaching into a row by **position** (`values[0]`) where
it should use the **name**. Positional access is right inside the storage layer and wrong
everywhere above it. Drawing that line cleanly is the chapter's real work.

### Docs

- SQLite's `sqlite_schema` table — catalog-as-a-table, from the source:
  <https://www.sqlite.org/schematab.html>
- SQLite record format §2.1 (variable-length encoding, if you take that path):
  <https://www.sqlite.org/fileformat2.html#record_format>

### Done when

- [ ] No table or column name appears as a literal outside a test.
- [ ] You can create two tables with different schemas and query both.
- [ ] Schemas survive restart. No leaks.

---

## Chapter 9 — `WHERE` and expression evaluation

### Goal

Evaluate the `Expr` tree from Chapter 3 against each row, and filter.

### Why a tree-walking interpreter

You already have the AST. Evaluating it is a recursive function over a tagged union —
about forty lines, and the same shape as an interpreter for a small programming language.
Worth having written once.

The important subtlety is **NULL**. SQL doesn't use two-valued boolean logic; it uses
**three-valued logic**, where `NULL = NULL` is not `TRUE` but `UNKNOWN`, and `WHERE` keeps
a row only when the result is exactly `TRUE`. This surprises people constantly in real
SQL, and implementing it once means you'll never be caught by it again.

`AND`:

| A | B | A AND B |
|---|---|---|
| T | T | T |
| T | U | U |
| T | F | F |
| U | U | U |
| U | F | F |
| F | F | F |

`OR` is the dual. `NOT UNKNOWN` is `UNKNOWN`.

### Data structure hints

- **Represent the result as a three-valued enum**, not a `bool`:
  ```c
  typedef enum { TRUTH_FALSE = 0, TRUTH_TRUE = 1, TRUTH_UNKNOWN = 2 } Truth;
  ```
  A `bool` plus a separate `is_null` flag invites exactly the bug the three-valued logic
  exists to prevent.
- **The evaluator is a switch over `ExprType`.** Omit `default:` so the compiler warns if
  you add a node type and forget to handle it — the closest C gets to exhaustiveness
  checking.
- **Comparison across types** needs a decision: is `1 = '1'` true, false, or an error?
  SQLite coerces; PostgreSQL errors. Pick one, write it down, test it. Making and
  documenting a semantics decision is exactly what you'll do on a real team.
- **`AND` short-circuits carefully.** `FALSE AND <anything>` is `FALSE`, so you can skip
  the right side. `UNKNOWN AND <anything>` **cannot** short-circuit to `UNKNOWN`, because
  a `FALSE` on the right makes the whole thing `FALSE`.
- **Recursion depth:** a deeply nested expression means deep recursion, and C has no
  stack-overflow exception — it segfaults. For hand-typed SQL this never happens. Note
  the limitation; a depth counter in the parser is the real fix if you care.

### Skeletons

```c
/* src/eval.h */
typedef enum { TRUTH_FALSE = 0, TRUTH_TRUE = 1, TRUTH_UNKNOWN = 2 } Truth;

/* Evaluates a predicate. A row is kept only on TRUTH_TRUE. */
Truth eval_predicate(const Expr *foo, const Row *row, const Schema *schema);

/* Evaluates to a value. Sets out->is_null for SQL NULL. */
DbResult eval_expr(const Expr *foo, const Row *row, const Schema *schema, Value *out);
```

```c
/* src/eval.c */
static Truth compare_values(const Value *left, BinaryOp op, const Value *right) {
    /* TODO: either side NULL -> TRUTH_UNKNOWN
     *       otherwise compare, respecting your coercion decision
     */
}
```

### Test list

- [ ] `WHERE id = 3` matches only the row with id 3.
- [ ] Each of `=`, `!=`, `<`, `<=`, `>`, `>=` filters correctly.
- [ ] `WHERE name = 'bob'` matches on strings.
- [ ] `WHERE id = NULL` matches **nothing** — not even rows where id is NULL.
- [ ] `WHERE id != NULL` also matches nothing.
- [ ] `TRUE AND UNKNOWN` is `UNKNOWN` (a direct unit test on the truth table).
- [ ] `FALSE AND UNKNOWN` is `FALSE`.
- [ ] `TRUE OR UNKNOWN` is `TRUE`.
- [ ] A `WHERE` on a nonexistent column errors — at plan time if possible, not per row.
- [ ] `WHERE` on an empty table returns no rows and doesn't crash.
- [ ] A `WHERE` matching nothing returns empty, not an error.

Write the truth-table tests as a table-driven loop — it's the C equivalent of a
parameterized test and it's exactly the right shape:
```c
static const struct { Truth a, b, expect; } AND_CASES[] = {
    {TRUTH_TRUE,    TRUTH_TRUE,    TRUTH_TRUE},
    {TRUTH_TRUE,    TRUTH_UNKNOWN, TRUTH_UNKNOWN},
    {TRUTH_FALSE,   TRUTH_UNKNOWN, TRUTH_FALSE},
    /* ... */
};
for (size_t i = 0; i < sizeof AND_CASES / sizeof AND_CASES[0]; i++) { /* assert */ }
```

### Docs

- SQLite's NULL handling, including the quirks: <https://www.sqlite.org/nulls.html>
- **Crafting Interpreters, Chapter 7 "Evaluating Expressions"** — the same tree-walking
  evaluator: <https://craftinginterpreters.com/evaluating-expressions.html>

### Done when

- [ ] Every comparison operator works on `INT` and `TEXT`.
- [ ] Three-valued logic is implemented and directly tested.
- [ ] You can state what `SELECT * FROM t WHERE x = NULL` returns, and why.

---

## Chapter 10 — `DELETE` and `UPDATE`

### Goal

Remove and modify rows, and deal with the free space that leaves behind.

### Why deletion is harder than insertion

Insertion grows things; deletion leaves holes, and holes raise questions with no obviously
right answer:

- **Delete in place, shifting cells left?** Simple, keeps pages compact, costs a
  `memmove`.
- **Tombstone it?** Mark the cell dead and skip it on read. Fast writes, but the page
  fills with garbage and every scan pays until you compact.
- **Merge underfull nodes?** Textbook B-trees merge with a sibling below half full. This
  is *substantially* harder than splitting — borrowing from a sibling, merging with a
  sibling, the parent becoming underfull in turn, recursively, up to a root that might
  collapse a level.

**Real SQLite does not merge on delete.** It leaves pages underfull and reclaims them
lazily. That's a deliberate engineering trade: merges are complex, bug-prone, and rarely
worth it for real workloads.

**Recommendation: shift-left within the leaf, don't merge.** Write that decision and its
reasoning in your README. Then implement merging on a branch as a stretch goal if you
want the challenge. Knowing that the textbook algorithm and the production algorithm
differ — and why — is genuinely valuable.

`UPDATE` is easier: if the key doesn't change and rows are fixed-width, it's an overwrite
in place. If the key changes, it's a delete plus an insert, because position in the tree
depends on the key.

### Data structure hints

- **`memmove`, not `memcpy`.** Shifting cells left means overlapping source and
  destination. `memcpy` with overlap is undefined behavior that usually appears to work.
  This is the same trap as Chapter 6 and it bites again here.
- You do **not** need to zero the freed tail — it's beyond `num_cells` and unreachable.
  It *will* still show in a hex dump, which confuses people, so add a comment.
- **Deleting a missing key is a no-op returning 0 affected rows**, not an error.
  `DELETE FROM t WHERE id = 999` matching nothing is valid SQL.
- **Return the affected-row count** from both. Users expect `2 rows deleted.`, and the
  count is exactly what you assert on.
- **Deleting through a cursor while iterating is a genuine hazard** — you shift cells
  under your own feet, and in C that's memory corruption rather than an exception.
  Simplest correct approach: collect the keys to delete in a first pass, delete in a
  second. Recognizing that iteration and mutation don't mix transfers far beyond
  databases.

### Test list

- [ ] Deleting the only row leaves an empty table.
- [ ] Deleting the first cell shifts the rest left correctly.
- [ ] Deleting the last cell just decrements the count.
- [ ] Deleting a middle cell leaves the rest in order and findable.
- [ ] Deleting a nonexistent key affects 0 rows and doesn't error.
- [ ] `DELETE FROM t` with no `WHERE` empties the table.
- [ ] `DELETE ... WHERE` deletes exactly the matching rows and no others.
- [ ] After deletes, a cursor scan returns everything remaining, in order.
- [ ] **Insert 100, delete every other one, verify the remaining 50 by key.** This is the
      test that catches shifting bugs. Write it.
- [ ] Delete then re-insert the same key succeeds (no ghost duplicate-key error).
- [ ] `UPDATE ... SET name = 'x' WHERE id = 3` changes only that row.
- [ ] `UPDATE` reports the correct affected-row count.
- [ ] Deletes and updates survive close and reopen.
- [ ] The delete-every-other test is ASan-clean — a `memmove` with a wrong length shows
      up here as a heap-buffer-overflow.

### Docs

- SQLite's free page list, for lazy reclamation:
  <https://www.sqlite.org/fileformat2.html#freelist>
- `memmove` — read the Notes: <https://en.cppreference.com/w/c/string/byte/memmove>
- **Database Internals**, Petrov, on deletion and rebalancing.

### Done when

- [ ] All four statement types work end to end.
- [ ] Your README states your deletion strategy and why you chose it.
- [ ] Deletes survive a restart. No leaks, no ASan errors.

---

## Chapter 11 — Durability and the write-ahead log

### Goal

Survive a crash mid-write without corrupting the database.

### Why this is a real problem

Right now `pager_flush` writes dirty pages one at a time. If the process dies after page
3 and before page 4, the file contains half an operation. A B+ tree split touches three
pages; two of three written is a corrupt tree — not "lost the last row" but "the file is
now garbage."

Worse: **`fwrite` doesn't mean it's on the disk, and neither does `fflush`.**

```
your buffer  --fwrite-->  libc buffer  --fflush-->  OS page cache  --fsync-->  DISK
```

`fflush` gets you to the OS. Only `fsync` gets you to the platter, and it costs
milliseconds versus microseconds. That gap is why every database has a durability story,
and why they all look roughly the same.

**The write-ahead log** solves it: before modifying any page, append a record of the
intended change to a separate log file, and `fsync` *that*. The log is append-only, so
it's sequential and fast. Then modify pages at leisure. On startup, replay any records
after the last checkpoint. If you crash mid-append, the last record is incomplete, you
detect that via a checksum, and discard it — safe, because that transaction never
reported success.

The principle underneath: **make one small write atomic and durable, and use it to
protect many large writes.** That idea appears everywhere in systems engineering.

### Data structure hints

- **Record layout:** `[length:4][lsn:8][page_num:4][page_image:4096][crc32:4]`.
  Physical logging — storing the whole after-image — is the simplest workable design.
  Logical logging ("insert key 5") is more compact and much harder to get right.
- **LSN:** a monotonically increasing `uint64_t`. How you know what's already applied.
- **Checksum:** write your own CRC32 — it's ~20 lines with a lookup table, it's a good
  exercise, and it avoids a dependency. Compute over the record, store at the end, verify
  on replay. **A torn record at the end of the log is expected on a crash, not an error.**
  Detecting it correctly is the whole mechanism.
- **`fsync`:**
  ```c
  #include <unistd.h>
  fflush(log_file);              /* libc buffer -> OS */
  fsync(fileno(log_file));       /* OS -> disk */
  ```
  On macOS, `fsync` doesn't fully flush the drive's own cache; `fcntl(fd, F_FULLFSYNC)`
  does. Note it in a comment — knowing that "durable" is platform-dependent is part of
  the lesson.
- **Checkpoint:** flush all dirty pages, `fsync` the database file, then truncate the log.
  Everything before the checkpoint is safely in the main file.
- **Recovery on open:** read the log from the start; for each record with a valid
  checksum, write its page image into the database; **stop at the first bad checksum.**

### Testing crashes

You can't easily kill your own process mid-test, so **simulate the crash**. Thinking your
way to this is half the value of the chapter. In C, a function pointer hook is the clean
way:

```c
/* pager.h — test seam */
extern int (*pager_write_hook)(uint32_t page_num);   /* NULL in production */

/* pager.c, in the write path */
if (pager_write_hook && pager_write_hook(page_num) != 0) return DB_SIMULATED_CRASH;
```

```c
/* in the test */
static int writes_until_crash;
static int crash_after_n(uint32_t page) {
    (void)page;
    return (--writes_until_crash < 0) ? 1 : 0;
}

writes_until_crash = 2;
pager_write_hook = crash_after_n;
/* ... do the split, expect DB_SIMULATED_CRASH ... */
pager_write_hook = NULL;          /* ALWAYS reset — it's a global */
```

Then loop N from 1 to the number of writes in a split, reopening and recovering each
time. That's a proper fault-injection harness, and building one is a genuinely senior
move.

### Test list

- [ ] An empty log replays to a no-op.
- [ ] A log with one record replays that page image.
- [ ] A log with a **truncated final record** replays everything before it and stops.
- [ ] A log with a corrupted checksum mid-file stops at that record.
- [ ] Crash after the log append but before the page write: the row is present after
      recovery. (The log did its job.)
- [ ] Crash during a split, at **each** of the writes in turn: after recovery the tree is
      valid and every pre-crash key is findable.
- [ ] A checkpoint truncates the log.
- [ ] **Recovery is idempotent** — running it twice gives the same result. This matters
      because you might crash *during* recovery.
- [ ] Your CRC32 matches a known-good value for a known input (e.g. the CRC32 of
      `"123456789"` is `0xCBF43926`). Catches a wrong polynomial or bit order.

That idempotence test separates a WAL you understand from one you copied.

### Docs

- **SQLite's WAL documentation** — the design, in the implementers' words:
  <https://www.sqlite.org/wal.html>
- **SQLite atomic commit** — how it works without a WAL. Illuminating:
  <https://www.sqlite.org/atomiccommit.html>
- `fsync`: <https://www.man7.org/linux/man-pages/man2/fsync.2.html>
- CRC32, with a worked table-driven implementation to check yours against:
  <https://www.rfc-editor.org/rfc/rfc1952#section-8>
- **ARIES** (the canonical recovery paper) if you want the deep version.

### Done when

- [ ] Fault-injection tests pass for a crash at every write point in a split.
- [ ] Recovery is idempotent.
- [ ] You can explain the difference between `fwrite`, `fflush`, and `fsync` to someone
      else.

---

## Chapter 12 — The planner, `ORDER BY`, and joins

### Goal

Stop executing the AST directly. Build a **query plan** — a tree of iterators — and
execute that. Add `ORDER BY`, `LIMIT`, and a simple join.

### Why a plan is separate from an AST

The AST says what the user *wrote*. The plan says what the engine will *do*. They differ
because there's more than one way to answer a query:

- `WHERE id = 3` on a primary key: **seek** the B+ tree. One or two page reads.
- `WHERE name = 'bob'` with no index: **scan** every row. Thousands of reads.

Choosing between them is the **query optimizer**, and it's why SQL is *declarative* — you
say what you want, the engine decides how. Your optimizer will have exactly one rule
("equality on the primary key → seek; otherwise scan"), and that's fine. Having *one*
rule in the right architectural place teaches you more than a hundred in the wrong one.

### The iterator model

Each plan node exposes `open`, `next`, `close` and pulls from its children. This is the
**Volcano model**, and it's how essentially every database executes queries.

```
Limit(10)
  └─ Sort(by name)
       └─ Filter(age > 21)
            └─ SeqScan(users)
```

`Limit.next()` calls `Sort.next()` calls `Filter.next()` calls `SeqScan.next()`, which
advances your Chapter 7 cursor. Rows flow up one at a time, so `LIMIT 10` over a
million-row table only touches the rows it needs.

**The elegance: `Filter` doesn't know whether its child is a scan, a seek, or a join.**
Composable operators over a uniform interface. Same layering idea as the pager, the
tokenizer, and the `FILE*` injection in Chapter 1. Four chapters, same lesson, four
scales. That repetition is the point of the whole project.

### Data structure hints — this is where function pointers earn their keep

Use the vtable pattern from Part 6:

```c
typedef struct Operator Operator;

typedef struct {
    DbResult (*open) (Operator *self);
    DbResult (*next) (Operator *self, Row *out);   /* DB_NOT_FOUND when exhausted */
    void     (*close)(Operator *self);
} OperatorVTable;

struct Operator {
    const OperatorVTable *vtable;
};

/* each concrete operator embeds Operator FIRST */
typedef struct {
    Operator  base;          /* MUST be first — see Part 6 */
    Cursor    cursor;
} SeqScan;

typedef struct {
    Operator     base;
    Operator    *child;      /* borrowed, or owned — DECIDE and document */
    const Expr  *predicate;
} Filter;
```

- **Ownership of the operator tree.** Who frees the children? Cleanest answer: **the
  planner allocates the whole tree from an arena**, exactly like the AST, and
  `close` never frees — it just releases cursors and file handles. One `arena_free` tears
  down the plan. Reusing the arena idea here is a sign you learned the right lesson in
  Chapter 3.
- **Blocking vs streaming.** `Filter` and `Limit` are streaming — constant memory. `Sort`
  is **blocking**: it must consume its whole input before producing the first row. That
  distinction is real and important; it's why `ORDER BY` on a huge table needs a
  spill-to-disk sort in real systems.
- **`Sort`** buffers into a growable array and uses `qsort`. Note the limitation in a
  comment.
  ```c
  #include <stdlib.h>
  qsort(rows, n, sizeof rows[0], compare_fn);
  int compare_fn(const void *a, const void *b);   /* the mandatory signature */
  ```
  **`qsort`'s comparator takes `const void*` and you must cast inside.** It must return
  negative/zero/positive — and **never write `return a->id - b->id`**, which overflows for
  large values and silently returns the wrong sign. Compare explicitly.
  `qsort` is also not stable; if stability matters, add a tiebreaker on the original
  index.
- **Nested loop join:** for each outer row, scan the inner. O(n·m) — genuinely terrible,
  and genuinely what databases fall back on when nothing better applies. Implement it,
  measure it, then read about hash joins and understand *why* they exist.
- **The planner** is a function `Statement* -> Operator*`. Keep it separate from both the
  parser and the operators. Test it by asserting on plan *shape*.

### Test list

**Plan shape (test the planner without executing anything)**
- [ ] `SELECT * FROM t` plans a bare `SeqScan`.
- [ ] `SELECT * FROM t WHERE id = 3` plans an `IndexSeek`, not a `SeqScan`.
- [ ] `SELECT * FROM t WHERE name = 'bob'` plans `Filter(SeqScan)`.
- [ ] `SELECT * FROM t WHERE id > 3` plans `Filter(SeqScan)` — a range isn't an equality.
- [ ] `SELECT name FROM t` plans a `Project` above the scan.
- [ ] `ORDER BY` plans a `Sort` above the filter, not below it.
- [ ] `LIMIT` plans a `Limit` at the very top.

**Execution**
- [ ] `SeqScan` returns every row, in key order.
- [ ] `Filter` returns only matching rows.
- [ ] `Limit(0)` returns nothing.
- [ ] `Limit(n)` where n exceeds the row count returns everything, no error.
- [ ] **`Limit` stops pulling from its child early** — count the child's `next` calls and
      assert it's `n+1`. This is the one place a call-counting test is legitimate, because
      early termination *is* the behavior under test.
- [ ] `Sort` orders correctly ascending and descending.
- [ ] `ORDER BY` on a column with NULLs puts them where you decided.
- [ ] Nested loop join produces the correct result.
- [ ] Join with an empty inner relation produces no rows.
- [ ] **`close` propagates to every child, even after an error.** Test with a child whose
      `next` fails.
- [ ] The whole plan tree is freed with one `arena_free` and leaks nothing.

### Refactor to look for

With seven operator types, `open`/`close` delegation to a single child is duplicated six
times. Extract a shared `unary_open` / `unary_close` that the vtables point at directly —
in C you don't need a base class, you just reuse the function in multiple vtables. That's
a nice demonstration that inheritance was only ever a way to share functions.

Learning to tell mechanical duplication (obviously extract) from genuinely different
behaviors that only look similar (leave them alone — the Chapter 7 node question) is one
of the more valuable judgments in this whole document.

### Docs

- **Volcano / iterator model** — Graefe's paper is the original; search "Volcano optimizer
  generator Graefe". Dense but foundational.
- SQLite query planner overview: <https://www.sqlite.org/optoverview.html>
- `EXPLAIN QUERY PLAN` in real SQLite — install `sqlite3` and run it against a real
  database. An hour comparing its output to your own plan trees is well spent:
  <https://www.sqlite.org/eqp.html>
- `qsort`: <https://en.cppreference.com/w/c/algorithm/qsort>

### Done when

- [ ] The executor never touches the AST — only the plan.
- [ ] Adding a new operator requires touching only the planner and the new file.
- [ ] You added an `EXPLAIN` meta-command that prints the plan tree. (Do this. An hour of
      work, and it makes the system feel finished.)

---

# Part 8 — Appendices

## A. Crash and error decoder

### Sanitizer output

| Report | What it means | Where to look |
|---|---|---|
| `heap-buffer-overflow` | Wrote or read past a `malloc`ed block | The line named; check a `<=` that should be `<`, or an offset+size |
| `stack-buffer-overflow` | Same, on a local array | Usually a fixed-size buffer that's too small |
| `heap-use-after-free` | Used memory after `free` | The report shows where it was freed — go there |
| `attempting double-free` | `free`d twice | Two owners. Set the pointer to NULL after free |
| `stack-use-after-return` | A pointer to a local outlived its function | Return heap memory or take an out-param |
| `LeakSanitizer: detected memory leaks` | Never freed | The allocation stack is in the report |
| `runtime error: signed integer overflow` | UBSan; `INT_MAX + 1` | Use unsigned, or a wider type |
| `runtime error: load of misaligned address` | UBSan; a pointer cast into a buffer | Use `memcpy` or the byte-at-a-time helpers |
| `runtime error: shift exponent 32 is too large` | UBSan; shifting a 32-bit value by ≥32 | Cast to a wider type before shifting |
| `runtime error: null pointer passed as argument` | UBSan; `memcpy(NULL, ...)` | Check the pointer before the call |

### Crashes with no sanitizer output

| Symptom | Usual cause |
|---|---|
| `Segmentation fault` | Dereferenced NULL or a wild pointer. Run `gdb`, then `bt` |
| `Bus error` | Misaligned access (common on ARM, rare on x86) |
| Silent wrong values that change between runs | Reading uninitialized memory. Run valgrind with `--track-origins=yes` |
| Works in debug, breaks with `-O2` | You have undefined behavior. UBSan will name it |
| Infinite loop | An unsigned counter going below zero, or a cursor that doesn't advance |
| Stack overflow / very deep `bt` | Infinite recursion — a tree with a cycle |

### Compile and link errors

See the table at the end of Part 6B.

## B. Complete byte layout reference

Keep this on a second monitor for Chapters 4–7.

```
PAGE (4096 bytes)
+----------------------------------------------------------+
| common header (6)                                        |
|   0: node_type      1 byte   0 = internal, 1 = leaf       |
|   1: is_root        1 byte                                |
|   2: parent         4 bytes  page number, big-endian      |
+----------------------------------------------------------+
| LEAF header (8)              | INTERNAL header (8)        |
|   6: num_cells      4 bytes  |   6: num_keys      4 bytes |
|  10: next_leaf      4 bytes  |  10: right_child   4 bytes |
+----------------------------------------------------------+
| body from offset 14                                       |
|                                                           |
| LEAF cells, 295 bytes each, max 13:                       |
|   key            4 bytes                                  |
|   value        291 bytes  (a serialized Row)              |
|                                                           |
| INTERNAL cells, 8 bytes each, max 510:                    |
|   child          4 bytes  page number                     |
|   key            4 bytes  separator                       |
+----------------------------------------------------------+

ROW (291 bytes) -- NOTE: sizeof(struct Row) is NOT 291. That's the point.
+----------------------------------------------------------+
|   0: id          4 bytes    big-endian uint32             |
|   4: username   32 bytes    UTF-8, zero-padded, no NUL    |
|  36: email     255 bytes    UTF-8, zero-padded, no NUL    |
+----------------------------------------------------------+

WAL RECORD (4116 bytes)
+----------------------------------------------------------+
|   0: length      4 bytes                                  |
|   4: lsn         8 bytes                                  |
|  12: page_num    4 bytes                                  |
|  16: page_image  4096 bytes                               |
| 4112: crc32      4 bytes  (over bytes 0..4111)            |
+----------------------------------------------------------+

Arithmetic to reproduce, not memorize:
  LEAF_SPACE      = 4096 - 14        = 4082
  LEAF_MAX_CELLS  = 4082 / 295       = 13   (187 bytes slack)
  INTERNAL_MAX    = 4082 / 8         = 510
  ROWS_PER_PAGE   = 4096 / 291       = 14   (Chapter 5 only, pre-B-tree)
```

## C. Glossary

**Access method** — the strategy for getting at rows (sequential scan, index seek).

**Alignment** — the requirement that a type live at an address divisible by its size.
Why you can't cast `page + 6` to `uint32_t*`.

**Arena** — an allocator that hands out chunks of one big block and frees them all at
once. Used for the AST and the plan tree.

**AST** — a tree representing parsed code with syntax noise discarded.

**B+ tree** — a balanced tree where all data lives in chained leaves and each node is one
page. The standard database index.

**Blocking operator** — one that must consume all input before producing output (`Sort`).
Contrast streaming (`Filter`).

**Buffer pool** — the in-memory cache of pages. Your `Pager`.

**Catalog** — metadata about tables, stored inside the database itself.

**Cell** — one key-value pair inside a node.

**Checkpoint** — flushing all dirty pages and truncating the WAL.

**Cursor** — a position within the data (page number plus cell index) that can advance.

**Dirty page** — modified in memory, not yet written to disk.

**`fsync`** — the system call that forces buffered writes to physical storage. Slow, and
non-optional for durability. Not the same as `fflush`.

**Idempotent** — running it twice has the same effect as once. Required of recovery.

**Iterator (Volcano) model** — query execution as a tree of `next()`-pulling operators.

**LSN** — log sequence number; the monotonically increasing id of a WAL record.

**Padding** — bytes the compiler inserts between struct fields for alignment. Why you
never `fwrite` a struct.

**Page** — the fixed-size unit of I/O. 4096 bytes here.

**Recursive descent** — a parser with one function per grammar rule.

**Split** — dividing a full node into two and promoting a separator key to the parent.

**String view** — a `(pointer, length)` pair into an existing buffer, with no copy and no
NUL. Your tokens.

**Tagged union** — a `struct` holding an enum tag plus a `union` payload. C's version of a
sum type; your AST.

**Three-valued logic** — SQL's `TRUE`/`FALSE`/`UNKNOWN`, arising from NULL.

**Tombstone** — a marker that a record is deleted, left in place rather than removed.

**Undefined behavior (UB)** — code the standard places no requirements on. May work today
and break tomorrow. Caught by UBSan.

**vtable** — a struct of function pointers, giving C polymorphism. Your operators.

**WAL** — an append-only log written and `fsync`ed before pages are modified, enabling
crash recovery.

## D. A README worth writing

When you're done, this repo is something you'll show people.

```markdown
# MiniDB

A single-file SQL database engine written from scratch in C, built to
understand how databases work internally.

![CI](https://github.com/<you>/minidb/actions/workflows/ci.yml/badge.svg)

## What it does
CREATE TABLE / INSERT / SELECT with WHERE, ORDER BY, LIMIT / UPDATE / DELETE,
persisted to a single file, indexed by a B+ tree, with crash recovery via a
write-ahead log. ~4000 lines of C17, no dependencies.

Tested under gcc and clang with AddressSanitizer, UndefinedBehaviorSanitizer,
and valgrind on every commit.

## Architecture
[the layer diagram]

## Design decisions
- Rows are serialized field-by-field rather than by writing structs. Why: ...
- Rows never straddle page boundaries. Why: ...
- The AST and query plan are arena-allocated. Why: ...
- Tokens are string views into the source rather than copies. Why: ...
- Deletion does not merge underfull nodes (as in SQLite). Why: ...
- Physical (page-image) WAL rather than logical. Why: ...

## What I'd do differently
[be honest — this section is the one that impresses people]

## Building
make test     # build and run the test suite under sanitizers
make run      # start the REPL
```

**The "design decisions" section is the most valuable part of the repo**, and "what I'd do
differently" is what an interviewer will actually ask about. Anyone can produce code;
explaining a trade-off you made and its cost is what distinguishes an engineer.

## E. If you get truly stuck on a chapter

In rough order of what to try:

1. **Re-read the "why" section of the chapter.** Most implementation confusion is
   actually unclear requirements. If you can't say in one sentence what the code should
   do, no amount of typing will help.
2. **Delete your attempt and restart from the skeleton.** Genuinely. A half-built wrong
   design is harder to fix than an empty file, and you'll rebuild it in a fraction of the
   time with what you now know. `git switch -c attempt-2` costs nothing.
3. **Write a smaller version first, with no bytes and no pages.** Can't do the B+ tree
   split? Write it for an in-memory `int` array with `malloc`ed nodes and normal struct
   fields. Get the *algorithm* right in isolation, then port it to the page layout.
   Solving the algorithm and the serialization simultaneously is two problems; separating
   them is one and then one. **This is the single most effective technique in this list.**
4. **Read cstack's corresponding part.** It's the same project in C, so you can read the
   code directly. Doing this *after* you've struggled is completely different from doing
   it first — the struggle is what makes it stick.
5. **Skip the chapter and come back.** Chapters 9 and 10 don't strictly require a perfect
   Chapter 7. A working-but-imperfect tree is enough to keep moving, and momentum matters
   more than perfection on a long project.
6. **Take the simpler design.** Every chapter has a "you could do the hard version" note.
   Take the easy version, write down *why* in the README, and move on. Shipping the
   simple thing and documenting the trade-off is what a working engineer does under a
   deadline — it is not a compromise of this project's goals, it *is* one of them.


---

# Part 9 — Questions you're going to have

These are the places people actually get stuck on this project in C, phrased the way
you'd phrase them at 11pm. Find yours, read the answer, keep going.

## Setup and tooling

**`implicit declaration of function 'getline'` — but I included `<stdio.h>`.**
`getline` is POSIX, not ISO C, and `-std=c17` hides it. Add
`-D_POSIX_C_SOURCE=200809L` to your `CFLAGS` (the Makefile in Part 2 already does).
The same applies to `fmemopen`, `open_memstream`, `fsync`, `mkstemp`, and
`strncasecmp`. This is the single most common setup error in a C project like this and
the error message gives no hint at the cause.

**`undefined reference to 'row_serialize'` — but it's right there in row.h.**
That's a *linker* error, not a compiler error, and it always means the same thing: the
declaration exists, the definition doesn't. Either you haven't written the function body
yet, or `row.c` isn't in the build. Check that your Makefile's `find src -name '*.c'`
is actually picking it up — `make` with no arguments and read the compile line.

**`Makefile:12: *** missing separator. Stop.`**
You have spaces where a tab should be. Every recipe line under a target must start with a
literal tab character. Configure your editor to preserve tabs in Makefiles; most editors
helpfully convert them and break your build.

**My editor shows red squiggles everywhere but it compiles fine.**
Your editor doesn't know your include paths. Generate a compilation database:
```bash
bear -- make clean all
```
That writes `compile_commands.json`, which clangd reads. In C this is a bigger quality of
life improvement than in most languages — you get real autocomplete, jump-to-definition,
and live errors instead of guesses.

**Should I use CMake instead of Make?**
Not for this. A 30-line Makefile you understand beats a 30-line `CMakeLists.txt` you
copied, and you learn what a build actually does. CMake earns its place with multiple
platforms, external dependencies, and IDE generation — none of which you have.

**Do I really need the sanitizers on all the time? They slow things down.**
Yes, and the slowdown is irrelevant here — your test suite runs in under a second either
way. They convert C's worst property (silent memory corruption) into a precise error
message with a line number. Turning them off to save 200 milliseconds is trading your
most valuable debugging tool for nothing. Build without them only when you're
benchmarking, or in the valgrind CI job where they conflict.

**Do I really need to write the test first? It feels backwards.**
It feels backwards for about two weeks. The concrete payoff, which you'll notice around
Chapter 4: writing the test first forces you to decide the function's *interface* before
you're distracted by the implementation. Test-first is really design-first. The other
payoff — much bigger in C than elsewhere — is that a memory bug found by a test you ran
two minutes ago lives in twenty lines of code, not four thousand.

## Chapter 1 — REPL

**My test hangs forever.**
Your loop isn't terminating on end-of-input. `getline` returns `-1` at EOF. If you're
checking `if (getline(...) == 0)` or ignoring the return entirely, you loop forever on
`-1`. Note that `getline` returns `ssize_t` (signed) — assigning it to a `size_t` makes
`-1` become a huge positive number and the comparison silently never fires. That's the
unsigned trap from Part 6B, in the wild.

**I run the REPL and nothing appears — it looks frozen.**
Output is buffered. You printed the prompt but it's sitting in the buffer while you block
on input. Add `fflush(out)` right after printing the prompt. Most common Chapter 1 bug.

**`.exit` isn't matching even though I typed it exactly.**
`getline` **keeps the trailing newline**. You're comparing `".exit\n"` to `".exit"`. Strip
it first. Second most common Chapter 1 bug, and it's invisible in a `printf` because the
newline just looks like the end of the line.

**ASan reports a leak from `getline`.**
`getline` allocates the buffer and you own it. Free it **once**, after the loop, not
inside it — and pass the *same* `&line, &cap` on every call so it reuses and grows the
buffer instead of allocating a fresh one each time. If you're freeing inside the loop you
must also reset `line = NULL; cap = 0;` or the next call reads a dangling pointer.

**Why can't `repl_run` just use `stdin` directly? Everyone does that.**
Because then the only way to test it is to run the program and read the screen. Two
`FILE*` parameters cost one line and buy you every test in the chapter. And `fmemopen` /
`open_memstream` make faking streams genuinely easy — arguably easier than the equivalent
in most languages.

**`open_memstream`'s buffer is empty / garbage when I read it.**
You must `fclose` (or at least `fflush`) the stream before the buffer pointer and length
are valid. And the buffer is yours — `free` it. Both of those are in the man page's
first paragraph, which is a good argument for reading man pages.

**Why an enum instead of returning a bool?**
Because `return true` gives the reader nothing — continue? success? handled? By Chapter 8
you'll want a third outcome (an error that shouldn't exit) and a bool can't express it.
The enum is honest from the start and costs nothing.

## Chapter 2 — Tokenizer

**Why not just use a regex?**
C has no built-in regex (POSIX `regex.h` exists and is clunky), so this barely comes up —
but more importantly, writing the state machine by hand is the point. You're building a
tokenizer to understand tokenizers.

**My tokenizer loops forever.**
There's a path through the main loop that doesn't call `advance()`. The usual culprit is
the default case: you hit an unexpected character, don't consume it, and spin. Every
iteration must consume at least one character or exit.

**`selection` is tokenizing as `SELECT` followed by `ion`.**
You're matching keywords by prefix. Always scan the *full* identifier first (letters,
digits, underscores), *then* look up the resulting string in the keyword table.

**My token's `start` pointer points at garbage after the function returns.**
Your tokens point into the source string, and something freed or overwrote it. This is
the lifetime dependency the string-view design creates. Either keep the source alive as
long as the tokens (correct, and what the design intends — document it), or switch to
`strdup`ing each lexeme (simpler, costs an allocation and a free per token). ASan reports
this as `heap-use-after-free` and names where it was freed.

**`printf("%s", token.start)` prints the entire rest of the query.**
Token lexemes are **not NUL-terminated** — they're a pointer plus a length into the
source. `%s` runs until it finds a NUL, which is the end of the whole string. Use:
```c
printf("%.*s", (int)token.length, token.start);
```
`%.*s` takes the length as an argument before the pointer. This will confuse you exactly
once.

**`isdigit(c)` is behaving strangely, or UBSan complains.**
The `<ctype.h>` functions take an `int` that must be representable as `unsigned char` or
`EOF`. Passing a plain `char` that's negative (any byte ≥ 128, in a UTF-8 string) is
undefined behavior. Always cast: `isdigit((unsigned char)c)`.

**My token array corrupts memory once it gets long.**
Your `realloc` growth is wrong. Common causes: growing by a fixed amount instead of
doubling and getting the boundary wrong; forgetting to update `capacity`; or the classic
`foo = realloc(foo, n)` which leaks `foo` if realloc returns NULL. Use a temp variable.
The 10,000-token test in the test list exists precisely to catch this, and ASan will
point at the exact line.

**Should keywords be case-insensitive? Should identifiers?**
SQL keywords are case-insensitive. Identifiers vary by database — PostgreSQL folds to
lowercase, SQLite is case-insensitive for ASCII. Pick one, write it in a comment, and
write a test. Making an explicit call on an ambiguous spec is the skill; which call you
make matters much less.

## Chapter 3 — Parser

**My parser recurses forever and segfaults with a huge stack trace.**
You have left recursion. A rule like `expression -> expression "+" term` calls
`expression()` first with nothing consumed. Recursive descent cannot handle it — rewrite
the rule to consume something first, typically as a loop. The grammar in the chapter
avoids this; watch for it if you extend the grammar.

Note that C has no `StackOverflowError` — infinite recursion is just a segfault. `gdb`
then `bt` will show you thousands of identical frames, which is the signature.

**`match` vs `expect` — I keep confusing them.**
`match(TYPE)` = "consume it *if* it's there, tell me whether it was." Use for optional
parts: `if (match(p, TOKEN_WHERE)) { where = expression(p); }`.
`expect(TYPE, msg)` = "it must be there; consume it or record an error." Use for required
parts.
Using `match` where you needed `expect` means malformed input silently parses into a
wrong tree instead of erroring — much worse than a crash, because it fails later
somewhere unrelated.

**Do I have to check for NULL after every single sub-parse call?**
Yes, and it's verbose, and that's the price of having no exceptions. Don't hide it behind
a macro that contains a hidden `return` — that's a real readability cost and it makes the
control flow invisible to anyone reading the function, including you in three weeks.
Explicit propagation is ugly and debuggable, which is the right trade in C.

**Why an arena? Can't I just `free` the tree recursively?**
You can, and writing `expr_free` that walks the tree is a reasonable exercise. But then
every early return on a parse error must free the partial tree it built so far, which is
where the leaks actually come from — the error paths, not the happy path. The arena makes
error paths free (one `arena_free` no matter how far you got) and makes leaks structurally
impossible. This is what real compilers do, and the test list's "a parse error still leaks
nothing" is where you feel the payoff.

**How do I even write an arena? It sounds advanced.**
It's about 30 lines and it's the least advanced allocator there is:
```c
void *arena_alloc(Arena *a, size_t size) {
    size_t aligned = (a->used + _Alignof(max_align_t) - 1)
                   & ~(_Alignof(max_align_t) - 1);   /* round up */
    if (aligned + size > a->capacity) return NULL;   /* or grow with a new block */
    void *p = a->buf + aligned;
    a->used = aligned + size;
    return p;
}
```
Bump a pointer, return the old value. `arena_free` is one `free` of the whole block. The
only subtlety is the alignment rounding, which is the two lines above.

**How much SQL should I support?**
Less than you think. `SELECT` with optional `WHERE`, and `INSERT`. That's it for Chapter
3. Every additional statement form is more parser surface with no new learning.

**Should the parser check that the table exists?**
No. That's a *semantic* check needing the catalog, which is Chapter 8. The parser's job is
shape, not meaning. This line is genuinely tempting to cross and keeping it clean is one
of the main things the project teaches.

## Chapter 4 — Serialization

**`sizeof(Row)` is 296, not 291. Did I do something wrong?**
No — that's struct padding, and discovering it here is the whole lesson. The compiler
inserts bytes so each field lands on a suitable boundary. It's also exactly why you must
never `fwrite(&row, sizeof row, 1, f)`: the padding is uninitialized garbage, its size
differs between compilers, and your file format would be unportable and
non-deterministic. Serialize field by field. Use `offsetof` if you want to see where the
padding went.

**My round-trip test fails but the strings look identical when I print them.**
Trailing NUL padding. `"bob"` and `"bob\0\0\0..."` print identically and are not equal.
Hex-dump both. Then fix your read function to scan for the first zero byte and copy only
up to there.

**`strcmp` says two identical-looking strings differ.**
Same cause as above, or your read function never NUL-terminated the destination at all,
so `strcmp` is reading past the end into garbage. ASan will flag the overread. Make
`read_fixed_str` always write `out[len] = '\0'`.

**Why big-endian? Does it matter?**
For correctness, no, as long as you're consistent. For everything else, yes: big-endian
means a hex dump reads left-to-right in the order you'd write the number, so
`00 00 00 2a` is visibly 42. Little-endian shows `2a 00 00 00`, which is correct and
harder to eyeball at 11pm. SQLite is big-endian for the same reason.

**Why not just use `htonl`?**
It works, but it's POSIX-only, the name says "network" rather than "file format," and
it's an extra header for eight lines of shifting you'll write once. The explicit version
is clearer and completely portable. Either is defensible.

**UBSan complains about a shift in my `write_u32_be`.**
You're shifting an `in[0]` that promoted to `int`, and `in[0] << 24` can reach the sign
bit, which is undefined for signed types. Cast first: `((uint32_t)in[0] << 24)`. The
version in Part 6 has the casts for exactly this reason.

**A 32-character username throws, but I only typed 30 characters.**
You typed 30 *characters* that encode to more than 32 *bytes* — accented or non-Latin
characters are multi-byte in UTF-8. That's correct behavior. In C this is actually easier
to get right than in most languages, because `strlen` already gives you bytes rather than
characters.

**291 bytes per row feels enormously wasteful for `{1, "bo", "b@x"}`.**
It is. That's the deal you're making for fixed-offset arithmetic. Real databases use
variable-length encoding with length prefixes, which Chapter 8 discusses. Take the simple
version first; the constraint is what makes Chapters 5 through 7 tractable.

## Chapter 5 — Pager

**I opened my database and all the data is gone.**
You used `fopen(path, "w+b")`. **`"w"` truncates the file to zero length immediately**,
before you read a byte. Use `"r+b"` for an existing file, and fall back to `"w+b"` only
when the open fails because the file doesn't exist. The "opening an existing file does
not truncate it" test exists specifically to catch this, and it's worth writing before
you need it.

**Data isn't persisting. I write, restart, and it's gone.**
Check in order: (1) did you call `pager_flush` or `pager_close`? (2) did you
`pager_mark_dirty` the page you modified? (3) does `pager_flush` actually write the dirty
set, or is it still a TODO? (4) did the mode string truncate your file (above)? Add a
`printf` of the file size after close — if it's 0, nothing was written.

**I modified a page but the change didn't survive the flush.**
You forgot `pager_mark_dirty`. This will happen repeatedly and the failure is silent. Two
defenses: make every mutation go through a function that marks dirty automatically, and
write a "write, close, reopen, verify" test for every new kind of page mutation.

**Why does `pager_get_page(3)` return the same pointer every time?**
Because it's a cache, and that's the design. Callers mutate the page in place, and the
pager writes that same block to disk on flush. If it returned a copy, mutations would
vanish. This aliasing is intentional — but it's surprising, so it deserves a comment in
the header.

**ASan says double-free in `pager_close`.**
Something else freed a page the pager returned. `pager_get_page` returns a **borrowed**
pointer; only the pager frees it. Find the stray `free` — ASan's report shows both free
sites, so this is a 30-second fix once you read it.

**`fread` returned fewer bytes than I asked for. Is that an error?**
Usually not. A short read at end-of-file is completely normal in a growing database — it's
how you learn the page doesn't exist on disk yet. Check `feof()` versus `ferror()` to tell
them apart, and zero-fill the remainder on EOF. Ignoring `fread`'s return value entirely
is a real bug that shows up as garbage in pages near the end of the file.

**Why `calloc` and not `malloc` for pages?**
Because a page past end-of-file must read as zeros, and `malloc` gives you whatever was
in that memory before. Uninitialized reads are the nastiest class of C bug — the behavior
changes between runs and between debug and release builds, so it looks like magic. `calloc`
costs nothing here and removes the entire category.

**Why the `(long)` cast in `fseek`?**
`page_num * PAGE_SIZE` in 32-bit arithmetic overflows at page 524,288, which is only a
2GB file — you'd get a negative offset and a confusing error. Note also that `fseek` takes
a `long`, which is 32 bits on some platforms; `fseeko` with `off_t` is the fix for files
over 2GB. Note the limit in a comment. You won't hit it; knowing it exists is the point.

**Why 4096 and not 512 or 65536?**
4096 matches the typical OS memory page and the size the filesystem is optimized to move.
Smaller means more I/O operations for the same data; larger means reading bytes you don't
need. It's the near-universal default, including SQLite's.

**A fixed `pages[1024]` array feels like cheating. Shouldn't I use a hash map?**
Not yet. Direct indexing has no hashing, no collisions, and is trivially debuggable, and
it caps you at a 4MB database which is fine for now. **Note the limit in a comment.**
Replacing it with an LRU cache that evicts is a well-scoped, genuinely interesting stretch
goal — but doing it now means debugging eviction and B+ trees simultaneously.

**My tests pass individually but fail when run together.**
Shared state. Either a global is carrying over, or two tests are using the same file path.
Give each test a unique temp file via `mkstemp` and `remove()` it at the end. Never write
to a fixed name like `"test.db"` — that's the root cause about 80% of the time.

## Chapter 6 — Leaf nodes

**Should I parse the page into a struct, or read the bytes directly?**
Read the bytes directly, through accessor functions. **The page bytes are the truth.** If
you deserialize into a struct and write it back later, you have two sources of truth and
they will diverge after a flush, in a way that only shows up on reopen. C makes the
byte-view approach natural in a way struct-oriented languages don't — this is one of the
places the language is genuinely a better fit for the problem.

**Can I just cast? `uint32_t *n = (uint32_t *)(page + 6);`**
No, and this one matters here specifically. `page + 6` is not 4-byte aligned, so that cast
is undefined behavior. It works on x86 — which is worse than crashing, because it works
on your laptop and traps on ARM. UBSan reports `load of misaligned address`. Use the
byte-at-a-time `read_u32_be` helper, which sidesteps alignment entirely and handles
endianness at the same time.

**My binary search loops forever.**
Your `low`/`high` update isn't making progress on some case — usually `high = mid` where
it should be `mid - 1` on one branch, or `while (low <= high)` paired with an update that
can leave the range unchanged. Write it on paper for a 3-element array. Better: write the
tests for `find_index` on sizes 0, 1, 2, and 3 *before* the implementation and let them
drive you.

**What exactly should `find_index` return for an absent key?**
The index where it *would* go — the position of the first key greater than the target.
That way one function serves find (check `key_at(result) == key`), insert (shift from
`result` right), and delete. Two separate search functions means two off-by-one bugs
instead of one.

**My cells get corrupted when I insert in the middle.**
You used `memcpy` for the shift. **Source and destination overlap, and `memcpy` with
overlap is undefined behavior** — it will usually appear to work, which is worse. Use
`memmove`. This is exactly what it exists for, and it's one of the most common real C
bugs in this whole project.

**ASan says heap-buffer-overflow when I insert the 13th cell.**
Off-by-one in your cell offset or shift length. Check `LEAF_MAX_CELLS` (should be 13, and
`static_assert`ed), and check that your shift length is
`(num_cells - index) * CELL_SIZE`, not `(num_cells - index + 1)`. The report tells you
how many bytes past the block you went, which usually identifies the wrong term directly.

**How do I even see what's in the page?**
Hex-dump it. That's what `hex_dump` is for. Dump bytes 0–40 and you should be able to
point at the node type, the cell count, and the first key. If you can't, your layout
constants are wrong.

**`LEAF_MAX_CELLS` of 13 seems oddly small.**
It is — 291-byte rows are fat. A real database with 20-byte rows fits ~200 cells per leaf.
Small is actually *good* for you: you can trigger a split with 14 inserts instead of 200,
which makes Chapter 7's tests fast and readable.

**Why store `next_leaf` when nothing uses it yet?**
Chapter 7's cursor uses it, and Chapter 12's sequential scan depends on it entirely.
Adding a header field later means every page already on disk has the wrong layout — a
format migration, for a project with no migration tooling. Reserve the space now.

## Chapter 7 — Splits (the hard one)

**Where do I put the 14th cell while I'm splitting? The page is full.**
A temporary buffer: copy the 13 existing cells out, insert the new one in sorted position,
then deal 7 to the old page and 7 to the new one. `uint8_t temp[(LEAF_MAX_CELLS + 1) *
LEAF_CELL_SIZE]` is about 4KB on the stack, which is fine (default stack is ~8MB).
Splitting in place without a temp buffer is possible and is a nightmare. Take the buffer.

**Why does splitting the root need *two* new pages?**
Because page 0 must remain the root forever — your `ROOT_PAGE` constant and every parent
pointer depend on it. So you can't make page 0 the left child. Instead: copy page 0's
contents to new page A, put the other half in new page B, then overwrite page 0 as an
internal node with children A and B. The tree grows at the top, which is exactly why B+
trees stay balanced without any rebalancing logic.

**After an internal split, some lookups return nothing.**
Two likely causes. (1) **Parent pointers**: children that moved to the new internal node
still record the old parent. (2) Your separator convention is inconsistent — "left holds
keys < separator" in the split code and "left holds keys <= separator" in the search code.
Pick one, write it in a comment above both functions, and verify both match.

**How do I test internal-node splits without inserting 500,000 rows?**
Make the max a variable rather than a constant — a non-`const` global with a default,
declared `extern` in the header. Tests set it to 3 and force splits with a dozen inserts.
**Reset it at the end of every test that changes it**, or you've created exactly the
shared global state that makes tests order-dependent. Making a rare code path cheap to
reach is a legitimate technique, not a hack.

**Segfault with a `bt` showing thousands of identical frames.**
Infinite recursion: your tree has a cycle. A child pointer points at itself or an
ancestor — usually a page number written as 0 (meaning "none") and then followed anyway,
or a parent pointer set to the wrong page during a split. Print the tree with
`btree_dump`; the cycle will be obvious.

**`btree_dump` looks right but tests still fail.**
Then the bug is in the *bytes*, not the structure — a value that round-trips wrong, or a
cell offset that's right for keys and wrong for values. Make `btree_dump` print values,
not just keys, for one small tree.

**How long should this chapter take? I've been stuck for a week.**
A week is normal. Two to three weeks of evenings is the realistic budget, and it's the
chapter most people abandon the project on. Three things help: (1) get the leaf split
completely green before touching internal nodes at all; (2) spend twenty minutes at
<https://www.cs.usfca.edu/~galles/visualization/BPlusTree.html> inserting keys and
watching splits — the animation makes the code make sense in a way reading doesn't;
(3) **write the split algorithm first for an in-memory tree of `malloc`ed structs with
normal fields, no pages and no bytes.** Get the algorithm right, then port it to the page
layout. Solving the algorithm and the serialization at the same time is two problems.

**Should `leaf_*` and `internal_*` share a vtable?**
Try it on a branch. They share the common header, which argues for sharing. They differ in
almost everything else, which argues against. My honest read: share the common-header
helpers (already done in `node.h`) and leave the bodies separate. But run the experiment —
`git switch -c try-node-vtable`, do it, look at the result, keep or revert.

## Chapters 8–12

**Changing `Row` to be schema-driven broke 40 tests. Did I do something wrong?**
No — this is what a real refactor feels like, and your test suite is doing its job. Don't
try to get all 40 green at once. Change the production code, then fix tests file by file,
committing after each. Add a test-only `Row *test_row(uint32_t, const char*, const char*)`
helper so most tests need a one-word change instead of a rewrite.

**Should I do variable-length rows in Chapter 8?**
Probably not, and that's a legitimate engineering decision rather than a cop-out. Slot
arrays and variable cells are a big change with a lot of new bug surface, and the
learning-per-hour is lower than in Chapters 6, 7, and 11. Keep fixed widths, make them
*schema-driven*, write down in the README that you chose not to and why. It's a
well-scoped follow-up project if you want it later.

**`WHERE id = NULL` returns nothing. Is that a bug?**
No, that's correct SQL and one of the most surprising things about the language. `NULL`
means "unknown," so `x = NULL` evaluates to `UNKNOWN`, not `TRUE`, and `WHERE` keeps rows
only on `TRUE`. Matching nulls needs `IS NULL`, a separate operator. Implementing this
once means you'll never be caught by it in real SQL again.

**Should deletion merge underfull nodes?**
Real SQLite doesn't. Merging requires borrowing from siblings, merging with siblings, and
propagating underflow up the tree recursively — substantially harder than splitting, for a
benefit most workloads don't notice. Shift-left within the leaf, don't merge, document the
decision. Do it as a stretch goal on a branch if you want the challenge.

**How do I test crash recovery when I can't actually kill the process?**
A function-pointer hook in the pager's write path that a test can set to fail after N
writes. Loop N from 1 to the number of writes in a split, reopening and recovering each
time. **Always reset the hook to NULL at the end of the test** — it's a global. That's a
proper fault-injection harness and building one is a genuinely senior move.

**Isn't `fwrite` already durable? Why do I need `fsync`?**
No, and there are *three* layers, not two. `fwrite` puts bytes in the C library's buffer.
`fflush` moves them to the operating system. Only `fsync` pushes them to the physical
device. If the machine loses power, anything short of `fsync` is gone. That gap —
microseconds versus milliseconds — is the entire reason write-ahead logging exists.

**Why separate the planner from the executor? My executor could just walk the AST.**
It could, and for `SELECT * FROM t` there'd be no difference. The difference appears when
`WHERE id = 3` should use an index seek and `WHERE name = 'bob'` should scan. That choice
is a *plan*, distinct from what the user wrote, and having a place to put it is what makes
SQL declarative. Even with one rule, having it in the right architectural place teaches
you more than ten rules in the wrong one.

**Function pointers make my head hurt. Do I have to?**
For Chapter 12, yes, and it's worth pushing through — seeing that "objects" are just a
struct plus a table of functions is genuinely clarifying about every OO language you'll
ever use. Start by writing one operator with a hardcoded `switch (op->type)` in `next`,
get it working, *then* convert to the vtable. Feeling the switch duplicate itself across
`open`/`next`/`close` is what makes the vtable make sense.

## Memory and crashes, in general

**I have a segfault and no idea where.**
```bash
gdb --args ./build/tests
(gdb) run
(gdb) bt
```
Thirty seconds, and `bt` shows you the exact line. People avoid the debugger for years and
it costs them enormously. Learn it in week one.

**ASan says heap-use-after-free but I never called free there.**
Read the *second* stack trace in the report — ASan shows where the memory was freed, not
just where it was used. That's usually the actual bug: something freed a pointer another
piece of code still holds. This is an ownership question, and the fix is deciding who owns
it and writing that down.

**I get different results on different runs.**
Uninitialized memory. `malloc` without initializing, or a struct field you never set.
ASan doesn't catch all of these; valgrind does:
```bash
make SAN= clean test
valgrind --track-origins=yes ./build/tests
```
`--track-origins=yes` tells you where the uninitialized value came from, which is usually
the entire answer.

**It works with `-O0` but breaks with `-O2`.**
You have undefined behavior. The optimizer is allowed to assume UB never happens, so it
optimizes on that assumption and your code changes meaning. This is not the compiler being
buggy — it's your code being wrong in a way that was previously invisible. UBSan will name
it.

**Valgrind reports leaks but ASan didn't.**
Different heuristics, and ASan's leak check only runs on a clean exit. If a test calls
`exit()` or aborts, ASan may not report. Make every path exit normally.

**How much should I worry about memory bugs?**
Much less than you'd think, *given the setup in Part 2*. Sanitizers plus a fast test suite
means a memory bug is found within two minutes of being written and lives in the twenty
lines you just typed. The people who find C miserable are debugging bugs that are months
old in code they didn't write. That's not your situation.

## Process and morale

**Was C the wrong choice? This is harder than I expected.**
It's harder in the first two chapters and roughly equal after that. What you're paying
for in Chapters 1–3 (manual strings, manual memory, no exceptions) you get back in
Chapters 4–7, where every other language would have you fighting a serialization layer to
do what C lets you say directly. And you can read cstack's tutorial and SQLite's actual
source as peers rather than translations. Stick with it through Chapter 5 before judging.

**How do I know if my design is any good?**
Three practical signals. (1) *Is it easy to test?* If you need 40 lines of setup, the unit
does too much. (2) *Does one change touch one file?* If adding a column type means editing
six files, the concept is smeared across the codebase. (3) *Can you explain a file's job in
one sentence without "and"?* "The pager reads and writes pages" is good. "The pager reads
pages and parses rows and tracks the schema" is three files.

A C-specific fourth: *can you say who owns every pointer that crosses a function
boundary?* If not, that's the design problem, not a memory problem.

**I want to rewrite everything. Should I?**
Rewriting one *chapter's* code from the skeleton is often the fastest path forward and you
should do it freely — a half-built wrong design is harder to fix than an empty file.
Rewriting the whole project is almost always the wrong call; you'll rebuild Chapters 1–4
identically and lose two weeks. If the urge is strong, branch, rewrite one file, compare.

**My test suite takes 40 seconds and I've stopped running it.**
That's a real problem, because TDD's value is proportional to how often you run the tests.
The sanitizers cost ~2x, which shouldn't get you near 40 seconds — check whether you're
doing real disk I/O in a loop, or flushing on every insert. If a specific test is slow (the
1,000-row ones), that's acceptable; split the suite so you can run one file while
iterating.

**Should I use an LLM for this project at all?**
Not for the implementations, and that's the point of this guide — the struggle in Chapters
6 and 7 *is* the learning, and having it written for you converts a project you'd remember
into an afternoon you'd forget. Where an assistant is genuinely fine: explaining a
sanitizer report after you've tried reading it, or explaining a concept you've already
been stuck on for an hour. The line worth holding is that you write every line of MiniDB
yourself.

**How do I talk about this in an interview?**
Not "I built a database." Everyone says that. Pick one decision and go deep: why you
serialize field-by-field instead of writing structs, why the root split allocates two
pages, why the AST is arena-allocated, what `fwrite` versus `fsync` actually cost you.
Trade-offs you can defend are what distinguish you; features are not. That's why the
README has a "design decisions" section and a "what I'd do differently" section, and those
two are the most valuable things in the repo.

Being able to say "it's tested under two compilers with ASan, UBSan, and valgrind on every
commit" is also a genuinely strong signal for anyone hiring for systems work — it says you
know what C's failure modes are and you engineered around them deliberately.

**I've lost momentum and haven't touched it in three weeks.**
Normal, and recoverable. Don't restart the chapter — restart with the smallest possible
action: run `make test`, read the one failing test, fix only that. Momentum comes back
from a green bar, not from a plan. If the chapter genuinely feels too big, take the
simpler design it offers, write down why in the README, and move on. Shipping the simple
thing and documenting the trade-off is what working engineers do under deadline — it is
not a compromise of this project's goals, it is one of them.
