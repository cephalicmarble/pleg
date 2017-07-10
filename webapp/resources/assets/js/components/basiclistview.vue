<template>
    <div class="basiclistview"
        >
        <div class="border">
            <ul name="list" @click="clicked">            
            </ul>
            <div id="the_detail"></div>
        </div>
    </div>
</template>
<script>
export default {
    props : {
        _model : {
            type : Object,
            "default" : function(){ return {name:{name:"name",children:[],prop:"value"}}; },
        },
        list_html : {
            type : String,
            "default" : "",
        },
        index : {
            type : Object,
            "default": null,
        },
        selection : {
            type : Object,
            "default" : null,
        },
        selectionChanged : {
            type: Function,
            "default" : function(what) {
                alert(what);
            }
        },
        enableButtonsCallback : {
            type: Function,
            "default" : function(){}
        },
        icons : {
            type: Object,
            "default": function(){ return {}; }
        }
    },
    computed : {
        selectedIndex : {
            get : function() {
                return this.getSelectedIndex();
            },
            set : function(_index){
                this.setSelectedIndex(_index);
            },
        },
        model : {
            get : function(){ 
                return this.getModel();
            },
            set : function(model){
                this.setModel(model);
            },
        }
    },
    methods : {
        getSelectedIndex : function() {
            return $(this.$el).find(".selected").attr("index");            
        },
        setSelectedIndex : function(_index) {
            if(!this.index[_index])
                return;
            $(this.$el).find("ul,li").removeClass("selected");
            $(this.$el).find("[index=\""+_index+"\"]").addClass("selected");
            this.detail(this.index[_index]);
        },
        getModel : function() {
            return this._model;
        },
        setModel : function(_model) {
            this._model = _model;
            var index = this.getSelectedIndex();
            this.index = {};
            $(this.$el).find("#the_detail").empty();
            if(_model.children && _model.children.length && _model.name && _model.name.match){
                this.list_html = this.nest(_model,_model.name,1);
            }else{
                this.list(_model);
            }
            this.setlist();
            this.setSelectedIndex(index);
        },
        detail : function(what) {
            var _detail = $(this.$el).find("#the_detail").empty(),
                $div = $(divS).appendTo(_detail),
                $p = $("<p></p>").appendTo($div);
            $p.text(JSON.stringify(what));
            this.$root.$children[0].cloneButtons($div,this);
            this.enableButtonsCallback.call(this,$div,this.selection);
        },
        clear : function() {
            this.index = {};
            $(this.$el).find("[name=list]").empty();
        },
        clicked : function(evt) {
            var index = $(evt.target).closest("li").attr("index");
            this.selection = this.index[index];
            this.setSelectedIndex(index);
            this.selectionChanged.call(this,this.selection);
        },
        nest : function(_item,name,top) {
            if(!top && (!_item || !_item.length))
                return "";
            var a = top?["<li index='{{n}}' type='directory'><span>{{n}}</span><ul>"._tpl_replace({n:name})]:["<ul>"];
            top && (this.index[name] = _item);    
            var that = this;
            var iter = top?_item.children:_item;
            iter.forEach(function(e,i){
                var _name = name+":"+e.name;
                that.index[_name] = e;
                a.push("<li index='{{n}}' type='{{type}}'><span>{{name}}</span>{{ch}}</li>"._tpl_replace({}._assign(e)._assign({ch:that.nest(e.children,_name),n:_name,name:e.name})));
            });
            a.push("</ul>");
            top && a.push("</li>");
            return a.join("");
        },
        list : function(_item) {
            var a = [];
            var that = this;
            for(var p in _item){
                var o = _item[p];
                that.index[o.name] = o;
                a.push("<li index='{{n}}' type='{{type}}'><span>{{n}}</span>{{ch}}</li>"._tpl_replace({}._assign(o)._assign({ch:that.nest(o.children,o.name),n:o.name})));
            }
            this.list_html = a.join("");
        },
        setlist : function() {
            var that = this;
            $(this.$el).find("[name=list]").empty().append($(this.list_html));
            $(this.$el).find("li").each(function(i,e){
                var tmp;
                (tmp = that.icons[$(e).attr("type")]) && $(e).find(">span").prepend($("<i class='fa fa-"+tmp+"'></i>"));
            });
        }
    },
    ready: function () {
        this.setlist();
    }
}
</script>
<style>
.basiclistview .border > ul { width:49.9%; display:inline-block; float:left; }
.basiclistview .border > div { width:48.91%; height:100%; display: inline-block; float:right; }
.basiclistview li.selected span { background-color: gray; color: blue; }
.basiclistview li.selected ul, .basiclistview li.selected ul span { background-color: #fff; color: #000; }
.basiclistview li span i { color: green; }
.basiclistview #the_detail button { display:none; }
</style>