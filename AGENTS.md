# Commit conventions

- Follow the existing commit subject style: a lowercase prefix such as `add:`,
  `fix:`, `change:`, `doc:`, or `release:`, followed by a short imperative summary.
- Add a commit body when useful to explain the change and its motivation.
- Include a `Signed-off-by` trailer using `git commit -s`.
- Include changelog entries in the same commit as the actual change, not in the
  version-bump commit.
- Keep version bumps in a separate commit touching only `CMakeLists.txt`, with
  the subject `release: bump version`.
- Run the repository's pre-commit checks and include any formatting fixes before
  committing.
