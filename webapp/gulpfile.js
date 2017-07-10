var exec = require('child_process').exec;
var path = require('path');
require(path.resolve(process.cwd(),'resources/assets/js/nodeextend.js'));

/*
 |--------------------------------------------------------------------------
 | Elixir Asset Management
 |--------------------------------------------------------------------------
 |
 | Elixir provides a clean, fluent API for defining some basic Gulp tasks
 | for your Laravel application. By default, we are compiling the Sass
 | file for our application, as well as publishing vendor resources.
 |
 */

var gulp = require('gulp');
var watch = require('gulp-watch');
var debug = require('gulp-debug');
var gutil = require('gulp-util');
var config = require('./webpack.config.js');
var webpack = require('webpack');
var gulpWebpack = require('webpack-stream');
var Q = require('q');
var deps = [];
function webpack_task(e){
    return function(){
        var myconfig = {}._assign(config);
        myconfig.entry = {};
        myconfig.entry[e] = config.entry[e];
        console.log(require('util').inspect(myconfig,{depth:null}));
        gulp.src("resources/assets/js")
            .pipe(gulpWebpack(myconfig,null, function(err,stats){
                if(err) throw new gutil.PluginError("webpack", err);
                gutil.log("[webpack]", stats.toString());
            }))
            .pipe(gulp.dest('public/js'));
    };
}
for(var e in config.entry){
    deps.push(e);
    gulp.task(e, e=='fileupload'?['blueimp']:[], webpack_task(e));
}
//gulp.task('blueimp',function(){
//    Q.nfapply(exec,['resources/assets/js/shim-canvasLoadImage.sh tmpl jquery']).then(function(){
//        return Q.nfapply(exec,['resources/assets/js/shim-tmpl.sh jquery']);
//    }).then(function(){
//        webpack_task('fileupload');
//    });
//});
////shimmy
//deps._remove('fileupload');
//deps.push('blueimp');
////!shimmy
gulp.task("default",["app"],function(){});

var sass = require('gulp-sass');
var stylus = require('gulp-stylus');
gulp.task('sass', function() {
  return gulp.src('resources/assets/sass/*.scss')
    .pipe(sass())
    .pipe(gulp.dest('public/css'));
});
gulp.task('stylus',function() {
  return gulp.src('resources/assets/stylus/*.styl')
    .pipe(stylus())
    .pipe(gulp.dest('public/css'));
});
gulp.task('watch', function(){
    var css_watcher = gulp.watch(["resources/assets/sass/*","resources/assets/stylus/*","resources/assets/css/*"],{},["elixir"]);
    var js_watcher = gulp.watch(["resources/assets/js/*","resources/assets/js/components/*","resources/assets/js/components/view/*","resources/assets/js/components/edit/*"],{},["default"]);
    var config_watcher = gulp.watch(["./gulpfile.js","webpack.config.js"],{},["default"]);
    css_watcher.on('change', function(event){
        console.log('File ' + event.path + ' was ' + event.type + ', running tasks...');
    });
    js_watcher.on('change', function(event){
       console.log('File ' + event.path + ' was ' + event.type + ', running tasks...'); 
    });
    config_watcher.on('change', function(event){
        console.log('File ' + event.path + ' was ' + event.type + ', running tasks...');
    });
});
