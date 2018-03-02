#include "ControlFieldClassMethod.h"

#include "ControlField.h"

using namespace MaximCodegen;

ControlFieldClassMethod::ControlFieldClassMethod(ControlField *controlField, std::string name, llvm::Type *returnType,
                                                 std::vector<llvm::Type *> paramTypes)
    : ModuleClassMethod(controlField, std::move(name), returnType, std::move(paramTypes)) {
    _contextLoadedPtr = builder().CreateLoad(contextPtr(), "controlctx");
}