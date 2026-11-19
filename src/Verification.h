
#ifndef VERIFICATION_H
#define VERIFICATION_H

#include "Parser.h"
#include <z3++.h>
#include <unordered_map>
#include <optional>

enum class VerificationResult {
    Success,
    Failed,
    Unknown
};

class Verification {
public:
    Verification();

    VerificationResult verify(
        const VerificationASTNode* block
    );

bool testNumber(
    const std::string& value,
    const std::string& typeName
);

private:
    struct IntegerType {
        unsigned bitWidth;
        bool isSigned;
    };

    struct SymbolicVariable {
        z3::expr value;
        IntegerType type;

        SymbolicVariable(
            const z3::expr& value,
            const IntegerType& type
        )
            : value(value), type(type) {}
    };

    bool getIntegerType(
        const std::string& typeName,
        IntegerType& type
    ) const;

    bool createSymbolicVariable(
        const std::string& name,
        const std::string& typeName
    );

std::optional<z3::expr> createNumber(
    const NumberASTNode& number,
    const std::string& typeName,
    bool negative = false
);


std::optional<z3::expr> createExpression(
    const ASTNode* node,
    const std::string& typeName
);

    std::unordered_map<std::string, SymbolicVariable> variables;

    unsigned symbolCounter = 0;

    z3::context context;
    z3::solver solver;
};

#endif

