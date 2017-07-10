define('ajax',
['jquery','json3'],
function(jQuery,JSON3){

jQuery.parseJSON = function(str){
    return JSON3.parse(str);
}

window.parse_cookie_line = function(xhr){
    var str = xhr.getResponseHeader('Set-Cookie');
    if(str){
        console.log(str);
        return str.split(';')._toObject(function(e){
            return e.split('=').map(function(e){
                return decodeURI(e);
            });
        });
    }
    return null;
};

window.update_csrf_token = function(xhr){
    var line = window.parse_cookie_line(xhr);
    console.log(line);
    if(!!line){
        console.log(line);
        var token = line["XSRF-TOKEN"];
        jQuery.ajaxSetup({
            headers: {
                'X-CSRF-TOKEN': token,
            }
        });
        jQuery('input[name=_token]').val(token);
        jQuery('meta[name="csrf-token"]').attr('content',token);
    }
};

(function($){
window.Cache = {};
window.Ajax = function(options){
//    if(options.logUrl){
//      $.ajax({
//        asynch:false,
//        method:'POST',
//        url:options.logUrl,
//        data:options.logData,
//        dataType:'html',
//        error:options.error,
//        success:function(){}
//      });
//    }
  var deferred = $.Deferred();
  if(options.cache && window.Cache[options.url]){
    deferred.resolve(window.Cache[options.url]);
    return deferred;
  }
  $.ajax({
    method:'GET',
    data:{},
    dataType:'html',
    error : function(xhr,status,e){
      var str = 'error during '+xhr.toString()+' : ';
      var j = false;
      try{
        j = JSON3.parse(xhr.responseText);
      }catch(e){}
      if(j&&j.message)
        str += j.message;
      else
        str += xhr.responseText;

      $('body').prepend(str);
    },
    success:function(data,status,xhr){
        window.update_csrf_token(xhr);
        if(options.cache){
            window.Cache[options.url] = data;
        }
        if(data.redirect){
            return window.location.href = data.redirect;
        }
        deferred.resolve(data);
    }
  }._assign(options._filter(['method','data','dataType','url','contentType'])));
  return deferred;
}

})(jQuery);

});
