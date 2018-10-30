#include "CreateConnectionAction.h"

#include "../ModelRoot.h"
#include "../PoolOperators.h"
#include "../objects/Connection.h"
#include "../objects/NodeSurface.h"

using namespace AxiomModel;

CreateConnectionAction::CreateConnectionAction(const QUuid &uuid, const QUuid &parentUuid, const QUuid &controlA,
                                               const QUuid &controlB, AxiomModel::ModelRoot *root)
    : Action(ActionType::CREATE_CONNECTION, root), _uuid(uuid), _parentUuid(parentUuid), _controlA(controlA),
      _controlB(controlB) {}

std::unique_ptr<CreateConnectionAction> CreateConnectionAction::create(const QUuid &uuid, const QUuid &parentUuid,
                                                                       const QUuid &controlA, const QUuid &controlB,
                                                                       AxiomModel::ModelRoot *root) {
    return std::make_unique<CreateConnectionAction>(uuid, parentUuid, controlA, controlB, root);
}

std::unique_ptr<CreateConnectionAction> CreateConnectionAction::create(const QUuid &parentUuid, const QUuid &controlA,
                                                                       const QUuid &controlB,
                                                                       AxiomModel::ModelRoot *root) {
    return create(QUuid::createUuid(), parentUuid, controlA, controlB, root);
}

void CreateConnectionAction::forward(bool) {
    root()->pool().registerObj(Connection::create(_uuid, _parentUuid, _controlA, _controlB, root()));
}

void CreateConnectionAction::backward() {
    find(root()->connections().sequence(), _uuid)->remove();
}
