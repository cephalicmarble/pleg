define("_d3",[
    "d3",
    "d3-axis",
    "d3-drag",
    "d3-path",
    "d3-scale",
    "d3-selection",
    "d3-shape",
    "d3-time",
    "d3-time-format",
    "d3-zoom",
    "simple-graph"
],function(d3,axis,drag,path,scale,selection,shape,time,timeFormat,zoom,simpleGraph){
    window.d3 = Object.assign(d3,axis,drag,path,scale,selection,shape,time,timeFormat,zoom);
    window.SimpleGraph = simpleGraph(d3);
    return window.d3;
});