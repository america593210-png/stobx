#include "MemoryManager.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <stdexcept>

MemoryManager::MemoryManager(llvm::LLVMContext& ctx,
                             llvm::Module& mod,
                             llvm::IRBuilder<>& bld)
    : context(ctx), module(mod), builder(bld) {}

llvm::Function* MemoryManager::getOrCreateMalloc() {
    if (auto* fn = module.getFunction("malloc")) {
        return fn;
    }

    llvm::FunctionType* mallocType = llvm::FunctionType::get(
        builder.getPtrTy(),
        {builder.getInt64Ty()},
        false
    );

    return llvm::Function::Create(
        mallocType,
        llvm::Function::ExternalLinkage,
        "malloc",
        &module
    );
}

llvm::Function* MemoryManager::getOrCreateFree() {
    if (auto* fn = module.getFunction("free")) {
        return fn;
    }

    llvm::FunctionType* freeType = llvm::FunctionType::get(
        builder.getVoidTy(),
        {builder.getPtrTy()},
        false
    );

    return llvm::Function::Create(
        freeType,
        llvm::Function::ExternalLinkage,
        "free",
        &module
    );
}

llvm::Value* MemoryManager::computeAllocationBytes(llvm::Type* elemType,
                                                 llvm::Value* countExpr) {
    if (!elemType) {
        throw std::runtime_error("Cannot allocate memory for null element type");
    }

    const llvm::DataLayout& dl = module.getDataLayout();
    uint64_t elemSize = dl.getTypeAllocSize(elemType);
    llvm::Value* totalBytes = builder.getInt64(elemSize);

    if (countExpr) {
        if (!countExpr->getType()->isIntegerTy()) {
            throw std::runtime_error("Allocation size must be an integer");
        }

        llvm::Value* count64 = builder.CreateZExtOrTrunc(
            countExpr,
            builder.getInt64Ty(),
            "memcount64"
        );

        totalBytes = builder.CreateMul(
            count64,
            builder.getInt64(elemSize),
            "membytes"
        );
    }

    return totalBytes;
}

llvm::Value* MemoryManager::emitAlloc(llvm::Type* elemType,
                                      llvm::Value* countExpr,
                                      const llvm::Twine& name) {
    llvm::Value* totalBytes = computeAllocationBytes(elemType, countExpr);
    llvm::Function* mallocFn = getOrCreateMalloc();
    return builder.CreateCall(mallocFn, {totalBytes}, name);
}

void MemoryManager::emitFree(llvm::Value* ptrVal) {
    if (!ptrVal || !ptrVal->getType()->isPointerTy()) {
        throw std::runtime_error("'delete' requires a pointer expression");
    }

    llvm::Function* freeFn = getOrCreateFree();
    builder.CreateCall(freeFn, {ptrVal});
}
