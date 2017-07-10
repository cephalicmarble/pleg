if (typeof define !== 'function') {
//  var define = require('amdefine')(module);
  var promise = require('q');
}

var once = 0;

function binding(promise){

if(!once++){
  Array.prototype.hide('unravel',function(args,resolve,before,after){
      var q,p = this.__promise.defer(),a = [],that = this;
      this.forEach(function(binding,i){
        before && before.apply(binding.object,args);
        q = binding(args);
        if(q && q.then)
          q.then((function(bind,args,i){
            return function(){
              after && after.call(bind,args);
              a.push(i);
              a.length == that.length && p.resolve(resolve);
            };
          })(binding.object,args,i))
        else{
          after && after.apply(binding.object,args);
          a.push(i);
          a.length == that.length && p.resolve(resolve);
        }
      });
      return p;
  });

  Array.prototype.hide('call',function() {
      if(this[2]){
        var a = [].slice.call(arguments,0);
        !!this[2] && a.unshift(this[2]);
        return this[1].apply(this[0],a);
      }else{
        return this[1].apply(this[0],arguments);
      }
  });

  Function.prototype.hide('call',function() {
      return this.apply.apply(this,arguments);
  });
}

function plain_old_binding(obj,method,args){
  var binding_array = obj.splice?obj:[obj,method,args];
  var binding = function(){
    return binding_array._call.apply(binding_array,arguments);
  };
  binding.call = function(){
    var a = [].slice.call(arguments,0);
    a.shift();
    return binding_array._call.apply(binding_array,a);
  }
  binding.apply = function(){
    return binding_array._call.apply(binding_array,arguments[1]);
  }
  binding.object = obj;
  binding.method = method;
  return binding;
}

plain_old_binding.aplus = function(obj,method,args){
  args || (args = {});
  var binding_array = obj.splice?obj:[obj,method,args];
  function _binding(resolve,reject){
    this.assign(args);
    return resolve(this.__promise.fcall(binding_array._call.apply(binding_array,arguments)));
  };
  _binding.call = function(){
    var a = [].slice.call(arguments,0);
    a.shift();
    return this.__promise.fcall(binding_array._call.apply(binding_array,a));
  }
  _binding.apply = function(){
    return this.__promise.fcall(binding_array._call.apply(binding_array,arguments[1]));
  }
  binding.object = obj;
  binding.method = method;
  return _binding;
}

return plain_old_binding;

}

if(typeof define == 'function'){
  define('binding',['q'],function(promise){return binding(promise);});
}else{
  module.exports = binding(promise);
}