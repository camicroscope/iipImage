#!/usr/bin/env bash
docker build . -t iipauth
docker run -d -p 4010:4010 -v "$(pwd)"/images/:/images/ iipauth
