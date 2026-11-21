#include "CodeGen.h"
#include "Parser.h"

#include <stdexcept>
#include <string>
#include <system_error>
#include <functional>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Intrinsics.h>

CodeGen::CodeGen() {
    context = std::make_unique<llvm::LLVMContext>();

    module = std::make_unique<llvm::Module>(
        "StobxModule",
        *context
    );

    builder = std::make_unique<llvm::IRBuilder<>>(
        *context
    );

    memoryManager = std::make_unique<MemoryManager>(
        *context,
        *module,
        *builder
    );

    registerFileIOFunctions();
}


CodeGen::~CodeGen() = default;

void CodeGen::registerFileIOFunctions() {
    llvm::Type* ptrTy = builder->getPtrTy();
    llvm::Type* i32Ty = builder->getInt32Ty();
    llvm::Type* sizeTy = builder->getInt64Ty();

    // FILE* fopen(const char* filename, const char* mode)
    if (!module->getFunction("fopen")) {
        llvm::FunctionType* fopenType =
            llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy}, false);
        module->getOrInsertFunction("fopen", fopenType);
    }

    // size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream)
    if (!module->getFunction("fwrite")) {
        llvm::FunctionType* fwriteType =
            llvm::FunctionType::get(sizeTy, {ptrTy, sizeTy, sizeTy, ptrTy}, false);
        module->getOrInsertFunction("fwrite", fwriteType);
    }

    // size_t fread(void* ptr, size_t size, size_t count, FILE* stream)
    if (!module->getFunction("fread")) {
        llvm::FunctionType* freadType =
            llvm::FunctionType::get(sizeTy, {ptrTy, sizeTy, sizeTy, ptrTy}, false);
        module->getOrInsertFunction("fread", freadType);
    }

    // int fclose(FILE* stream)
    if (!module->getFunction("fclose")) {
        llvm::FunctionType* fcloseType =
            llvm::FunctionType::get(i32Ty, {ptrTy}, false);
        module->getOrInsertFunction("fclose", fcloseType);

// malloc: i64 -> ptr
llvm::FunctionType* mallocType = llvm::FunctionType::get(
    llvm::PointerType::getUnqual(*context),
    {llvm::Type::getInt64Ty(*context)},
    false
);
module->getOrInsertFunction("malloc", mallocType);

// free: ptr -> void
llvm::FunctionType* freeType = llvm::FunctionType::get(
    llvm::Type::getVoidTy(*context),
    {llvm::PointerType::getUnqual(*context)},
    false
);
module->getOrInsertFunction("free", freeType);


    }
}



llvm::Type* CodeGen::getTypeFromName(const std::string& typeName) {
    if (typeName == "u1" || typeName == "i1" || typeName == "bool") {
        return builder->getInt1Ty();
    }

    if (typeName == "u8" || typeName == "i8" || typeName == "char") {
        return builder->getInt8Ty();
    }

    if (typeName == "u16" || typeName == "i16") {
        return builder->getInt16Ty();
    }

    if (typeName == "u32" || typeName == "i32") {
        return builder->getInt32Ty();
    }

    if (typeName == "u64" || typeName == "i64") {
        return builder->getInt64Ty();
    }

    if (typeName == "f32") {
        return builder->getFloatTy();
    }

    if (typeName == "f64") {
        return builder->getDoubleTy();
    }

    if (typeName == "string" || typeName == "ptr") {
        return builder->getPtrTy();
    }

    throw std::runtime_error(
        "Unknown data type: " + typeName
    );
}

llvm::Function* CodeGen::getPrintfFunction() {
    llvm::Function* printfFunc =
        module->getFunction("printf");

    if (!printfFunc) {
        llvm::FunctionType* printfType =
            llvm::FunctionType::get(
                builder->getInt32Ty(),
                builder->getPtrTy(),
                true
            );

        printfFunc = llvm::Function::Create(
            printfType,
            llvm::Function::ExternalLinkage,
            "printf",
            module.get()
        );
    }

    return printfFunc;
}

llvm::Value* CodeGen::generateExpression(ASTNode* node) {
    if (!node) {
        throw std::runtime_error("Invalid expression");
    }

    if (auto* callNode = dynamic_cast<FunctionCallASTNode*>(node)) {
        llvm::Function* function =
            module->getFunction(callNode->name);

        if (!function) {
            throw std::runtime_error(
                "Unknown function: " + callNode->name
            );
        }

        if (function->arg_size() != callNode->arguments.size()) {
            throw std::runtime_error(
                "Incorrect number of arguments in function call: " +
                callNode->name
            );
        }

        std::vector<llvm::Value*> arguments;

        for (std::size_t i = 0; i < callNode->arguments.size(); ++i) {
            llvm::Value* argument =
                generateExpression(callNode->arguments[i].get());

llvm::Type* expectedType =
    function->getFunctionType()->getParamType(i);

if (argument->getType() != expectedType) {
    if (argument->getType()->isIntegerTy() && expectedType->isIntegerTy()) {
        unsigned srcBits = argument->getType()->getIntegerBitWidth();
        unsigned dstBits = expectedType->getIntegerBitWidth();

        if (srcBits < dstBits) {
            argument = builder->CreateZExt(argument, expectedType, "zexttmp");
        } else if (srcBits > dstBits) {
            argument = builder->CreateTrunc(argument, expectedType, "trunctmp");
        }
    }
}

if (argument->getType() != expectedType) {
    throw std::runtime_error(
        "Function argument type mismatch: " +
        callNode->name
    );
}


            if (argument->getType() != expectedType) {
                throw std::runtime_error(
                    "Function argument type mismatch: " +
                    callNode->name
                );
            }

            arguments.push_back(argument);
        }

        return builder->CreateCall(
            function,
            arguments,
            "calltmp"
        );
    }

    if (auto* numNode = dynamic_cast<NumberASTNode*>(node)) {
        return builder->getInt32(
            std::stoi(numNode->value)
        );
    }

    if (auto* floatNode = dynamic_cast<FloatASTNode*>(node)) {
        return llvm::ConstantFP::get(
            builder->getDoubleTy(),
            std::stod(floatNode->value)
        );
    }

    if (auto* strNode = dynamic_cast<StringASTNode*>(node)) {
        return builder->CreateGlobalStringPtr(
            strNode->value
        );
    }

    if (auto* charNode = dynamic_cast<CharASTNode*>(node)) {
        char ch = charNode->value.empty() ? '\0' : charNode->value[0];
        return builder->getInt8(static_cast<uint8_t>(ch));
    }

    if (auto* boolNode = dynamic_cast<BoolASTNode*>(node)) {
        return builder->getInt1(boolNode->value);
    }

    if (auto* newNode = dynamic_cast<NewASTNode*>(node)) {
        llvm::Type* elemType = nullptr;
        try {
            elemType = getTypeFromName(newNode->typeName);
        } catch (...) {
            elemType = builder->getPtrTy();
        }

        llvm::Value* countVal = nullptr;
        if (newNode->sizeExpr) {
            countVal = generateExpression(newNode->sizeExpr.get());
        }

        return memoryManager->emitAlloc(elemType, countVal, "newtmp");
    }


    if (auto* varNode = dynamic_cast<VariableASTNode*>(node)) {
        auto it = variables.find(varNode->name);

        if (it == variables.end()) {
            throw std::runtime_error(
                "Unknown variable: " + varNode->name
            );
        }

        return builder->CreateLoad(
            it->second.allocation->getAllocatedType(),
            it->second.allocation,
            varNode->name
        );
    }

    if (auto* unaryNode = dynamic_cast<UnaryASTNode*>(node)) {
        llvm::Value* operand =
            generateExpression(unaryNode->operand.get());

        if (unaryNode->op == "not" || unaryNode->op == "!") {
            if (operand->getType()->isIntegerTy(1)) {
                return builder->CreateNot(operand, "nottmp");
            }
            if (operand->getType()->isIntegerTy()) {
                llvm::Value* isZero = builder->CreateICmpEQ(
                    operand,
                    llvm::ConstantInt::get(operand->getType(), 0),
                    "isz"
                );
                return isZero;
            }

            throw std::runtime_error(
                "'" + unaryNode->op + "' requires a boolean or integer expression"
            );
        }

        throw std::runtime_error(
            "Unknown unary operator: " + unaryNode->op
        );
    }

    if (auto* binNode = dynamic_cast<BinaryASTNode*>(node)) {
        llvm::Value* leftVal =
            generateExpression(binNode->left.get());

        llvm::Value* rightVal =
            generateExpression(binNode->right.get());

        bool isFP = leftVal->getType()->isFloatingPointTy() ||
                    rightVal->getType()->isFloatingPointTy();

        if (isFP) {
            if (!leftVal->getType()->isFloatingPointTy()) {
                leftVal = builder->CreateSIToFP(
                    leftVal,
                    rightVal->getType(),
                    "sitofpL"
                );
            } else if (!rightVal->getType()->isFloatingPointTy()) {
                rightVal = builder->CreateSIToFP(
                    rightVal,
                    leftVal->getType(),
                    "sitofpR"
                );
            } else if (leftVal->getType() != rightVal->getType()) {
                if (leftVal->getType()->isDoubleTy()) {
                    rightVal = builder->CreateFPExt(rightVal, builder->getDoubleTy(), "fpextR");
                } else {
                    leftVal = builder->CreateFPExt(leftVal, builder->getDoubleTy(), "fpextL");
                }
            }

            if (binNode->op == "+") return builder->CreateFAdd(leftVal, rightVal, "faddtmp");
            if (binNode->op == "-") return builder->CreateFSub(leftVal, rightVal, "fsubtmp");
            if (binNode->op == "*") return builder->CreateFMul(leftVal, rightVal, "fmultmp");
            if (binNode->op == "/") return builder->CreateFDiv(leftVal, rightVal, "fdivtmp");
            if (binNode->op == ">") return builder->CreateFCmpOGT(leftVal, rightVal, "fcmptmp");
            if (binNode->op == "<") return builder->CreateFCmpOLT(leftVal, rightVal, "fcmptmp");
            if (binNode->op == ">=") return builder->CreateFCmpOGE(leftVal, rightVal, "fcmptmp");
            if (binNode->op == "<=") return builder->CreateFCmpOLE(leftVal, rightVal, "fcmptmp");
            if (binNode->op == "==") return builder->CreateFCmpOEQ(leftVal, rightVal, "fcmptmp");
            if (binNode->op == "!=") return builder->CreateFCmpONE(leftVal, rightVal, "fcmptmp");

            throw std::runtime_error(
                "Unsupported floating-point binary operator: " + binNode->op
            );
        }

        if (binNode->op == "+") {
            return builder->CreateAdd(
                leftVal, rightVal, "addtmp"
            );
        }

        if (binNode->op == "-") {
            return builder->CreateSub(
                leftVal, rightVal, "subtmp"
            );
        }

        if (binNode->op == "*") {
            return builder->CreateMul(
                leftVal, rightVal, "multmp"
            );
        }

        if (binNode->op == "/") {
            return builder->CreateSDiv(
                leftVal, rightVal, "divtmp"
            );
        }

        if (binNode->op == "%") {
            return builder->CreateSRem(
                leftVal, rightVal, "remtmp"
            );
        }

        if (binNode->op == ">") {
            return builder->CreateICmpSGT(
                leftVal, rightVal, "cmptmp"
            );
        }

        if (binNode->op == "<") {
            return builder->CreateICmpSLT(
                leftVal, rightVal, "cmptmp"
            );
        }

        if (binNode->op == ">=") {
            return builder->CreateICmpSGE(
                leftVal, rightVal, "cmptmp"
            );
        }

        if (binNode->op == "<=") {
            return builder->CreateICmpSLE(
                leftVal, rightVal, "cmptmp"
            );
        }

        if (binNode->op == "==") {
            return builder->CreateICmpEQ(
                leftVal, rightVal, "cmptmp"
            );
        }

        if (binNode->op == "!=") {
            return builder->CreateICmpNE(
                leftVal, rightVal, "cmptmp"
            );
        }

        if (binNode->op == "and") {
            if (!leftVal->getType()->isIntegerTy(1) ||
                !rightVal->getType()->isIntegerTy(1)) {
                throw std::runtime_error(
                    "'and' requires comparison or logical expressions"
                );
            }

            return builder->CreateAnd(
                leftVal, rightVal, "andtmp"
            );
        }

        if (binNode->op == "or") {
            if (!leftVal->getType()->isIntegerTy(1) ||
                !rightVal->getType()->isIntegerTy(1)) {
                throw std::runtime_error(
                    "'or' requires comparison or logical expressions"
                );
            }
            return builder->CreateOr(
                leftVal, rightVal, "ortmp"
            );
        }

        if (binNode->op == "&" ||
            binNode->op == "|" ||
            binNode->op == "^" ||
            binNode->op == "<<" ||
            binNode->op == ">>") {

            if (!leftVal->getType()->isIntegerTy() ||
                !rightVal->getType()->isIntegerTy()) {
                throw std::runtime_error(
                    "Bitwise operators require integer operands"
                );
            }

            if (binNode->op == "&" ||
                binNode->op == "|" ||
                binNode->op == "^") {

                if (leftVal->getType() != rightVal->getType()) {
                    throw std::runtime_error(
                        "Bitwise operands must have the same integer type"
                    );
                }

                if (binNode->op == "&") {
                    return builder->CreateAnd(
                        leftVal, rightVal, "bitandtmp"
                    );
                }

                if (binNode->op == "|") {
                    return builder->CreateOr(
                        leftVal, rightVal, "bitortmp"
                    );
                }

                return builder->CreateXor(
                    leftVal, rightVal, "bitxortmp"
                );
            }

            llvm::Value* shiftAmount = builder->CreateZExtOrTrunc(
                rightVal,
                leftVal->getType(),
                "shiftamount"
            );

            if (binNode->op == "<<") {
                return builder->CreateShl(
                    leftVal, shiftAmount, "shltmp"
                );
            }

            std::function<std::string(ASTNode*)> getSourceTypeName;

            getSourceTypeName = [&](ASTNode* expression) -> std::string {
                if (auto* varNode =
                        dynamic_cast<VariableASTNode*>(expression)) {

                    auto it = variables.find(varNode->name);

                    if (it == variables.end() ||
                        it->second.typeName.empty()) {
                        throw std::runtime_error(
                            "Cannot determine signedness of variable: " +
                            varNode->name
                        );
                    }

                    return it->second.typeName;
                }

                if (auto* numberNode =
                        dynamic_cast<NumberASTNode*>(expression)) {
                    return "i32";
                }

                if (auto* binary =
                        dynamic_cast<BinaryASTNode*>(expression)) {
                    return getSourceTypeName(binary->left.get());
                }

                if (auto* unary =
                        dynamic_cast<UnaryASTNode*>(expression)) {
                    return getSourceTypeName(unary->operand.get());
                }

                throw std::runtime_error(
                    "Cannot determine source type for right shift"
                );
            };

            std::string sourceType =
                getSourceTypeName(binNode->left.get());

            if (sourceType[0] == 'i') {
                return builder->CreateAShr(
                    leftVal, shiftAmount, "ashrtmp"
                );
            }

            if (sourceType[0] == 'u') {
                return builder->CreateLShr(
                    leftVal, shiftAmount, "lshrtmp"
                );
            }

            throw std::runtime_error(
                "Right shift requires a signed or unsigned integer type"
            );
        }

        throw std::runtime_error(
            "Unknown binary operator: " + binNode->op
        );
    }

    throw std::runtime_error("Unknown expression node");
}

void CodeGen::generate(ProgramASTNode* program) {
    if (!program) {
        throw std::runtime_error("Program is empty");
    }

    if (module->getFunction("main")) {
        throw std::runtime_error(
            "Code generation has already been performed"
        );
    }

    variables.clear();
    bool hasUserMain = false;

    for (const auto& statement : program->statements) {
        if (auto* functionNode =
                dynamic_cast<FunctionASTNode*>(statement.get())) {

            if (functionNode->name == "main" && !functionNode->isExtern) {
                hasUserMain = true;
                break;
            }
        }
    }

    // Pre-declare all functions before generating their bodies.
    for (const auto& statement : program->statements) {
        auto* functionNode =
            dynamic_cast<FunctionASTNode*>(statement.get());

        if (!functionNode) {
            continue;
        }

        if (module->getFunction(functionNode->name)) {
            throw std::runtime_error(
                "Function already declared: " + functionNode->name
            );
        }

        llvm::Type* returnType =
            getTypeFromName(functionNode->returnType);

        std::vector<llvm::Type*> parameterTypes;

        for (const auto& parameter : functionNode->parameters) {
            parameterTypes.push_back(
                getTypeFromName(parameter->type)
            );
        }

        llvm::FunctionType* functionType =
            llvm::FunctionType::get(
                returnType,
                parameterTypes,
                false
            );

        llvm::Function::Create(
            functionType,
            llvm::Function::ExternalLinkage,
            functionNode->name,
            module.get()
        );
    }

    if (!hasUserMain) {
        llvm::FunctionType* mainType =
            llvm::FunctionType::get(
                builder->getInt32Ty(),
                false
            );

        llvm::Function* mainFunction =
            llvm::Function::Create(
                mainType,
                llvm::Function::ExternalLinkage,
                "main",
                module.get()
            );

        llvm::BasicBlock* entryBlock =
            llvm::BasicBlock::Create(
                *context,
                "entry",
                mainFunction
            );

        builder->SetInsertPoint(entryBlock);
    }

    llvm::Function* printfFunction = getPrintfFunction();

    llvm::Value* intFormat =
        builder->CreateGlobalStringPtr(
            "%d\n", "intFormat", 0, module.get()
        );

    llvm::Value* floatFormat =
        builder->CreateGlobalStringPtr(
            "%f\n", "floatFormat", 0, module.get()
        );

    llvm::Value* charFormat =
        builder->CreateGlobalStringPtr(
            "%c\n", "charFormat", 0, module.get()
        );

    llvm::Value* stringFormat =
        builder->CreateGlobalStringPtr(
            "%s\n", "stringFormat", 0, module.get()
        );

    llvm::Value* trueString =
        builder->CreateGlobalStringPtr(
            "true", "trueString", 0, module.get()
        );

    llvm::Value* falseString =
        builder->CreateGlobalStringPtr(
            "false", "falseString", 0, module.get()
        );

    std::function<void(ASTNode*)> generateStatement;

    generateStatement = [&](ASTNode* node) {
        if (!node) {
            return;
        }

        if (auto* deleteNode = dynamic_cast<DeleteASTNode*>(node)) {
            llvm::Value* targetVal = generateExpression(deleteNode->target.get());
            memoryManager->emitFree(targetVal);
            return;
        }

        if (auto* returnNode = dynamic_cast<ReturnASTNode*>(node)) {
            llvm::Value* returnValue =
                generateExpression(returnNode->value.get());

            llvm::Function* currentFunction =
                builder->GetInsertBlock()->getParent();

            if (returnValue->getType() != currentFunction->getReturnType()) {
                throw std::runtime_error(
                    "Return type does not match function return type"
                );
            }

            builder->CreateRet(returnValue);
            return;
        }

        if (auto* functionNode = dynamic_cast<FunctionASTNode*>(node)) {
            if (functionNode->isExtern) {
                return;
            }

            llvm::Function* function =
                module->getFunction(functionNode->name);

            if (!function) {
                throw std::runtime_error(
                    "Function was not pre-declared: " + functionNode->name
                );
            }

            auto savedInsertPoint = builder->saveIP();
            auto savedVariables = variables;

            variables.clear();

            llvm::BasicBlock* entry =
                llvm::BasicBlock::Create(
                    *context,
                    "entry",
                    function
                );

            builder->SetInsertPoint(entry);

            std::size_t index = 0;

            for (auto& argument : function->args()) {
                const auto& parameter =
                    functionNode->parameters[index];

                argument.setName(parameter->name);

                llvm::AllocaInst* alloca =
                    builder->CreateAlloca(
                       argument.getType(),
                        nullptr,
                        parameter->name
                    );

                builder->CreateStore(&argument, alloca);

                variables[parameter->name] = {
                    alloca,
                    parameter->type
                };

                ++index;
            }

            for (const auto& statement : functionNode->statements) {
                generateStatement(statement.get());
            }

            if (!builder->GetInsertBlock()->getTerminator()) {
                throw std::runtime_error(
                    "Function is missing a return statement: " +
                    functionNode->name
                );
            }

            variables = std::move(savedVariables);

            if (functionNode->name != "main") {
                builder->restoreIP(savedInsertPoint);
            }

            return;
        }

        if (auto* assertNode = dynamic_cast<AssertASTNode*>(node)) {
            llvm::Value* condition =
                generateExpression(assertNode->condition.get());

            if (!condition->getType()->isIntegerTy(1)) {
                throw std::runtime_error(
                    "Assert condition must be a boolean expression"
                );
            }

            llvm::Function* currentFunction =
                builder->GetInsertBlock()->getParent();

            llvm::BasicBlock* successBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "assert.success",
                    currentFunction
                );

            llvm::BasicBlock* failureBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "assert.failure",
                    currentFunction
                );

            builder->CreateCondBr(
                condition,
                successBlock,
                failureBlock
            );

            builder->SetInsertPoint(failureBlock);

            llvm::Function* trapFunction =
                llvm::Intrinsic::getDeclaration(
                    module.get(),
                    llvm::Intrinsic::trap
                );

            builder->CreateCall(trapFunction);
            builder->CreateUnreachable();

            builder->SetInsertPoint(successBlock);
            return;
        }

        if (auto* assumeNode = dynamic_cast<AssumeASTNode*>(node)) {
            llvm::Value* condition =
                generateExpression(assumeNode->condition.get());

            if (!condition->getType()->isIntegerTy(1)) {
                throw std::runtime_error(
                    "Assume condition must be a boolean expression"
                );
            }

            return;
        }

        if (auto* contractNode = dynamic_cast<ContractASTNode*>(node)) {
            llvm::Value* condition =
                generateExpression(contractNode->condition.get());

            if (!condition->getType()->isIntegerTy(1)) {
                throw std::runtime_error(
                    "Contract condition must be a boolean expression"
                );
            }

            return;
        }

        if (auto* assignmentNode =
                dynamic_cast<AssignmentASTNode*>(node)) {

            llvm::Value* value =
                generateExpression(assignmentNode->value.get());

            auto it = variables.find(assignmentNode->name);

            llvm::AllocaInst* variable = nullptr;

            if (it == variables.end()) {
                variable = builder->CreateAlloca(
                    value->getType(),
                    nullptr,
                    assignmentNode->name
                );

                variables[assignmentNode->name] = {
                    variable,
                    ""
                };
            } else {
                variable = it->second.allocation;

                if (variable->getAllocatedType() != value->getType()) {
                    throw std::runtime_error(
                        "Cannot assign different type to variable: " +
                        assignmentNode->name
                    );
                }
            }

            builder->CreateStore(value, variable);
            return;
        }

        if (auto* printNode =
                dynamic_cast<PrintASTNode*>(node)) {

            ASTNode* expression = printNode->expression.get();

            llvm::Value* value =
                generateExpression(expression);

            if (dynamic_cast<StringASTNode*>(expression) ||
                value->getType()->isPointerTy()) {

                builder->CreateCall(
                    printfFunction,
                    {stringFormat, value}
                );
            } else if (value->getType()->isFloatingPointTy()) {
                llvm::Value* doubleVal = builder->CreateFPExt(
                    value,
                    builder->getDoubleTy(),
                    "fpextprint"
                );
                builder->CreateCall(
                    printfFunction,
                    {floatFormat, doubleVal}
                );
            } else if (value->getType()->isIntegerTy(1)) {
                llvm::Value* selectedString =
                    builder->CreateSelect(
                        value,
                        trueString,
                        falseString,
                        "boolString"
                    );

                builder->CreateCall(
                    printfFunction,
                    {stringFormat, selectedString}
                );
            } else if (dynamic_cast<CharASTNode*>(expression) ||
                       value->getType()->isIntegerTy(8)) {
                builder->CreateCall(
                    printfFunction,
                    {charFormat, value}
                );
            } else {
                builder->CreateCall(
                    printfFunction,
                    {intFormat, value}
                );
            }

            return;
        }

        if (auto* forNode =
                dynamic_cast<ForASTNode*>(node)) {

            // Initialization
            generateStatement(forNode->initialization.get());

            llvm::Function* function =
                builder->GetInsertBlock()->getParent();

            llvm::BasicBlock* conditionBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "for.condition",
                    function
                );

            llvm::BasicBlock* bodyBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "for.body",
                    function
                );

            llvm::BasicBlock* updateBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "for.update",
                    function
                );

            llvm::BasicBlock* endBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "for.end",
                    function
                );

            breakTargets.push_back(endBlock);
            continueTargets.push_back(updateBlock);

            builder->CreateBr(conditionBlock);

            builder->SetInsertPoint(conditionBlock);

            llvm::Value* condition =
                generateExpression(forNode->condition.get());

            if (!condition->getType()->isIntegerTy(1)) {
                throw std::runtime_error(
                    "The for condition must be a comparison or logical expression"
                );
            }

            builder->CreateCondBr(
                condition,
                bodyBlock,
                endBlock
            );

            builder->SetInsertPoint(bodyBlock);

            for (const auto& statement : forNode->statements) {
                generateStatement(statement.get());
            }

            if (!builder->GetInsertBlock()->getTerminator()) {
                builder->CreateBr(updateBlock);
            }

            builder->SetInsertPoint(updateBlock);

            generateStatement(forNode->update.get());

            if (!builder->GetInsertBlock()->getTerminator()) {
                builder->CreateBr(conditionBlock);
            }

            builder->SetInsertPoint(endBlock);

            continueTargets.pop_back();
            breakTargets.pop_back();

            return;
        }

        if (auto* whileNode =
                dynamic_cast<WhileASTNode*>(node)) {

            llvm::Function* function =
                builder->GetInsertBlock()->getParent();

            llvm::BasicBlock* conditionBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "while.condition",
                    function
                );

            llvm::BasicBlock* bodyBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "while.body",
                    function
                );

            llvm::BasicBlock* endBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "while.end",
                    function
                );

            breakTargets.push_back(endBlock);
            continueTargets.push_back(conditionBlock);

            builder->CreateBr(conditionBlock);

            builder->SetInsertPoint(conditionBlock);

            llvm::Value* condition =
                generateExpression(whileNode->condition.get());

            if (!condition->getType()->isIntegerTy(1)) {
                throw std::runtime_error(
                    "The while condition must be a comparison or logical expression"
                );
            }

            builder->CreateCondBr(
                condition,
                bodyBlock,
                endBlock
            );

            builder->SetInsertPoint(bodyBlock);

            for (const auto& statement : whileNode->statements) {
                generateStatement(statement.get());

                if (builder->GetInsertBlock()->getTerminator()) {
                    break;
                }
            }

            if (!builder->GetInsertBlock()->getTerminator()) {
                builder->CreateBr(conditionBlock);
            }

            builder->SetInsertPoint(endBlock);

            continueTargets.pop_back();
            breakTargets.pop_back();

            return;
        }

        if (auto* ifNode = dynamic_cast<IfASTNode*>(node)) {
            llvm::Value* condition =
                generateExpression(ifNode->condition.get());

            if (!condition->getType()->isIntegerTy(1)) {
                throw std::runtime_error(
                    "The if condition must be a comparison or logical expression"
                );
            }

            llvm::Function* function =
                builder->GetInsertBlock()->getParent();

            llvm::BasicBlock* thenBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "if.then",
                    function
                );

            llvm::BasicBlock* elseBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "if.else",
                    function
                );

            llvm::BasicBlock* mergeBlock =
                llvm::BasicBlock::Create(
                    *context,
                    "if.end",
                    function
                );

            builder->CreateCondBr(
                condition,
                thenBlock,
                elseBlock
            );

            builder->SetInsertPoint(thenBlock);

            for (const auto& statement : ifNode->statements) {
                generateStatement(statement.get());
            }

            if (!builder->GetInsertBlock()->getTerminator()) {
                builder->CreateBr(mergeBlock);
            }

            builder->SetInsertPoint(elseBlock);

            for (const auto& statement : ifNode->elseStatements) {
                generateStatement(statement.get());
            }

            if (!builder->GetInsertBlock()->getTerminator()) {
                builder->CreateBr(mergeBlock);
            }

            builder->SetInsertPoint(mergeBlock);
            return;
        }

        if (auto* varDeclNode =
                dynamic_cast<VariableDeclarationASTNode*>(node)) {

            llvm::Type* varType = nullptr;
            if (!varDeclNode->type.empty()) {
                varType = getTypeFromName(varDeclNode->type);
            }

            llvm::Value* initValue = nullptr;
            if (varDeclNode->value) {
                initValue = generateExpression(varDeclNode->value.get());
            }

            if (!varType) {
                if (!initValue) {
                    throw std::runtime_error(
                        "Cannot infer type for variable: " + varDeclNode->name
                    );
                }
                varType = initValue->getType();
            }

            llvm::AllocaInst* alloca = builder->CreateAlloca(
                varType,
                nullptr,
                varDeclNode->name
            );

            variables[varDeclNode->name] = {
                alloca,
                varDeclNode->type
            };

            if (initValue) {
                if (alloca->getAllocatedType() != initValue->getType()) {
                    throw std::runtime_error(
                        "Initializer type does not match variable type: " +
                        varDeclNode->name
                    );
                }
                builder->CreateStore(initValue, alloca);
            }

            return;
        }


        throw std::runtime_error("Unknown statement node");
    };

    for (const auto& statement : program->statements) {
        generateStatement(statement.get());
    }

    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateRet(
            builder->getInt32(0)
        );
    }
}

void CodeGen::printIR() {
    std::error_code error;

    llvm::raw_fd_ostream file(
        "output.ll",
        error
    );

    if (error) {
        throw std::runtime_error(
            "Could not open output.ll: " + error.message()
        );
    }

    module->print(file, nullptr);
}
