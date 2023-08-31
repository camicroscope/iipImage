# iipImage
Containerized IIP

## building and running

Unless using caMicroscope Distro Docker, [BFBridge](https://github.com/camicroscope/BFBridge) needs to be cloned and placed next to iipsrv/, so that this project's root iipimage/ has subfolders iipimage/ and BFBridge/

docker build . -t iipsrv

docker run iipsrv -d -p 4010:80

## usage
(include a directory of slides for iip)
http://localhost:4010/fcgi-bin/iipsrv.fcgi?DeepZoom=(path to slide)
