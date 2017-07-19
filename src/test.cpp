#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include <algorithm>
using namespace std;
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
#include "thread.h"
#include "test.h"
#include "event.h"
#include "thread.h"
#include "cursor.h"
#include "application.h"
#include "connection.h"
#include "glib.h"
using namespace drumlin;

namespace Pleg {

///**
// * @brief TestLoop::startBluetooth : thread starter
// * @param task const char*
// * @param config tring const&
// * @return Bluetooth* (BluetoothScanner*) if !strcmp(task,"scan")
// */
//Bluetooth *TestLoop::startBluetooth(const char *task,const tring &config)
//{
//    tracepoint
//    QMutexLocker ml(&mutex);
//    Thread *thread(new Thread(task));
//    Bluetooth *bluet;
//    if(!strcmp(task,"scan")){
//        bluet = new BluetoothScanner(thread,config);
//    }else{
//        bluet = new Bluetooth(thread);
//    }
//    app->addThread(thread,true);
//    return bluet;
//}

TestLoop::TestLoop(int &argc,char *argv[])
    : Base(argc,argv)
{
    terminator = (Terminator*)1;
}

template class Test<asio::ip::tcp>;
template class SocketTestHandler<asio::ip::tcp>;
template class Test<asio::ip::udp>;
template class SocketTestHandler<asio::ip::udp>;

} // namespace Pleg
