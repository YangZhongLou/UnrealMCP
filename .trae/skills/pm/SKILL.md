---
name: pm
description: Project management skill. Invoke when planning projects, breaking down work, prioritizing tasks, tracking progress, or managing scope and stakeholders.
metadata:
  type: skill
  trigger: manual
---

# Project Manager

## Principles

- **Ship small, ship often.** The smallest releasable unit beats the perfect release. Slice work so something ships every iteration.
- **Value over volume.** Prioritize by impact, not by who asked loudest. Every task earns its place on the board.
- **Visible work.** Status lives in the tracker, not in DMs. Anyone can answer "what's the state of X" by looking.
- **Explicit scope.** Undefined scope is the fastest way to miss a date. Every item has a clear "done" condition.
- **Say no early.** Killing a bad idea in planning costs nothing. Killing it mid-sprint costs morale and momentum.

## Planning

1. **Define the goal.** One sentence. What changes for the user after this ships?
2. **List what's in.** User stories, tasks, enablers. Verb-first: "Add login", "Fix timeout", "Document API".
3. **List what's out.** Explicitly. Prevents scope creep. "Not in scope: admin dashboard, mobile app."
4. **Order by risk.** Risky/unknown items first. Known/mechanical items last.
5. **Estimate relatively.** T-shirt sizes (S/M/L/XL) or story points. Never hours — precision is fake at this stage.

## Milestones & Phases

Version scheme: `v<milestone>.<phase>`

| Level | Meaning | Duration | Example |
| --- | --- | --- | --- |
| Milestone | User-visible deliverable. Ships independently. | 2-6 weeks | `v1` — user auth + basic dashboard |
| Phase | Small, shippable increment within a milestone. | 3-7 days | `v1.1` — login page, `v1.2` — session persistence |

### Milestone Rules

- **One goal per milestone.** If the milestone description has "and", split it.
- **Every milestone ships.** No "internal only" milestones. If it doesn't reach users, it's not a milestone.
- **Lock scope at start.** New requests go to the next milestone, not this one.
- **Gate at end.** All tests pass, no P0/P1 bugs, stakeholder sign-off before moving on.

### Phase Structure

Each phase within a milestone:

```text
v<M>.<N>: <phase goal>
├── Tasks (<8, mostly S/M)
├── Exit criteria (testable, binary pass/fail)
└── Depends on: <previous phase or none>
```

### Phase Flow

1. **Plan.** Pick the next most valuable slice. Write tasks + exit criteria.
2. **Build.** Work through tasks. Update status daily.
3. **Verify.** Exit criteria pass? If not, fix before next phase.
4. **Close.** Mark phase complete. Retro if something broke the flow.

### Phase Exit Criteria

- **Binary, not fuzzy.** "All 12 unit tests pass" not "tests look good".
- **User-visible where possible.** "Login page renders and accepts credentials" not "auth module integrated".
- **Cumulative.** Each phase's criteria include the previous phase's (prevent regressions).

## Task Breakdown

- **One outcome per task.** If a task description has "and", split it.
- **<1 day per task.** If a task is bigger, it's under-specified. Break it further.
- **Clear done criteria.** Each task answers: "How do I know this is finished?"
- **Dependencies explicit.** "Blocked by #42" in the task, not in someone's head.

## Prioritization

| Priority | Meaning | Example |
| --- | --- | --- |
| P0 | Drop everything. User-visible breakage. | Login returns 500 |
| P1 | Must ship this iteration. Core path. | Checkout flow complete |
| P2 | Should ship. Important but not blocking. | Export to CSV |
| P3 | Nice to have. Ships if time allows. | Dark mode toggle |

MoSCoW when scope is loose: Must / Should / Could / Won't this round.

## Estimation

| Size | Effort | Confidence |
| --- | --- | --- |
| S | Hours | High — done it before, clear path |
| M | 1-2 days | Medium — clear approach, some unknowns |
| L | 3-5 days | Low — novel, external deps, or under-specified |
| XL | >5 days | None — must split before committing |

Never estimate in a vacuum. Reference a past task of similar size: "About the same as the password reset feature (M)."

## Progress Tracking

- **Daily:** What moved yesterday? What moves today? What's blocked?
- **Weekly:** Burndown/up. Are we trending toward the milestone? Adjust scope or dates, not both.
- **Blocker SLA:** P0 unblock within hours. P1 within 24h. Escalate if stalled.

## Risk Management

| Risk | Likelihood | Impact | Mitigation |
| --- | --- | --- | --- |
| <what could go wrong> | High/Med/Low | High/Med/Low | <concrete action now> |

Never log a risk without a mitigation. Risks without mitigations are just complaints.

## Stakeholder Update Template

```text
## Status: <project> — <date>

**Health:** 🟢 on track / 🟡 at risk / 🔴 blocked

**Since last update:**
- <done item>
- <done item>

**Next:**
- <upcoming item>
- <upcoming item>

**Risks:**
- <risk> → <mitigation>
```

## Anti-Patterns

- **Everything is P1.** If everything is urgent, nothing is. Force rank.
- **Scope sneaks in.** "While we're here..." additions without trading something out.
- **Status by surprise.** Stakeholders learn about delays in the update, not when the delay happened.
- **Hopium estimates.** "We'll figure it out" is not a plan. De-risk before committing.
- **Sunk-cost shipping.** "We spent 3 weeks on this so we must ship it." If it's wrong, cut it.
