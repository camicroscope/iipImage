# iipImage
Containerized IIP

## building and running

Unless using caMicroscope Distro Docker, [BFBridge](https://github.com/camicroscope/BFBridge) needs to be cloned and placed next to iipsrv/, so that this project's root iipimage/ has subfolders iipimage/ and BFBridge/

docker build . -t iipsrv

docker run iipsrv -d -p 4010:80

## usage
(include a directory of slides for iip)
http://localhost:4010/fcgi-bin/iipsrv.fcgi?DeepZoom=(path to slide)

## Security

iipsrv has **no authentication or authorization of its own** — it is a bare
FastCGI responder that trusts every request it receives. It must always run
behind a reverse proxy (or, as in caMicroscope Distro, behind the `back`
service) that authenticates callers before ever forwarding a request here.
**Never publish this service's port directly to a host or the public
internet.** In Distro, this is enforced by never giving the `iip` container a
`ports:` mapping; if you deploy this image yourself, don't add one either.

`FILESYSTEM_PREFIX` must be set to the directory containing your images (see
`fcgid.conf`). Since a security review on 2026-07-30, iipsrv refuses to start
if it's unset, since an empty prefix would otherwise let `FIF=`/IIIF requests
read any file the process has permission to open. See `SECURITY_REVIEW.md`
for the full review.

## Testing

There's no C++ unit test suite; instead `test/smoke-test.sh <image-tag>`
builds/runs the real container and checks it over HTTP (normal tile
requests, the `FILESYSTEM_PREFIX` jail, and the fail-closed startup check).
`./check-hardening.sh` guards against regressing on the fixes in
`SECURITY_REVIEW.md`. Both run in CI on every push/PR
(`.github/workflows/test.yml`).

```
docker build -t iip-test .
./test/smoke-test.sh iip-test
./check-hardening.sh
```
