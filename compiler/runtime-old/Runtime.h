#pragma once

#include <memory>
#include <vector>
#include <QtCore/QMutex>

#include "Jit.h"
#include "ValueOperator.h"
#include "../codegen/MaximContext.h"
#include "RootSchematic.h"

namespace MaximRuntime {

    class ControlGroup;

    class Runtime {
    private:

    public:
        Runtime();

        MaximCodegen::MaximContext *context() { return &_context; }

        Jit *jit() { return &_jit; }

        ValueOperator *op() { return &_op; }

        RootSchematic &mainSchematic() { return _mainSchematic; }

        void compileAndDeploy();

        void generate();

        void fillBuffer(float **buffer, size_t size);

        void lock() { _mutex.lock(); }

        void unlock() { _mutex.unlock(); }

        bool tryLock(int timeout = 0) { return _mutex.tryLock(timeout); }

    private:
        QMutex _mutex;
        Jit _jit;
        MaximCodegen::MaximContext _context;
        ValueOperator _op;
        RootSchematic _mainSchematic;
        llvm::Module _module;

        bool _isDeployed = false;
        Jit::ModuleKey _deployKey;

        void (*_generateFuncPtr)() = nullptr;
    };

}