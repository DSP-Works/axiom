#include "ExpressionGenerator.h"

#include <llvm/IR/DerivedTypes.h>

#include "Context.h"
#include "Function.h"
#include "Scope.h"
#include "CodegenError.h"
#include "Control.h"
#include "values/Value.h"
#include "values/MidiValue.h"
#include "values/NumValue.h"
#include "values/TupleValue.h"

#include "../ast/NoteExpression.h"
#include "../ast/NumberExpression.h"
#include "../ast/TupleExpression.h"
#include "../ast/CallExpression.h"
#include "../ast/CastExpression.h"
#include "../ast/VariableExpression.h"
#include "../ast/UnaryExpression.h"
#include "../ast/PostfixExpression.h"

#include "../util.h"

using namespace MaximCodegen;
using namespace MaximAst;

ExpressionGenerator::ExpressionGenerator(Context *context) : _context(context) {

}

std::unique_ptr<Value> ExpressionGenerator::generateExpr(MaximAst::Expression *expr, Function *function, Scope *scope) {
    if (auto note = dynamic_cast<NoteExpression *>(expr)) return generateNote(note, function, scope);
    if (auto number = dynamic_cast<NumberExpression *>(expr)) return generateNumber(number, function, scope);
    if (auto tuple = dynamic_cast<TupleExpression *>(expr)) return generateTuple(tuple, function, scope);
    if (auto call = dynamic_cast<CallExpression *>(expr)) return generateCall(call, function, scope);
    if (auto cast = dynamic_cast<CastExpression *>(expr)) return generateCast(cast, function, scope);
    if (auto control = dynamic_cast<ControlExpression *>(expr)) return generateControl(control, function, scope);
    if (auto variable = dynamic_cast<VariableExpression *>(expr)) return generateVariable(variable, function, scope);
    if (auto math = dynamic_cast<MathExpression *>(expr)) return generateMath(math, function, scope);
    if (auto unary = dynamic_cast<UnaryExpression *>(expr)) return generateUnary(unary, function, scope);
    if (auto assign = dynamic_cast<AssignExpression *>(expr)) return generateAssign(assign, function, scope);
    if (auto postfix = dynamic_cast<PostfixExpression *>(expr)) return generatePostfix(postfix, function, scope);

    assert(false);
    throw;
}

std::unique_ptr<Value>
ExpressionGenerator::generateNote(MaximAst::NoteExpression *expr, Function *function, Scope *scope) {
    auto constFloat = _context->getConstantFloat(expr->note);
    auto constVec = llvm::ConstantVector::get(std::array<llvm::Constant *, 2> {constFloat, constFloat});

    return std::make_unique<NumValue>(true, constVec, FormValue(
            Form::Type::NOTE, FormValue::ParamArr {}, _context, function
    ), _context, function);
}

std::unique_ptr<Value>
ExpressionGenerator::generateNumber(MaximAst::NumberExpression *expr, Function *function, Scope *scope) {
    auto constFloat = _context->getConstantFloat(expr->value);
    auto constVec = llvm::ConstantVector::get(std::array<llvm::Constant *, 2> {constFloat, constFloat});

    return std::make_unique<NumValue>(true, constVec, FormValue(
            Form::Type::LINEAR, FormValue::ParamArr {}, _context, function
    ), _context, function);
}

std::unique_ptr<Value>
ExpressionGenerator::generateTuple(MaximAst::TupleExpression *expr, Function *function, Scope *scope) {
    std::vector<llvm::Value *> tupleValues;
    tupleValues.reserve(expr->expressions.size());
    bool isConst = true;
    for (const auto &tupleItem : expr->expressions) {
        auto val = generateExpr(tupleItem.get(), function, scope);
        tupleValues.push_back(val->value());
        if (!val->isConst()) isConst = false;
    }
    auto finalVal = std::make_unique<TupleValue>(isConst, tupleValues, _context, function);
    return std::move(finalVal);
}

std::unique_ptr<Value>
ExpressionGenerator::generateCall(MaximAst::CallExpression *expr, Function *function, Scope *scope) {
    auto func = _context->getFunction(expr->name);
    if (!func) {
        throw CodegenError(
                "WHAT IS THIS??!?! def not a valid function name :(",
                expr->startPos, expr->endPos
        );
    }

    std::vector<Function::ParamData> params;
    params.reserve(expr->arguments.size());
    for (const auto &arg : expr->arguments) {
        params.emplace_back(
                generateExpr(arg.get(), function, scope),
                arg->startPos,
                arg->endPos
        );
    }
    return func->generateCall(params, expr->startPos, expr->endPos, function);
}

std::unique_ptr<Value>
ExpressionGenerator::generateCast(MaximAst::CastExpression *expr, Function *function, Scope *scope) {
    // get base expression value
    auto exprValue = generateExpr(expr->expr.get(), function, scope);
    _context->checkPtrType(exprValue->value(), Context::Type::NUM, expr->expr->startPos, expr->expr->endPos);
    auto numValue = AxiomUtil::strict_unique_cast<NumValue>(std::move(exprValue));

    // get form expression value
    auto targetForm = expr->target.get();
    if (targetForm->arguments.size() > Context::formParamCount) {
        throw CodegenError(
                "Oy, you doin me a bamboozle. I only want " + std::to_string(Context::formParamCount) +
                " parameters here.",
                targetForm->arguments[0]->startPos,
                targetForm->arguments[targetForm->arguments.size() - 1]->endPos
        );
    }

    auto isConst = numValue->isConst();
    FormValue::ParamArr params = {};
    for (size_t i = 0; i < targetForm->arguments.size(); i++) {
        auto argExpr = generateExpr(targetForm->arguments[i].get(), function, scope);
        _context->checkPtrType(argExpr->value(), Context::Type::NUM, targetForm->arguments[i]->startPos,
                            targetForm->arguments[i]->endPos);
        auto numArg = AxiomUtil::strict_unique_cast<NumValue>(std::move(argExpr));
        params[i] = function->codeBuilder().CreateLoad(numArg->valuePtr(function->codeBuilder()), "cast_param");
        if (!numArg->isConst()) isConst = false;
    }

    FormValue form(expr->target->type, params, _context, function);

    if (expr->isConvert) {
        // todo
        assert(false);
        throw;
    } else {
        return std::make_unique<NumValue>(
                isConst,
                function->codeBuilder().CreateLoad(numValue->valuePtr(function->codeBuilder()), "cast_val_temp"),
                form, _context, function
        );
    }
}

std::unique_ptr<Value>
ExpressionGenerator::generateControl(MaximAst::ControlExpression *expr, Function *function, Scope *scope) {
    auto control = scope->getControl(expr->name, expr->type);
    control->setMode(Control::Mode::INPUT);
    auto prop = control->getProperty(expr->prop);
    if (!prop) {
        throw CodegenError(
                "My longest ye boi ever: before you tried to read the " + expr->prop + " property which doesn't freakin exist!",
                expr->startPos, expr->endPos
        );
    }

    return prop->clone();
}

std::unique_ptr<Value>
ExpressionGenerator::generateVariable(MaximAst::VariableExpression *expr, Function *function, Scope *scope) {
    auto val = scope->findValue(expr->name);
    if (!val) {
        throw CodegenError("Ah hekkers mah dude! This variable hasn't been set yet!", expr->startPos, expr->endPos);
    }

    return val->clone();
}

std::unique_ptr<Value>
ExpressionGenerator::generateMath(MaximAst::MathExpression *expr, Function *function, Scope *scope) {
    auto leftExpr = generateExpr(expr->left.get(), function, scope);
    auto rightExpr = generateExpr(expr->right.get(), function, scope);

    // todo: if both left and right are tuples, add them piece-wise
    //       if one is a tuple but the other isn't, add the constant to each of them

    _context->checkPtrType(leftExpr->value(), Context::Type::NUM, expr->left->startPos, expr->left->endPos);
    _context->checkPtrType(rightExpr->value(), Context::Type::NUM, expr->right->startPos, expr->right->endPos);

    auto leftNum = AxiomUtil::strict_unique_cast<NumValue>(std::move(leftExpr));
    auto rightNum = AxiomUtil::strict_unique_cast<NumValue>(std::move(rightExpr));

    auto isConst = leftNum->isConst() && rightNum->isConst();
    auto cb = function->codeBuilder();
    auto leftVal = cb.CreateLoad(leftNum->valuePtr(cb), "math_left");
    auto rightVal = cb.CreateLoad(rightNum->valuePtr(cb), "math_right");

    auto newVal = generateFloatIntCompMath(expr->type, leftVal, rightVal, function);
    auto finalVal = std::make_unique<NumValue>(
            isConst, newVal,
            FormValue(leftNum->formPtr(cb), _context),
            _context, function
    );
    return evaluateConstVal(std::move(finalVal));
}

llvm::Value *ExpressionGenerator::generateFloatIntCompMath(MaximAst::MathExpression::Type type, llvm::Value *leftVal,
                                                           llvm::Value *rightVal, Function *function) {
    switch (type) {
        case MathExpression::Type::ADD:
            return function->codeBuilder().CreateFAdd(leftVal, rightVal, "math_add");
        case MathExpression::Type::SUBTRACT:
            return function->codeBuilder().CreateFSub(leftVal, rightVal, "math_sub");
        case MathExpression::Type::MULTIPLY:
            return function->codeBuilder().CreateFMul(leftVal, rightVal, "math_mul");
        case MathExpression::Type::DIVIDE:
            return function->codeBuilder().CreateFDiv(leftVal, rightVal, "math_div");
        case MathExpression::Type::MODULO:
            return function->codeBuilder().CreateFRem(leftVal, rightVal, "math_rem");
        case MathExpression::Type::POWER:
            // todo: call intrinsic?
            assert(false);
            throw;
            break;
        default:
            return generateIntCompMath(type, leftVal, rightVal, function);
    }
}

llvm::Value *ExpressionGenerator::generateIntCompMath(MaximAst::MathExpression::Type type, llvm::Value *leftVal,
                                                      llvm::Value *rightVal, Function *function) {
    auto floatVec = llvm::VectorType::get(llvm::Type::getFloatTy(_context->llvm()), 2);;
    auto intVec = llvm::VectorType::get(llvm::Type::getInt32Ty(_context->llvm()), 2);

    auto intLeft = function->codeBuilder().CreateFPToSI(leftVal, intVec, "comp_left_int");
    auto intRight = function->codeBuilder().CreateFPToSI(rightVal, intVec, "comp_right_int");

    llvm::Value *result;

    switch (type) {
        case MathExpression::Type::BITWISE_AND:
            result = function->codeBuilder().CreateAnd(intLeft, intRight, "comp_and");
            break;
        case MathExpression::Type::BITWISE_OR:
            result = function->codeBuilder().CreateOr(intLeft, intRight, "comp_or");
            break;
        case MathExpression::Type::BITWISE_XOR:
            result = function->codeBuilder().CreateXor(intLeft, intRight, "comp_xor");
            break;
        default:
            return generateCompareMath(type, leftVal, rightVal, function);
    }

    return function->codeBuilder().CreateSIToFP(result, floatVec, "comp_result");
}

llvm::Value *ExpressionGenerator::generateCompareMath(MaximAst::MathExpression::Type type, llvm::Value *leftVal,
                                                      llvm::Value *rightVal, Function *function) {
    auto floatVec = llvm::VectorType::get(llvm::Type::getFloatTy(_context->llvm()), 2);

    auto zeroConst = _context->getConstantFloat(0);
    auto zeroVec = llvm::ConstantVector::get(std::array<llvm::Constant *, 2> {zeroConst, zeroConst});

    llvm::Value *result;

    switch (type) {
        case MathExpression::Type::LOGICAL_AND:
            result = function->codeBuilder().CreateAnd(
                    function->codeBuilder().CreateFCmpONE(leftVal, zeroVec, "comp_land_left"),
                    function->codeBuilder().CreateFCmpONE(leftVal, zeroVec, "comp_land_right"),
                    "comp_land"
            );
            break;
        case MathExpression::Type::LOGICAL_OR:
            result = function->codeBuilder().CreateOr(
                    function->codeBuilder().CreateFCmpONE(leftVal, zeroVec, "comp_lor_left"),
                    function->codeBuilder().CreateFCmpONE(leftVal, zeroVec, "comp_lor_right"),
                    "comp_lor"
            );
            break;
        case MathExpression::Type::LOGICAL_EQUAL:
            result = function->codeBuilder().CreateFCmpOEQ(leftVal, rightVal, "comp_leq");
            break;
        case MathExpression::Type::LOGICAL_NOT_EQUAL:
            result = function->codeBuilder().CreateFCmpONE(leftVal, rightVal, "comp_lne");
            break;
        case MathExpression::Type::LOGICAL_GT:
            result = function->codeBuilder().CreateFCmpOGT(leftVal, rightVal, "comp_gt");
            break;
        case MathExpression::Type::LOGICAL_LT:
            result = function->codeBuilder().CreateFCmpOLT(leftVal, rightVal, "comp_lt");
            break;
        case MathExpression::Type::LOGICAL_GTE:
            result = function->codeBuilder().CreateFCmpOGE(leftVal, rightVal, "comp_gte");
            break;
        case MathExpression::Type::LOGICAL_LTE:
            result = function->codeBuilder().CreateFCmpOLE(leftVal, rightVal, "comp_lte");
            break;
        default:
            assert(false);
            throw;
    }

    return function->codeBuilder().CreateSIToFP(result, floatVec, "comp_result");
}

std::unique_ptr<Value>
ExpressionGenerator::generateUnary(MaximAst::UnaryExpression *expr, Function *function, Scope *scope) {
    auto valExpr = generateExpr(expr->expr.get(), function, scope);

    _context->checkPtrType(valExpr->value(), Context::Type::NUM, expr->expr->startPos, expr->expr->endPos);

    auto valNum = AxiomUtil::strict_unique_cast<NumValue>(std::move(valExpr));
    auto readVal = function->codeBuilder().CreateLoad(valNum->valuePtr(function->codeBuilder()), "unary_temp");

    auto floatVec = llvm::VectorType::get(llvm::Type::getFloatTy(_context->llvm()), 2);

    llvm::Value *result;
    switch (expr->type) {
        case UnaryExpression::Type::POSITIVE:
            result = readVal;
            break;
        case UnaryExpression::Type::NEGATIVE:
            result = function->codeBuilder().CreateFMul(
                    readVal,
                    _context->getConstantFloat(-1),
                    "unary_negate"
            );
            break;
        case UnaryExpression::Type::NOT:
            result = function->codeBuilder().CreateSIToFP(function->codeBuilder().CreateFCmpOEQ(
                    readVal,
                    _context->getConstantFloat(0),
                    "unary_not_temp"
            ), floatVec, "unary_not");
            break;
    }

    auto cb = function->codeBuilder();
    auto finalVal = std::make_unique<NumValue>(
            valNum->isConst(), result,
            FormValue(valNum->formPtr(cb), _context),
            _context, function
    );
    return evaluateConstVal(std::move(finalVal));
}

std::unique_ptr<Value>
ExpressionGenerator::generateAssign(MaximAst::AssignExpression *expr, Function *function, Scope *scope) {
    auto rightExpr = generateExpr(expr->right.get(), function, scope);

    auto leftTuple = expr->left->assignments.size() > 1;
    auto rightTuple = dynamic_cast<TupleValue *>(rightExpr.get());

    if (leftTuple && rightTuple) {
        auto leftSize = expr->left->assignments.size();
        auto rightSize = rightTuple->type()->getNumElements();
        if (leftSize != rightSize) {
            throw CodegenError(
                    "OOOOOOOOOOOOOOOOOOOOOOYYYYYY!!!!1! You're trying to assign " + std::to_string(rightSize) +
                    " values to " + std::to_string(leftSize) + " ones!",
                    expr->startPos, expr->endPos
            );
        }

        for (size_t i = 0; i < leftSize; i++) {
            auto leftAssignable = expr->left->assignments[i].get();
            auto rightValue = _context->llToValue(
                    rightExpr->isConst(),
                    function->codeBuilder().CreateLoad(rightTuple->itemPtr((unsigned int) i, function->codeBuilder()), "assign_temp")
            );
            generateSingleAssign(leftAssignable, rightValue.get(), expr->type, expr->right->startPos,
                                 expr->right->endPos, function, scope);
        }

    } else if (leftTuple) {
        for (const auto &assignment : expr->left->assignments) {
            generateSingleAssign(assignment.get(), rightExpr.get(), expr->type, expr->right->startPos,
                                 expr->right->endPos, function, scope);
        }
    } else {
        generateSingleAssign(expr->left->assignments[0].get(), rightExpr.get(), expr->type, expr->right->startPos,
                             expr->right->endPos, function, scope);
    }

    return std::move(rightExpr);
}

void ExpressionGenerator::generateSingleAssign(MaximAst::AssignableExpression *leftExpr, Value *rightValue,
                                               MaximAst::AssignExpression::Type type, SourcePos rightStart,
                                               SourcePos rightEnd, Function *function, Scope *scope) {
    if (type == AssignExpression::Type::ASSIGN) {
        generateBasicAssign(leftExpr, rightValue, function, scope);
        return;
    }

    auto leftValue = generateExpr(leftExpr, function, scope);
    _context->checkPtrType(leftValue->value(), Context::Type::NUM, leftExpr->startPos, leftExpr->endPos);
    auto leftNum = AxiomUtil::strict_unique_cast<NumValue>(std::move(leftValue));

    _context->checkPtrType(rightValue->value(), Context::Type::NUM, rightStart, rightEnd);
    auto rightNum = dynamic_cast<NumValue *>(rightValue);
    assert(rightNum);

    auto cb = function->codeBuilder();
    auto leftVal = cb.CreateLoad(leftNum->valuePtr(cb), "assign_left");
    auto rightVal = cb.CreateLoad(rightNum->valuePtr(cb), "assign_right");

    llvm::Value *newRight;

    switch (type) {
        case AssignExpression::Type::ADD:
            newRight = cb.CreateFAdd(leftVal, rightVal, "assign_add");
            break;
        case AssignExpression::Type::SUBTRACT:
            newRight = cb.CreateFSub(leftVal, rightVal, "assign_sub");
            break;
        case AssignExpression::Type::MULTIPLY:
            newRight = cb.CreateFMul(leftVal, rightVal, "assign_mul");
            break;
        case AssignExpression::Type::DIVIDE:
            newRight = cb.CreateFDiv(leftVal, rightVal, "assign_div");
            break;
        case AssignExpression::Type::MODULO:
            newRight = cb.CreateFRem(leftVal, rightVal, "assign_mod");
            break;
        case AssignExpression::Type::POWER:
            // todo: call intrinsic?
            break;
        default:
            assert(false);
    }

    NumValue realVal(
            leftNum->isConst() && rightNum->isConst(), newRight,
            FormValue(leftNum->formPtr(cb), _context),
            _context, function
    );
    generateBasicAssign(leftExpr, &realVal, function, scope);
}

void ExpressionGenerator::generateBasicAssign(MaximAst::AssignableExpression *leftExpr, Value *rightValue,
                                              Function *function, Scope *scope) {
    if (auto var = dynamic_cast<VariableExpression *>(leftExpr)) {
        return generateVariableAssign(var, rightValue, function, scope);
    }
    if (auto control = dynamic_cast<ControlExpression *>(leftExpr)) {
        return generateControlAssign(control, rightValue, function, scope);
    }
    assert(false);
}

void ExpressionGenerator::generateVariableAssign(MaximAst::VariableExpression *leftExpr, Value *rightValue,
                                                 Function *function, Scope *scope) {
    scope->setValue(leftExpr->name, rightValue->clone());
}

void ExpressionGenerator::generateControlAssign(MaximAst::ControlExpression *leftExpr, Value *rightValue,
                                                Function *function, Scope *scope) {
    auto control = scope->getControl(leftExpr->name, leftExpr->type);
    control->setMode(Control::Mode::OUTPUT);
    if (!control->setProperty(leftExpr->prop, rightValue->clone())) {
        throw CodegenError(
            "A wise man once said, you can't bake your cake and eat it too. On a completely unrelated topic, " + leftExpr->prop + " ISN\"T A VALID PROPERTY HERE!! D:",
            leftExpr->startPos, leftExpr->endPos
        );
    }
}

std::unique_ptr<Value>
ExpressionGenerator::generatePostfix(MaximAst::PostfixExpression *expr, Function *function, Scope *scope) {
    bool isResultConst = true;
    std::vector<llvm::Value *> resultValues;
    resultValues.reserve(expr->left->assignments.size());

    for (const auto &var : expr->left->assignments) {
        auto leftValue = generateExpr(var.get(), function, scope);
        _context->checkPtrType(leftValue->value(), Context::Type::NUM, var->startPos, var->endPos);
        auto leftNum = AxiomUtil::strict_unique_cast<NumValue>(std::move(leftValue));

        auto leftVal = function->codeBuilder().CreateLoad(leftNum->valuePtr(function->codeBuilder()), "postfix_temp");

        llvm::Value *newRight;
        switch (expr->type) {
            case PostfixExpression::Type::INCREMENT:
                newRight = function->codeBuilder().CreateFAdd(
                        leftVal,
                        _context->getConstantFloat(1),
                        "postfix_inc"
                );
                break;
            case PostfixExpression::Type::DECREMENT:
                newRight = function->codeBuilder().CreateFSub(
                        leftVal,
                        _context->getConstantFloat(1),
                        "postfix_dec"
                );
                break;
        }

        auto rightVal = evaluateConstVal(std::make_unique<NumValue>(
                leftNum->isConst(), newRight,
                FormValue(leftNum->formPtr(function->codeBuilder()), _context),
                _context, function
        ));
        generateBasicAssign(var.get(), rightVal.get(), function, scope);

        if (!leftNum->isConst()) isResultConst = false;
        resultValues.push_back(rightVal->value());
    }

    return std::make_unique<TupleValue>(isResultConst, resultValues, _context, function);
}

std::unique_ptr<NumValue> ExpressionGenerator::evaluateConstVal(std::unique_ptr<NumValue> value) {
    // todo
    return value;
}

std::unique_ptr<MidiValue> ExpressionGenerator::evaluateConstVal(std::unique_ptr<MidiValue> value) {
    // todo
    return value;
}

std::unique_ptr<TupleValue> ExpressionGenerator::evaluateConstVal(std::unique_ptr<TupleValue> value) {
    // todo
    return value;
}