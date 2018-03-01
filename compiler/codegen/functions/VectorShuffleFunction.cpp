#include "VectorShuffleFunction.h"

#include "../MaximContext.h"
#include "../ComposableModuleClassMethod.h"
#include "../Num.h"

using namespace MaximCodegen;

VectorShuffleFunction::VectorShuffleFunction(MaximContext *ctx, llvm::Module *module, std::string name,
                                             llvm::ArrayRef<uint32_t> shuffle)
    : Function(ctx, module, std::move(name), ctx->numType(), {Parameter(ctx->numType(), false, false)}, nullptr),
      _shuffle(std::move(shuffle)) {

}

std::unique_ptr<VectorShuffleFunction> VectorShuffleFunction::create(MaximContext *ctx, llvm::Module *module,
                                                                     std::string name,
                                                                     llvm::ArrayRef<uint32_t> shuffle) {
    return std::make_unique<VectorShuffleFunction>(ctx, module, name, shuffle);
}

std::unique_ptr<Value> VectorShuffleFunction::generate(ComposableModuleClassMethod *method,
                                                       const std::vector<std::unique_ptr<Value>> &params,
                                                       std::unique_ptr<VarArg> vararg) {
    auto &b = method->builder();
    auto xNum = dynamic_cast<Num *>(params[0].get());
    assert(xNum);

    auto xVec = xNum->vec(b);
    auto newVec = b.CreateShuffleVector(
        xVec, llvm::UndefValue::get(ctx()->numType()->vecType()),
        _shuffle, "shuffled"
    );

    auto undefPos = SourcePos(-1, -1);
    return xNum->withVec(b, newVec, undefPos, undefPos);
}
