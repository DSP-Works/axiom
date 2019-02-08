#pragma once

#include <optional>

#include "GroupSurface.h"
#include "Node.h"
#include "common/Promise.h"

namespace AxiomModel {

    class GroupNode : public Node {
    public:
        GroupNode(const QUuid &uuid, const QUuid &parentUuid, QPoint pos, QSize size, bool selected, QString name,
                  const QUuid &controlsUuid, const QUuid &innerUuid, ModelRoot *root);

        static std::unique_ptr<GroupNode> create(const QUuid &uuid, const QUuid &parentUuid, QPoint pos, QSize size,
                                                 bool selected, QString name, const QUuid &controlsUuid,
                                                 const QUuid &innerUuid, ModelRoot *root);

        QString debugName() override;

        AxiomCommon::Promise<GroupSurface *> &nodes() { return *_nodes; }

        const AxiomCommon::Promise<GroupSurface *> &nodes() const { return *_nodes; }

        void attachRuntime(MaximCompiler::Runtime *runtime) override;

        void buildAll(MaximCompiler::Transaction *transaction) override;

        void updateRuntimePointers(MaximCompiler::Runtime *runtime, void *surfacePtr) override;

        void remove() override;

    private:
        std::shared_ptr<AxiomCommon::Promise<GroupSurface *>> _nodes;
    };
}
