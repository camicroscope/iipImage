#!/usr/bin/env bash
docker build . -t iipsrv
docker run -d -p 80:4010 -v "$(pwd)"/images/:/images/ iipsrv
