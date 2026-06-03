#include "main/gremlin_extension.h"

#include "function/gremlin_query.h"
#include "main/client_context.h"
#include "main/database.h"

namespace lbug {
namespace gremlin_extension {

using namespace extension;

void GremlinExtension::load(main::ClientContext* context) {
    auto& db = *context->getDatabase();
    ExtensionUtils::addStandaloneTableFunc<GremlinQueryFunction>(db);
}

} // namespace gremlin_extension
} // namespace lbug

#if defined(BUILD_DYNAMIC_LOAD)
extern "C" {
#if defined(_WIN32)
#define INIT_EXPORT __declspec(dllexport)
#else
#define INIT_EXPORT __attribute__((visibility("default")))
#endif
INIT_EXPORT void init(lbug::main::ClientContext* context) {
    lbug::gremlin_extension::GremlinExtension::load(context);
}

INIT_EXPORT const char* name() {
    return lbug::gremlin_extension::GremlinExtension::EXTENSION_NAME;
}
}
#endif
