---
name: md-lint
description: Invoke when reviewing any .md file for formatting issues.
metadata:
  type: skill
  trigger: manual
---

# Markdown Linter

Check `.md` files. Report: file, line, issue → fix. Group by severity.

## Critical

| Rule | Check |
| --- | --- |
| MD001 | Skipped heading level (`##` → `####`) |
| MD040 | Code block without language tag |
| MD042 | Empty link (`[]()` or `[](#)`) |
| MD045 | Image without alt text |
| MD051 | Broken fragment link (`[text](#missing)`) |

## Warning

| Rule | Check |
| --- | --- |
| MD018 | No space after `#` in heading |
| MD019 | Multiple spaces after `#` in heading |
| MD022 | Heading not surrounded by blank lines |
| MD023 | Heading indented instead of at line start |
| MD024 | Duplicate heading text |
| MD025 | Multiple h1 in file |
| MD026 | Trailing punctuation in heading (`## Title.`) |
| MD032 | List not surrounded by blank lines |
| MD034 | Bare URL (should be `[text](url)` or `` `url` ``) |
| MD036 | Bold/italic used as heading instead of `##` |
| MD039 | Bare link text ("here", "link", "click here") |
| MD041 | First non-frontmatter line is not h1 |
| MD047 | File doesn't end with single newline |

## Info

| Rule | Check |
| --- | --- |
| MD004 | Inconsistent unordered list marker (`-` vs `*` vs `+`) |
| MD005 | Inconsistent list indent |
| MD009 | Trailing whitespace |
| MD012 | Multiple consecutive blank lines (>1) |
| MD060 | Table separator missing spaces (`|---|---|`→`| --- | --- |`) — custom rule |

## Content Checks (beyond markdownlint)

- **Paragraph >4 sentences.** Split long paragraphs.
- **Fillers.** very, just, actually, really, basically, note that.
- **CAPS emphasis.** Use **bold**, not ALL CAPS.
- **Empty section.** Heading immediately followed by another heading.
- **Broken relative link.** Target file doesn't exist in the repo.

## Output Format

```text
## Lint: <file>

### Critical (N)
- L<line>: <rule> <issue> → <fix>

### Warning (N)
- L<line>: <rule> <issue> → <fix>

### Info (N)
- L<line>: <rule> <issue> → <fix>

**Summary:** X critical, Y warning, Z info — clean / needs fixes / major rework
```

No issues: `## Lint: <file> — clean ✓`
