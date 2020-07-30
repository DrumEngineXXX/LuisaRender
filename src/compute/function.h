//
// Created by Mike Smith on 2020/7/10.
//

#pragma once

#include <map>
#include <memory>

#include <compute/statement.h>

namespace luisa::dsl {

class Function {
    
    friend class Variable;

private:
    std::vector<Variable> _arguments;
    std::vector<std::unique_ptr<Expression>> _expressions;
    std::vector<std::unique_ptr<Statement>> _statements;
    std::vector<const TypeDesc *> _used_structs;
    
    uint32_t _used_builtins{0u};
    uint32_t _uid_counter{0u};
    
    [[nodiscard]] uint32_t _get_uid() noexcept { return _uid_counter++; }

public:
    template<typename T>
    Variable arg() noexcept { return _arguments.emplace_back(this, type_desc<T>, _get_uid()); }
    
    Variable literal(std::string l) noexcept { return add_expression(std::make_unique<LiteralExpr>(this, std::move(l))); }
    
    template<typename T>
    void use() noexcept {
        auto desc = type_desc<T>;
        if (desc->type == TypeCatalog::STRUCTURE) {
            _used_structs.emplace_back(desc);
        } else {
            LUISA_WARNING("Type \"", to_string(desc), "\" is not a user-defined structure, usage ignored.");
        }
    }
    
    Variable thread_id() noexcept {
        Variable tid{this, type_desc<uint32_t>, BuiltinTag::THREAD_ID};
        if (auto bit = static_cast<uint32_t>(BuiltinTag::THREAD_ID); (_used_builtins & bit) == 0u) {
            _used_builtins |= bit;
            _arguments.emplace_back(tid);
        }
        return tid;
    }
    
    template<typename T>
    Variable var(Variable init) noexcept {
        Variable v{this, type_desc<T>, _get_uid()};
        add_statement(std::make_unique<DeclareStmt>(v, init));
        return v;
    }
    
    template<typename T>
    Variable var(std::string init_literal) noexcept {
        return var<T>(literal(std::move(init_literal)));
    }

#define MAKE_AUTO_VAR_DECLARE(func, T)                          \
    Variable func(Variable init) noexcept {                     \
        Variable v{this, type_desc_##T, _get_uid()};            \
        add_statement(std::make_unique<DeclareStmt>(v, init));  \
        return v;                                               \
    }                                                           \
    Variable func(std::string init_literal) noexcept {          \
        return func(literal(std::move(init_literal)));          \
    }                                                           \

    MAKE_AUTO_VAR_DECLARE(auto_var, auto)
    MAKE_AUTO_VAR_DECLARE(auto_const_var, auto_const)
    MAKE_AUTO_VAR_DECLARE(auto_ptr, auto_ptr)
    MAKE_AUTO_VAR_DECLARE(auto_ref, auto_ref)
    MAKE_AUTO_VAR_DECLARE(auto_const_ptr, auto_const_ptr)
    MAKE_AUTO_VAR_DECLARE(auto_const_ref, auto_const_ref)

#undef MAKE_AUTO_VAR_DECLARE
    
    [[nodiscard]] Variable add_expression(std::unique_ptr<Expression> expr) noexcept {
        return Variable{_expressions.emplace_back(std::move(expr)).get()};
    }
    
    void add_statement(std::unique_ptr<Statement> stmt) noexcept {
        _statements.emplace_back(std::move(stmt));
    }
    
    template<typename Block, std::enable_if_t<std::is_invocable_v<Block>, int> = 0>
    void block(Block &&def_block) noexcept {
        add_statement(std::make_unique<BlockBeginStmt>());
        def_block();
        add_statement(std::make_unique<BlockEndStmt>());
    }
    
    [[nodiscard]] const std::vector<Variable> &arguments() const noexcept { return _arguments; }
    [[nodiscard]] const std::vector<std::unique_ptr<Statement>> &statements() const noexcept { return _statements; }
    [[nodiscard]] const std::vector<const TypeDesc *> &used_structures() const noexcept { return _used_structs; }
};

}