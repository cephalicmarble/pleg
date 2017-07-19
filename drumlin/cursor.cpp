#include <drumlin.h>
#include <tao/json.hpp>
using namespace tao;
#include "cursor.h"
#define UNW_LOCAL_ONLY
#include <cxxabi.h>
#include <cstdio>
#include <cstdlib>

#include "time.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <boost/regex.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
using namespace boost;
#include "thread.h"
#include "event.h"
#include <tao/json.hpp>
using namespace tao;

#ifndef _WIN32
#include <libunwind.h>
#else
#include <windows.h>
#include <psdk_inc/_dbg_LOAD_IMAGE.h>
#include <psdk_inc/_dbg_common.h>
#endif

using namespace std;

namespace drumlin {

namespace Tracer {

const char *Tracer::EndOfTrace = "\n\n\n";

Tracer *tracer = nullptr;

/**
 * @brief startTrace : initialize global pointer
 * @param filename string
 */
void startTrace(string filename)
{
    tracer = new Tracer(filename);
}

/**
 * @brief endTrace : write to file
 */
void endTrace()
{
    if(tracer!=nullptr)
        tracer->write();
}

/**
 * @brief Cursor::backtrace : using libunwind
 * @param n
 */
void Cursor::backtrace(int n) {
#ifndef _WIN32

    if(nullptr==tracer)
        return;

    unw_cursor_t cursor;
    unw_context_t context;

    // Initialize cursor to current frame for local unwinding.
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    std::stringstream cout;

//    cout << clock();

    // Unwind frames one by one, going up the frame stack.
    for(int i=1;i<=n && unw_step(&cursor) > 0;i++) {
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0) {
            break;
        }
        cout.setf(ios_base::hex, ios_base::basefield);
        cout.setf(ios_base::left, ios_base::adjustfield);
        cout << "0x" << pc << ":";

        char sym[256];
        if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
            char* nameptr = sym;
            int status;
            char* demangled = abi::__cxa_demangle(sym, nullptr, nullptr, &status);
            if (status == 0) {
                nameptr = demangled;
            }
            cout << " (" << nameptr << "+" << "0x" << offset << ")" << endl;
            free(demangled);
        } else {
            cout << " -- error: unable to obtain symbol name for this frame\n";
        }
    }

    cout << "\n";

    printf(cout.str().c_str());
    tracer->addBlock(cout.str().c_str());
#else
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    CONTEXT context;
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);

    SymInitialize(process, NULL, TRUE);

    DWORD image;
    STACKFRAME64 stackframe;
    ZeroMemory(&stackframe, sizeof(STACKFRAME64));

#ifdef _M_IX86
    image = IMAGE_FILE_MACHINE_I386;
    stackframe.AddrPC.Offset = context.Eip;
    stackframe.AddrPC.Mode = AddrModeFlat;
    stackframe.AddrFrame.Offset = context.Ebp;
    stackframe.AddrFrame.Mode = AddrModeFlat;
    stackframe.AddrStack.Offset = context.Esp;
    stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    image = IMAGE_FILE_MACHINE_AMD64;
    stackframe.AddrPC.Offset = context.Rip;
    stackframe.AddrPC.Mode = AddrModeFlat;
    stackframe.AddrFrame.Offset = context.Rsp;
    stackframe.AddrFrame.Mode = AddrModeFlat;
    stackframe.AddrStack.Offset = context.Rsp;
    stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
    image = IMAGE_FILE_MACHINE_IA64;
    stackframe.AddrPC.Offset = context.StIIP;
    stackframe.AddrPC.Mode = AddrModeFlat;
    stackframe.AddrFrame.Offset = context.IntSp;
    stackframe.AddrFrame.Mode = AddrModeFlat;
    stackframe.AddrBStore.Offset = context.RsBSP;
    stackframe.AddrBStore.Mode = AddrModeFlat;
    stackframe.AddrStack.Offset = context.IntSp;
    stackframe.AddrStack.Mode = AddrModeFlat;
#endif

    for (int i = 0; i < n; i++) {

        BOOL result = StackWalk64(
                    image, process, thread,
                    &stackframe, &context, NULL,
                    SymFunctionTableAccess64, SymGetModuleBase64, NULL);

        if (!result) { break; }

        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        cout << "0x" << n << ":";
        DWORD64 displacement = 0;
        if (SymFromAddr(process, stackframe.AddrPC.Offset, &displacement, symbol)) {
            cout << " (" << symbol->Name << "+" << "0x" << 0 << ")" << endl;
        } else {
            cout << " -- error: unable to obtain symbol name for this frame\n";
        }

    }

    SymCleanup(process);
#endif
}

/**
 * @brief Tracer::Tracer
 * @param _filename string
 */
Tracer::Tracer(string _filename) :
    filename(_filename)
{
    roll = new json::value(json::empty_array);
    chain = new json::value(json::empty_array);
}

Tracer::~Tracer()
{
    delete roll;
    delete chain;
}

/**
 * @brief Tracer::addBlock
 * @param block string
 */
void Tracer::addBlock(string block)
{
//    quint32 clock(lines.front().toDouble());
//    lines.pop_front(); //clock
    list<string> lines;
    algorithm::split(lines,block,algorithm::is_any_of("\n"),algorithm::token_compress_on);
    string line;
    json::value callchain(json::empty_array);
    while((line = lines.front())!=""){
        lines.pop_front();
        boost::regex rx("0x[0-9a-f]+: \\(([^ ]+ )?(([^:]+::)+[^\\(]+)\\([^\\)]+\\)\\+0x[0-9a-f]+\\)",boost::regex::icase);
        boost::smatch cap;
        if(!boost::regex_match(line,cap,rx))
            continue;
        string name(cap[cap.length()-1]);
        auto _roll = roll->get_array();
        auto it = std::find_if(_roll.begin(),_roll.end(),[name](json::value::array_t::value_type &rollcall){
            return name == rollcall.get_object().at("name").get_string();
        });
        if(it != _roll.end()){
            callchain.append({ std::distance(_roll.begin(),it)});
        }else{
            roll->get_array().push_back( { { { "name", name }, { "module", "" } } } );
            callchain.append({ std::distance(_roll.begin(),_roll.end())});
        }
    }
    chain->get_array().push_back(json::value{ { "callchain", callchain }, { "cost", json::value::array_t{ 1 } } });
}

/**
 * @brief Tracer::write
 */
void Tracer::write()
{
    std::ostream &ostream(cout);
    std::fstream fstream;
    std::ostream *out;
    if(filename == "-"){
        out = &ostream;
    }else{
        fstream.open(filename.c_str(),std::ios_base::out);
        out = &fstream;
    }
    std::ostream &file(*out);
    file << "{\"version\":0,\"costs\":[{\"description\":\"elapsed\"},{\"unit\":\"ns\"}],\"functions\":" << json::to_string(*roll) << ",\"events\":" << json::to_string(*chain) << "}";
}

} // namespace Tracer

} // namespace drumlin
