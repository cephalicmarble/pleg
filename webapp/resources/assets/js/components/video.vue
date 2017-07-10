<template>
    <div class="plegvideo">
        <div v-if="png" id="the_image"></div>
        <video v-else class="" controls preload="none" width="640" height="264" data-setup="{}">
            <source v-bind:src="url" type='video/mp4'></source>
            <a v-bind:href="url">Not working video.</a>
        </video>
    </div>
</template>
<script>
export default {
    ready: function () {
        if(this.png)
            this.refresh();
    },
    methods : {
        refresh : function() {
            var that = this;
            var i = new Image();
            i.src = this.png+"?c="+(new Date()).getMilliseconds();
            i.onload = function(){
                $(that.$el).find("#the_image").replaceWith($("<img src='"+i.src+"' id='the_image' width='640' height='480'/>"));
                $(that.$el).find("#the_image").bind("click",function(){
                    that.refresh();
                });
                setTimeout(function(){
                    that.refresh();
                },1400);
            }
        }
    }
}
</script>

