---
name: programmer
description: Invoke when writing, debugging, or reviewing code. Covers TS/JS, Python, Go, Rust, Java/Kotlin, SQL, Shell.
metadata:
  type: skill
  trigger: manual
  languages: [typescript, javascript, python, go, rust, java, kotlin, sql, shell, bash]
---

# Programmer

## Principles

- **Simplicity.** Simplest solution first. No premature abstraction.
- **Correctness.** Working + readable > clever + fragile.
- **Safety.** No XSS, SQL injection, command injection, path traversal. Validate at boundaries. Use assertions for internal invariants.
- **No destruction.** Never delete repos, drop databases, `rm -rf`, or force push main without confirmation.
- **Minimal diffs.** Change only what's needed. No drive-by refactors.
- **No dead code.** Delete unused code. No commented-out blocks, `_unused` vars.

## Style

- **Naming.** Descriptive. Functions = verb, vars = noun. Abbreviate only universally (`idx`, `ctx`).
- **Comments.** Only explain WHY, never WHAT. Omit if the name suffices.
- **Functions.** Short, single-purpose. Boolean flags → two functions.
- **Errors.** Fail fast. Handle where you can act. Don't guard impossible paths.
- **Types.** Strongest available. Compile-time > runtime.

## Workflow

1. Read, then write. Check git blame for context.
2. Plan multi-file or architectural changes before coding.
3. One logical change per step. Keep it buildable.
4. Test happy path then edges: empty, bounds, errors, concurrency.
5. Run the app. Passing tests ≠ working feature.

## Debugging

- Reproduce first. Fix root cause, not symptom. One bug per commit. Add regression test.

## Refactoring

- Have a measurable goal. Never mix with features. Use automated tools first. Tests as safety net.

## Testing

- Test behavior, not implementation. One concept per test. Descriptive names (`it_returns_404_for_missing_user`).
- Wrap external deps in adapters; mock adapters, not the third-party code.
- Unit for logic, integration for wiring. Both needed.

## Code Review

Check: correctness, safety, clarity, simplicity, consistency, completeness (tests + errors + logging).

## Language Guides

### TS/JS
`const` > `let`, never `var`. `async/await` > promises. `===` unless `==` intended. Avoid `any`, prefer `unknown`.

### Python
PEP 8. Type hints everywhere. Dataclasses > dicts. Context managers for resources.

### Go
Errors explicit. Interfaces > inheritance. Clear goroutine lifecycle. `context.Context` for cancellation.

### Rust
Clippy. `Result`/`Option` > panic. `&str` for params, `String` for owned. Derive common traits.

### Java/Kotlin
Immutable by default. Composition > inheritance. Optional / nullable > null.

### SQL
Parameterized queries always. Explicit column lists. Indexes match queries. Transactions for multi-statement. JOINs over N+1.

### Shell
Quote expansions. `set -euo pipefail`. `[[ ]]` over `[ ]`. Long flags in scripts. Check exit codes.
