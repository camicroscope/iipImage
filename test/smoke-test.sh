#!/usr/bin/env bash
#
# smoke-test.sh <image-tag> - functional and security regression tests for
# the iipsrv Docker image. There's no unit test suite for the underlying C++
# server, so this builds/runs the real container and exercises it over HTTP,
# the same way SECURITY_REVIEW.md's fixes were verified by hand.
#
# Usage: test/smoke-test.sh <image-tag>
# Must be run from the repo root (paths below are relative to it).

set -uo pipefail

IMAGE="${1:?usage: smoke-test.sh <image-tag>}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT=18080
CONTAINER=iip-smoke-test
FAIL=0

cleanup() {
  docker rm -f "$CONTAINER" "$CONTAINER-noprefix" "$CONTAINER-optout" >/dev/null 2>&1
}
trap cleanup EXIT

fail() { echo "FAIL: $1"; FAIL=1; }
ok()   { echo "OK: $1"; }

### Test 1-4: normal server behind FILESYSTEM_PREFIX ###

docker rm -f "$CONTAINER" >/dev/null 2>&1
docker run -d --name "$CONTAINER" -p "$PORT:8080" \
  -v "$REPO_ROOT/images:/images:ro" "$IMAGE" >/dev/null

echo "Waiting for iipsrv to come up..."
for i in $(seq 1 30); do
  if curl -sf "http://localhost:$PORT/fcgi-bin/iipsrv.fcgi?FIF=/CMU-1-Small-Region.svs&obj=IIP,1.0" >/dev/null 2>&1; then
    break
  fi
  sleep 2
done

resp=$(curl -s "http://localhost:$PORT/fcgi-bin/iipsrv.fcgi?FIF=/CMU-1-Small-Region.svs&obj=IIP,1.0")
resp="${resp%$'\r'}"
if [[ "$resp" == "IIP:1.0" ]]; then
  ok "basic IIP handshake"
else
  fail "basic IIP handshake returned unexpected body: '$resp'"
fi

tile_type=$(curl -s -o /tmp/smoketest_tile.jpg -w '%{content_type}' \
  "http://localhost:$PORT/fcgi-bin/iipsrv.fcgi?FIF=/CMU-1-Small-Region.svs&JTL=0,0")
if [[ "$tile_type" == "image/jpeg" ]] && [[ -s /tmp/smoketest_tile.jpg ]]; then
  ok "JTL tile request returns a JPEG tile"
else
  fail "JTL tile request did not return a JPEG (content-type: $tile_type)"
fi

# Exercises the patched resampling paths in Transforms.cc (CVE-2021-46389 backport).
resize_type=$(curl -s -o /tmp/smoketest_resize.jpg -w '%{content_type}' \
  "http://localhost:$PORT/fcgi-bin/iipsrv.fcgi?FIF=/CMU-1-Small-Region.svs&WID=800&CVT=jpeg")
if [[ "$resize_type" == "image/jpeg" ]] && [[ -s /tmp/smoketest_resize.jpg ]]; then
  ok "resized CVT request returns a JPEG (Transforms.cc regression check)"
else
  fail "resized CVT request did not return a JPEG (content-type: $resize_type)"
fi

# The actual CVE class the FILESYSTEM_PREFIX jail defends against: an absolute
# path must NOT escape the jail and return the real host file.
passwd_body=$(curl -s "http://localhost:$PORT/fcgi-bin/iipsrv.fcgi?FIF=/etc/passwd&obj=IIP,1.0")
if [[ "$passwd_body" != *"root:"* ]]; then
  ok "absolute-path escape attempt (FIF=/etc/passwd) did not return real file contents"
else
  fail "absolute-path escape attempt returned real /etc/passwd contents -- FILESYSTEM_PREFIX jail is broken"
fi

docker rm -f "$CONTAINER" >/dev/null 2>&1

### Test 5: fail-closed when FILESYSTEM_PREFIX is unset ###

docker rm -f "$CONTAINER-noprefix" >/dev/null 2>&1
docker run -d --name "$CONTAINER-noprefix" \
  --entrypoint /var/www/localhost/fcgi-bin/iipsrv.fcgi \
  -e LOGFILE=/tmp/iipsrv.log \
  "$IMAGE" --bind /tmp/iipsrv.sock >/dev/null
# A correctly fail-closed server should exit almost immediately. If it's
# still running after a few seconds, it didn't fail closed -- don't block
# on `docker wait` forever (it would hang the whole test/CI run instead).
sleep 3
if [[ "$(docker inspect -f '{{.State.Running}}' "$CONTAINER-noprefix" 2>/dev/null)" == "true" ]]; then
  exit_code=0  # still running = did not fail closed
else
  exit_code="$(docker inspect -f '{{.State.ExitCode}}' "$CONTAINER-noprefix" 2>/dev/null || echo 0)"
fi
# The app logs to a LOGFILE inside the container via ofstream, not to
# stdout/stderr, so `docker logs` never sees it -- pull the file out instead.
log=$(docker cp "$CONTAINER-noprefix:/tmp/iipsrv.log" - 2>/dev/null | tar -xO 2>/dev/null || true)

if [[ $exit_code -ne 0 ]] && [[ "$log" == *"FATAL"*"FILESYSTEM_PREFIX"* ]]; then
  ok "server refuses to start when FILESYSTEM_PREFIX is unset (exit $exit_code)"
else
  fail "server did not fail closed when FILESYSTEM_PREFIX was unset (exit $exit_code)"
fi
docker rm -f "$CONTAINER-noprefix" >/dev/null 2>&1

### Test 6: explicit opt-out still allows startup ###

docker rm -f "$CONTAINER-optout" >/dev/null 2>&1
docker run -d --name "$CONTAINER-optout" \
  --entrypoint /var/www/localhost/fcgi-bin/iipsrv.fcgi \
  -e LOGFILE=/tmp/iipsrv.log -e ALLOW_UNJAILED_FILESYSTEM=true \
  "$IMAGE" --bind /tmp/iipsrv.sock >/dev/null
sleep 2
log=$(docker cp "$CONTAINER-optout:/tmp/iipsrv.log" - 2>/dev/null | tar -xO 2>/dev/null || true)
if [[ "$log" == *"Initialisation Complete"* ]]; then
  ok "ALLOW_UNJAILED_FILESYSTEM=true explicitly allows startup without a prefix"
else
  fail "ALLOW_UNJAILED_FILESYSTEM=true did not allow startup as expected"
fi
docker rm -f "$CONTAINER-optout" >/dev/null 2>&1

if [[ $FAIL -eq 0 ]]; then
  echo "All smoke tests passed."
else
  echo "One or more smoke tests failed."
fi
exit $FAIL
