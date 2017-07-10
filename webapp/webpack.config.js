var path = require('path');
var webpack = require('webpack');
var utils = require('./utils');
var projectRoot = path.resolve(__dirname);
var context = path.resolve(process.cwd(), 'resources/assets/');
var glob = require("glob");
require('./resources/assets/js/nodeextend');
var config = require('./webpack_config').dev._assign({
    context: context,    
    entry: {
    	'app'  : './js/app',
        'app-preload' : './js/app-preload',
        '_d3'  : './js/_d3',
	},			
    output: {
    	path: path.resolve(process.cwd(), 'public/js'),
        filename: '[name].js',
        chunkFilename: '[hash]-[id].js',
		publicPath: "public/js/"
  	},
    resolveLoader: {
        fallback: [
            path.resolve(process.cwd(),'node_modules'),
        ]
    },
    resolve: {
        alias: {
            "jquery": path.resolve(process.cwd(),"node_modules/jquery/dist/jquery.min.js"),
            "jquery-ui/widget": path.resolve(process.cwd(),"node_modules/jquery-ui/ui/widget.js"),
            "jquery.ui.widget": path.resolve(process.cwd(),"node_modules/jquery-ui/ui/widget.js"),
            "jquery.ui.unique": path.resolve(process.cwd(),"node_modules/jquery-ui/ui/unique-id.js"),
            "jquery.ui.draggable": path.resolve(process.cwd(),"node_modules/jquery-ui/ui/widgets/draggable.js"),
            "jquery.ui.droppable": path.resolve(process.cwd(),"node_modules/jquery-ui/ui/widgets/droppable.js"),
            "jquery.ui.sortable": path.resolve(process.cwd(),"node_modules/jquery-ui/ui/widgets/sortable.js"),
            "jquery.ui.resizable": path.resolve(process.cwd(),"node_modules/jquery-ui/ui/widgets/resizable.js"),
            "jquery.ui.uniqueid": path.resolve(process.cwd(),"node_modules/jquery-ui/ui/unique-id.js"),
            "jquery.datetimepicker": path.resolve(process.cwd(),"node_modules/jquery-datetimepicker/build/jquery.datetimepicker.full.min.js"),
            "clipboard": path.resolve(process.cwd(),"node_modules/clipboard/dist/clipboard.js"),
            "simple-graph": path.resolve(process.cwd(),"resources/assets/js/simple-graph.js"),
            "video": path.resolve(process.cwd(),"node_modules/video.js/dist/video.js"),
        }
    },
    module: {
//        preLoaders: [
//            {
//                test: /\.vue$/,
//                loader: 'eslint',
//                include: context,
//                exclude: /node_modules/
//            },
//            {
//                test: /\.js$/,
//                loader: 'eslint',
//                include: context,
//                exclude: /node_modules/
//            }
//        ],
        loaders: [
            {
                test: /\.js$/,
                exclude: /(fileupload-ui)/,
                loader: 'babel',
            },
            {
                test: /\.json$/,
                loader: 'json'
            },
            {
                test: /\.vue$/,
                loader: 'vue'
            },
            
//            {
//                test: /\.(png|jpe?g|gif|svg)(\?.*)?$/,
//                loader: 'url',
//                query: {
//                    limit: 10000,
//                    name: utils.assetsPath('img/[name].[hash:7].[ext]')
//                }
//            },
//            {
//                test: /\.(woff2?|eot|ttf|otf)(\?.*)?$/,
//                loader: 'url',
//                query: {
//                    limit: 10000,
//                    name: utils.assetsPath('fonts/[name].[hash:7].[ext]')
//                }
//            }   
        ],
    },
    eslint: {
        formatter: require('eslint-friendly-formatter')
    },
    vue: {
        loaders: {
            js: 'babel'
        }
    },
    plugins: [
        new webpack.NormalModuleReplacementPlugin(
            /\.(jpe?g|png|gif|svg)$/,
            'node-noop'
        )	
    ]
});
glob(path.resolve(process.cwd(),"node_modules/d3*"),function(_path){
    if(!_path)return;
    var basename = path.basename(_path);
    config.resolve.alias[basename] = path.resolve(process.cwd(),"node_modules",basename,"build",basename+".js");
});
require("util").inspect(config,{maxdepth:-1});
module.exports = config;
