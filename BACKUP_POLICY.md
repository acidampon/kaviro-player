# KAVIRO Backup Policy

Every recovery milestone must have:
1. A protected recovery branch.
2. A separate backup branch created from the verified milestone.
3. A local compressed source snapshot with SHA-256.
4. A recovery note recording exactly what was verified.

Previous recovery and backup branches must not be deleted when starting the next milestone.
