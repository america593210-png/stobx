
#include "Verification.h"

Verification::Verification() 
: solver(context){}

bool Verification::getIntegerType(
    const std::string& typeName,
    IntegerType& type
) const
 {
    if (typeName == "I1" || typeName == "U1") {
        type.bitWidth = 1;
        type.isSigned = typeName == "I1";
        return true;
    }

    if (typeName == "I8" || typeName == "U8") {
        type.bitWidth = 8;
        type.isSigned = typeName == "I8";
        return true;
    }

    if (typeName == "I16" || typeName == "U16") {
        type.bitWidth = 16;
        type.isSigned = typeName == "I16";
        return true;
    }

    if (typeName == "I32" || typeName == "U32") {
        type.bitWidth = 32;
        type.isSigned = typeName == "I32";
        return true;
    }

    if (typeName == "I64" || typeName == "U64") {
        type.bitWidth = 64;
        type.isSigned = typeName == "I64";
        return true;
    }

    return false;
}

bool Verification::createSymbolicVariable(
    const std::string& name,
    const std::string& typeName
) {
    IntegerType type;

    if (!getIntegerType(typeName, type)) {
        return false;
    }

    std::string symbolName =
        name + "_" + std::to_string(symbolCounter++);

    z3::expr value = context.bv_const(
        symbolName.c_str(),
        type.bitWidth
    );

    variables.insert_or_assign(
        name,
        SymbolicVariable(value, type)
    );

    return true;
}

bool Verification::testNumber(
    const std::string& value,
    const std::string& typeName
) {
    NumberASTNode number(value);

    auto result = createNumber(number, typeName);

    return result.has_value();
}

std::optional<z3::expr> Verification::createNumber(
    const NumberASTNode& number,
    const std::string& typeName,
bool negative
) {
    IntegerType type;

    if (!getIntegerType(typeName, type)) {
        return std::nullopt;
    }

    std::string digits = number.value;

    if (digits.empty()) {
        return std::nullopt;
    }

    for (char digit : digits) {
        if (digit < '0' || digit > '9') {
            return std::nullopt;
        }
    }

    std::size_t firstDigit = digits.find_first_not_of('0');

    if (firstDigit == std::string::npos) {
        digits = "0";
    } else {
        digits.erase(0, firstDigit);
    }

    std::string maxValue;
    std::string minValue;

if (type.isSigned) {
    switch (type.bitWidth) {
        case 1:
            maxValue = "0";
            minValue = "1";
            break;
        case 8:
            maxValue = "127";
            minValue = "128";
            break;
        case 16:
            maxValue = "32767";
            minValue = "32768";
            break;
        case 32:
            maxValue = "2147483647";
            minValue = "2147483648";
            break;
        case 64:
            maxValue = "9223372036854775807";
            minValue = "9223372036854775808";
            break;
        default:
            return std::nullopt;
    }
} else {
    if (negative) {
        return std::nullopt;
    }

    switch (type.bitWidth) {
        case 1: maxValue = "1"; break;
        case 8: maxValue = "255"; break;
        case 16: maxValue = "65535"; break;
        case 32: maxValue = "4294967295"; break;
        case 64: maxValue = "18446744073709551615"; break;
        default: return std::nullopt;
    }
}
    
    const std::string& limit = negative ? minValue : maxValue;

if (
    digits.size() > limit.size() ||
    (
        digits.size() == limit.size() &&
        digits > limit
    )
) {
    return std::nullopt;
}


    try {
        z3::expr value = context.bv_val(
            digits.c_str(),
            type.bitWidth
        );
if (negative) {
    value = -value;
}
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<z3::expr> Verification::createExpression(
    const ASTNode* node,
    const std::string& typeName
) {
    if (!node) {
        return std::nullopt;
    }

    if (auto* number = dynamic_cast<const NumberASTNode*>(node)) {
        return createNumber(*number, typeName);
    }

    if (auto* unary = dynamic_cast<const UnaryASTNode*>(node)) {
        if (unary->op != "-") {
            return std::nullopt;
        }

if (auto* number = dynamic_cast<const NumberASTNode*>(
    unary->operand.get()
)) {
    return createNumber(*number, typeName, true);
}

        IntegerType type;

        if (!getIntegerType(typeName, type) || !type.isSigned) {
            return std::nullopt;
        }

        auto operand = createExpression(
            unary->operand.get(),
            typeName
        );

        if (!operand) {
            return std::nullopt;
        }

        return -(*operand);
    }

    return std::nullopt;
}
VerificationResult Verification::verify(
    const VerificationASTNode* block
) {
    if (!block || block->statements.empty()) {
        return VerificationResult::Failed;
    }

    solver.reset();
    variables.clear();
    symbolCounter = 0;

    for (const auto& statement : block->statements) {
        auto* declaration =
            dynamic_cast<const VariableDeclarationASTNode*>(
                statement.get()
            );

        if (!declaration) {
            return VerificationResult::Unknown;
        }

        auto expression = createExpression(
            declaration->value.get(),
            declaration->type
        );

        if (!expression) {
            return VerificationResult::Failed;
        }

        if (!createSymbolicVariable(
            declaration->name,
            declaration->type
        )) {
            return VerificationResult::Failed;
        }

        auto variable = variables.find(declaration->name);

        if (variable == variables.end()) {
            return VerificationResult::Failed;
        }

        solver.add(variable->second.value == *expression);
    }

    z3::check_result result = solver.check();

    if (result == z3::sat) {
        return VerificationResult::Success;
    }

    if (result == z3::unsat) {
        return VerificationResult::Failed;
    }

    return VerificationResult::Unknown;
}
