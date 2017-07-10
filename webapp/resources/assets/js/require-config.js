window.boot = function(dep){
    if(!(dep instanceof Array))
        dep = [dep];
    require.config({
        "paths":{
            "jquery": "/node_modules/jquery/dist/jquery.min",
            "jquery.ui.widget": "/node_modules/jquery-ui/ui/widget",
            "jquery.ui.unique": "/node_modules/jquery-ui/ui/unique-id",
            "jquery.ui.draggable": "/node_modules/jquery-ui/ui/widgets/draggable",
            "jquery.ui.droppable": "/node_modules/jquery-ui/ui/widgets/droppable",
            "jquery.ui.sortable": "/node_modules/jquery-ui/ui/widgets/sortable",
            "jquery.datetimepicker": "/tmp/vendor/jquery-datetimepicker/jquery.datetimepicker.full",
            "load-image": "/tmp/blueimp",
            "load-image-meta": '/tmp/vendor/load-image-meta',
            "load-image-exif": '/tmp/vendor/load-image-exif',
            "canvas-to-blob" : "/tmp/blueimp",
            "blueimp" : "/tmp/blueimp",
            "tmpl": "/tmp/blueimp-tmpl",
            "blueimp-helper": '/tmp/vendor/blueimp-gallery/blueimp-helper',
            "codemirror": '/node_modules/codemirror/lib/codemirror',
            "marble-frame": "/tmp/marble-frame",
            "moment":"/node_modules/moment/moment",
            "bootstrap":"/tmp/bootstrap",
            "extend":"/tmp/extend",
            "nodeextend":"/tmp/nodeextend",
            "ajax":"/tmp/ajax",
            "vendor/position":"/tmp/vendor/position",
            "vendor/json3":"/tmp/vendor/json3",
            "binding":"/tmp/binding",
            "lodash":"/node_modules/lodash/lodash",
            "vue":"/node_modules/vue/dist/vue",
            "vendor/iscroll":"/tmp/vendor/iscroll",
            "vue-resource":"/node_modules/vue-resource/dist/vue-resource",
            "jquery-mousewheel":"/tmp/vendor/jquery.mousewheel",
            "q":"/node_modules/q/q",
            "bootstrap-sass":"/node_modules/bootstrap-sass/assets/javascripts/bootstrap",
        },
        map: {
            '*': {
                "vendor/q":"q",
            },
        },
        deps:dep,
    });
};