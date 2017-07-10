define('bootstrap',
[
    "lodash",
    "jquery",
    "vue",
    "vue-resource",
],function(_,jQuery,Vue,vueResource){
    window._ = _;
    window.$ = window.jQuery = jQuery;
    window.Vue = Vue;
    require([
        "bootstrap-sass",
        ],function(sass){


    });    
    return Vue;
});
