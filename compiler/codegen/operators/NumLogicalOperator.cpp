#include "NumLogicalOperator.h"

#include "../MaximContext.h"
#include "../Num.h"
#include "../ModuleClassMethod.h"

using namespace MaximCodegen;

NumLogicalOperator::NumLogicalOperator(MaximContext *context, MaximCommon::OperatorType type, ActiveMode activeMode,
                                       llvm::Instruction::BinaryOps op)
    : NumOperator(context, type, activeMode), _op(op) {
}

std::unique_ptr<NumLogicalOperator> NumLogicalOperator::create(MaximContext *context, MaximCommon::OperatorType type,
                                                               ActiveMode activeMode, llvm::Instruction::BinaryOps op) {
    return std::make_unique<NumLogicalOperator>(context, type, activeMode, op);
}

std::unique_ptr<Num> NumLogicalOperator::call(ModuleClassMethod *method, Num *numLeft, Num *numRight) {
    auto &b = method->builder();
    auto zeroConst = context()->constFloat(0);
    auto zeroVec = llvm::ConstantVector::get({zeroConst, zeroConst});

    auto operatedInt = b.CreateBinOp(
        _op,
        b.CreateFCmp(llvm::CmpInst::Predicate::FCMP_ONE, numLeft->vec(b), zeroVec, "logical.left"),
        b.CreateFCmp(llvm::CmpInst::Predicate::FCMP_ONE, numRight->vec(b), zeroVec, "logical.right"),
        "op.ivec"
    );
    auto isActive = b.CreateAnd(getActive(b, numLeft, numRight), operatedInt, "op.active");
    auto operatedFloat = b.CreateUIToFP(operatedInt, context()->numType()->vecType(), "op.vec");

    auto undefPos = SourcePos(-1, -1);
    auto newNum = Num::create(context(), method->allocaBuilder(), undefPos, undefPos);
    newNum->setVec(b, operatedFloat);
    newNum->setForm(b, numLeft->form(b));
    newNum->setActive(b, isActive);
    return std::move(newNum);
}
