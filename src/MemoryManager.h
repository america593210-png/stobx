#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

class MemoryManager {
public:
    MemoryManager(llvm::LLVMContext& context,
                  llvm::Module& module,
                  llvm::IRBuilder<>& builder);

    llvm::Value* computeAllocationBytes(llvm::Type* elemType, llvm::Value* countExpr = nullptr);

    llvm::Value* emitAlloc(llvm::Type* elemType,
                           llvm::Value* countExpr = nullptr,
                           const llvm::Twine& name = "memalloc");

    void emitFree(llvm::Value* ptrVal);

private:
    llvm::LLVMContext& context;
    llvm::Module& module;
    llvm::IRBuilder<>& builder;

    llvm::Function* getOrCreateMalloc();
    llvm::Function* getOrCreateFree();
};
