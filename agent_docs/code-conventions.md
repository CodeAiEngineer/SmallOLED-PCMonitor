# Code Conventions

## SOLID

- Single Responsibility: one class/function, one reason to change.
- Open/Closed: extend via new classes; do not modify existing ones.
- Liskov Substitution: subtypes must be drop-in replacements for base types.
- Interface Segregation: small, focused interfaces; no unused method dependencies.
- Dependency Inversion: depend on abstractions, inject concretions.

## General

- Parallelize everything that can be; sequential I/O is a bug, not a default.
- Performance is a feature: prefer async, batch, and concurrent patterns over simple but slow ones.
- No file exceeds 300 lines; split by responsibility if it does.
- Backend must expose functionality as API endpoints; no logic tied to a specific transport or UI.
- No magic numbers or strings; use named constants.
- Delete dead code; do not comment it out.
- Prefer pure functions; minimize side effects.
- Names must be descriptive; no abbreviations except universally known ones (id, url, ctx).
- Fail fast: validate inputs at boundaries, not deep inside logic.

## Python

- Router -> Service -> Repository; no business logic in routers.
- DB sessions injected via dependency injection; never created ad hoc.
- Raise specific exception types with descriptive messages.
- HTTP error responses use {"detail": "..."} format; no raw exception text.
- Every external service call has an explicit timeout.
- Use dataclasses or pydantic models instead of raw dicts for structured data.
- Prefer async/await for I/O-bound operations.
- Use asyncio.gather() for concurrent async calls; never await them sequentially if independent.
- Debug prints must use ANSI colors via colorama or rich.
- Critical prints must be wrapped with a separator: print("*" * 60) before and after.
- Add tests for complex additions when the test is short and does not cost tokens.

## TypeScript / React

- Functional components only; no class components.
- One component per file; filename matches component name.
- API calls live in src/services/; never fetch directly inside components.
- Fire independent API calls with Promise.all(); never await them sequentially.
- Every async operation handles both loading and error states explicitly.
- No inline styles.
- Prefer const over let; never use var.
- Use optional chaining (?.) and nullish coalescing (??) over manual null checks.
