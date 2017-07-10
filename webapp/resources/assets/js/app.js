define("app",[],function(){

    window.bsComponents = {};
    for(var p in window.VueStrap){
        var n = p.toLowerCase();
        window.bsComponents["bs-"+n] = Vue.component("bs-"+n, window.VueStrap[p]);
    }

    window.Components = {};
    [
        "basiclistview",
        "collection",
        "log-detail",
        "controller",
        "chart",
        "video",
    ].forEach(function(e){
        window.Components[e] = window.Vue.component(e,require("./components/"+e+".vue"));
    });

    window.vm = new Vue({
        el:'body',
    });
    
//    $(function(){
//
//        var router = window.VueRouter;
//
//        var routerMap = {};
//        router.beforeEach( function (transition) {
//
//            $(".thunk-view").empty();
//
//            var path = transition.to.path;
//            if(path == '/' && window.location.hash == '#!/'){
//                transition.abort();
//                return;
//            }
//            if (!(path in routerMap)) {
//                Ajax({
//                    url:path,
//                    dataType:"html",
//                    async:false,
//                }).then(function(res){
//                    routerMap[path] = {
//                        component: {
//                            template : res,
////                            attached : function() {
////                                for(var t in window.viewTypes){
////                                    var rx = new RegExp("/\<"+t+"\>/");
////                                    if(rx.test(res)){ //detect interesting things
////                                        this.is_a_vue_view = true;
////                                    }
////                                }
////                            },
////                            detached : function() {
////                                if(this.is_a_vue_view){ //delete interesting things
////                                    delete routerMap[path];
////                                }
////                            }
//                        },
//                    }; 
//                    router.on(path, routerMap[path]);   //associate dynamically loaded component            
//
//                    transition.abort();     //abort current
//                    router.stop();                          
//                    router.start();         //restart router
//                    router.go(path);        //init new transition to the same path
//                });
//            } else {
//                transition.next();          //default action for already loaded content
//            }
//        });
//        
//        router.start(router,'body');        
//    });
});
