#include "function/gremlin_query.h"

#include <cctype>
#include <sstream>

#include "common/exception/runtime.h"
#include "function/table/bind_data.h"
#include "function/table/bind_input.h"
#include "function/table/simple_table_function.h"
#include "function/table/table_function.h"

namespace lbug {
namespace gremlin_extension {

using namespace lbug::common;
using namespace lbug::function;
using namespace lbug::main;

namespace {

struct GremlinQueryBindData final : TableFuncBindData {
    std::string query;

    explicit GremlinQueryBindData(std::string query)
        : TableFuncBindData{binder::expression_vector{}, 0 /* maxOffset */},
          query{std::move(query)} {}

    std::unique_ptr<TableFuncBindData> copy() const override {
        return std::make_unique<GremlinQueryBindData>(*this);
    }
};

struct Traversal {
    std::string hasKey;
    std::string hasValue;
    std::vector<std::string> outLabels;
    std::string valuesKey;
};

class GremlinParser {
public:
    explicit GremlinParser(std::string query) : query{std::move(query)} {}

    Traversal parse() {
        consumeWhitespace();
        consumeToken("g");
        consumeToken(".");
        consumeCall("V");
        consumeWhitespace();
        Traversal traversal;
        while (!isAtEnd()) {
            consumeToken(".");
            const auto step = parseIdentifier();
            consumeWhitespace();
            consumeToken("(");
            if (step == "has") {
                if (!traversal.hasKey.empty()) {
                    throw RuntimeException{"GREMLIN supports a single has(key, value) step."};
                }
                traversal.hasKey = parseString();
                consumeWhitespace();
                consumeToken(",");
                traversal.hasValue = parseString();
            } else if (step == "out") {
                traversal.outLabels.push_back(parseString());
            } else if (step == "values") {
                traversal.valuesKey = parseString();
                consumeWhitespace();
                consumeToken(")");
                consumeWhitespace();
                if (!isAtEnd()) {
                    throw RuntimeException{"GREMLIN values(key) must be the final step."};
                }
                validate(traversal);
                return traversal;
            } else {
                throw RuntimeException{"GREMLIN supports only has(key, value), out(label), and "
                                       "values(key) after g.V()."};
            }
            consumeWhitespace();
            consumeToken(")");
            consumeWhitespace();
        }
        validate(traversal);
        return traversal;
    }

private:
    bool isAtEnd() const { return pos >= query.size(); }

    void consumeWhitespace() {
        while (!isAtEnd() && std::isspace(static_cast<unsigned char>(query[pos]))) {
            pos++;
        }
    }

    void consumeToken(const std::string& token) {
        consumeWhitespace();
        if (query.substr(pos, token.size()) != token) {
            throw RuntimeException{"Invalid GREMLIN traversal near '" + query.substr(pos) + "'."};
        }
        pos += token.size();
    }

    void consumeCall(const std::string& name) {
        consumeToken(name);
        consumeToken("(");
        consumeToken(")");
    }

    std::string parseIdentifier() {
        consumeWhitespace();
        const auto start = pos;
        while (!isAtEnd() &&
               (std::isalnum(static_cast<unsigned char>(query[pos])) || query[pos] == '_')) {
            pos++;
        }
        if (start == pos) {
            throw RuntimeException{"Expected GREMLIN step name."};
        }
        return query.substr(start, pos - start);
    }

    std::string parseString() {
        consumeWhitespace();
        if (isAtEnd() || (query[pos] != '"' && query[pos] != '\'')) {
            throw RuntimeException{"Expected GREMLIN string literal."};
        }
        const auto quote = query[pos++];
        std::string result;
        while (!isAtEnd()) {
            const auto ch = query[pos++];
            if (ch == quote) {
                return result;
            }
            if (ch == '\\') {
                if (isAtEnd()) {
                    throw RuntimeException{"Unterminated GREMLIN string escape."};
                }
                result.push_back(query[pos++]);
            } else {
                result.push_back(ch);
            }
        }
        throw RuntimeException{"Unterminated GREMLIN string literal."};
    }

    static void validate(const Traversal& traversal) {
        if (traversal.hasKey.empty() || traversal.valuesKey.empty()) {
            throw RuntimeException{
                "GREMLIN traversal must contain has(key, value) and final values(key) steps."};
        }
    }

private:
    std::string query;
    size_t pos = 0;
};

static std::string quoteIdentifier(const std::string& identifier) {
    std::string result = "`";
    for (const auto ch : identifier) {
        if (ch == '`') {
            result += "``";
        } else {
            result.push_back(ch);
        }
    }
    result += "`";
    return result;
}

static std::string quoteStringLiteral(const std::string& value) {
    std::string result = "'";
    for (const auto ch : value) {
        if (ch == '\'') {
            result += "\\'";
        } else if (ch == '\\') {
            result += "\\\\";
        } else {
            result.push_back(ch);
        }
    }
    result += "'";
    return result;
}

static std::string translateToCypher(const std::string& gremlinQuery) {
    const auto traversal = GremlinParser{gremlinQuery}.parse();
    std::ostringstream cypher;
    cypher << "MATCH (v0";
    for (auto i = 0u; i < traversal.outLabels.size(); i++) {
        cypher << ")-[:" << quoteIdentifier(traversal.outLabels[i]) << "]->(v" << (i + 1);
    }
    cypher << ") WHERE v0." << quoteIdentifier(traversal.hasKey) << " = "
           << quoteStringLiteral(traversal.hasValue) << " RETURN v" << traversal.outLabels.size()
           << "." << quoteIdentifier(traversal.valuesKey) << " AS "
           << quoteIdentifier(traversal.valuesKey) << ";";
    return cypher.str();
}

static std::unique_ptr<TableFuncBindData> bindFunc(ClientContext* /*context*/,
    const TableFuncBindInput* input) {
    return std::make_unique<GremlinQueryBindData>(input->getLiteralVal<std::string>(0));
}

static std::string rewriteQuery(ClientContext& /*context*/, const TableFuncBindData& bindData) {
    return translateToCypher(bindData.constPtrCast<GremlinQueryBindData>()->query);
}

} // namespace

function_set GremlinQueryFunction::getFunctionSet() {
    function_set functionSet;
    auto func = std::make_unique<TableFunction>(name, std::vector{LogicalTypeID::STRING});
    func->tableFunc = TableFunction::emptyTableFunc;
    func->bindFunc = bindFunc;
    func->initSharedStateFunc = SimpleTableFunc::initSharedState;
    func->initLocalStateFunc = TableFunction::initEmptyLocalState;
    func->rewriteFunc = rewriteQuery;
    func->canParallelFunc = [] { return false; };
    functionSet.push_back(std::move(func));
    return functionSet;
}

} // namespace gremlin_extension
} // namespace lbug
