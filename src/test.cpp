#include "test.h"

#include <algorithm>
#include <mutex>
using namespace std;
#include <boost/algorithm/string.hpp>
using namespace boost;
#include "drumlin/thread.h"
#include "drumlin/event.h"
#include "drumlin/thread.h"
#include "drumlin/cursor.h"
#include "drumlin/application.h"
#include "drumlin/connection.h"
using namespace drumlin;
#include "pleg.h"
using namespace Pleg;

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
