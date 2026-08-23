# Branching Strategy

This project uses Git Flow.

Branches:

- `master` - production only
- `develop` - integration branch
- `feature/*` - branch off `develop` for feature work
- `fix/*` - branch off `develop` for fixes
- `release/*` - branch off `develop` for staging before merging to `master`
- `hotfix/*` - branch off `master` for emergency fixes
