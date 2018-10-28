#include "CompositeAction.h"

using namespace AxiomModel;

CompositeAction::CompositeAction(std::vector<std::unique_ptr<AxiomModel::Action>> actions, AxiomModel::ModelRoot *root)
    : Action(ActionType::COMPOSITE, root), _actions(std::move(actions)) {}

std::unique_ptr<CompositeAction> CompositeAction::create(std::vector<std::unique_ptr<AxiomModel::Action>> actions,
                                                         AxiomModel::ModelRoot *root) {
    return std::make_unique<CompositeAction>(std::move(actions), root);
}

void CompositeAction::forward(bool first) {
    for (const auto &action : _actions) {
        action->forward(first);
    }
}

void CompositeAction::backward() {
    for (auto i = _actions.end() - 1; i >= _actions.begin(); i--) {
        (*i)->backward();
    }
}
