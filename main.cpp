#include <pleg.h>
using namespace Pleg;
#define TAOCPP_JSON_INCLUDE_INTERNAL_ERRORS_HH
#include <string>

#include "tao/json/external/pegtl/normal.hpp"
#include "tao/json/external/pegtl/parse_error.hpp"

#include "tao/json/internal/grammar.hpp"

namespace tao
{
   namespace json
   {
      namespace internal
      {
         template< typename Rule >
         struct errors
               : public tao_json_pegtl::normal< Rule >
         {
            static const std::string error_message;

            template< typename Input, typename ... States >
            static void raise( const Input & in, States && ... )
            {
               throw tao_json_pegtl::parse_error( error_message, in );
            }
         };

         template<> const std::string errors< rules::text >::error_message /*__attribute__(( weak ))*/ = "no valid JSON";

         template<> const std::string errors< rules::end_array >::error_message /*__attribute__(( weak ))*/ = "incomplete array, expected ']'";
         template<> const std::string errors< rules::end_object >::error_message /*__attribute__(( weak ))*/ = "incomplete object, expected '}'";
         template<> const std::string errors< rules::member >::error_message /*__attribute__(( weak ))*/ = "expected member";
         template<> const std::string errors< rules::name_separator >::error_message /*__attribute__(( weak ))*/ = "expected ':'";
         template<> const std::string errors< rules::array_element >::error_message /*__attribute__(( weak ))*/ = "expected value";
         template<> const std::string errors< rules::value >::error_message /*__attribute__(( weak ))*/ = "expected value";

         template<> const std::string errors< rules::edigits >::error_message /*__attribute__(( weak ))*/ = "expected at least one exponent digit";
         template<> const std::string errors< rules::fdigits >::error_message /*__attribute__(( weak ))*/ = "expected at least one fraction digit";
         template<> const std::string errors< rules::xdigit >::error_message /*__attribute__(( weak ))*/ = "incomplete universal character name";
         template<> const std::string errors< rules::escaped >::error_message /*__attribute__(( weak ))*/ = "unknown escape sequence";
         template<> const std::string errors< rules::chars >::error_message /*__attribute__(( weak ))*/ = "invalid character in string";
         template<> const std::string errors< rules::string::content >::error_message /*__attribute__(( weak ))*/ = "unterminated string";
         template<> const std::string errors< rules::key::content >::error_message /*__attribute__(( weak ))*/ = "unterminated key";

         template<> const std::string errors< tao_json_pegtl::eof >::error_message /*__attribute__(( weak ))*/ = "unexpected character after JSON value";

      } // internal

   } // json

} // tao
#include <tao/json.hpp>
using namespace tao;
#include <glib.h>
#include <gst/gst.h>
#include <gst/gstsample.h>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
namespace po = boost::program_options;
#include "application.h"
#include "plegapp.h"
#include "thread.h"
#include "exception.h"
#include <algorithm>
#include <memory>
#include <libgen.h>
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

using namespace std;

int retval = 127;
const char *nullstr = "";
namespace Pleg {
PlegServer *main_server;
Application<PlegApplication> *app;
}
using namespace Pleg;
/**
 * @brief main loop of Server
 * @param argc int
 * @param argv char*[]
 * @return int
 */
int main(int argc, char *argv[])
{
    Debug() << "Main Thread: " << this_thread::get_id();

    string server("Server");
    string client("Client");
    string control("Control");
    string trace("trace.json");
    string executable(basename(argv[0]));

    Debug() << argv[0];

    do{

    if(algorithm::find_head(server,executable.length()) == executable){
        GStreamer::gst_initializer init(&argc, &argv);

        string trace;
        int port;

        po::options_description desc(server);
        desc.add_options()
            ("help", "produce help message")
            ("devices-config,f"    ,po::value<string>(Config::devices_config_file)->default_value("devices.json")       ,"Devices config file path.")
            ("gstreamer-config,g"  ,po::value<string>(Config::gstreamer_config_file)->default_value("gstreamer.json")   ,"gstreamer config file path.")
            ("files-config,o"      ,po::value<string>(Config::files_config_file)->default_value("files.json")           ,"files config file path.")
            ("debug,d"                                                                                                  ,"Prompt to continue")
            ("all,a"                                                                                                    ,"Connect all sources in config at startup.")
            ("gstreamer,G"                                                                                              ,"Connect all gstreamer sources in config at startup.")
            ("mock,m"                                                                                                   ,"Connect mock source at startup.")
            ("trace,t"             ,po::value<string>(trace)->default_value("trace.json")                               ,"Attach trace process.")
            ("port,p"              ,po::value<int>(port)->default_value(4999)                                           ,"Server port")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(ac, av, desc), vm);
        po::notify(vm);

        if(vm.count("trace")){
            Tracer::startTrace(trace);
        }

        if(vm.count("debug")){
            scanf("Ready?");
        }

        Server a(argc,argv,port);
        app = &a;
        main_server = &a;
        Server &server(a);

        string sz_all("all");
        if(vm.count("mock")){
            app->startBluetooth("mock");
        }else if(all){
            ThreadWorker *worker = app->startBluetooth("sources");
            while(!worker->getThread()->isStarted())
                this_thread::yield();
            make_pod_event(Event::Type::BluetoothConnectDevices,"connectDevices",sz_all)->send(worker->getThread());
        }

        if(gst){
            ThreadWorker *worker = app->startGStreamer("gstreamer");
            while(!worker->getThread()->isStarted())
                this_thread::yield();
            make_pod_event(Event::Type::GstConnectDevices,"connectPipes",sz_all)->send(worker->getThread());
        }

        server.start();

        try{
            retval = 0;
            a.exec();
        }catch(Exception &e){
            Debug() << e.message;
            retval = 1;
            break;
        }

        if(vm.count("trace")){
            Tracer::endTrace();
        }

        std::cout << std::endl;

        break;
    }else if(executable.startsWith(client)){
        TestLoop a(argc, argv);

        app = &a;

        string get,post,patch,host,port,bluetooth,trace;
        vector<string> headers;
        int repeats;

        po::options_description desc(server);
        desc.add_options()
            ("help", "produce help message")
            ("get,g"               ,po::value<string>(get)->default_value("/status")                                    ,"GET test [/status].")
            ("post,p"              ,po::value<string>(post)->default_value("/status")                                   ,"POST test [/status].")
            ("patch,s"             ,po::value<string>(patch)->default_value("/status")                                  ,"PATCH test [/status].")
            ("udp,u"                                                                                                    ,"Run the UDP test.")
            ("debug,d"                                                                                                  ,"Prompt to continue")
            ("host,h"              ,po::value<string>(host)->default_value("localhost")                                 ,"Host name of the server [localhost].")
            ("port,P"              ,po::value<string>(port)->default_value("4999")                                      ,"Port on the server.")
            ("json,j"                                                                                                   ,"Content-Type: text/json")
            ("headers,H"           ,po::value<vector<string>>(headers)                                                  ,"headers for request.")
            ("repeats,r"           ,po::value<int>(repeats)->default_value(1)                                           ,"Number of repeated attempts")
            ("uri,U"                                                                                                    ,"Test UriParseFunc.")
            ("bluetooth,B"         ,po::value<string>(bluetooth)->default_value("scan")                                 ,"Bluetooth test")
            ("scan,S"                                                                                                   ,"Scan for bluetooth devices.")
            ("devices-config,f"    ,po::value<string>(Config::devices_config_file)->default_value("devices.json")       ,"Devices config file path.")
            ("trace,t"             ,po::value<string>(trace)->default_value("trace.json")                               ,"Attach trace process.")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(ac, av, desc), vm);
        po::notify(vm);

        if(debug){
            scanf("Ready?");
        }

        Bluetooth *bluet;

        guint16 i(0);
        vector<string> hdr;
        algorithm::split(hdr,parser.value(headersOption),is_any_of("\n"),algorithm::token_compress_on);
        if(json){
            hdr.push_back("Content-Type: text/json");
        }

        if(vm.count("trace")){
            Tracer::startTrace(trace);
        }

        if(vm.count("get")){
            for(i=0;i<repeat;i++){
                Thread *thread = new Thread("GET");
                Test *test(new Test(thread,host,port,GET));
                test->setRelativeUrl(get)->setHeaders(hdr)->getThread();
                app->addThread(thread,true);
            }
        }
        if(vm.count("post")){
            for(i=0;i<repeat;i++){
                Thread *thread = new Thread("POST");
                Test *test(new Test(thread,host,port,POST));
                test->setRelativeUrl(post)->setHeaders(hdr)->getThread();
                app->addThread(thread,true);
            }
        }
        if(vm.count("patch")){
            hdr << "X-Method: PATCH";
            for(i=0;i<repeat;i++){
                Thread *thread = new Thread("PATCH");
                Test *test(new Test(thread,host,port,PATCH));
                test->setRelativeUrl(patch)->setHeaders(hdr)->getThread();
                app->addThread(thread,true);
            }
        }
        if(vm.count("udp")){
            Thread *thread = new Thread("UDP");
            new Test<asio::ip::udp>(thread,host,port,UDP);
            app->addThread(thread,true);
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

        retval = 0;
        a.exec();

        if(vm.count("trace")){
            Tracer::endTrace();
        }

        std::cout << std::endl;

        break;
    }

    }while(false);

    Debug() << __func__ << "returning" << retval;

    return retval;
}
