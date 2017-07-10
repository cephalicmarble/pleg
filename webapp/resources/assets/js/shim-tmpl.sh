#!/bin/sh
res="resources/assets/js/"
test -f "${res}blueimp-tmpl.js" && rm "${res}blueimp-tmpl.js"

req=""
comma=""
for i in $@ ; do req="$req$comma\"$i\"" ; comma="," ; done
cat "${res}shim.in" | sed -re "s/%REQUIREMENTS%/$req/g" >> "${res}blueimp-tmpl.js"

cat "node_modules/blueimp-tmpl/js/tmpl.js" >> "${res}blueimp-tmpl.js"

echo >> "${res}blueimp-tmpl.js"
echo "return $.tmpl;}));" >> "${res}blueimp-tmpl.js"
