#include "pleg.h"
using namespace Pleg;
#include "drumlin/main_tao.hpp"
#include <glib.h>
#include <gst/gst.h>
#include <algorithm>
#include <memory>
#include <libgen.h>
using namespace std;
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/core/demangle.hpp>
using namespace boost;
namespace po = boost::program_options;
#include "application.h"
#include "plegapp.h"
#include "thread.h"
#include "exception.h"
#include "server.h"
//#include "bluetooth.h"
#include "gstreamer.h"
#include "source.h"
#include "test.h"
#include "uri.h"
#include "event.h"
#include "cursor.h"
#include "gstreamer.h"
#include "jsonconfig.h"
#include "application.h"
#include "connection.h"
using namespace drumlin;
#include "pleg.h"
using namespace Pleg;

const char *nullstr = "";
namespace Pleg {
Server *main_server;
}
/**
 * @brief main loop of Server
 * @param argc int
 * @param argv char*[]
 * @return int
 */
int main(int argc, char *argv[])
{
    int retval = 0;

    Debug() << "Main Thread: " << boost::this_thread::get_id();

    const char *server = "Server";
    const char *client = "Client";
    const char *executable = basename(argv[0]);

    Debug() << executable;

    do{

    if(0 == strcmp(executable,server)){
        GStreamer::gst_initializer init(&argc, &argv);

        string trace,host,mangle;
        int port;

        po::options_description desc(server);
        desc.add_options()
            ("help", "produce help message")
            ("devices-config,f"    ,po::value<string>(&Config::devices_config_file)->default_value("./devices.json")       ,"Devices config file path.")
            ("gstreamer-config,g"  ,po::value<string>(&Config::gstreamer_config_file)->default_value("./gstreamer.json")   ,"gstreamer config file path.")
            ("files-config,o"      ,po::value<string>(&Config::files_config_file)->default_value("./files.json")           ,"files config file path.")
            ("debug,d"                                                                                                   ,"Prompt to continue")
            ("all,a"                                                                                                     ,"Connect all sources in config at startup.")
            ("gstreamer,G"                                                                                               ,"Connect all gstreamer sources in config at startup.")
            ("mock,m"                                                                                                    ,"Connect mock source at startup.")
            ("demangle,w"          ,po::value<string>(&mangle)->default_value("")                                        ,"Demangle a typeid")
            ("trace,t"             ,po::value<string>(&trace)->default_value("trace.json")                               ,"Attach trace process.")
            ("address,a"           ,po::value<string>(&host)                                                             ,"Address to listen on")
            ("port,p"              ,po::value<int>(&port)->default_value(4999)                                           ,"Server port")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("demangle")) {
          cout << mangle.c_str() << std::endl;
          cout << boost::core::demangle( mangle.c_str() ) << std::endl;
          return 1;
        }

        if (vm.count("help")) {
            cout << desc << endl;
            return 1;
        }

        if(vm.count("trace")){
            drumlin::Tracer::startTrace(trace);
        }

        if(vm.count("debug")){
            scanf("Ready?");
        }

        Pleg::Server a(argc,argv,host,port);
        drumlin::iapp = app = main_server = &a;

        string sz_all("all");
        if(vm.count("mock")){
            app->startMock();
        }
//else if(vm.count("all")){
//            ThreadWorker *worker = app->startBluetooth("sources");
//            while(!worker->getThread()->isStarted())
//                boost::this_thread::yield();
//            make_pod_event(EventType::BluetoothConnectDevices,"connectDevices",sz_all)->send(worker->getThread());
//        }

        if(vm.count("gstreamer")){
            ThreadWorker *worker = app->startGStreamer("gstreamer");
            while(!worker->getThread()->isStarted())
                boost::this_thread::yield();
            make_pod_event(EventType::GstConnectDevices,"connectPipes",sz_all)->send(worker->getThread());
        }

        main_server->start();

        try{
            drumlin::start_io();
            retval = a.exec();
        }catch(Exception &e){
            Debug() << e.message;
            retval = 1;
            break;
        }

        if(vm.count("trace")){
            drumlin::Tracer::endTrace();
        }

        std::cout << std::endl;

        break;
    }else if(0 == strcmp(executable,client)){
        TestLoop a(argc, argv);

        app = &a;
        drumlin::iapp = app;

        string get = "/mock",post,patch = "/status",host,port,bluetooth = "scan",trace = "trace.json";
        string_list headers;
        int repeats;

        po::options_description desc(server);
        desc.add_options()
            ("help", "produce help message")
            ("get,g"               ,po::value<string>(&get)                                                             ,"GET test [/status].")
            ("post,p"              ,po::value<string>(&post)                                                            ,"POST test [/status].")
            ("patch,s"             ,po::value<string>(&patch)                                                           ,"PATCH test [/status].")
            ("udp,u"                                                                                                    ,"Run the UDP test.")
            ("debug,d"                                                                                                  ,"Prompt to continue")
            ("host,h"              ,po::value<string>(&host)->default_value("127.0.0.1")                                ,"Host name of the server [localhost].")
            ("port,P"              ,po::value<string>(&port)->default_value("4999")                                     ,"Port on the server.")
            ("json,j"                                                                                                   ,"Content-Type: text/json")
            ("headers,H"           ,po::value<vector<string>>(&headers)                                                 ,"headers for request.")
            ("repeats,r"           ,po::value<int>(&repeats)->default_value(1)                                          ,"Number of repeated attempts")
            ("uri,U"                                                                                                    ,"Test UriParseFunc.")
            ("bluetooth,B"         ,po::value<string>(&bluetooth)                                                       ,"Bluetooth test")
            ("scan,S"                                                                                                   ,"Scan for bluetooth devices.")
            ("devices-config,f"    ,po::value<string>(&Config::devices_config_file)->default_value("devices.json")      ,"Devices config file path.")
            ("trace,t"             ,po::value<string>(&trace)                                                           ,"Attach trace process.")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            cout << desc << endl;
            return 1;
        }

        if(vm.count("debug")){
            scanf("Ready?");
        }

//        Bluetooth *bluet;

        guint16 i(0);
        if(vm.count("json")){
            headers << "Content-Type: text/json";
        }

        if(vm.count("trace")){
            drumlin::Tracer::startTrace(trace);
        }

        drumlin::start_io();

        if(vm.count("get")){
            for(i=0;i<repeats;i++){
                Test *test(new Test(host,port,test_GET));
                app->addThread(test->setRelativeUrl(get).setHeaders(headers).getThread());
            }
        }
        if(vm.count("post")){
            for(i=0;i<repeats;i++){
                Test *test(new Test(host,port,test_POST));
                app->addThread(test->setRelativeUrl(post).setHeaders(headers).getThread());
            }
        }
        if(vm.count("patch")){
            headers << "X-Method: PATCH";
            for(i=0;i<repeats;i++){
                Test *test(new Test(host,port,test_PATCH));
                app->addThread(test->setRelativeUrl(patch).setHeaders(headers).getThread());
            }
        }
        if(vm.count("udp")){
            app->addThread((new Test(host,port,test_UDP))->getThread());
        }
//        if(vm.count("uri")){
//            bluet = app->startBluetooth("mock",config);
//            bluet->defineMockSources();
//        }
//        if(vm.count("bluetooth")){
//            app->startBluetooth(bluetooth.c_str(),config);
//        }
//        if(vm.count("scan")){
//            app->startBluetooth("scan",config);
//        }

        retval = a.exec();

        if(vm.count("trace")){
            drumlin::Tracer::endTrace();
        }

        std::cout << std::endl;

        break;
    }

    }while(false);

    Debug() << __func__ << "returning" << retval;

    return retval;
}
