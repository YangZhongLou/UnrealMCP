---
name: qa-engineer
description: Invoke when planning tests, designing test cases, or investigating quality issues.
metadata:
  type: skill
  trigger: manual
---

# QA Engineer

## Test Strategy

- **Risk-driven.** Test the most dangerous things first. Money, data loss, auth, privacy.
- **Pyramid, not ice cream.** Unit > Integration > E2E. 70/20/10 split.
- **Shift left.** Find bugs at the lowest-cost stage. Review designs, not just code.
- **Traceability.** Every test maps to a requirement or a known failure mode.

## Test Case Design

| Technique | When | Example |
| --- | --- | --- |
| Boundary value | Numeric inputs | `min-1`, `min`, `max`, `max+1` |
| Equivalence class | Partitionable input | Valid email formats → one test covers the class |
| Pairwise | Many interacting options | 5 fields × 3 values → don't test all 243 combos |
| Decision table | Business rules | Permissions matrix: role × action |
| State transition | Workflows, status machines | Order: pending → paid → shipped → delivered |
| Error guessing | Experience-based | Unicode in names, negative amounts, concurrent edits |

## Test Types

### Unit
- Fast, isolated, deterministic. One concept per test.
- `method_scenario_expected` naming. AAA structure.
- Mock adapters, not third-party code. Wrap external deps first.

### Integration
- Test real wiring between modules. Real database, real queue, real HTTP (or in-memory fakes).
- Focus on: serialization, schema mismatches, timeout/retry, partial failures.
- One happy path + each failure mode.

### E2E
- Critical user journeys only: signup, login, checkout, payment, delete account.
- Page object pattern for UI. API-level E2E when UI is unstable.
- Assert on user-visible outcomes, not DOM structure.

### Regression
- Record every production bug as a regression test. Bug → test → fix → verify.
- Full suite before every release. Smoke subset for every commit.

### Performance (when applicable)
- Set thresholds: p50 < 200ms, p99 < 1s. Fail the build if exceeded.
- Ramp tests, not spike. Find the breaking point, not just "it's fast."

## Bug Report

```text
Severity: <critical/high/medium/low>
Title: <component> <symptom> when <condition>

Steps:
1. <precise action>
2. <precise action>

Expected: <what should happen>
Actual:   <what actually happens>
Notes:    <env, branch, commit, logs, screenshots>
```

No "it doesn't work." No "sometimes." Be specific enough that anyone can reproduce.

## Quality Gates

- [ ] All tests pass (unit, integration, e2e smoke)
- [ ] No critical or high-severity open bugs
- [ ] Regression suite green
- [ ] New code has corresponding tests
- [ ] Coverage on changed files ≥ baseline

## Anti-Patterns

- **Testing the framework.** Don't verify that libraries work.
- **Flaky tests.** Fix them immediately or quarantine. A flaky test is worse than no test.
- **Test duplication.** Same behavior tested at multiple layers → pick the lowest layer that covers it.
- **Over-mocking.** If your test is 80% mocks, it tests mocks, not code.
- **Ice cream cone.** More E2E than unit tests. Slow, flaky, hard to debug.
