#include <pleg.h>
using namespace Pleg;
#include "factory.h"
#include "event.h"
#include "transform.h"
#include "response.h"
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

using namespace Transforms;
namespace Factory {

    /**
     * @brief mutex : to protect malloc implementation
     */
    recursive_mutex factory_mutex;

    /**
     * @brief transforms : container for Transform
     */
    transforms_type transforms;

    /**
     * @brief remove : call delete and remove a factory object
     * @param transform
     */

    /*
     * defines a thread-safe class factory for Passthrough
     */
    defFactory(Transform,Passthrough,Response,transforms)
    defRemove(Transform,Passthrough,transforms)

    namespace Response {
        /**
         * @brief create a Response that will service the Request
         * @param that Request*
         * @return Response*
         */
        Response *create(Request *that)
        {
            lock_guard l(&factory_mutex); \
            Response *response(nullptr);

            switch(that->getVerb()){
            case Route::HEAD:
                response = new Head(that);
                break;
            case Route::CONTROL:
            case Route::PATCH:
                response = new Patch(that);
                break;
            case Route::GET:
                response = new Get(that);
                break;
            case Route::POST:
                response = new Post(that);
                break;
            case Route::CATCH:
            {
                filesystem::path p(that->getUrl());
                if(filesystem::exists(p)){
                    vector<string> parts;
                    algorithm::split(parts,that->getUrl(),is_any_of("/"),algorithm::token_compress_on);
                    Relevance *rel(that->getRelevance());
                    rel->arguments.clear();
                    rel->arguments.insert({"name",parts.back()});
                    parts.pop_back();
                    rel->params.clear();
                    rel->params.insert({"r",parts.join("/")});
                    response = new Get(that);
                }else{
                    response = new Dummy(that);
                }
                break;
            }
            case Route::OPTIONS:
                response = new Options(that);
                break;
            case Route::NONE:
            default:
                throw new Exception("Factory shouldn't go there.");
            }
            ResponseWriter *writer = new ResponseWriter(response);
            return response->setWriter(writer);
        }

    }

}
