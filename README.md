# iipImage
Containerized IIP with authorization

This version only requires that any authorization header is provided.

future plans include checking validity and permissions for routes given.

You can disable the header checks with -e CHECK_HEADER='no'

## building and running

docker build . -t iipauth
docker run iipauth -d -p 4010:4010

(alrenatively run.sh and run_nocheck.sh run with and without checks respectively)


## usage
(include a directory of slides for iip)
http://localhost:4010/fcgi-bin/iipsrv.fcgi?DeepZoom=(path to slide)
