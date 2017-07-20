#!/bin/sh
PREFIX=/usr/local
SYSTEMD=/usr/lib/systemd/system

cp gmrender-resurrect/gmediarenderer $PREFIX/bin
cp gmrender-resurrect/gmrender.service $SYSTEMD/.

