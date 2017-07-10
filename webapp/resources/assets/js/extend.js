define('extend',['jquery','position'/*,'./vendor/iscroll'*/],function(jQuery,position/*,IScroll*/){
window.divS = '<div></div>';
(function($){
$.fn.extend({
  prev_sibling:function(selector,nowrap){
    var prev = this.prevAll(selector).first();
    if(0==prev.length&&!nowrap)
      prev = this.siblings(selector).last();
    return prev;
  },
  next_sibling:function(selector,nowrap){
    var next = this.nextAll(selector).first();
    if(0==next.length&&!nowrap)
      next = this.siblings(selector).first();
    return next;
  },
  adjacent_siblings:function(selector){
    var $e = this.eq(0);
    return new Array($e.prev_sibling(selector),$e.next_sibling(selector));
  },
  prev_branches_height:function(selector,outerid) {
    var ok = true;
    var that = this;
    var h = 0;
    while(that.attr('id')!=outerid){
      h += that.prevAll(selector).sumWidthHeight(true,true)[1];
      that = that.parent();
      if(that.hasClass('column')){
        that.prevAll('div.column').each(function(i,e){
          h += $(e).children(selector).sumWidthHeight(true,true)[1];
        });
        h += that.prevAll('div.column').sumWidthHeight(true,true)[1];
      }else if(that.hasClass('rowitem')){
        h += that.prevAll('div.rowitem').sumWidthHeight(true,true)[1];
        h += that.prevAll('div.rowitem-spacer').sumWidthHeight(true,true)[1];
      }
    }
    return h;
  },
  sumWidthHeight:function(outer,includeMargin){
    var ew = 0;
    var eh = 0;
    var minw = Number.MAX_VALUE;
    var maxw = 0;
    var minh = Number.MAX_VALUE;
    var maxh = 0;
    this.each(function(i,e){
      var w,h;
      if($(e).is('script'))
        return;
      if(outer){
        w = $(e).outerWidth(includeMargin);
        h = $(e).outerHeight(includeMargin);
      }else{
        w = $(e).width();
        h = $(e).height();
      }
      ew += w;
      eh += h;
      minw = Math.min(minw,w);
      minh = Math.min(minh,h);
      maxw = Math.max(maxw,w);
      maxh = Math.max(maxh,h);
    });
    return [ew,eh,minw,minh,maxw,maxh];
  },
  confine_image:function(maxw,maxh) {
    var that = this;
    var w = that.attr('data-width');
    var h = that.attr('data-height');
    var wr = w/maxw;
    var hr = h/maxh;
    if(wr>1 || hr>1){
      if(wr>hr){
        that.css({height:'auto',width:maxw+'px'});
      }else{
        that.css({height:maxh+'px',width:'auto'});
      }
      return this;
    }
    if(wr>hr){
      that.css({width:maxw+'px',height:'auto'});
    }else if(wr==hr){
      that.css({width:maxw+'px',height:maxh+'px'});
    }else{
      that.css({width:'auto',height:maxh+'px'});
    }
    return this;
  },
  expand:function() {
    this.each(function(i,e){
      var that = $(e);
      that.find('img').confine_image(that.width(),that.height());
    });
    return this;
  },
  center_image:function(bord) {
    this.each(function(i,e){
      var $cont = $(e);
      var img = $cont.children().eq(0);
      var l = Math.max(0,Math.floor(($cont.width()-img.width() - bord)/2));
      var t = Math.max(0,Math.floor(($cont.height()-img.height() - bord)/2));
//      if($cont.css('position')=='absolute')
//        img.css('position','absolute');
//      else
//        img.css('position','relative');
      img.css({'left':l+'px','top':t+'px'});
    });
    return this;
  },
  xpath:function() {
    var ret = new Array();
    this.each(function(i,elm){
      var segs,sib;
      for (segs = []; elm && elm.nodeType == 1; elm = elm.parentNode) {
        if (elm.hasAttribute('id')) {
          segs.unshift('id("' + elm.getAttribute('id') + '")')
          return ret.push(segs.join('/'));
        }
        else if (elm.hasAttribute('class'))
          segs.unshift(elm.localName.toLowerCase() + '[@class="' + elm.getAttribute('class') + '"]')
        else {
          for (i = 1, sib = elm.previousSibling; sib; sib = sib.previousSibling)
            if (sib.localName == elm.localName) i++
          segs.unshift(elm.localName.toLowerCase() + '[' + i + ']')
        }
      }
      ret.push(segs.length ? '/' + segs.join('/') : null);
    });
    return ret;
  },
  simplexmlpath:function() {
    var ret = new Array();
    this.each(function(i,elm){
      var e = $(elm);
      var path = elm.tagName;
      while(e =e.parent()){
        if(undefined == $(e).get(0).tagName)
          break;
        path = $(e).get(0).tagName + '/' + path;
      }
      ret.push(path);
    });
    return ret;
  },
  windowInnerHeight:function(){
    if(this.is_idevice()&&this.is_landscape())
      return window.innerWidth;
    else
      return window.innerHeight;
  },
  windowInnerWidth:function(){
    if(this.is_idevice()&&this.is_landscape())
      return window.innerHeight;
    else
      return window.innerWidth;
  },
  is_mobile:function() {
    return $('body').hasClass('mobile');
  },
  is_idevice:function() {
    return $('body').hasClass('idevice');
  },
  test_portrait:function(){
    return window.innerHeight>window.innerWidth;
  },
  is_portrait:function() {
    return $('body').hasClass('portrait');
  },
  test_landscape:function(){
    return window.innerWidth>window.innerHeight;
  },
  is_landscape:function() {
    return $('body').hasClass('landscape')||window.innerHeight>window.innerWidth;
  },
  dimsarray:function() {
    return this.dimensions().split('x');
  },
  dimensions:function() {
    return window.innerWidth+"x"+window.innerHeight;
  },
  attachWheel:function(_onWheel) {
    $(this).each(function(i,e){
      if (window.addEventListener)
      {
        e.addEventListener('DOMMouseScroll', _onWheel, false );
        e.addEventListener('mousewheel', _onWheel, false );
        e.addEventListener('MozMousePixelScroll', _onWheel, false );
      }
      else
      {
        document.attachEvent("onmousewheel", _onWheel)
      }
    });
    return this;
  },
  printStackTrace:function() {
    var e = new Error('dummy');
//    var stack = e.stack.replace(/^[^\(]+?[\n$]/gm, '')
//      .replace(/^\s+at\s+/gm, '')
//      .replace(/^Object.<anonymous>\s*\(/gm, '{anonymous}()@')
//      .split('\n');
    console.log(e.stack);
  },
  resizeRow:function() {
    
  },
  tableHack:function(options) {
    options = $.extend({},options);
    this.each(function(i,e){
      var that = $(e);
      var those = that.children();
      var iw = that.innerWidth();
      var ih = that.innerHeight();
      var a = those.sumWidthHeight(true,false);
      var npr = Math.floor(iw/a[4]);
      var nr = Math.ceil(those.length/npr);

      if(nr*npr>those.length)
        npr = those.length;

      if(npr = 1)
        return;

      var m = Math.floor((iw-(npr*a[4]))/(npr-1))-1;
      those.css({'margin-left':m+'px'});
      those.first().css({'margin-left':'0px'});      
      if(!options.horizOnly){
        var o = Math.floor((ih-(nr*a[5]))/(nr-1));
        those.each(function(i,e){
          var cell = $(e);
          var down = Math.floor(i/npr)-1;
          those.css({'margin-top':(down*o)+'px'});
        })
      }
    
      var big = nr * a[5];
      if(parseInt(big) > parseInt(ih)){
        switch(options.tooHigh) {
          case 'embiggen':
            that.height(big);
            break;
          case 'scroll':
            that.css({'overflow-y':'scroll'});
            break;
          case 'hide':
          default:
            that.css({'overflow-y':'hidden'});
        }
      }
    });
    return this;
  },
  ajaxSubmit:function(options) {
    var post = {};
    this.each(function(j,e){
      var that = $(e);
      that.find('input').each(function(i,v){
        var input = $(v);
        if(input.attr('type')=='checkbox')
          post[input.attr('name')] = input[0].checked;
        else
          post[input.attr('name')] = input.val();
      });
    });
    var o = $.extend(options,{data:post});
    $.ajax(o);
    return this;
  },
  cellHack:function(options) {
    options = $.extend({},options);
    this.each(function(i,e){
      var that = $(e);
      if(options.width)
        that.css({width:that.closest('td').width()+'px'});
      if(options.height)
        that.height({height:that.closest('td').height()+'px'});
    });
    return this;
  },
  formValues:function(){
    var values = {};
    this.each(function(i,e){
      if(!$(e).prop('disabled'))
        values[e.name] = e.value;
    });    
    return values;
  },
  appendDiv:function(str){
    if(str)
      return $(str).appendTo(this);
    return $(divS).appendTo(this);
  },
  scroll:function(options){
//      this.each(function(i,e){
//         $(e).jScrollPane(); 
//      });
    var opt = $.extend({
        bounce:true,
        click:true,
        keyBindings:true,
        momentum:true,
        mouseWheel:true,
        mouseWheelSpeed:240,
        scrollbars:true,
        interactiveScrollbars:true,
        probeType:3
      },options);
    this.each(function(i,e){
      if(!!opt.scrollY){
        $(e).children().each(function(i,e){
          $(e).height($(e).height()*1.035);
        }).css({'margin-right':'13px'});
      }
      if(!!opt.scrollX){
        $(e).children().each(function(i,e){
          $(e).width($(e).width()*1.035);
        }).css({'margin-bottom':'13px'});
      }
      if($(e).closest('.hasIscroll').length>0){
        opt.stopPropagation = true;
      }else{
        $(e).addClass('outermost');
      }
      e.scroll_ = new IScroll(e,opt);
      $(e).addClass('hasIscroll');
    });
    return this;
  },
  setHeight:function(num,inner){
    var b = this.outerHeight(true)-this.height();
    this.height(num + (inner?-1:1)*b);
  },
  setWidth:function(num,inner){
    var b = this.outerWidth(true)-this.width();
    this.width(num + (inner?-1:1)*b);
  },
  untoobiggen:function(selector){
    this.each(function(i,e){
      var $e = $(e);
      var c = $e.parent();
      var p = 0;
      do{
        $e.css('font-size',++p+'px');
        if(p==977)
          break;
      }while($e.height() == e.scrollHeight || $e.outerWidth(true) <= (c.width()-$(selector).sumWidthHeight(true,true)[0]));
      $e.css({'font-size':(p-1)+'px',overflow:'hidden'});
    })
    return this;
  },
  depthFirstLevel:function(treat,ignore){
    var h = 0;
    this.each(function(i,e){
      var $e = $(e);
      if($e.is('script'))
        return;
      if($e.is('div')){
        if($e.in(ignore))
          return;
        if($e.in(treat)){
          h += Math.max($e.children().sumWidthHeight(true,true)[1],$e.outerHeight(true));
        }else{
          h += $e.depthFirstHeight(treat,ignore);
        }
      }else{
        h += $e.outerHeight(true);
      }
    });
    return h;    
  },
  depthFirstHeight:function(treat,ignore){
    var h = this.children().depthFirstLevel(treat,ignore);
    this.height(h);
    return h;
  },
  in:function(selector){
    return $.inArray(this.get(0), $(selector).get()) !== -1;
  },
  searchPoint:function(position){
    var that = this;
    if(!is_within(position,Position.get(this[0]))){
      return false;
    }
    var ci = $.makeArray(this.children());
    var what = false;
    for(var i=0;i<ci.length;i++){
      what = is_within(position,Position.get(ci[i]))?ci[i]:what;
    }
    if(what)
      return $(what).searchPoint(position);
    return this;
  },
  sort:function() {
    return this.pushStack( [].sort.apply( this, arguments ), []);
  },
  textExtent:function(str){
    var $test = $(divS).attr('id','Test').text(str).appendTo($('body'));
    var test = $test[0];
    $test.cloneCss(this);
    var height = (test.clientHeight + 1);
    var width = (test.clientWidth + 1);
    $test.remove();
    return {width:width,height:height};
  },
  cloneCss:function(selector){
    var from = $(selector)[0];
    this.each(function(i,e){
      var cs = false;
        if (from.currentStyle)
          cs = from.currentStyle;
        else if (window.getComputedStyle)
          cs = document.defaultView.getComputedStyle(from,null);
        if(!cs)
          return null;
        for(var prop in cs)
        {
          if(cs[prop] != undefined && cs[prop].length > 0 && typeof cs[prop] !== 'object' && typeof cs[prop] !== 'function' && prop != parseInt(prop) )
          {
            e.style[prop] = cs[prop];
          }   
        }   
      });
    return this;
  },
  mergeUnique:function(array){
    var ret = $.merge([],this);
    $(array).each(function(i,e){
      if(0>$.inArray(e,ret))
        ret.push(e);
    })
    ret.remove('');
    return ret;
  },
  attrObj:function(obj){
    var that = this;
    obj._each(function(o,p){
      that.attr(p,o);
    });
    return this;
  },
  grab:function(ary){
    var that = this;
    return ary._toObject(function(name){
      return [name,that.attr(name)];
    });
  },
  center:function(X,Y){
    !X && !Y && (X=Y=1);
    this.each(function(i,e){
      var wh = $(e).sumWidthHeight();
      var css = {};
      $(e).css(css._assign(X?{
          left:'+50%',
          'margin-left':-(wh[0]/2)+'px',
        }:{},Y?{
          top:'+50%',
          'margin-top':-(wh[1]/2)+'px',
        }:{}));
    });
    return this;
  },
  centerY:function(){
    return this.center(0,1);
  },
  centerX:function(){
    return this.center(1,0);
  },
  submitProxy:function(func,selector){
    this.unbind("click submit").bind('click submit',function(evt){
      evt.preventDefault();
      var $that = $(evt.currentTarget);
      $that.attr('for') && (selector = $that.attr('for'));
      var $this = selector?
        $that.closest(selector).find('form').eq(0):
        $that.closest('form');
      var data = {};
      $this.find('input,textarea,select').not('input[type=file]').each(function(i,e){
        data[e.name] = $(e).val();
      });
      window[$this.attr('beforeSubmit')] && (data = window[$this.attr('beforeSubmit')](data));
      if(data==null)
        return false;
      $.ajax({
        url:$this.attr('action'),
        method:$this.attr('method'),
        dataType:'html',
        data:data,
        error:function(){
          $this.html([].slice.call(arguments,0).join(','));
        },
        success:function(data){
          //window[$this.attr('onSuccess')] && window[$this.attr('onSuccess')](data);
          if(func)
            func(data);
          else
            $this.html(data);
        }
      }._assign($this.attr('enctype')?{contentType:$this.attr('enctype')}:{}));
      return false;
    });
    return this;
  },
});
})(jQuery);

function base64_encode(data) {
  //  discuss at: http://phpjs.org/functions/base64_encode/
  // original by: Tyler Akins (http://rumkin.com)
  // improved by: Bayron Guevara
  // improved by: Thunder.m
  // improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // improved by: Rafał Kukawski (http://kukawski.pl)
  // bugfixed by: Pellentesque Malesuada
  //   example 1: base64_encode('Kevin van Zonneveld');
  //   returns 1: 'S2V2aW4gdmFuIFpvbm5ldmVsZA=='
  //   example 2: base64_encode('a');
  //   returns 2: 'YQ=='

  var b64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';
  var o1, o2, o3, h1, h2, h3, h4, bits, i = 0,
    ac = 0,
    enc = '',
    tmp_arr = [];

  if (!data) {
    return data;
  }

  do { // pack three octets into four hexets
    o1 = data.charCodeAt(i++);
    o2 = data.charCodeAt(i++);
    o3 = data.charCodeAt(i++);

    bits = o1 << 16 | o2 << 8 | o3;

    h1 = bits >> 18 & 0x3f;
    h2 = bits >> 12 & 0x3f;
    h3 = bits >> 6 & 0x3f;
    h4 = bits & 0x3f;

    // use hexets to index into b64, and append result to encoded string
    tmp_arr[ac++] = b64.charAt(h1) + b64.charAt(h2) + b64.charAt(h3) + b64.charAt(h4);
  } while (i < data.length);

  enc = tmp_arr.join('');

  var r = data.length % 3;

  return (r ? enc.slice(0, r - 3) : enc) + '==='.slice(r || 3);
}

String.prototype.trim=function(chars){
  if(chars)
    return this.replace(new RegExp('^['+chars+'\\s]+|['+chars+'\\s]+$'),'');
  else 
    return this.replace(/^\s+|\s+$/g, '');
};

String.prototype.ltrim=function(){return this.replace(/^\s+/,'');};

String.prototype.rtrim=function(){return this.replace(/\s+$/,'');};

String.prototype.fulltrim=function(){return this.replace(/(?:(?:^|\n)\s+|\s+(?:$|\n))/g,'').replace(/\s+/g,' ');};

//Array.prototype.concat = function(){  
//  for(var i=0;i<arguments.length;i++){
//    if(arguments[i].splice)
//      this.concat.apply(this,arguments[i]);
//    else
//      this.push(arguments[i]);
//  }
//  debugger;
//  return this;
//}

function click(){
  if(jQuery('html').is_mobile())
    return 'touchstart';
  else
    return 'mousedown';
}

function foist(o){
  for(var p in o)
    return [p,o[p]];
}

function is_within(p,p2){
  return (p.left>=p2.left&&p.left<=(p2.left+p2.width))
          &&
          (p.top>=p2.top&&p.top<=(p2.top+p2.height));
}

window.selectElement = function(element) {
    if (window.getSelection) {
        var sel = window.getSelection();
        sel.removeAllRanges();
        var range = document.createRange();
        range.selectNodeContents(element);
        sel.addRange(range);
    } else if (document.selection) {
        var textRange = document.body.createTextRange();
        textRange.moveToElementText(element);
        textRange.select();
    }
}

});
