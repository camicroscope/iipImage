# iipImage
Containerized IIP

## building and running

docker build . -t iipsrv

docker run iipsrv -d -p 4010:80

## usage
(include a directory of slides for iip)
http://localhost:4010/fcgi-bin/iipsrv.fcgi?DeepZoom=(path to slide)
