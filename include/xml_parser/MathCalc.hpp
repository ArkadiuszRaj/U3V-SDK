#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace MathEvaluate {
    enum class Op {
        ADD, SUB, MUL, DIV, MOD, POW,
        BIT_AND, BIT_OR, BIT_XOR, BIT_NOT,
        LOG_AND, LOG_OR, LOG_NOT,
        LT, GT, LE, GE, EQ, NE, SHL, SHR,
        NEG,
        TERNARY_Q, TERNARY_COLON,
        COMMA,

        DUP, DROP, SWAP, OVER, ROT
    };

    enum class Func {
        SGN, NEG, ATAN, COS, SIN, TAN, ABS, EXP, LN, LG, SQRT,
        TRUNC, FLOOR, CEIL, ROUND, ASIN, ACOS, E
    };

    struct Number     { std::variant<double, int64_t> value; };
    struct Variable   { std::size_t id; };
    struct Function   { Func func; };
    struct Operator   { Op op; };
    struct ParenLeft  {};
    struct ParenRight {};
    struct Colon      {};

    using Token = std::variant<Number, Variable, Function, Operator, Colon, ParenLeft, ParenRight>;

    template<typename T>
    T fromNumber(const Number& num) {
        return std::visit([](auto v) -> T { return static_cast<T>(v); }, num.value);
    }

    template<typename T>
    class Parser {
        static int precedence(Op op) {
            switch (op) {
                case Op::COMMA:      return -1;
                case Op::TERNARY_Q:  return 0;
                case Op::LOG_OR:     return 1;
                case Op::LOG_AND:    return 2;
                case Op::BIT_OR:     return 3;
                case Op::BIT_XOR:    return 4;
                case Op::BIT_AND:    return 5;
                case Op::EQ: case Op::NE: return 6;
                case Op::LT: case Op::LE: case Op::GT: case Op::GE: return 7;
                case Op::SHL: case Op::SHR: return 8;
                case Op::ADD: case Op::SUB: return 9;
                case Op::MUL: case Op::DIV: case Op::MOD: return 10;
                case Op::POW:        return 11;
                case Op::NEG: case Op::BIT_NOT: case Op::LOG_NOT: return 12;
                default:             return 99;
            }
        }

        static bool rightAssoc(Op op) {
            return op == Op::POW || op == Op::NEG || op == Op::BIT_NOT || op == Op::LOG_NOT;
        }

        /*
         * Try to match an operator string starting at input[pos].
         * Uses greedy matching: tries longest possible operator first.
         * Handles XML entities: &amp; &lt; &gt;
         * Returns the Op and advances pos past the matched operator, or returns nullopt.
         */
        static std::optional<std::pair<Op, size_t>> matchOperator(const std::string& input, size_t pos) {
            size_t remaining = input.size() - pos;

            // Try XML entities first (they're longest)
            if (remaining >= 10 && input.substr(pos, 10) == "&amp;&amp;") return {{Op::LOG_AND, pos + 10}};
            if (remaining >= 5  && input.substr(pos, 5)  == "&amp;")      return {{Op::BIT_AND, pos + 5}};
            if (remaining >= 4  && input.substr(pos, 4)  == "&gt;")       return {{Op::GT,      pos + 4}};
            if (remaining >= 4  && input.substr(pos, 4)  == "&lt;")       return {{Op::LT,      pos + 4}};

            // Two-char operators
            if (remaining >= 2) {
                char c0 = input[pos], c1 = input[pos + 1];
                if (c0 == '!' && c1 == '=') return {{Op::NE,      pos + 2}};
                if (c0 == '&' && c1 == '&') return {{Op::LOG_AND, pos + 2}};
                if (c0 == '|' && c1 == '|') return {{Op::LOG_OR,  pos + 2}};
                if (c0 == '*' && c1 == '*') return {{Op::POW,     pos + 2}};
                if (c0 == '<' && c1 == '<') return {{Op::SHL,     pos + 2}};
                if (c0 == '<' && c1 == '=') return {{Op::LE,      pos + 2}};
                if (c0 == '<' && c1 == '>') return {{Op::NE,      pos + 2}};
                if (c0 == '>' && c1 == '>') return {{Op::SHR,     pos + 2}};
                if (c0 == '>' && c1 == '=') return {{Op::GE,      pos + 2}};
                if (c0 == '=' && c1 == '=') return {{Op::EQ,      pos + 2}};
            }

            // Single-char operators
            char c = input[pos];
            switch (c) {
                case '+': return {{Op::ADD,     pos + 1}};
                case '-': return {{Op::SUB,     pos + 1}};
                case '*': return {{Op::MUL,     pos + 1}};
                case '/': return {{Op::DIV,     pos + 1}};
                case '%': return {{Op::MOD,     pos + 1}};
                case '<': return {{Op::LT,      pos + 1}};
                case '>': return {{Op::GT,      pos + 1}};
                case '=': return {{Op::EQ,      pos + 1}};
                case '!': return {{Op::LOG_NOT, pos + 1}};
                case '&': return {{Op::BIT_AND, pos + 1}};
                case '|': return {{Op::BIT_OR,  pos + 1}};
                case '^': return {{Op::BIT_XOR, pos + 1}};
                case '~': return {{Op::BIT_NOT, pos + 1}};
                case '?': return {{Op::TERNARY_Q, pos + 1}};
                case ':': return {{Op::TERNARY_COLON, pos + 1}};
                default:  return std::nullopt;
            }
        }

        template<typename Table>
        static auto matchTable(const Table& table, std::string_view label)
            -> std::optional<typename std::decay_t<Table>::value_type::second_type>
        {
            for (const auto& [str, val] : table) {
                if (label == str) return val;
            }
            return std::nullopt;
        }

        std::vector<Token> tokenize(
            const std::string& input,
            std::function<std::optional<Token>(std::string_view)> convertId = {}
        ) {
            static const auto op_words = std::to_array({
                std::pair{"DROP", Op::DROP},
                std::pair{"DUP",  Op::DUP},
                std::pair{"OVER", Op::OVER},
                std::pair{"ROT",  Op::ROT},
                std::pair{"SWAP", Op::SWAP},
            });
            static const auto func_map = std::to_array({
                std::pair{"ABS",   Func::ABS},
                std::pair{"ACOS",  Func::ACOS},
                std::pair{"ASIN",  Func::ASIN},
                std::pair{"ATAN",  Func::ATAN},
                std::pair{"CEIL",  Func::CEIL},
                std::pair{"COS",   Func::COS},
                std::pair{"E",     Func::E},
                std::pair{"EXP",   Func::EXP},
                std::pair{"FLOOR", Func::FLOOR},
                std::pair{"LG",    Func::LG},
                std::pair{"LN",    Func::LN},
                std::pair{"NEG",   Func::NEG},
                std::pair{"ROUND", Func::ROUND},
                std::pair{"SGN",   Func::SGN},
                std::pair{"SIN",   Func::SIN},
                std::pair{"SQRT",  Func::SQRT},
                std::pair{"TAN",   Func::TAN},
                std::pair{"TRUNC", Func::TRUNC},
            });

            enum class Last { None, Value, Operator, LeftParen };
            Last last = Last::None;

            std::vector<Token> tokens;
            size_t i = 0;

            while (i < input.size()) {
                // Skip whitespace
                if (std::isspace(static_cast<unsigned char>(input[i]))) { ++i; continue; }

                // 1. Numbers: integers and floats, including hex (0x...)
                if (std::isdigit(static_cast<unsigned char>(input[i])) ||
                    (input[i] == '.' && i + 1 < input.size() &&
                     std::isdigit(static_cast<unsigned char>(input[i + 1])))) {

                    size_t start = i;
                    bool is_hex = false;
                    bool is_float = false;

                    // Check for hex prefix
                    if (input[i] == '0' && i + 1 < input.size() &&
                        (input[i + 1] == 'x' || input[i + 1] == 'X')) {
                        is_hex = true;
                        i += 2;
                        while (i < input.size() && std::isxdigit(static_cast<unsigned char>(input[i]))) ++i;
                    } else {
                        while (i < input.size() && std::isdigit(static_cast<unsigned char>(input[i]))) ++i;
                        if (i < input.size() && input[i] == '.') {
                            is_float = true;
                            ++i;
                            while (i < input.size() && std::isdigit(static_cast<unsigned char>(input[i]))) ++i;
                        }
                        // Scientific notation
                        if (i < input.size() && (input[i] == 'e' || input[i] == 'E')) {
                            is_float = true;
                            ++i;
                            if (i < input.size() && (input[i] == '+' || input[i] == '-')) ++i;
                            while (i < input.size() && std::isdigit(static_cast<unsigned char>(input[i]))) ++i;
                        }
                    }

                    std::string numstr = input.substr(start, i - start);
                    if (is_float) {
                        tokens.emplace_back(Number{std::stod(numstr)});
                    } else if (is_hex) {
                        tokens.emplace_back(Number{static_cast<int64_t>(std::stoull(numstr, nullptr, 16))});
                    } else {
                        tokens.emplace_back(Number{static_cast<int64_t>(std::stoll(numstr))});
                    }
                    last = Last::Value;
                    continue;
                }

                // 2. Identifiers (variables, functions, stack ops)
                if (std::isalpha(static_cast<unsigned char>(input[i])) || input[i] == '_') {
                    size_t start = i;
                    while (i < input.size() &&
                           (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_'))
                        ++i;
                    std::string name = input.substr(start, i - start);
                    std::string upper = name;
                    std::transform(upper.begin(), upper.end(), upper.begin(),
                                   [](unsigned char c) { return std::toupper(c); });

                    if (auto op = matchTable(op_words, upper)) {
                        tokens.emplace_back(Operator{op.value()});
                        last = Last::Operator;
                    } else if (auto fn = matchTable(func_map, upper)) {
                        tokens.emplace_back(Function{fn.value()});
                        last = Last::Operator; // functions act like operators for unary minus detection
                    } else if (convertId) {
                        if (auto t = convertId(name)) {
                            tokens.emplace_back(*t);
                            last = Last::Value;
                        } else {
                            return {}; // unknown identifier
                        }
                    } else {
                        return {}; // unknown identifier, no resolver
                    }
                    continue;
                }

                // 3. Parentheses
                if (input[i] == '(') {
                    tokens.emplace_back(ParenLeft{});
                    ++i;
                    last = Last::LeftParen;
                    continue;
                }
                if (input[i] == ')') {
                    tokens.emplace_back(ParenRight{});
                    ++i;
                    last = Last::Value;
                    continue;
                }

                // 4. Operators
                auto opMatch = matchOperator(input, i);
                if (!opMatch) return {}; // unknown token

                auto [op, newPos] = *opMatch;
                i = newPos;

                // Detect unary minus / unary plus
                if (op == Op::SUB && (last == Last::None || last == Last::Operator || last == Last::LeftParen)) {
                    tokens.emplace_back(Operator{Op::NEG});
                    last = Last::Operator;
                    continue;
                }
                if (op == Op::ADD && (last == Last::None || last == Last::Operator || last == Last::LeftParen)) {
                    // Unary plus - just skip it
                    continue;
                }

                if (op == Op::TERNARY_COLON) {
                    tokens.emplace_back(Colon{});
                } else {
                    tokens.emplace_back(Operator{op});
                }
                last = Last::Operator;
            }

            return tokens;
        }

        std::vector<Token> toPostfix(const std::vector<Token>& infix) {
            std::vector<Token> output;
            std::vector<Token> opStack;

            for (size_t i = 0; i < infix.size(); ++i) {
                const Token& tok = infix[i];

                // Number / Variable -> output
                if (std::holds_alternative<Number>(tok) ||
                    std::holds_alternative<Variable>(tok)) {
                    output.push_back(tok);
                    continue;
                }

                // Left paren -> stack
                if (std::holds_alternative<ParenLeft>(tok)) {
                    opStack.push_back(tok);
                    continue;
                }

                // Right paren -> pop until '('
                if (std::holds_alternative<ParenRight>(tok)) {
                    while (!opStack.empty() && !std::holds_alternative<ParenLeft>(opStack.back())) {
                        output.push_back(opStack.back());
                        opStack.pop_back();
                    }
                    if (opStack.empty()) return {};
                    opStack.pop_back(); // remove '('
                    // If top of stack is a function, pop it to output
                    if (!opStack.empty() && std::holds_alternative<Function>(opStack.back())) {
                        output.push_back(opStack.back());
                        opStack.pop_back();
                    }
                    continue;
                }

                // Colon ':' in ternary '?:' -> pop until '?'
                if (std::holds_alternative<Colon>(tok)) {
                    while (!opStack.empty() && !isOp(opStack.back(), Op::TERNARY_Q)) {
                        output.push_back(opStack.back());
                        opStack.pop_back();
                    }
                    if (opStack.empty()) return {};
                    // Leave '?' on stack - it will be popped when ternary completes
                    continue;
                }

                // Operator
                if (const auto* opr = std::get_if<Operator>(&tok)) {
                    Op op = opr->op;

                    if (op == Op::COMMA) {
                        while (!opStack.empty() &&
                               !std::holds_alternative<ParenLeft>(opStack.back()) &&
                               !isOp(opStack.back(), Op::TERNARY_Q)) {
                            output.push_back(opStack.back());
                            opStack.pop_back();
                        }
                        continue;
                    }

                    while (!opStack.empty()) {
                        if (std::holds_alternative<ParenLeft>(opStack.back())) break;
                        if (const auto* o2 = std::get_if<Operator>(&opStack.back())) {
                            if ((precedence(o2->op) > precedence(op)) ||
                                (precedence(o2->op) == precedence(op) && !rightAssoc(op))) {
                                output.push_back(opStack.back());
                                opStack.pop_back();
                                continue;
                            }
                        }
                        break;
                    }
                    opStack.push_back(tok);
                    continue;
                }

                // Function -> stack
                if (std::holds_alternative<Function>(tok)) {
                    opStack.push_back(tok);
                    continue;
                }

                return {}; // unknown token type
            }

            // Flush remaining operators
            while (!opStack.empty()) {
                if (std::holds_alternative<ParenLeft>(opStack.back()) ||
                    std::holds_alternative<ParenRight>(opStack.back()))
                    return {}; // mismatched parentheses
                output.push_back(opStack.back());
                opStack.pop_back();
            }
            return output;
        }

        static bool isOp(const Token& tok, Op o) {
            const auto* op = std::get_if<Operator>(&tok);
            return op && op->op == o;
        }

    public:
        std::vector<Token> parse(
            const std::string& expression,
            std::function<std::optional<Token>(std::string_view)> convertId = {}
        ) {
            auto tokens = tokenize(expression, convertId);
            if (tokens.empty())
                return tokens;
            return toPostfix(tokens);
        }
    }; // class Parser

    template<typename T>
    class Calculator {
        using VariableSolver = std::function<std::optional<T>(const Variable&)>;

    public:
        Calculator(VariableSolver var = {})
            : variables_(std::move(var)) {}

        std::optional<T> eval(std::span<const Token> rpn) {
            stack_.clear();

            for (const auto& token : rpn) {
                if (auto num = std::get_if<Number>(&token)) {
                    stack_.push_back(fromNumber<T>(*num));
                } else {
                    if (!evalToValue(token)) return std::nullopt;
                }
            }

            if (stack_.size() != 1) return std::nullopt;
            return stack_.back();
        }

    private:
        std::vector<T> stack_;
        VariableSolver variables_;

        bool evalToValue(const Token& tok) {
            if (auto var = std::get_if<Variable>(&tok)) {
                if (variables_) {
                    if (auto val = variables_(*var)) {
                        stack_.push_back(*val);
                        return true;
                    }
                }
                return false;
            }

            if (auto fn = std::get_if<Function>(&tok)) {
                return evalFunction(fn->func);
            }

            if (auto op = std::get_if<Operator>(&tok)) {
                return evalOperator(op->op);
            }

            return false;
        }

        bool evalFunction(Func f) {
            if (stack_.empty()) return false;

            // E() is a constant with no arguments... but it was already popped
            // Actually E produces euler's number - handle it specially
            if (f == Func::E) {
                stack_.push_back(static_cast<T>(M_E));
                return true;
            }

            T x = pop();
            T r{};

            switch (f) {
                case Func::NEG: r = -x; break;
                case Func::SGN: r = static_cast<T>((x > T{0}) - (x < T{0})); break;
                case Func::ABS: r = static_cast<T>(std::abs(static_cast<double>(x))); break;
                case Func::SIN: r = static_cast<T>(std::sin(static_cast<double>(x))); break;
                case Func::COS: r = static_cast<T>(std::cos(static_cast<double>(x))); break;
                case Func::TAN: r = static_cast<T>(std::tan(static_cast<double>(x))); break;
                case Func::ASIN: r = static_cast<T>(std::asin(static_cast<double>(x))); break;
                case Func::ACOS: r = static_cast<T>(std::acos(static_cast<double>(x))); break;
                case Func::ATAN: r = static_cast<T>(std::atan(static_cast<double>(x))); break;
                case Func::EXP: r = static_cast<T>(std::exp(static_cast<double>(x))); break;
                case Func::LN:  r = static_cast<T>(std::log(static_cast<double>(x))); break;
                case Func::LG:  r = static_cast<T>(std::log10(static_cast<double>(x))); break;
                case Func::SQRT: r = static_cast<T>(std::sqrt(static_cast<double>(x))); break;
                case Func::FLOOR: r = static_cast<T>(std::floor(static_cast<double>(x))); break;
                case Func::CEIL:  r = static_cast<T>(std::ceil(static_cast<double>(x))); break;
                case Func::ROUND: r = static_cast<T>(std::round(static_cast<double>(x))); break;
                case Func::TRUNC: r = static_cast<T>(std::trunc(static_cast<double>(x))); break;
                default: return false;
            }

            stack_.push_back(r);
            return true;
        }

        bool evalOperator(Op op) {
            // Zero-operand (stack manipulation) operators
            switch (op) {
                case Op::DUP:
                    if (stack_.empty()) return false;
                    stack_.push_back(stack_.back());
                    return true;
                case Op::DROP:
                    if (stack_.empty()) return false;
                    stack_.pop_back();
                    return true;
                case Op::SWAP:
                    if (stack_.size() < 2) return false;
                    std::swap(stack_[stack_.size()-1], stack_[stack_.size()-2]);
                    return true;
                case Op::OVER:
                    if (stack_.size() < 2) return false;
                    stack_.push_back(stack_[stack_.size()-2]);
                    return true;
                case Op::ROT:
                    if (stack_.size() < 3) return false;
                    std::rotate(stack_.end()-3, stack_.end()-2, stack_.end());
                    return true;
                default:
                    break;
            }

            // Ternary operator: needs 3 operands on stack
            if (op == Op::TERNARY_Q) {
                if (stack_.size() < 3) return false;
                T falseVal = pop();
                T trueVal = pop();
                T cond = pop();
                stack_.push_back(cond ? trueVal : falseVal);
                return true;
            }

            // Unary operators
            if (op == Op::NEG || op == Op::BIT_NOT || op == Op::LOG_NOT) {
                if (stack_.empty()) return false;
                T val = pop();
                switch (op) {
                    case Op::NEG:
                        stack_.push_back(-val);
                        return true;
                    case Op::BIT_NOT:
                        stack_.push_back(static_cast<T>(~static_cast<int64_t>(val)));
                        return true;
                    case Op::LOG_NOT:
                        stack_.push_back(static_cast<T>(val == T{0} ? 1 : 0));
                        return true;
                    default:
                        return false;
                }
            }

            // Binary operators: need 2 operands
            if (stack_.size() < 2) return false;
            T b = pop();   // right operand (top of stack)
            T a = pop();   // left operand

            switch (op) {
                case Op::ADD: stack_.push_back(a + b); return true;
                case Op::SUB: stack_.push_back(a - b); return true;
                case Op::MUL: stack_.push_back(a * b); return true;
                case Op::DIV:
                    if (b == T{0}) return false;
                    stack_.push_back(a / b);
                    return true;
                case Op::MOD:
                    if (b == T{0}) return false;
                    if constexpr (std::is_floating_point_v<T>)
                        stack_.push_back(std::fmod(a, b));
                    else
                        stack_.push_back(a % b);
                    return true;
                case Op::POW:
                    stack_.push_back(static_cast<T>(std::pow(static_cast<double>(a), static_cast<double>(b))));
                    return true;
                case Op::BIT_AND:
                    stack_.push_back(static_cast<T>(static_cast<int64_t>(a) & static_cast<int64_t>(b)));
                    return true;
                case Op::BIT_OR:
                    stack_.push_back(static_cast<T>(static_cast<int64_t>(a) | static_cast<int64_t>(b)));
                    return true;
                case Op::BIT_XOR:
                    stack_.push_back(static_cast<T>(static_cast<int64_t>(a) ^ static_cast<int64_t>(b)));
                    return true;
                case Op::SHL:
                    stack_.push_back(static_cast<T>(static_cast<int64_t>(a) << static_cast<int64_t>(b)));
                    return true;
                case Op::SHR:
                    stack_.push_back(static_cast<T>(static_cast<int64_t>(a) >> static_cast<int64_t>(b)));
                    return true;
                case Op::LOG_AND: stack_.push_back(static_cast<T>((a != T{0}) && (b != T{0}))); return true;
                case Op::LOG_OR:  stack_.push_back(static_cast<T>((a != T{0}) || (b != T{0}))); return true;
                case Op::EQ: stack_.push_back(static_cast<T>(a == b)); return true;
                case Op::NE: stack_.push_back(static_cast<T>(a != b)); return true;
                case Op::LT: stack_.push_back(static_cast<T>(a < b));  return true;
                case Op::LE: stack_.push_back(static_cast<T>(a <= b)); return true;
                case Op::GT: stack_.push_back(static_cast<T>(a > b));  return true;
                case Op::GE: stack_.push_back(static_cast<T>(a >= b)); return true;
                default:
                    return false;
            }
        }

        T pop() {
            T val = stack_.back();
            stack_.pop_back();
            return val;
        }
    }; // class Calculator
}; // namespace MathEvaluate
