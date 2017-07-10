<template>
    <div class="chart">
        <div style="float:right;"><button class="btn-primary" @click="stop"><a class="fa fa-close"></a></button></div>
    </div>
</template>
<script>
export default {
    props : {
        source : {
            type: String,
            "default": "",
        },
        graph : {
            type: Object,
            "default": function(){ return {}; },
        },
        datumTime : {
            type: Number,
            "default": 0,
        },
        metadata : {
            type: Object,
            "default": function(){ return {}; },
        }
    },
    events : {
        ready: function () {
        }
    },
    methods : {
        stop : function() {
            $(this.$el).remove();
            this.$parent.$emit("stop",this.source);
        },
        setSource : function(source) {
            this.source = source;
        },
        chartSource : function(source) {
            var that = this;
            if(!this.metadata[source]){
                this.$parent.http({
                    method:"PATCH",
                    url:"/meta/"+source,
                    hdr:{"Content-Type":"text/json"},
                }).then(function(data){
                    that.metadata[source] = data;
                    that.getChartData(source);
                });
            }else{
                this.getChartData(source);
            }
        },
        getChartData : function(source) {
            var that = this;
            this.$parent.http({
                url:"/"+source,
                hdr:{"Content-Type":"text/json"},
            }).then(function(data){
                for(var i=0;i<data.length;i++){
                    that.plotDataPoint(source,data[i]);
                }
            });
        },
        plotDataPoint : function(source,datum) {
            if(!this.datumTime)
                this.datumTime = datum.timestamp
            var xBounds = [0,60*5];
            for(var i=0;i<this.metadata[source].charts.length;i++){
                var chart = this.metadata[source].charts[i];
                for(var p in chart){
                    var name = chart[p];
                    var chart = this.graph[name];
                    if(!chart){
                        switch(p){
                        case "heart_rate":
                            chart = this.createSeries("Line",name,xBounds,[0,200]);
                            break;
                        case "rr_intervals":
                            chart = this.createSeries("Scatter",name,xBounds,[0,2000]);
                            break;
                        case "mock":
                            chart = this.createSeries("Line",name,xBounds,[0,100]);
                            break;
                        }
                    }
                    this.graph[name] = chart;
                    if(!datum[p])
                        continue;
                    if(datum[p].length){
                        chart.plot_point(datum.timestamp-this.datumTime,datum[p][0]);
                    }else{
                        chart.plot_point(datum.timestamp-this.datumTime,datum[p]);
                    }
                }
            }
        },
        createSeries : function(type,name,xBounds,yBounds) {
            var chart = $("<div></div>").appendTo($(this.$el));
            chart.height($(this.$parent.$el).find(".plegcontroller").height());
            chart.width($(this.$parent.$el).find(".plegcontroller").width());
            var id = chart.uniqueId().attr("id");
            var graph = new SimpleGraph(id, {
                "xmax": xBounds[1], "xmin": xBounds[0],
                "ymax": yBounds[1], "ymin": yBounds[0], 
                "title": name,
                "xlabel": "Time",
                "ylabel": name, 
            });
            return graph;
        }
    },
}
</script>
<style>
body { font: 13px sans-serif; }
rect { fill: #fff; }
ul {
  list-style-type: none;
  margin: 0.5em 0em 0.5em 0em;
  width: 100%; }
  ul li {
    vertical-align: middle;
    margin: 0em;
    padding: 0em 1em; }
.axis { font-size: 1.5em; }
.chart {
}
circle, .line {
  fill: none;
  stroke: steelblue;
  stroke-width: 2px; }
circle {
  fill: white;
  fill-opacity: 0.2;
  cursor: move; }
  circle.selected {
    fill: #ff7f0e;
    stroke: #ff7f0e; }
  circle:hover {
    fill: #ff7f0e;
    stroke: #707f0e; }
  circle.selected:hover {
    fill: #ff7f0e;
    stroke: #ff7f0e; }
</style>
