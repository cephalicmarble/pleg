if (typeof define !== 'function') {
  var define = require('amdefine')(module);
	var fs = require('fs');
	var path = require('path');
}
define(function(require){

return (function(maybe){

//if(!this.self){
//	fs.mkdirParent = function(dirPath, mode, callback) {
//	  if(mode instanceof Function){
//	    callback = mode;
//	    mode = 0777;
//	  }
//	  //Call the standard fs.mkdir
//	  fs.mkdir(dirPath, mode, function(error) {
//	    //When it fail in this way, do the custom steps
//	    if (error && error.code == 'ENOENT') {
//	      //Create all the parents recursively
//	      fs.mkdirParent(path.dirname(dirPath), mode, callback);
//	      //And then the directory
//	      fs.mkdirParent(dirPath, mode, callback);
//	    }
//	    //Manually run the callback since we used our own callback to do all these
//	    callback && callback(error);
//	  });
//	};
//}
	
var hide = {
  enumerable: false,
  writable: true,
  value: function(name,value){
    Object.defineProperty(this,'_'+name,{
      enumerable: false,
      writable: true,
      value: value
    });
  }
};
Object.defineProperty(Object.prototype,'hide',hide);

Array.prototype.hide('push',function(thing){
  this.push(thing);
  return this;
});

Array.prototype.hide('fold',function(func){
  var a = [];
  this.forEach(function(e){
    if(e instanceof Array){
      a.concat(e.fold(func));
    }else{
      a.push(func(e));
    }
  });
  return a;
});

Array.prototype.hide('pushUnique',function(str){
  if(typeof str == 'String' && ~this.indexOf(str))return;
  return this.push(str);
});

String.prototype.hide('slugify',function(){
  return this.replace(/[ _"'£$%&*\(\)]+/g,'-').toLowerCase();
});

Object.prototype.hide('myclass',function(){
  if(this.ui)
    return this.ui[0].className;
  return this.$name?this.$name:this.$class?this.$class:this.$base?this.$base:'unknown';
});

Object.prototype.hide('dup',function (props) {
  var defs = {}, key;
  for (key in props) {
    if (props.hasOwnProperty(key)) {
      defs[key] = {value: props[key], enumerable: true};
    }
  }
  return Object.create(this, defs);
});

Object.prototype.hide('serialize',function(){
  var jsonObj = {};
  for (p in this){
    if(typeof(p) == 'function'){
      jsonObj[p] = {f:this._fspaceh,d:this._dspaceh,func:p};
    }else{
      jsonObj[p] = this[p];
    }
  }
  return JSON3.stringify(jsonObj);
});

Array.prototype.hide('dupa',function(a){
  if(!a)a = [];
  return a = this.slice(this,this.length);
});

Object.prototype.hide('array',function(func,object,ignore){
	var a = [];
	for(var p in this){
      if(ignore && (~ignore.indexOf(p) || ~ignore.indexOf(typeof p)))
        continue;
      if(!!p || !this.hasOwnProperty(p))
        continue;
      func && (a.push(func.call(object,this[p],p)));
      func || (a.push(this[p]));
	}
	return a;
});

Array.prototype.hide('fremove',function(){
    if(!arguments[0].slice)
      return this.remove.apply(this,arguments);
    var that = this;
    var ids = [],i;
    for(var j=0;j<arguments[0].length;j++){
      if(typeof(arguments[0][j]) == 'function'){
        for(i=0;i<that.length;i++){
          if(arguments[0][j](that[i]))
            ids.push(i);
        }
      }else{
        for(i=0;i<that.length;i++){
          if(arguments[0][j]==that[i])
            ids.push(i);
        }
      }
    }
    var ret = [];
    for(i=0;i<that.length;i++){
      if(!~ids.indexOf(i))
        ret.push(that[i]);
    }
    return ret;
});

Array.prototype.hide('remove',function(func) {
    var that = this;
    if(func instanceof Array){
        func.forEach(function(e,i){
            var idx = that.indexOf(e);
            if(idx>=0)
                that.splice(idx,1);
        });
        return this;
    }
    if(func instanceof Function){
        var c = 1;
        that.slice(0).reverse().forEach(function(e){
            if(func(e))
                that.splice(that.length-c,1);
            else
                c++;
        });
        return this;
    }
    if(func instanceof Object){
        func.array.forEach(function(funci){
            var c = 1;
            that.slice(0).reverse().forEach(function(e,i){
                if(func.func(e,funci))
                    that.splice(that.length-c,1);
                else
                    c++;
            });
        });
        return this;
    }
    var what, a = arguments, L = a.length, ax;
    while (L && this.length) {
        what = a[--L];
        while ((ax = this.indexOf(what)) !== -1) {
            this.splice(ax, 1);
        }
    }
    return this;
});

Array.prototype.hide('unique',function() {
    var what, L = this.length, ax = [];
    while (L) {
        what = this[--L];
        !~ax.indexOf(what) && (ax.push(what));
    }
    return ax;
});

Function.prototype.hide('chain',function(func,reverse){
  var that = this;
  if(reverse){
    return function(){
      var done = 0;
      var ret;
      this._next = function(){done = 1;ret = that.apply(this,arguments);};
      ret = func.apply(this,arguments);
      if(ret)
        arguments[0] = ret;
      return done?ret:that.apply(this,arguments);
    }    
  }else{
    return function(){
      var done = 0;
      var ret;
      this._next = function(){done = 1;ret = func.apply(this,arguments);};
      ret = func.apply(this,arguments);
      if(ret)
        arguments[0] = ret;
      return done?ret:func.apply(this,arguments);
    }
  }
});

// USEFUL FUNCTIONS

Object.prototype.hide('filter',function(func,reverse){
  var obj = {};
  if(func===undefined)
    func = true;  
  if(func===true || func===false){
    var truthy = !!func;
    func || (func = function(o){return !!o === truthy;});
  }
  if(func instanceof Array){
    var filter = [].slice.call(func,0);
    if(reverse){
      func = function(o,p){return !~filter.indexOf(p);};
    }else{
      func = function(o,p){return ~filter.indexOf(p);};
    }
  }
  for(var p in this){
    if(!p || !this.hasOwnProperty(p))continue;
    if(func.call(this,this[p],p))
      obj[p] = this[p];
  }
  return obj;
});

Object.prototype.hide('map',function(func,replace){
  var obj = {};
  for(var p in this){
    if(!p || !this.hasOwnProperty(p))continue;
    if(!replace && obj[p])
      obj[p]._assign(func.call(this,this[p],p));
    else
      obj[p] = func.call(this,this[p],p);
  }
  return obj;
});

Object.prototype.hide('fold',function(func){
  var o = {};
  for(var p in this){
    if(!p || !this.hasOwnProperty(p))continue;
    var a = func(this[p],p);
    if(!a)continue;
    if(o[a[0]] instanceof Array && a[1] instanceof Array)
      o[a[0]] = o[a[0]].concat(a[1]);
    else if(o[a[0]] instanceof Object)
      o[a[0]]._assign(a[1]);
    else
      o[a[0]] = a[1];
  }
  return o;
});

Array.prototype.hide('intersect',function(){
  var ret = [],tmp,arr;
  for(var a=0;a<arguments.length;a++){
    arr = arguments[a];
    for (var i = 0; i < arr.length; i++) {
        tmp = arr[i];
        if (!~ret.indexOf(tmp) && ~this.indexOf(tmp))
            ret.push(tmp);
        
    }
  }
});

Array.prototype.hide('intersect_sorted',function(arr){
  var ai=0, bi=0;
  var result = [];

  while( ai < this.length && bi < arr.length )
  {
     if      (this[ai] < arr[bi] ){ ai++; }
     else if (this[ai] > arr[bi] ){ bi++; }
     else /* they're equal */
     {
       result.push(this[ai]);
       ai++;
       bi++;
     }
  }
  
  return result;
});

Object.prototype.hide('each',function(func,object){
  object || (object = this);
  for(var p in this){
    if(!p || !this.hasOwnProperty(p))continue;
    func.call(object,this[p],p);
  }
});

Object.prototype.hide('tpl_replace',function(str){
  var ret = str.substr(0),
    rx = new RegExp('\{\{([^\}]+)\}\}','g'),
    matches = str.match(rx),
    that = this;
  matches.forEach(function(regexp){
    var match = regexp.replace("{{","").replace("}}","");
    if(!that.hasOwnProperty(match))return;
    ret = ret.replace(new RegExp(regexp,'g'),that[match] instanceof Function?that[match]():that[match]);
  });
  return ret;
});

String.prototype.hide('tpl_replace',function(obj){
  return obj._tpl_replace(this);
});

Object.prototype.hide('find',function(func){
  var tmp;
  for(var p in this){
    if(tmp=func.call(this,this[p],p))
      return tmp;
  }  
  return null;
});

Array.prototype.hide('find',function(func){
    var tmp;
  try{
    this.forEach(function(e){
      if(tmp = func(e))
          throw tmp;
    });
  }catch(tmp){
      return tmp;
  }
  return null;
});

Object.prototype.hide('empty',function(){
  for(var p in this){
    if(!p || !this.hasOwnProperty(p))continue;
    return false;
  }
  return true;
});

Object.prototype.hide('assign',function(){
  for(var i = 0;i<arguments.length;i++){
    var obj = arguments[i];
    for(var p in obj){
      if(!p || !obj.hasOwnProperty(p))continue;
      if(this[p] instanceof Function)
        this[p] = obj[p];
      else if(this[p] instanceof Array && obj[0] instanceof Array)
        this[p] = this[p].concat(obj[p]);
      else if(this[p] instanceof Array)
        this[p] = obj[p]._dupa();
      else if(obj[p] && obj[p].substr)
        this[p] = obj[p];
      else if(this[p] instanceof Object)
        this[p] = {}._assign(obj[p]);
      else
        this[p] = obj[p];
    }
  }
  return this;
});

Object.prototype.hide('toArray',function(func){
  if(func===undefined){
    func = function(o,p){return o;};
  }else if(func===false){
    func = function(o,p){return p;};
  }
  if(func instanceof Array){
    var filter = [].slice.call(func,0);
    func = function(o,p){return ~filter.indexOf(p);};
  }
  var a = [],tmp;
  for(var p in this){
    if(!p || !this.hasOwnProperty(p))continue;
    if(tmp=func(this[p],p))
      a.push(tmp);
  }
  return a;
});

Array.prototype.hide('toObject',function(func,overwrite){
  func = func || function(e,i){return [i,e];};
  var o = {},a;
  this.forEach(function(e,i){
    a = func(e,i);
    if(!a)return;
    if(!overwrite && o[a[0]] instanceof Array && a[1] instanceof Array)
      o[a[0]] = o[a[0]].concat(a[1]);
    else if(a[1] instanceof Array)
      o[a[0]] = a[1].slice(0);
    else if(o[a[0]] instanceof Object && a[1] instanceof Object)
      o[a[0]]._assign(a[1]);
    else
      o[a[0]] = a[1];
  });
  return o;
});

String.prototype.hide('trim',function(chars){
  return this.replace(new RegExp('^['+chars+'\\s]+|['+chars+'\\s]+$'),'');
});

//cheers val shumann
String.prototype.toCamelCase = function() {
  // uppercase letters preceeded by a hyphen or a space or an underline
  var s = this.replace(/([ -_]+)([a-zA-Z0-9])/g, function(a,b,c) {
    return b.toUpperCase() + c.toLowerCase();
  });
  // uppercase letters following numbers
  s = s.replace(/([0-9]+)([a-zA-Z])/g, function(a,b,c) {
    return b + c.toUpperCase();
  });
  // uppercase first letter
  s = s.replace(/^([a-zA-Z])/g, function(a,b,c) {
    return b.toUpperCase();
  });
  return s;
}

Date.prototype.hide('forSql',function(){
  return this.toISOString().substring(0, 19).replace('T', ' ');
});

String.prototype.hide('parseMysql',function() {
  var t = this.replace(/T|Z/g,' ').split(/[- :]/);
  return new Date(t[0], t[1]-1, t[2], t[3]||0, t[4]||0, t[5]||0);
});

})(this.self?this:module);

});
