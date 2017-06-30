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

#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QThread>
#include <QCommandLineParser>
#include <QException>
#include <algorithm>
#include <memory>
#include <libgen.h>
#include "server.h"
#include "bluetooth.h"
#include "gstreamer.h"
#include "source.h"
#include "test.h"
#include "uri.h"
#include "event.h"
#include "jsonmodel.h"
#include "control.h"
#include "cursor.h"
#include "gstreamer.h"
#include "jsonmodel.h"
#include "jsonconfig.h"
#include "qsvideowidget.h"
#include "httprequest.h"
#include "application.h"

using namespace std;

int retval = 127;
const char *nullstr = "";
namespace QS {
QSServer *main_server;
QSApplication<QCoreApplication> *app;
QSApplication<QApplication> *gui;
}
/**
 * @brief main loop of QSServer
 * @param argc int
 * @param argv char*[]
 * @return int
 */
int main(int argc, char *argv[])
{
    qDebug() << "Main Thread: " << QThread::currentThread();

    qRegisterMetaType<QSEvent>("QSEvent");
    qRegisterMetaType<QEvent*>("QEvent*");
    qRegisterMetaType<QSEvent*>("QSEvent*");
    qRegisterMetaType<QSEvent::Type>("QSEvent::Type");
    qRegisterMetaType<json::value*>("json::value*");
#if defined(QTGSTREAMER)
    qRegisterMetaType<QGst::Ui::GraphicsVideoSurface*>("QGst::Ui::GraphicsVideoSurface*");
    qRegisterMetaType<QGst::SamplePtr>("QGst::SamplePtr");
    qmlRegisterType<QGst::Quick::VideoItem>("com.affectivestate.qs", 0, 1, "VideoItem");
#endif
#if defined(GSTREAMER)
#endif
#ifdef QSCONTROL
    qmlRegisterType<Models::QSJsonModel>("com.affectivestate.qs", 0, 1, "JsonModel");
    qmlRegisterType<QSControl>("com.affectivestate.qs", 0, 1, "QSControl");
    qmlRegisterType<QSVideoWidget>("com.affectivestate.qs", 0, 1, "QSVideoWidget");
    qmlRegisterType<QSHttpRequest>("com.affectivestate.qs", 0, 1, "HttpRequest");
    qRegisterMetaType<QSHttpRequest*>("HttpRequest");
#endif

    QString server("QSServer");
    QString client("QSClient");
    QString control("QSControl");
    QString trace("trace.json");
    QString executable(basename(argv[0]));

    qDebug() << argv[0];

    do{

    if(executable.startsWith(server)){
#if defined(GSTREAMER)
        GStreamer::gst_initializer init(&argc, &argv);
#elif defined(QTGSTREAMER)
        QGst::init(&argc, &argv);
#endif
        QSServer a(argc,argv);
        app = &a;

        QCoreApplication::setApplicationName(server);
        QCoreApplication::setApplicationVersion("O_o");

        QCommandLineParser parser;
        parser.setApplicationDescription(server);
        parser.addHelpOption();
        parser.addVersionOption();

        // (-f, --devices-config)
        QCommandLineOption configOption(QStringList() << "f" << "devices-config",
             QCoreApplication::translate("devices-config", "Devices config file path."),
             "devices-config","devices.json");
        parser.addOption(configOption);

        // (-g, --gstreamer-config)
        QCommandLineOption gconfigOption(QStringList() << "g" << "gstreamer-config",
             QCoreApplication::translate("gstreamer-config", "gstreamer config file path."),
             "gstreamer-config","gstreamer.json");
        parser.addOption(gconfigOption);

        // (-o, --files-config)
        QCommandLineOption filesOption(QStringList() << "o" << "files-config",
             QCoreApplication::translate("files-config", "files config file path."),
             "files-config","files.json");
        parser.addOption(filesOption);

        // (-d, --debug)
        QCommandLineOption debugOption(QStringList() << "d" << "debug",
             QCoreApplication::translate("debug", "Prompt to continue."));
        parser.addOption(debugOption);

        // (-a, --all)
        QCommandLineOption allOption(QStringList() << "a" << "all",
             QCoreApplication::translate("all", "Connect all sources in config at startup."));
        parser.addOption(allOption);

#if defined(GSTREAMER) || defined(QTGSTREAMER)
        // (-G, --gstreamer)
        QCommandLineOption gstOption(QStringList() << "G" << "gstreamer",
             QCoreApplication::translate("gstreamer", "Connect all gstreamer sources in config at startup."));
        parser.addOption(gstOption);
#endif
        // (-m, --mock)
        QCommandLineOption mockOption(QStringList() << "m" << "mock",
             QCoreApplication::translate("mock", "Connect mock source at startup."));
        parser.addOption(mockOption);

        // (-t, --trace)
        QCommandLineOption traceOption(QStringList() << "t" << "trace",
             QCoreApplication::translate("trace", "Attach trace process."),
            "trace","trace.json");
        parser.addOption(traceOption);

        parser.process(a);

        const QStringList args = parser.positionalArguments();

        Config::devices_config_file = parser.value(configOption).toStdString();
        Config::gstreamer_config_file = parser.value(gconfigOption).toStdString();
        Config::files_config_file = parser.value(filesOption).toStdString();

        bool all = parser.isSet(allOption);
#if defined(GSTREAMER) || defined(QTGSTREAMER)
        bool gst = parser.isSet(gstOption);
#endif
        bool output_trace = parser.isSet(traceOption);
        bool debug = parser.isSet(debugOption);

        if(output_trace){
            Tracer::startTrace(trace);
        }

        if(debug){
            scanf("Ready?");
        }

        main_server = &a;
        QSServer &server(a);

        QString sz_all("all");
        if(parser.isSet(mockOption)){
            server.startBluetooth("mock");
        }else if(all){
            QSThreadWorker *worker = server.startBluetooth("sources");
            while(!worker->getQSThread()->isStarted())
                QSThread::yieldCurrentThread();
            make_pod_event(QSEvent::Type::BluetoothConnectDevices,"connectDevices",sz_all)->send(worker->getQSThread());
        }

#if defined(GSTREAMER) || defined(QTGSTREAMER)
        if(gst){
            QSThreadWorker *worker = server.startGStreamer("gstreamer");
            while(!worker->getQSThread()->isStarted())
                QSThread::yieldCurrentThread();
            make_pod_event(QSEvent::Type::GstConnectDevices,"connectPipes",sz_all)->send(worker->getQSThread());
        }
#endif

        server.start();

        try{
            retval = 0;
            a.exec();
        }catch(QSException &e){
            qDebug() << e.message;
            retval = 1;
            break;
        }

        if(output_trace){
            Tracer::endTrace();
        }

#if defined(QGSTREAMER)
        QGst::cleanup();
#endif
        std::cout << std::endl;

        break;
    }else if(executable.startsWith(client)){
        QSTestLoop a(argc, argv);

        app = &a;

        QCoreApplication::setApplicationName(client);
        QCoreApplication::setApplicationVersion("O_o");

        QCommandLineParser parser;
        parser.setApplicationDescription(client);
        parser.addHelpOption();
        parser.addVersionOption();

        // (-d, --debug)
        QCommandLineOption debugOption(QStringList() << "d" << "debug",
             QCoreApplication::translate("debug", "Prompt to continue."));
        parser.addOption(debugOption);

        // (-g, --get)
        QCommandLineOption getOption(QStringList() << "g" << "get",
             QCoreApplication::translate("get", "Run the GET test."),
             "get","/status");
        parser.addOption(getOption);

        // (-p, --post)
        QCommandLineOption postOption(QStringList() << "p" << "post",
             QCoreApplication::translate("post", "Run the POST test."),
             "post","/status");
        parser.addOption(postOption);

        // (-s, --patch)
        QCommandLineOption patchOption(QStringList() << "s" << "patch",
             QCoreApplication::translate("patch", "Run the PATCH test."),
             "patch","/status");
        parser.addOption(patchOption);

        // (-u, --udp)
        QCommandLineOption udpOption(QStringList() << "u" << "udp",
             QCoreApplication::translate("udp", "Run the UDP test."));
        parser.addOption(udpOption);

        // (-h, --host)
        QCommandLineOption hostOption(QStringList() << "h" << "host",
             QCoreApplication::translate("host", "Host name of the server [localhost]."),
             "host","localhost");
        parser.addOption(hostOption);

        // (-P, --port)
        QCommandLineOption portOption(QStringList() << "P" << "port",
             QCoreApplication::translate("port", "Port on the server."),
             "port","4999");
        parser.addOption(portOption);

        // (-j, --json)
        QCommandLineOption jsonOption(QStringList() << "j" << "json",
             QCoreApplication::translate("json", "Content-Type: text/json"));
        parser.addOption(jsonOption);

        // (-H, --headers)
        QCommandLineOption headersOption(QStringList() << "H" << "headers",
             QCoreApplication::translate("headers", "headers for request."),
             "headers","");
        parser.addOption(headersOption);

        // (-r, --repeats)
        QCommandLineOption repeatOption(QStringList() << "r" << "repeats",
             QCoreApplication::translate("repeats", "Number of repeated attempts."),
             "repeats","1");
        parser.addOption(repeatOption);

        // (-U, --uri)
        QCommandLineOption uriOption(QStringList() << "U" << "uri",
             QCoreApplication::translate("uri", "Test UriParseFunc."));
        parser.addOption(uriOption);

        // (-B, --bluetooth)
        QCommandLineOption blueOption(QStringList() << "B" << "bluetooth",
             QCoreApplication::translate("bluetooth", "Bluetooth test funtionality."),
             "bluetooth","scan");
        parser.addOption(blueOption);

        // (-S, --scan)
        QCommandLineOption scanOption(QStringList() << "S" << "scan",
             QCoreApplication::translate("scan", "Scan for bluetooth devices."));
        parser.addOption(scanOption);

        // (-f, --config)
        QCommandLineOption configOption(QStringList() << "f" << "config",
             QCoreApplication::translate("config", "Config file path."),
             "config","devices.json");
        parser.addOption(configOption);

        // (-t, --trace)
        QCommandLineOption traceOption(QStringList() << "t" << "trace",
             QCoreApplication::translate("trace", "Attach trace process."),
            "trace","trace.json");
        parser.addOption(traceOption);

        parser.process(a);

        const QStringList args = parser.positionalArguments();

        QString host = parser.isSet(hostOption)?parser.value(hostOption):"localhost";
        quint16 port = parser.isSet(portOption)?parser.value(portOption).toInt():4999;
        quint16 repeat = parser.isSet(repeatOption)?parser.value(repeatOption).toInt():1;
        QString blue = parser.isSet(blueOption)?parser.value(blueOption):"scan";
        QString config = parser.isSet(configOption)?parser.value(configOption):"devices.json";
        QString get = parser.isSet(getOption)?parser.value(getOption):"/status";
        QString post = parser.isSet(postOption)?parser.value(postOption):"/status";
        QString patch = parser.isSet(patchOption)?parser.value(patchOption):"/status";
        bool output_trace = parser.isSet(traceOption);
        QString trace = parser.isSet(traceOption)?parser.value(traceOption):"";
        bool json = parser.isSet(jsonOption);
        bool debug = parser.isSet(debugOption);

        Config::devices_config_file = "./devices.json";
        Config::gstreamer_config_file = "./gstreamer.json";

        if(debug){
            scanf("Ready?");
        }

        QSBluetooth *bluet;

        quint16 i(0);
        QStringList hdr(parser.value(headersOption).split("\n",QString::SplitBehavior::SkipEmptyParts));
        if(json){
            hdr << "Content-Type: text/json";
        }

        if(output_trace){
            Tracer::startTrace(trace);
        }

        if(parser.isSet(getOption)){
            for(i=0;i<repeat;i++){
                QSThread *thread = new QSThread("GET");
                QSTest *test(new QSTest(thread,host,port,GET));
                test->setRelativeUrl(get)->setHeaders(hdr)->getQSThread();
                app->addThread(thread,true);
            }
        }
        if(parser.isSet(postOption)){
            for(i=0;i<repeat;i++){
                QSThread *thread = new QSThread("POST");
                QSTest *test(new QSTest(thread,host,port,POST));
                test->setRelativeUrl(post)->setHeaders(hdr)->getQSThread();
                app->addThread(thread,true);
            }
        }
        if(parser.isSet(patchOption)){
            hdr << "X-Method: PATCH";
            for(i=0;i<repeat;i++){
                QSThread *thread = new QSThread("PATCH");
                QSTest *test(new QSTest(thread,host,port,PATCH));
                test->setRelativeUrl(patch)->setHeaders(hdr)->getQSThread();
                app->addThread(thread,true);
            }
        }
        if(parser.isSet(udpOption)){
            QSThread *thread = new QSThread("UDP");
            new QSTest(thread,host,port,UDP);
            app->addThread(thread,true);
        }
        if(parser.isSet(uriOption)){
            bluet = a.startBluetooth("mock",config);
            bluet->defineMockSources();
        }
        if(parser.isSet(blueOption)){
            a.startBluetooth(blue.toStdString().c_str(),config);
        }
        if(parser.isSet(scanOption)){
            a.startBluetooth("scan","devices.json");
        }

        retval = 0;
        a.exec();

        if(output_trace){
            Tracer::endTrace();
        }

        std::cout << std::endl;

        break;
    }
#ifdef QSCONTROL
    else if(executable.startsWith(control)){
        QSControl controller(argc, argv);
        ::controller = &controller;
        gui = &controller;
        gui->setQuitOnLastWindowClosed(true);
#if defined(GSTREAMER)
        GStreamer::gst_initializer init(&argc, &argv);
#elif defined(QTGSTREAMER)
        QGst::init(&argc, &argv);
#endif
        controller.init();

        QQmlApplicationEngine engine(gui);

        engine.connect(&engine,SIGNAL(quit()),gui,SLOT(quit()));

        QCoreApplication::setApplicationName(control);
        QCoreApplication::setApplicationVersion("O_o");

        QCommandLineParser parser;
        parser.setApplicationDescription(control);
        parser.addHelpOption();
        parser.addVersionOption();

        // (-h, --host)
        QCommandLineOption hostOption(QStringList() << "h" << "host",
             QCoreApplication::translate("host", "Host name of the server [localhost]."),
             "host","localhost");
        parser.addOption(hostOption);

        // (-P, --port)
        QCommandLineOption portOption(QStringList() << "P" << "port",
             QCoreApplication::translate("port", "Port on the server."),
             "port","4999");
        parser.addOption(portOption);

        // (-q, --quiet)
        QCommandLineOption quietOption(QStringList() << "q" << "quiet",
             QCoreApplication::translate("quiet", "Quiet events."));
        parser.addOption(quietOption);

        parser.process(*gui);

        const QStringList args = parser.positionalArguments();

        QString host = parser.isSet(hostOption)?parser.value(hostOption):"localhost";
        QString port = parser.isSet(portOption)?parser.value(portOption):"4999";
        setQuietEvents(parser.isSet(quietOption));

#ifdef ANDROID
        QQuickView view;
        Notifications *notificationClient = new Notifications(&view);
        view.engine()->rootContext()->setContextProperty(QLatin1String("notificationClient"),                                                         notificationClient);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl(QStringLiteral("qrc:/main.qml")));
        view.show();
#endif

        engine.addImportPath("qrc:/");
        engine.addImportPath("qml/");
        engine.rootContext()->setContextProperty("rootUrl","http://"+host+":"+port);

        gui->installEventFilter(&controller);
        controller.setEngine(&engine);
        engine.rootContext()->setContextProperty("controller",&controller);

        QQmlComponent component(&engine, "qrc:/main.qml");
        QObject *main = component.create();
        if(!main){
            for(auto error : component.errors()){
                qDebug() << error;
            }
            return 1;
        }

        engine.rootContext()->setContextProperty("main",main);
        engine.rootContext()->setContextProperty("charts",false);

        retval = 0;
        controller.exec();

        std::cout << std::endl;

        break;
    }
#endif
    }while(false);

    qDebug() << __func__ << "returning" << retval;

    return retval;
}
