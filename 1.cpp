#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <memory>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <limits>
//g++ -std=c++17 -o calculator 1.cpp

// Убирает пробелы в начале и конце строки
static inline std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.length() && std::isspace(s[start])) ++start;
    size_t end = s.length();
    while (end > start && std::isspace(s[end - 1])) --end;
    return s.substr(start, end - start);
}

// Типы лексем
enum class TokenType {
    Number,
    Identifier,
    Plus, Minus, Mul, Div, Pow,
    LeftParen, RightParen,
    End
};

// Лексема
class Token {
public:
    TokenType type;
    std::string text;
    Token(TokenType t, const std::string& txt) : type(t), text(txt) {}
};

// Лексический анализатор (разбивает строку на токены)
class Lexer {
    std::string input;
    size_t pos = 0;

public:
    explicit Lexer(const std::string& expr) : input(expr) {}

    Token nextToken() {
        // Пропуск пробелов
        while (pos < input.length() && std::isspace(input[pos])) ++pos;
        if (pos >= input.length()) return Token(TokenType::End, "");

        char current = input[pos];

        // Число (целое или с плавающей точкой)
        if (std::isdigit(current)) {
            std::string numStr;
            while (pos < input.length() && std::isdigit(input[pos]))
                numStr += input[pos++];

            if (pos < input.length() && input[pos] == '.') {
                if (pos + 1 < input.length() && std::isdigit(input[pos + 1])) {
                    numStr += '.';
                    ++pos;
                    while (pos < input.length() && std::isdigit(input[pos]))
                        numStr += input[pos++];
                } else {
                    throw std::runtime_error("Lexical Error");
                }
            }

            if (pos < input.length() && (std::isalpha(input[pos]) || input[pos] == '_'))
                throw std::runtime_error("Lexical Error");

            // Проверка ведущих нулей
            size_t dotPos = numStr.find('.');
            if (dotPos == std::string::npos) {
                if (numStr.length() > 1 && numStr[0] == '0')
                    throw std::runtime_error("Lexical Error");
            } else {
                std::string intPart = numStr.substr(0, dotPos);
                if (intPart.length() > 1 && intPart[0] == '0')
                    throw std::runtime_error("Lexical Error");
            }

            return Token(TokenType::Number, numStr);
        }

        // переменная или функция
        if (std::isalpha(current) || current == '_') {
            std::string id;
            while (pos < input.length() && (std::isalnum(input[pos]) || input[pos] == '_'))
                id += std::tolower(input[pos++]);
            return Token(TokenType::Identifier, id);
        }

        // Операторы и скобки
        ++pos;
        switch (current) {
            case '+': return Token(TokenType::Plus, "+");
            case '-': return Token(TokenType::Minus, "-");
            case '*': return Token(TokenType::Mul, "*");
            case '/': return Token(TokenType::Div, "/");
            case '^': return Token(TokenType::Pow, "^");
            case '(': return Token(TokenType::LeftParen, "(");
            case ')': return Token(TokenType::RightParen, ")");
            default:  throw std::runtime_error("Lexical Error");
        }
    }
};

// Режим "только лексер" – выводит токены по одному на строку
std::string runLexer(const std::string& expression) {
    Lexer lexer(expression);
    std::string result;
    Token tok = lexer.nextToken();
    while (tok.type != TokenType::End) {
        result += tok.text + "\n";
        tok = lexer.nextToken();
    }
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

//----------------------------------------------------------------------
// Абстрактное синтаксическое дерево (AST)
//----------------------------------------------------------------------

// Базовый класс для всех узлов выражения
class Expression {
public:
    virtual ~Expression() = default;
    virtual double evaluate(const std::map<std::string, double>& vars) const = 0;
    virtual std::unique_ptr<Expression> derivative(const std::string& var) const = 0;
    virtual std::string toString() const = 0;
    virtual std::unique_ptr<Expression> clone() const = 0;
};

using ExprPtr = std::unique_ptr<Expression>;

// Узел числа
class Number : public Expression {
    double value;
public:
    explicit Number(double v) : value(v) {}
    double evaluate(const std::map<std::string, double>&) const override { return value; }
    ExprPtr derivative(const std::string&) const override { return std::make_unique<Number>(0.0); }
    std::string toString() const override {
        std::ostringstream oss;
        oss.precision(15);
        oss << value;
        return oss.str();
    }
    ExprPtr clone() const override { return std::make_unique<Number>(*this); }
};

// Узел переменной
class Variable : public Expression {
    std::string name;
public:
    explicit Variable(const std::string& n) : name(n) {}
    double evaluate(const std::map<std::string, double>& vars) const override {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        throw std::runtime_error("Unknown variable: " + name);
    }
    ExprPtr derivative(const std::string& var) const override {
        return std::make_unique<Number>(name == var ? 1.0 : 0.0);
    }
    std::string toString() const override { return name; }
    ExprPtr clone() const override { return std::make_unique<Variable>(*this); }
};

// Узел унарной операции ( + или - )
class UnaryOp : public Expression {
public:
    enum Op { Plus, Minus };
private:
    Op op;
    ExprPtr operand;
public:
    UnaryOp(Op o, ExprPtr expr) : op(o), operand(std::move(expr)) {}
    double evaluate(const std::map<std::string, double>& vars) const override {
        double v = operand->evaluate(vars);
        return op == Plus ? v : -v;
    }
    ExprPtr derivative(const std::string& var) const override {
        return std::make_unique<UnaryOp>(op, operand->derivative(var));
    }
    std::string toString() const override {
        std::string sign = (op == Plus) ? "+" : "-";
        std::string inner = operand->toString();
        // Скобки вокруг сложного аргумента
        if (!(dynamic_cast<const Number*>(operand.get()) || dynamic_cast<const Variable*>(operand.get())))
            inner = "(" + inner + ")";
        return sign + inner;
    }
    ExprPtr clone() const override { return std::make_unique<UnaryOp>(op, operand->clone()); }
};

// Узел бинарной операции ( +, -, *, /, ^ )
class BinaryOp : public Expression {
public:
    enum Op { Add, Sub, Mul, Div, Pow };
private:
    Op op;
    ExprPtr left, right;
public:
    BinaryOp(Op o, ExprPtr l, ExprPtr r) : op(o), left(std::move(l)), right(std::move(r)) {}
    double evaluate(const std::map<std::string, double>& vars) const override {
        double l = left->evaluate(vars);
        double r = right->evaluate(vars);
        switch (op) {
            case Add: return l + r;
            case Sub: return l - r;
            case Mul: return l * r;
            case Div: return l / r;                     // деление на 0 даёт inf
            case Pow: return std::pow(l, r);
            default: throw std::runtime_error("Unknown binary operation");
        }
    }
    ExprPtr derivative(const std::string& var) const override;
    std::string toString() const override {
        static const char* opStr[] = {" + ", " - ", " * ", " / ", " ^ "};
        std::string l = left->toString();
        std::string r = right->toString();
        if (!(dynamic_cast<const Number*>(left.get()) || dynamic_cast<const Variable*>(left.get())))
            l = "(" + l + ")";
        if (!(dynamic_cast<const Number*>(right.get()) || dynamic_cast<const Variable*>(right.get())))
            r = "(" + r + ")";
        return l + opStr[static_cast<int>(op)] + r;
    }
    ExprPtr clone() const override { return std::make_unique<BinaryOp>(op, left->clone(), right->clone()); }
};

// Узел математической функции (sin, cos, exp, ...)
class Function : public Expression {
public:
    enum Func { Sin, Cos, Tan, Asin, Acos, Atan, Exp, Log, Sqrt };
private:
    Func func;
    ExprPtr arg;
public:
    Function(Func f, ExprPtr a) : func(f), arg(std::move(a)) {}
    double evaluate(const std::map<std::string, double>& vars) const override {
        double v = arg->evaluate(vars);
        switch (func) {
            case Sin:  return std::sin(v);
            case Cos:  return std::cos(v);
            case Tan:  return std::tan(v);
            case Asin: if (v < -1.0 || v > 1.0) throw std::runtime_error("Domain error"); return std::asin(v);
            case Acos: if (v < -1.0 || v > 1.0) throw std::runtime_error("Domain error"); return std::acos(v);
            case Atan: return std::atan(v);
            case Exp:  return std::exp(v);
            case Log:
                if (v < 0.0) throw std::runtime_error("Domain error");
                if (v == 0.0) return -std::numeric_limits<double>::infinity();
                return std::log(v);
            case Sqrt: if (v < 0.0) throw std::runtime_error("Domain error"); return std::sqrt(v);
            default: throw std::runtime_error("Unknown function");
        }
    }
    ExprPtr derivative(const std::string& var) const override;
    std::string toString() const override {
        static const char* names[] = {"sin", "cos", "tan", "asin", "acos", "atan", "exp", "log", "sqrt"};
        std::string inner = arg->toString();
        if (!(dynamic_cast<const Number*>(arg.get()) || dynamic_cast<const Variable*>(arg.get())))
            inner = "(" + inner + ")";
        return std::string(names[static_cast<int>(func)]) + "(" + inner + ")";
    }
    ExprPtr clone() const override { return std::make_unique<Function>(func, arg->clone()); }
};

//----------------------------------------------------------------------
// Реализация производных (вынесена после объявления всех классов)
//----------------------------------------------------------------------

ExprPtr BinaryOp::derivative(const std::string& var) const {
    ExprPtr dl = left->derivative(var);
    ExprPtr dr = right->derivative(var);
    switch (op) {
        case Add: return std::make_unique<BinaryOp>(Add, std::move(dl), std::move(dr));
        case Sub: return std::make_unique<BinaryOp>(Sub, std::move(dl), std::move(dr));
        case Mul:
            return std::make_unique<BinaryOp>(Add,
                std::make_unique<BinaryOp>(Mul, left->clone(), std::move(dr)),
                std::make_unique<BinaryOp>(Mul, std::move(dl), right->clone())
            );
        case Div:
            return std::make_unique<BinaryOp>(Div,
                std::make_unique<BinaryOp>(Sub,
                    std::make_unique<BinaryOp>(Mul, std::move(dl), right->clone()),
                    std::make_unique<BinaryOp>(Mul, left->clone(), std::move(dr))
                ),
                std::make_unique<BinaryOp>(Pow, right->clone(), std::make_unique<Number>(2.0))
            );
        case Pow: {
            if (auto* num = dynamic_cast<Number*>(right.get())) {
                double n = num->evaluate({});
                if (n == 0.0) return std::make_unique<Number>(0.0);
                return std::make_unique<BinaryOp>(Mul,
                    std::make_unique<Number>(n),
                    std::make_unique<BinaryOp>(Mul,
                        std::make_unique<BinaryOp>(Pow, left->clone(), std::make_unique<Number>(n - 1)),
                        std::move(dl)
                    )
                );
            } else {
                // Общая формула: (f^g)' = f^g * ( g' * ln(f) + g * (f'/f) )
                ExprPtr f = left->clone();
                ExprPtr g = right->clone();
                ExprPtr ln_f = std::make_unique<Function>(Function::Log, f->clone());
                ExprPtr term1 = std::make_unique<BinaryOp>(Mul, std::move(dr), std::move(ln_f));
                ExprPtr div_term = std::make_unique<BinaryOp>(Div, std::move(dl), f->clone());
                ExprPtr term2 = std::make_unique<BinaryOp>(Mul, g->clone(), std::move(div_term));
                ExprPtr sum = std::make_unique<BinaryOp>(Add, std::move(term1), std::move(term2));
                ExprPtr f_pow_g = std::make_unique<BinaryOp>(Pow, std::move(f), std::move(g));
                return std::make_unique<BinaryOp>(Mul, std::move(f_pow_g), std::move(sum));
            }
        }
        default: throw std::runtime_error("Unknown op for derivative");
    }
}

ExprPtr Function::derivative(const std::string& var) const {
    ExprPtr darg = arg->derivative(var);
    switch (func) {
        case Sin:
            return std::make_unique<BinaryOp>(BinaryOp::Mul,
                std::make_unique<Function>(Cos, arg->clone()), std::move(darg));
        case Cos:
            return std::make_unique<BinaryOp>(BinaryOp::Mul,
                std::make_unique<UnaryOp>(UnaryOp::Minus, std::make_unique<Function>(Sin, arg->clone())),
                std::move(darg));
        case Tan:
            return std::make_unique<BinaryOp>(BinaryOp::Mul,
                std::make_unique<BinaryOp>(BinaryOp::Add,
                    std::make_unique<Number>(1.0),
                    std::make_unique<BinaryOp>(BinaryOp::Pow,
                        std::make_unique<Function>(Tan, arg->clone()),
                        std::make_unique<Number>(2.0)
                    )
                ),
                std::move(darg));
        case Asin:
            return std::make_unique<BinaryOp>(BinaryOp::Div, std::move(darg),
                std::make_unique<Function>(Sqrt,
                    std::make_unique<BinaryOp>(BinaryOp::Sub,
                        std::make_unique<Number>(1.0),
                        std::make_unique<BinaryOp>(BinaryOp::Pow, arg->clone(), std::make_unique<Number>(2.0))
                    )
                ));
        case Acos:
            return std::make_unique<BinaryOp>(BinaryOp::Div,
                std::make_unique<UnaryOp>(UnaryOp::Minus, std::move(darg)),
                std::make_unique<Function>(Sqrt,
                    std::make_unique<BinaryOp>(BinaryOp::Sub,
                        std::make_unique<Number>(1.0),
                        std::make_unique<BinaryOp>(BinaryOp::Pow, arg->clone(), std::make_unique<Number>(2.0))
                    )
                ));
        case Atan:
            return std::make_unique<BinaryOp>(BinaryOp::Div, std::move(darg),
                std::make_unique<BinaryOp>(BinaryOp::Add,
                    std::make_unique<Number>(1.0),
                    std::make_unique<BinaryOp>(BinaryOp::Pow, arg->clone(), std::make_unique<Number>(2.0))
                ));
        case Exp:
            return std::make_unique<BinaryOp>(BinaryOp::Mul,
                std::make_unique<Function>(Exp, arg->clone()), std::move(darg));
        case Log:
            return std::make_unique<BinaryOp>(BinaryOp::Div, std::move(darg), arg->clone());
        case Sqrt:
            return std::make_unique<BinaryOp>(BinaryOp::Div, std::move(darg),
                std::make_unique<BinaryOp>(BinaryOp::Mul,
                    std::make_unique<Number>(2.0),
                    std::make_unique<Function>(Sqrt, arg->clone())
                ));
        default: throw std::runtime_error("Unknown function derivative");
    }
}

//----------------------------------------------------------------------
// Парсер (рекурсивный спуск)
//----------------------------------------------------------------------

class Parser {
    std::string input;
    size_t pos = 0;

    char peek() const {
        return pos < input.length() ? input[pos] : '\0';
    }
    void advance() {
        if (pos < input.length()) ++pos;
    }
    bool match(char c) {
        if (peek() == c) {
            advance();
            return true;
        }
        return false;
    }

    ExprPtr parseExpression();
    ExprPtr parseTerm();
    ExprPtr parseUnary();
    ExprPtr parseFactor();
    ExprPtr parsePrimary();

    ExprPtr parseNumber() {
        std::string num;
        while (std::isdigit(peek()) || peek() == '.')
            num += peek(), advance();
        return std::make_unique<Number>(std::stod(num));
    }

public:
    explicit Parser(const std::string& expr) : input(expr) {}
    ExprPtr parse();
};

ExprPtr Parser::parseExpression() {
    ExprPtr left = parseTerm();
    while (true) {
        if (match('+'))
            left = std::make_unique<BinaryOp>(BinaryOp::Add, std::move(left), parseTerm());
        else if (match('-'))
            left = std::make_unique<BinaryOp>(BinaryOp::Sub, std::move(left), parseTerm());
        else
            break;
    }
    return left;
}

ExprPtr Parser::parseTerm() {
    ExprPtr left = parseUnary();
    while (true) {
        if (match('*'))
            left = std::make_unique<BinaryOp>(BinaryOp::Mul, std::move(left), parseUnary());
        else if (match('/'))
            left = std::make_unique<BinaryOp>(BinaryOp::Div, std::move(left), parseUnary());
        else
            break;
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (match('+'))
        return std::make_unique<UnaryOp>(UnaryOp::Plus, parseUnary());
    else if (match('-'))
        return std::make_unique<UnaryOp>(UnaryOp::Minus, parseUnary());
    else
        return parseFactor();
}

ExprPtr Parser::parseFactor() {
    ExprPtr left = parsePrimary();
    if (match('^')) {
        ExprPtr right = parseFactor();      // правоассоциативно
        return std::make_unique<BinaryOp>(BinaryOp::Pow, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parsePrimary() {
    // Обработка унарных + и - (на случай если parseUnary не сработал)
    if (peek() == '+' || peek() == '-') {
        char op = peek();
        advance();
        ExprPtr operand = parsePrimary();
        return std::make_unique<UnaryOp>(
            op == '+' ? UnaryOp::Plus : UnaryOp::Minus,
            std::move(operand)
        );
    }

    if (std::isdigit(peek()) || peek() == '.') {
        return parseNumber();
    }
    else if (std::isalpha(peek()) || peek() == '_') {
        std::string name;
        while (std::isalnum(peek()) || peek() == '_')
            name += peek(), advance();

        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        if (name == "sin") return std::make_unique<Function>(Function::Sin, parsePrimary());
        if (name == "cos") return std::make_unique<Function>(Function::Cos, parsePrimary());
        if (name == "tan") return std::make_unique<Function>(Function::Tan, parsePrimary());
        if (name == "asin") return std::make_unique<Function>(Function::Asin, parsePrimary());
        if (name == "acos") return std::make_unique<Function>(Function::Acos, parsePrimary());
        if (name == "atan") return std::make_unique<Function>(Function::Atan, parsePrimary());
        if (name == "exp") return std::make_unique<Function>(Function::Exp, parsePrimary());
        if (name == "log") return std::make_unique<Function>(Function::Log, parsePrimary());
        if (name == "sqrt") return std::make_unique<Function>(Function::Sqrt, parsePrimary());

        return std::make_unique<Variable>(name);
    }
    else if (match('(')) {
        ExprPtr node = parseExpression();
        if (!match(')')) throw std::runtime_error("Expected ')'");
        return node;
    }

    throw std::runtime_error("Unexpected character");
}

ExprPtr Parser::parse() {
    ExprPtr ast = parseExpression();
    if (pos < input.length())
        throw std::runtime_error("Unexpected character at end");
    return ast;
}

//----------------------------------------------------------------------
// Главная программа
//----------------------------------------------------------------------

int main() {
    std::string command;
    if (!std::getline(std::cin, command)) return 0;
    command = trim(command);

    // Режим лексера
    if (command != "evaluate" && command != "derivative" && command != "evaluate_derivative") {
        try {
            std::cout << runLexer(command) << std::endl;
        } catch (...) {
            std::cout << "ERROR!!!" << std::endl;
        }
        return 0;
    }

    // Чтение количества переменных
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << "ERROR Invalid command format" << std::endl;
        return 0;
    }
    int varCount;
    try {
        varCount = std::stoi(trim(line));
    } catch (...) {
        std::cout << "ERROR Invalid command format" << std::endl;
        return 0;
    }

    // Чтение имён переменных (одна строка через пробел)
    if (!std::getline(std::cin, line)) {
        std::cout << "ERROR Parsing variable names failed." << std::endl;
        return 0;
    }
    std::istringstream namesStream(line);
    std::vector<std::string> varNames(varCount);
    for (int i = 0; i < varCount; ++i) {
        if (!(namesStream >> varNames[i])) {
            std::cout << "ERROR Parsing variable names failed." << std::endl;
            return 0;
        }
        varNames[i] = trim(varNames[i]);
    }

    // Чтение значений переменных (одна строка через пробел)
    if (!std::getline(std::cin, line)) {
        std::cout << "ERROR Missing values line" << std::endl;
        return 0;
    }
    std::istringstream valuesStream(line);
    std::vector<double> varValues(varCount);
    for (int i = 0; i < varCount; ++i) {
        if (!(valuesStream >> varValues[i])) {
            std::cout << "ERROR Parsing value for variable " << varNames[i] << " failed." << std::endl;
            return 0;
        }
    }

    // Чтение выражения
    std::string expression;
    if (!std::getline(std::cin, expression)) {
        std::cout << "ERROR Missing expression" << std::endl;
        return 0;
    }
    // Удаляем все пробелы
    expression.erase(std::remove_if(expression.begin(), expression.end(), ::isspace), expression.end());

    // Словарь переменных
    std::map<std::string, double> variables;
    for (int i = 0; i < varCount; ++i)
        variables[varNames[i]] = varValues[i];

    try {
        Parser parser(expression);
        ExprPtr ast = parser.parse();

        if (command == "evaluate") {
            double result = ast->evaluate(variables);
            if (std::isinf(result)) {
                std::cout << (result > 0 ? "inf" : "-inf") << std::endl;
            } else {
                std::cout << result << std::endl;
            }
        }
        else if (command == "derivative") {
            if (varCount == 0) {
                std::cout << "ERROR No variable provided for differentiation." << std::endl;
                return 0;
            }
            ExprPtr deriv = ast->derivative(varNames[0]);
            std::cout << deriv->toString() << std::endl;
        }
        else if (command == "evaluate_derivative") {
            if (varCount == 0) {
                std::cout << "ERROR No variable provided for differentiation." << std::endl;
                return 0;
            }
            ExprPtr deriv = ast->derivative(varNames[0]);
            double result = deriv->evaluate(variables);
            if (std::isinf(result)) {
                std::cout << (result > 0 ? "inf" : "-inf") << std::endl;
            } else {
                std::cout << result << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "ERROR " << e.what() << std::endl;
    }

    return 0;
}