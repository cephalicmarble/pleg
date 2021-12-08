#define TAOJSON
#include "factory.h"

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include "drumlin/event.h"
#include "transform.h"
#include "response.h"
#include "pleg.h"

using namespace Transforms;
namespace Pleg {

    /**
     * @brief mutex : to protect malloc implementation
     */
    std::recursive_mutex factory_mutex;

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
}
defFactory(Transform,Passthrough,Response,transforms)
defRemove(Transform,Passthrough,transforms)
namespace Factory {
    namespace Response {
        /**
         * @brief create a Response that will service the Request
         * @param that Request*
         * @return Response*
         */
        Pleg::Response *create(Pleg::Request *that)
        {
            std::lock_guard<std::recursive_mutex> l(Pleg::factory_mutex); \
            Pleg::Response *response(nullptr);

            switch(that->getVerb()){
            case HEAD:
                response = new Head(that);
                break;
            case PATCH:
                response = new Patch(that);
                break;
            case GET:
                response = new Get(that);
                break;
            case POST:
                response = new Post(that);
                break;
            case CATCH:
            {
                boost::filesystem::path p(that->getUrl());
                if(boost::filesystem::exists(p)){
                    vector<string> parts;
                    string str(that->getUrl());
                    algorithm::split(parts,str,algorithm::is_any_of("/"),algorithm::token_compress_on);
                    Relevance *rel(that->getRelevance());
                    rel->arguments.clear();
                    rel->arguments.insert({"name",parts.back()});
                    parts.pop_back();
                    rel->params.clear();
                    rel->params.insert({"r",algorithm::join(parts,"/")});
                    response = new Get(that);
                }else{
                    response = new Catch(that);
                }
                break;
            }
            case OPTIONS:
                response = new Options(that);
                break;
            case NONE:
            default:
                throw new Exception("Factory shouldn't go there.");
            }
            ResponseWriter *writer = new ResponseWriter(response);
            return response->setWriter(writer);
        }
    }
}
