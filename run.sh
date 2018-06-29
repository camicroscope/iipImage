#!/usr/bin/env bash
docker build . -t iipauth
docker run iipauth -d -p 4010:4010 -v images:/images
