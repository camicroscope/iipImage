# iipImage
Containerized IIP

## building and running

docker build . -t iipauth
docker run iipauth -d -p 4010:80

(alrenatively run.sh)


## usage
(include a directory of slides for iip)
http://localhost:4010/fcgi-bin/iipsrv.fcgi?DeepZoom=(path to slide)
