#pragma once

#include "extension/extension.h"

namespace lbug {
namespace gremlin_extension {

class GremlinExtension final : public extension::Extension {
public:
    static constexpr char EXTENSION_NAME[] = "GREMLIN";

public:
    static void load(main::ClientContext* context);
};

} // namespace gremlin_extension
} // namespace lbug
