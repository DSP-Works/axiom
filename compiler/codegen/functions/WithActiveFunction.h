#pragma once

#include "../Function.h"

namespace MaximCodegen {

    class WithActiveFunction : public Function {
    public:
        WithActiveFunction(MaximContext *ctx, llvm::Module *module);

        static std::unique_ptr<WithActiveFunction> create(MaximContext *ctx, llvm::Module *module);

    protected:
        std::unique_ptr<Value>
        generate(ComposableModuleClassMethod *method, const std::vector<std::unique_ptr<Value>> &params, std::unique_ptr<VarArg> vararg) override;
    };

}
