<template>
    <div class="collection">
        <div class="container">
            <bs-aside :show.sync="showLeft" placement="left" :width="350">
                <slot name="header">DSP Admin Menu</slot>
                <slot name="sidebar">
                </slot>
                <div class="aside-footer">
                    <button type="button" class="btn btn-default" @click="showLeft=false">Close</button>
                </div>
            </bs-aside>
            <div class="collection row" v-show.sync="showCollection">
                <div class="col-md-6">
                    <label for="titles">{{ model_name | capitalize }}</label>&nbsp;<a id="collection-create" @click="create"><i class="fa fa-star"></i></a>&nbsp;<a id="collection-delete" @click="delete"><i class="fa fa-trash"></i></a>
                    <bs-select :value.sync="selected"
                        :options.sync="titles"
                        name="titles" 
                        search justified close-on-select
                        >
                    </bs-select>
                </div>
            </div>
            <router-view></router-view>
            <div class="table" v-show.sync="showTable">
                <div class="data"></div>
            </div>
        </div>
        <bs-modal v-ref:confirm-delete :show.sync="showConfirmDelete"
            title="Confirm Delete"
            ok-text="OK" cancel-text="Cancel"
            width="50%" large="true" backdrop="true" 
            :callback="checkConfirmedDelete"
            >
            <div v-if="showConfirmDelete" slot="modal-header"><h4>Delete {{ model_name | capitalize }}</h4></p></div>
            <div slot="model-footer"></div>
            <div slot="modal-body">Really delete this {{ model_name | capitalize }}?</div>
        </bs-modal>
    </div>
</template>

<script>
export default {
    data () {
        return {
          showLeft: false,
          showConfirmDelete: false,
          showCollection: false,
          showTable: false,
        }
    },
    props : {
        titles : {
            type: Array,
            twoWay: true
        },
        id : {
            type: String,
            "default": 0,
            twoWay: true
        },
        model_name : {
            type: String,
            twoWay: true,
        },
        type: { type:String,"default":"event" },
        selected : { type: String,"default":"0" },
    },
    watch : {
        selected : function(value) {
//            if(this.id != value) {
                this.id = value;
                this.show();
//            }
        }
    },
    computed: {
        show_url : {
            get:function() { 
                var t = this.collection?this.collection.show.replace("%PLACEHOLDER%",this.id):"";
                console.log("show "+t);
                return t;
            }
        },
    },
    events : {
        showTabularItem : function(id) {            
            this.hideModals();
            var that = this;
            Ajax({
                url:this.collection.show.replace("%PLACEHOLDER%",id),
                method:'get',
                dataType:'json'
            }).then(function(show){
                that.$children[3].$emit("show",{}._assign(show));
            });
        },
    },
    methods : {
        create : function() {
            this.hideModals();
            this.$children[3].$emit("create");
        },
        "delete" : function() {
            this.hideModals();
            this.showConfirmDelete = true;
        },
        checkConfirmedDelete : function() {
            this.hideModals();
            var that = this;
            Ajax({
                url:this.show_url,
                method:'delete',
                data:{id:this.id},
            }).then(function(del){
                alert("deleted");
                that.selected_ = 0;
            });
        },
        show : function() {
            this.hideModals();
            this.selected = this.id;
            var that = this;
            Ajax({
                url:this.show_url,
                method:'get',
                dataType:'json'
            }).then(function(show){
                that.$children[3].$emit("show",{}._assign(show));
            });
        },
        hideModals : function() {
            this.showConfirmDelete = false;
        }
    },
    attached () {        
        var that = this,
            script = $('.collection script[type="text/json"]');
        $($('#login').text()).appendTo($(".nav.navbar-nav.collection"));        
        Ajax({
            url:"/admin/"+this.type+"/_",
            dataType:"json",
        }).then(function(data){
            that.collection = data;
            $('title').text("DSP - "+that.collection.title);
            that.model_name = that.collection.title.replace(" ","-").toLowerCase();
            that.$router.go("/"+that.model_name);
            setTimeout(function(){
                that.$children[3].caps && that.$children[3].caps()._toArray(function(o,p){
                    $("#collection-"+p).prop("disabled",!o[p]);
                });
            },0);
            if(!that.collection.table){
                that.showCollection = true;
                that.titles = that.collection.collection._toArray(function(name,id){
                    return {value:id,label:name};
                });
            }else{
                that.showTable = true;
                Ajax({
                    url:that.collection.table,
                }).then(function(html){
                    var tabledata = $(that.$el).find('.table .data');
                    window.datatables.call(tabledata.html(html).find('table'),{});
                    that.tabledata = new Vue({el:tabledata[0]});
                    that.tabledata.$emit("show");
                });
            }
            if(!!that.id){
                that.show();
            }
        });
    },
}
</script>