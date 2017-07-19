#!/bin/sh
until(./Server $@) do echo "Kicking..."; sleep 4 ; done
echo $?
