#!/usr/bin/env bash
#
# check-hardening.sh - guards against regressing on the security fixes made
# in SECURITY_REVIEW.md (2026-07-30). Run locally before pushing, or from CI.

set -uo pipefail

FAIL=0

# CVE-2021-46389 backport: dataLength must stay a size_t, not regress to int.
if ! grep -q 'size_t dataLength;' iipsrv/src/RawTile.h; then
  echo "FAIL: iipsrv/src/RawTile.h :: dataLength is no longer size_t (CVE-2021-46389 regression)."
  FAIL=1
fi

if ! grep -q 'if( !buffer )' iipsrv/src/TileManager.cc; then
  echo "FAIL: iipsrv/src/TileManager.cc :: crop() no longer null-checks its malloc() result."
  FAIL=1
fi

# FILESYSTEM_PREFIX must fail closed: Main.cc must still refuse to start unjailed.
if ! grep -q 'ALLOW_UNJAILED_FILESYSTEM' iipsrv/src/Main.cc; then
  echo "FAIL: iipsrv/src/Main.cc :: the FILESYSTEM_PREFIX fail-closed check is missing."
  FAIL=1
fi

# Shipped configs must either jail iipsrv under a non-empty FILESYSTEM_PREFIX,
# or opt out of the jail explicitly via ALLOW_UNJAILED_FILESYSTEM. What must
# never happen is an empty prefix with no opt-out: that is the silent-unjail
# regression this check exists to catch. Main.cc enforces the same rule at
# startup; this keeps the shipped config honest about which mode it is in.
for conf in fcgid.conf apache2-iipsrv-fcgid.conf; do
  if grep -qE '^\s*FcgidInitialEnv\s+FILESYSTEM_PREFIX\s+"[^"]+"' "$conf"; then
    continue
  fi
  if grep -qE '^\s*FcgidInitialEnv\s+ALLOW_UNJAILED_FILESYSTEM\s+"[^"]+"' "$conf"; then
    echo "NOTE: $conf :: unjailed by explicit ALLOW_UNJAILED_FILESYSTEM opt-out."
    continue
  fi
  echo "FAIL: $conf :: neither a non-empty FILESYSTEM_PREFIX nor an explicit ALLOW_UNJAILED_FILESYSTEM opt-out is set."
  FAIL=1
done

# Base image must be pinned by digest, not a mutable tag like ':latest'.
if grep -qE '^FROM\s+\S+:latest\s*$' Dockerfile; then
  echo "FAIL: Dockerfile :: base image uses a mutable ':latest' tag instead of a pinned digest."
  FAIL=1
fi
if ! grep -qE '^FROM\s+\S+@sha256:[0-9a-f]{64}' Dockerfile; then
  echo "FAIL: Dockerfile :: base image is not pinned by digest (expected 'FROM ...@sha256:<digest>')."
  FAIL=1
fi

# Compiler hardening flags must remain in configure.in.
if ! grep -q '_FORTIFY_SOURCE' iipsrv/configure.in; then
  echo "FAIL: iipsrv/configure.in :: compiler hardening flags (_FORTIFY_SOURCE etc.) are missing."
  FAIL=1
fi

if [[ $FAIL -eq 0 ]]; then
  echo "OK: hardening checks passed."
fi

exit $FAIL
