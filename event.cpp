#include "event.h"
#include <qs.h>
using namespace QS;
#include <QMetaMethod>
#include "exception.h"
#include <QDebug>
#include "thread.h"

bool quietEvents = false;

void setQuietEvents(bool keepQuiet)
{
    quietEvents = keepQuiet;
}

/**
 * @brief operator << : qDebug stream operator
 * @param stream QDebug
 * @param event QSEvent
 * @return QDebug
 */
QDebug operator <<(QDebug &stream, const QSEvent &event)
{
    stream << event.getName();
    return stream;
}

/**
 * specialization for const char
 */
template <> template<>
const char *QSThreadEvent<const char>::getPointer<const char>()const
{
    return static_cast<const char*>(ptr);
}

/**
 * @brief QSEvent::send : send the event to a thread queue
 * @param thread QSThread*
 */
void QSEvent::send(QSThread *target)const
{
    if(!quietEvents)
        qDebug() << "sending" << type() << ":" << getName() << "from" << QSThread::currentThread() << "to" << target << ":" << target->getWorker();
    if(type() > QSEvent::Type::Thread_first && type() < QSEvent::Type::Thread_last){
        target->metaObject()->invokeMethod(target,"event",Qt::AutoConnection,Q_ARG(QEvent*,const_cast<QEvent*>(static_cast<const QEvent*>(this))));
    /*}else if(type() > QSEvent::Type::first && type() < QSEvent::Type::last){
        target->getWorker()->metaObject()->invokeMethod(target->getWorker(),"event",Qt::AutoConnection,Q_ARG(QEvent*,const_cast<QEvent*>(static_cast<const QEvent*>(this))));*/
    }else{
        QCoreApplication::postEvent(target->getWorker(),const_cast<QSEvent*>(static_cast<const QSEvent*>(this)));
        //QMetaMethod::fromSignal(&QSThreadWorker::signalEventFilter).invoke(target->getWorker(),Qt::QueuedConnection,Q_ARG(QObject*,target->getWorker()),Q_ARG(QSEvent*,const_cast<QSEvent*>(static_cast<const QSEvent*>(this))));
    }
    QThread::yieldCurrentThread();
}

/**
 * @brief QSEvent::punt : punt the event to the application queue
 */
void QSEvent::punt()const
{
    if(!quietEvents)
        qDebug() << "punting" << getName() << ":" << type() << "to application";
    QCoreApplication::instance()->postEvent(QCoreApplication::instance(),const_cast<QEvent*>(static_cast<const QEvent*>(this)),Qt::HighEventPriority);
}
