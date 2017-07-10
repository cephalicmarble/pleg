<template>
    <div class="log-detail">
        <div id="the_log" style="width:49.9%;height:100%;display:inline-block;"></div>
    </div>
</template>
<script>
export default {
    props : {
    },
    watch : {
    },
    methods : {
        log : function(what) {
            var _log = $(this.$el).find("#the_log");
            var text = $("<p>"+what+"</p>").appendTo(_log);
            if(_log.scrollTop()){
                text.scrollIntoView();
            }
        },
    },
    ready: function () {
    }
}
</script>
