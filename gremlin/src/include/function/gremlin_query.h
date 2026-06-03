#pragma once

#include "function/function.h"

namespace lbug {
namespace gremlin_extension {

struct GremlinQueryFunction {
    static constexpr const char* name = "GREMLIN";

    static function::function_set getFunctionSet();
};

} // namespace gremlin_extension
} // namespace lbug
