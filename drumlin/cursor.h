#ifndef CURSOR_H_
#define CURSOR_H_

#include "thread.h"
#include <signalhandler.h>
#include "tao_forward.h"
using namespace tao;

namespace drumlin {

namespace Tracer {

class Tracer;

extern Tracer *tracer;

extern void startTrace(string filename);
extern void endTrace();

/**
 * @brief The Cursor class : defines tracepoints
 */
class Cursor
{
    static const int depth = 100;
public:
    static void backtrace(int d = depth);
};

/**
 * @brief The Tracer class : collects them
 */
class Tracer
{
public:
    static const char *EndOfTrace;
private:
    string filename;
    json::value *roll;
    json::value *chain;
public:
    Tracer(string _filename);
    ~Tracer();
    void addBlock(string block);
    void write();
};

#ifdef DRUMLIN_TRACE
#define tracepoint drumlin::Cursor::backtrace();
#define tracepoint_(n) drumlin::Cursor::backtrace(n);
#else
#define tracepoint
#define tracepoint_(n)
#endif
} // namespace Tracer

} // namespace drumlin

#endif // CURSOR_H_
