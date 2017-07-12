#!/bin/sh
TOTAL=0; 
for i in "$(find . -iname "*.cpp" -or -iname "*.h" | grep -v node_modules | grep -v CMakeFiles | grep -v vendor | grep -v cmake-build-debug 2>&1) $(find webapp -iname "*.js" -or -iname "*.vue" -or -iname "*.html" | grep -v node_modules | grep -v vendor | grep -v public | grep -v webpack 2>&1)" ; do 
	TOTAL=$(($TOTAL + $(wc -l $i | tee /dev/stderr | cut -d\  -f1))) ; 
done ; 
echo $TOTAL
