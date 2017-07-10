<template>
    <div class="app">
        <nav class="navbar">
            <button class="navbar-btn" id="scan" @click="scan">Scan</button>
            <button class="navbar-btn" id="shutdown" @click="shutdown">Shutdown</button>
            <button class="navbar-btn" id="restart" @click="restart">Restart</button>
            <div>Server : <bs-input type="input" :value.sync="rootUrl"></bs-input></div>
            <div>Controller : <bs-input type="input" :value.sync="streamIp"></bs-input></div>
            <button class="navbar-btn" id="begin" @click="begin">Start</button>
            <button class="navbar-btn" id="begin" @click="end">Stop</button>
        </nav>
        <div id="buttons" style="display:none;">
            <button class="btn-primary" name="connect" disabled="true"><i class="fa fa-link"></i>Connect</button>
            <button class="btn-primary" name="disconnect" disabled="true"><i class="fa fa-unlink"></i>Disconnect</button>
            <button class="btn-primary" name="chart" disabled="true"><i class="fa fa-area-chart"></i>Chart</button>
            <button class="btn-primary" name="view" disabled="true"><i class="fa fa-eye"></i>View</button>
            <button class="btn-primary" name="record" disabled="true"><i class="fa fa-save"></i>Record</button>
            <button class="btn-primary" name="cease" disabled="true"><i class="fa fa-stop"></i>Stop</button>
        </div>
        <div class="plegcontroller">
            <bs-tabset v-ref:tabset>  
                <bs-tab header="Configuration" v-ref:tab-config>
                    <div class="container-fluid ui-resizable">
                        <div class="row">
                            <div>
                                <h4>Sources</h4>
                                <basiclistview v-ref:sources 
                                    :selection-changed="sourceChanged"
                                    :enable-buttons-callback="sourceButtons"
                                    :icons="{Video:'eye',HeartRate:'heartbeat',BatteryLevel:'battery-half',Offset:'tachometer'}"
                                    >
                                </basiclistview>
                            </div>
                        </div>
                        <div class="row">
                            <div>
                                <h4>Files&nbsp;<i class="fa fa-refresh" @click="refreshFiles"></i></h4>
                                <bs-input :value.sync="newFileName"></bs-input>&nbsp;<i class="fa fa-plus-square" @click="addFile"></i>&nbsp;<i class="fa fa-folder-o" @click="addDir"></i>
                                <basiclistview v-ref:files 
                                    :selection-changed="filesChanged"
                                    :enable-buttons-callback="filesButtons"
                                    :icons="{file:'file',directory:'folder-open'}"
                                    >
                                </basiclistview>
                            </div>
                        </div>
                        <div class="row">
                            <div>
                                <h4>Lsof&nbsp;<i class="fa fa-refresh" @click="refreshLsof"></i></h4>
                                <basiclistview v-ref:lsof 
                                    :selection-changed="lsofChanged"
                                    :enable-buttons-callback="lsofButtons"
                                    :icons="{file:'file',directory:'folder-open'}"
                                    >
                                </basiclistview>
                            </div>
                        </div>
                        <div class="row">
                            <div>
                                <h4>Configuration<i class="fa fa-refresh" @click="refreshConfig"></i></h4>
                                <bs-select v-ref:configFiles close-on-select
                                    :value.sync="configFile"
                                    :options="['devices.json','gstreamer.json','files.json']"
                                    >
                                </bs-select>
                                <basiclistview v-ref:config 
                                    :selection-changed="configChanged"
                                    :enable-buttons-callback="configButtons"
                                    :icons="{source:'tachometer',device:'cog',pipe:'pied-piper'}"
                                    >
                                </basiclistview>
                            </div>
                        </div>
                        <div class="row">
                            <div>
                                <h4>Log / Details</h4>
                                <log-detail v-ref:log>
                            </div>
                        </div>
                    </div>
                </bs-tab>
                <bs-tab header="Charts" v-ref:tab-charts>
                    <div class="row" id="charts"></div>
                </bs-tab>
                <bs-tab header="Videos" v-ref:tab-videos>
                    <div class="row" id="videos"></div>
                </bs-tab>
            </bs-tabset>
        </div>
        <div id="statusText"><p></p></div>
    </div>
</template>
<script>
export default {
    props : {
        selection : {
            type: Object,
            "default": function() { return {}; }
        },
        chartSources : {
            type: Object,
            "default": function() { return {}; }
        },
        serverStatus : {
            type: Object,
            "default": function() { return {}; }
        },
        statusText : {
            type: String,
            "default" : "",
        },
        sources : {
            type: Array,
            "default" : function() { return []; }
        },
        configs : {
            type: Object,
            "default": function() { return {}; }
        },
        hInterval : {
            type: Number,
            "default" : 0
        },
        rootUrl : {
            type: String,
            "default" : "http://localhost:4999",
        },
        streamIp : {
            type: String,
            "default" : "127.0.0.1",
        },
        charts : {
            type: Object,
            "default": function() { return {}; }
        },
        configFile : {
            type: String,
            "default": "devices.json",
            twoWay: true,
        },
        newFileName : {
            type: String,
            "default": "foo.txt",
            twoWay: true,
        },
    },
    watch : {
        statusText : function(text) {
            this.setStatusText(text);
        },
        configFile : function(value) {
            this.fetchConfig(value);                
        },
    },
    events : {
        stop : function(source) {
            this.chartSources[source] = false;
            this.charts[source] = null;
        },
    },
    methods : {        
        addFile : function() {
            var url = "/touch/"+this.newFileName;
            if(this.$refs.files.selection.type == "directory"){
                url += "?r="+(this.$refs.files.selection.path?this.$refs.files.selection.path+"/":"")+this.$refs.files.selection.name;
            }
            var that = this;
            this.http({
                method:"POST",
                url:url
            }).then(function(data){
                that.refreshFiles();
            });
        },
        addDir : function() {
            var url = "/mkdir/"+this.newFileName;
            if(this.$refs.files.selection.type == "directory"){
                url += "?r="+this.$refs.files.selection.path+"/"+this.$refs.files.selection.name;
            }
            var that = this;
            this.http({
                method:"POST",
                url:url
            }).then(function(data){
                that.refreshFiles();
            });
        },
        refreshFiles : function() {
            this.fetchFiles();
        },
        refreshLsof : function() {
            this.fetchLsof();
        },
        refreshConfig : function() {
            this.fetchConfig(this.configFile);
        },
        cloneButtons : function(elem,$list) {
            $(this.$el).find("#buttons").clone().appendTo(elem).show();
            var that = this;
            elem.find("button").each(function(i,button){
                $(button).click(function(evt){
                    that[evt.target.name].call(that,$list.selection);
                });
            });
        },
        chart : function(selection) {
            var chart = $("<div></div>").appendTo($(this.$el).find("#charts"));
            var id = chart.uniqueId().attr("id");
            var Chart = this.$options.components['chart'];
            var chart = new Chart({
                el: "#"+id,
                parent: this,
            });
            this.charts[selection.name] = chart;
            this.chartSources[selection.name] = true;
            this.selectTab("tabCharts");
        },
        view : function(selection) {
            var that = this;
            if(selection.type.toLowerCase() == "video"){
//                var port = this.nextPort();
//                var p;
//                if(this.serverStatus.threads._find(function(o){
//                    if(o.type=="gstreamer"){
//                        return o.jobs._find(function(o,p){
//                            return p == selection.name+".tee";
//                        });
//                    }
//                    return false;
//                })){
//                    p = $.Deferred().resolve({});
//                }else{
//                    p = this.http({
//                        method:"POST",
//                        url:"/tee/"+selection.name+"/"+this.streamIp+"/"+port,
//                    });
//                }
//                p.then(function(data){
//                    var video = $("<div></div>").appendTo($(that.$el).find("#videos").empty());
//                    var id = video.uniqueId().attr("id");
//                    var Video = that.$options.components['video'];
//                    var video = new Video({
//                        el: "#"+id,
//                        parent: that,
//                    });
//                    that.selectTab("tabVideos");
//                    this.$refs.tabset.select(this.$refs.tabVideos);
//                });
                var video = $("<div></div>").appendTo($(that.$el).find("#videos").empty());
                var id = video.uniqueId().attr("id");
                var Video = that.$options.components['video'];
                var video = new Video({
                    el: "#"+id,
                    parent: that,
                    data: { png: this.rootUrl + "/" + selection.name },
                });
                that.selectTab("tabVideos");
                this.$refs.tabset.select(this.$refs.tabVideos);
            }else{ 
                if(selection.name.split(".").pop() == 'mp4'){
                    var video = $("<div></div>").appendTo($(that.$el).find("#videos").empty());
                    var id = video.uniqueId().attr("id");
                    var Video = that.$options.components['video'];
                    var video = new Video({
                        el: "#"+id,
                        parent: that,
                        data: { url: this.rootUrl + selection.path + "/" + selection.name },
                    });
                    that.selectTab("tabVideos");
                    this.$refs.tabset.select(this.$refs.tabVideos);
                }else{
                    debugger;
                    this.http({
                        method:"GET",
                        url:"/file/"+selection.name+"?r="+selection.path,
                    }).then(function(data){
                        $("#videos").empty().append($("<p>"+data+"</p>"));
                        that.selectTab("tabVideos");
                    });
                }
            }
        },
        record : function(selection) {
            if(!selection || !selection.name
            || this.$refs.files.selection._empty() || !this.$refs.files.selection.name){
                alert("Please selet a file to record to...");
                return;
            }
            var that = this;
            this.http({
                method:"POST",
                url:"/write/"+this.$refs.sources.selection.name+"/"+this.$refs.files.selection.name,
            }).then(function(){
                that.refreshLsof();
            });          
        },
        cease : function(selection) {
            if(!selection || !selection.name
            || this.$refs.files.selection._empty() || !this.$refs.files.selection.name)
                return;
            var that = this;
            this.http({
                method:"POST",
                url:"/stop/"+this.$refs.sources.selection.name+"/"+this.$refs.files.selection.name,
            }).then(function(){
                that.refreshLsof();
            });          
        },
        fetchFiles : function() {
            this.$refs.files.clear();
            var that = this;
            this.http({
                method:"GET",
                url:"/dir",
            }).then(function(data){
                that.$refs.files.model = data;
                return data;
            });
        },
        fetchLsof : function() {
            this.$refs.lsof.clear();
            var that = this;
            this.http({
                method:"GET",
                url:"/lsof",
            }).then(function(data){
                var tree = [];
                for(var p in data){
                    var file = data[p];
                    tree.push({
                        type:"file",
                        name:p,
                        data:file,
                    });
                }
                that.$refs.lsof.model = tree;
                return tree;
            });
        },
        fetchConfig : function(file) {
            this.$refs.config.clear();
            var that = this;
            this.http({
                method:"PATCH",
                url:"/config/"+file,
            }).then(function(data){
                that.configs[file] = data;
                var tree = {};
                if(file=="devices.json"){
                    for(var d in data.devices){
                        var device = data.devices[d];
                        tree[d] = {
                            type:"device",
                            addr:device.addr,
                            name:device.name,
                            children:[],
                        };
                        for(var u in device.services){
                            var service = device.services[u];
                            tree[d].children.push({
                                type:"source",
                                device:device.addr,
                                uuid:service.uuid,
                                name:service.name,
                                source:service.source,
                                children:[],
                            });
                        }
                    }
                }else{
                    for(var p in data.pipes){
                        var pipe = data.pipes[p];
                        tree[p] = {
                            type:"pipe",
                            name:p,
                            pipeline:pipe.pipeline,
                            children:[],
                        };
                    }
                }
                that.$refs.config.model = tree;
                return tree;
            });
        },
        sourceChanged : function(selection) {
            if(!selection)return;
            var that = this;
            this.selection = selection;
        },
        sourceButtons : function($div,selection) {
            this.disableButtons($div);
            this.enableButton($div,"disconnect");
            var that = this;
            selection.actions.split(",").forEach(function(e){
                that.enableButton($div,e);
            });
        },
        filesChanged : function(selection) {
            this.selection = selection;
        },
        filesButtons : function($div,selection) {
            this.disableButtons($div);
            this.enableButton($div,"view");
        },
        lsofChanged : function(selection) {
            this.selection = selection;
        },        
        lsofButtons : function($div,selection) {
            this.disableButtons($div);
            this.enableButton($div,"cease");
        },
        configChanged : function(selection) {
            if(!selection)return;
            this.selection = selection;
        },
        configButtons : function($div,selection) {
            this.disableButtons($div);
            this.enableButton($div,"connect");
            var that = this;
            selection.actions && selection.actions.split(",").forEach(function(e){
                that.enableButton($div,e);
            });
        },
        disableButtons : function($div) {
            var that = this;
            ["connect","chart","view","record"].forEach(function(name){
                $div.find("[name=\""+name+"\"]").prop("disabled",true);
            });
        },
        enableButton : function($div,name) {
            $div.find("[name=\""+name+"\"]").prop("disabled",false).show();    
        },
        setStatusText : function(text){
            $(this.$el).find("#statusText").height($(this.$el).find("#statusText p").text(text).height());
            this.resize();
        },
        scan : function() {
            this.log("Scanning...");
            var that = this;
            this.http({
                url:"/scan",
            }).then(function(data){
                that.log("Scan finished.");
                that.log(JSON.stringify(data));
                that.$refs.config.model = data;
            });            
        },
        shutdown : function() {
            this.stop();
            this.log("Shutting down the server...");
            var that = this;
            this.http({
                url: "/shutdown",
                method: "PATCH",
            }).then(function(data){
                that.log(data);
            });
        },
        restart : function() {
            this.stop();
            this.log("Restarting the server...");
            var that = this;
            this.http({
                url: "/restart",
                method: "PATCH",
            }).then(function(data){
                that.log(data);
                that.start();
            });
        },
        connect : function() {
            var url = false;
            if(typeof this.selection == "string"){
                this.log(this.selection);
            }else if(typeof this.selection == "object"){
                switch(this.selection.type){
                case "device":
                    url = "/connect/"+this.selection.addr;
                    break;
                case "source":
                    url = "/connect/"+this.selection.device+"/"+this.selection.source;
                    break;
                case "pipe":
                    url = "/pipe/"+this.selection.name;
                }
            }
            var that = this;
            this.http({
                 url:url,
                 method:"PATCH",
            });
        },
        disconnect : function() {
            var that = this;
            this.http({
                 url:"/disconnect/"+this.$refs.sources.selection.name,
                 method:"PATCH",
            });
        },
        start : function() {
            this.hInterval = setInterval(binding(this,this.onTimer),1000);
        },
        stop : function () {
            this.hInterval && clearInterval(this.hInterval);
        },
        onTimer : function() {
            var that = this;
            this.status().then(function(connected){
                if(!connected)
                    return;
                if(that.charts._empty())
                    return;
                for(var s in that.chartSources){
                    if(that.charts[s]){
                        that.charts[s].chartSource(s);
                    }
                }
            });
        },
        status : function() {
            var that = this;
            return this.http({
                method:"PATCH",
                url:"/status",
                hdr:{"X-Quiet":"true"},
            }).then(function(data){
                that.updateStatus(data);
                return true;
            });
        },        
        updateStatus : function(status) {
            this.serverStatus = status;
            this.setStatusText(JSON.stringify(status));
            this.sources = status.sources;
            this.$refs.sources.model = status.sources;
            var that = this;
            if(status.log){
                status.log.forEach(function(l){
                    that.log(l);
                });
            }
        },
        log : function(text) {
            this.$refs.log.log(text);
        },
        http : function(obj) {
            var p = $.Deferred();
            var doc = new XMLHttpRequest();
            doc.onreadystatechange = function() {
                if (doc.readyState === XMLHttpRequest.DONE) {
                    try{
                        p.resolve(JSON.parse(doc.responseText));
                    }catch(e){
                        p.resolve(doc.responseText);
                    }
                }
            };
            if(obj.method === "PATCH"){
                doc.open("GET", this.rootUrl + obj.url);
                doc.setRequestHeader("X-Method","PATCH");
            }else{
                doc.open(obj.method || "GET", this.rootUrl + obj.url);
            }
            for(var i in obj.hdr){
                doc.setRequestHeader(i,obj.hdr[i]);
            }
            doc.send(obj.data || undefined);
            return p;            
        },
        begin : function () {
            this.stop();
            this.start();
            this.fetchConfig(this.configFile);
            this.fetchFiles(this.filesSelection);
        },        
        end : function() {
            this.stop();
        },
        nextPort : function() {
            if(!this.port){
                return this.port = 5000;
            }
            return ++this.port;
        },
        selectTab : function(tab) {
            this.$refs.tabset.select(this.$refs[tab]);
            this.resize();
        },
        resize : function() {
            $(".container-fluid,#charts,#videos").height($(".app").height()-$("#statusText").outerHeight(true)-$(".app>nav").outerHeight(true)-$(".plegcontroller ul:first-child").outerHeight(true));
            $(".plegcontroller .row > div").css("min-height",Math.floor($(".container-fluid").innerHeight() / 4)+"px");
        }
    },
    ready : function() {
        $(window).resize(binding(this,this.resize)).resize();
    }
}
</script>
<style>
.app { height:100%; }
.app nav { margin-bottom: 0px; }
.plegcontroller { border: 1px solid green; }
.container-fluid,#charts,#videos { overflow-y: scroll; }
#charts.container-fluid .row { height:109px; }  
.plegcontroller > div:first-child { height:100%; }
.plegcontroller .row { min-height:100px; padding-left:11px; padding-right:11px; }        
.plegcontroller .row > div { height:100%; }
.plegcontroller .row { border-radius:11px; border: 2px solid green; margin:4px; }
#statusText { min-height:100px; position:absolute; bottom:0px; padding:10px; }
.plegcontroller button { border: 0; background-color: green; color: yellow; }
</style>
