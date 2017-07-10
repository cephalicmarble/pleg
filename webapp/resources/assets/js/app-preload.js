define('app-preload',[
    "jquery",
    "moment",
    "./bootstrap",
    "./extend",
    "./nodeextend",
    "./ajax",
//    "./vendor/q",
    "position",
    "json3",
    "./binding",
    "vue-strap",
    "vue-router",
    "jquery.ui.uniqueid",
    "clipboard",
    "jquery.ui.resizable",
    "video",
    ],function(jquery,moment,Vue,ex,nex,ajax/*,q*/
        ,position,json3,binding,strap,VueRouter,uniq,clipboard,resizable,video)
{
    
    var jQuery = window.$ = window.jQuery = jquery;

    window.moment = moment;
    window.binding = binding;
    window.Vue = Vue;
    window.VueRouter = new VueRouter();
    window.VueStrap = strap;

    Vue.use(VueRouter);

});
