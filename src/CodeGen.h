#ifndef CODEGEN_H
#define CODEGEN_H

#include "Parser.h"
#include "MemoryManager.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <memory>
#include <unordered_map>
#include <vector>

class CodeGen {
public:
    CodeGen();
    ~CodeGen();
    void generate(ProgramASTNode* program);
    void printIR();
void registerFileIOFunctions();


private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<MemoryManager> memoryManager;

    struct VariableInfo {
        llvm::AllocaInst* allocation;
        std::string typeName;
    };

    std::unordered_map<std::string, VariableInfo> variables;
    std::vector<llvm::BasicBlock*> breakTargets;
    std::vector<llvm::BasicBlock*> continueTargets;

    llvm::Value* generateExpression(ASTNode* node);
    llvm::Function* getPrintfFunction();
    llvm::Type* getTypeFromName(const std::string& typeName);
};

#endif
