#include "event.h"
#include <drumlin.h>
#include "exception.h"
#include "thread.h"
#include "application.h"

namespace drumlin {

bool quietEvents = false;

void setQuietEvents(bool keepQuiet)
{
    quietEvents = keepQuiet;
}

/**
 * @brief operator << : qDebug stream operator
 * @param stream QDebug
 * @param event Event
 * @return QDebug
 */
logger &operator <<(logger &stream, const Event &event)
{
    stream << event.getName();
    return stream;
}

/**
 * specialization for const char
 */
template <> template<>
const char *ThreadEvent<const char>::getPointer<const char>()const
{
    return static_cast<const char*>(m_ptr);
}

/**
 * @brief Event::send : send the event to a thread queue
 * @param thread Thread*
 */
void Event::send(Thread *target)const
{
    target->post(const_cast<Event*>(static_cast<const Event*>(this)));
}

/**
 * @brief Event::punt : punt the event to the application queue
 */
void Event::punt()const
{
    drumlin::iapp->post(const_cast<Event*>(static_cast<const Event*>(this)));
}

} // namespace drumlin
