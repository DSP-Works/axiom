#include "SurfaceMirBuilder.h"

#include <cmath>

#include "../model/ModelRoot.h"
#include "../model/PoolOperators.h"
#include "../model/objects/ControlSurface.h"
#include "../model/objects/CustomNode.h"
#include "../model/objects/GroupNode.h"
#include "../model/objects/GroupSurface.h"
#include "../model/objects/NumControl.h"
#include "../model/objects/RootSurface.h"

using namespace MaximCompiler;

struct ValueGroup {
    std::vector<QUuid> controls;

    void mergeInto(ValueGroup *target) {
        target->controls.insert(target->controls.end(), controls.begin(), controls.end());
    }
};

bool areAnyExposed(const std::vector<AxiomModel::Control *> &controls) {
    for (const auto &control : controls) {
        if (!control->exposerUuid().isNull()) {
            return true;
        }
    }
    return false;
}

AxiomModel::PortalControl *findPortal(const std::vector<AxiomModel::Control *> &controls) {
    for (const auto &control : controls) {
        if (control->controlType() == AxiomModel::Control::ControlType::NUM_PORTAL ||
            control->controlType() == AxiomModel::Control::ControlType::MIDI_PORTAL) {
            auto portalControl = dynamic_cast<AxiomModel::PortalControl *>(control);
            assert(portalControl);
            return portalControl;
        }
    }
    return nullptr;
}

AxiomModel::Control::ControlType getGroupType(const std::vector<AxiomModel::Control *> &controls) {
    assert(!controls.empty());
    for (const auto &control : controls) {
        if (control->controlType() == AxiomModel::Control::ControlType::NUM_EXTRACT ||
            control->controlType() == AxiomModel::Control::ControlType::MIDI_EXTRACT) {
            return control->controlType();
        }
    }
    return controls[0]->controlType();
}

struct PortalTemp {
    AxiomModel::PortalControl *control;
    VarType vartype;
};

void SurfaceMirBuilder::build(MaximCompiler::Transaction *transaction, AxiomModel::NodeSurface *surface) {
    if (!surface->root()->runtime()) return;

    auto mir = transaction->buildSurface(surface->getRuntimeId(), surface->name());

    // build control groups
    std::unordered_map<ValueGroup *, std::unique_ptr<ValueGroup>> groups;
    QHash<QUuid, ValueGroup *> controlGroups;

    for (const auto &node : surface->nodes().sequence()) {
        // skip if the node is a custom node and couldn't be compiled to avoid making empty groups later on
        if (auto customNode = dynamic_cast<AxiomModel::CustomNode *>(node);
            customNode && !customNode->hasValidBlock()) {
            continue;
        }

        auto controlsContainer = *node->controls().value();
        for (const auto &control : controlsContainer->controls().sequence()) {
            if (control->connectedControls().sequence().empty()) {
                // assign the control a single group
                auto newGroup = std::make_unique<ValueGroup>();
                newGroup->controls.push_back(control->uuid());
                controlGroups.insert(control->uuid(), newGroup.get());
                groups.emplace(newGroup.get(), std::move(newGroup));
            } else {
                auto myGroupIndex = controlGroups.find(control->uuid());
                ValueGroup *myGroup;
                if (myGroupIndex == controlGroups.end()) {
                    auto newGroup = std::make_unique<ValueGroup>();
                    newGroup->controls.push_back(control->uuid());
                    myGroup = newGroup.get();
                    controlGroups.insert(control->uuid(), newGroup.get());
                    groups.emplace(newGroup.get(), std::move(newGroup));
                } else {
                    myGroup = myGroupIndex.value();
                }

                for (const auto &connectedControl : control->connectedControls().sequence()) {
                    // skip the control if it's on a CustomNode that can't be compiled
                    auto connectedGroupIndex = controlGroups.find(connectedControl);
                    if (connectedGroupIndex == controlGroups.end()) {
                        // add the control to our group
                        myGroup->controls.push_back(connectedControl);
                        controlGroups.insert(connectedControl, myGroup);
                    } else if (connectedGroupIndex.value() != myGroup) {
                        auto connectedGroup = connectedGroupIndex.value();
                        // merge the group into ours: first, update all entries in controlGroups
                        for (const auto &groupControl : connectedGroup->controls) {
                            controlGroups.insert(groupControl, myGroup);
                        }

                        // next merge the controls list in the group
                        connectedGroup->mergeInto(myGroup);

                        // now remove it from the groups array
                        groups.erase(connectedGroup);
                    };
                }
            }
        }
    }

    // merge control groups for controls that are merged internally
    for (const auto &node : surface->nodes().sequence()) {
        auto groupNode = dynamic_cast<AxiomModel::GroupNode *>(node);
        if (!groupNode) continue;

        auto groupSurface = *groupNode->nodes().value();
        auto &portalControlGroups = groupSurface->compileMeta()->portals;
        for (const auto &group : portalControlGroups) {
            assert(!group.externalControls.empty());

            auto targetControlGroupIndex = controlGroups.find(group.externalControls[0]);
            assert(targetControlGroupIndex != controlGroups.end());
            auto targetControlGroup = targetControlGroupIndex.value();

            for (size_t i = 1; i < group.externalControls.size(); i++) {
                auto &controlUuid = group.externalControls[i];
                auto controlGroupIndex = controlGroups.find(controlUuid);
                assert(controlGroupIndex != controlGroups.end());
                auto controlGroup = controlGroupIndex.value();

                if (controlGroup == targetControlGroup) continue;

                // merge the group into the target
                for (const auto &groupControl : controlGroup->controls) {
                    controlGroups.insert(groupControl, targetControlGroup);
                }

                controlGroup->mergeInto(targetControlGroup);
                groups.erase(controlGroup);
            }
        }
    }

    auto rootSurface = dynamic_cast<AxiomModel::RootSurface *>(surface);
    std::vector<PortalTemp> rootPortals;

    // build a map of value groups to indices while building the MIR
    std::vector<ValueGroup *> valueGroups;
    std::unordered_map<ValueGroup *, size_t> valueGroupIndices;
    std::vector<size_t> sockets;
    size_t index = 0;
    for (auto &pair : groups) {
        auto currentIndex = index++;
        valueGroupIndices.emplace(pair.first, currentIndex);
        valueGroups.push_back(pair.first);
        auto controlPointers = AxiomCommon::collect(AxiomCommon::filter(
            AxiomModel::findMap<std::vector<QUuid>>(pair.first->controls, surface->root()->controls().sequence()),
            [](AxiomModel::Control *control) {
                // if the control is on a CustomNode that can't be compiled, we need to skip it
                auto customNode = dynamic_cast<AxiomModel::CustomNode *>(control->surface()->node());
                return !customNode || customNode->hasValidBlock();
            }));

        auto groupType = getGroupType(controlPointers);
        auto vartype = VarType::ofControl(fromModelType(groupType));

        if (rootSurface) {
            if (auto portal = findPortal(controlPointers)) {
                auto socketIndex = sockets.size();
                rootPortals.push_back({portal, vartype.clone()});
                sockets.push_back(currentIndex);
                mir.addValueGroup(std::move(vartype), ValueGroupSource::socket(socketIndex));
                continue;
            }
        }

        if (areAnyExposed(controlPointers)) {
            auto socketIndex = sockets.size();
            sockets.push_back(currentIndex);
            mir.addValueGroup(std::move(vartype), ValueGroupSource::socket(socketIndex));
            continue;
        } else if (groupType == AxiomModel::Control::ControlType::NUM_SCALAR) {
            // determine if the group is written to
            auto isWrittenTo = false;
            for (const auto &control : controlPointers) {
                assert(control->compileMeta());
                if (control->compileMeta()->writtenTo) {
                    isWrittenTo = true;
                    break;
                }
            }

            if (!isWrittenTo) {
                // save a constant value
                auto numControl = dynamic_cast<AxiomModel::NumControl *>(controlPointers[0]);
                assert(numControl);

                auto numVal = numControl->value();
                if ((numVal.left != 0 || numVal.right != 0 || (int) numVal.form != 0) && !std::isnan(numVal.left) &&
                    !std::isnan(numVal.right)) {
                    auto constVal = ConstantValue::num(numVal);
                    mir.addValueGroup(std::move(vartype), ValueGroupSource::default_val(std::move(constVal)));
                    continue;
                }
            }
        }

        mir.addValueGroup(std::move(vartype), ValueGroupSource::none());
    }

    // build nodes
    size_t nodeIndex = 0;
    for (const auto &node : surface->nodes().sequence()) {
        if (auto customNode = dynamic_cast<AxiomModel::CustomNode *>(node)) {
            // ignore the node if it's not compiled yet
            if (!customNode->hasValidBlock()) {
                continue;
            }

            customNode->setCompileMeta(AxiomModel::NodeCompileMeta(nodeIndex));
            auto mirNode = mir.addCustomNode(customNode->getRuntimeId());

            // we need a sorted list of controls
            auto sortedControls = AxiomCommon::collect((*customNode->controls().value())->controls().sequence());
            std::sort(sortedControls.begin(), sortedControls.end(), [](AxiomModel::Control *a, AxiomModel::Control *b) {
                return a->compileMeta()->index < b->compileMeta()->index;
            });

            for (const auto &control : sortedControls) {
                auto controlGroup = controlGroups.find(control->uuid());
                assert(controlGroup != controlGroups.end());
                auto groupIndex = valueGroupIndices.find(*controlGroup);
                assert(groupIndex != valueGroupIndices.end());

                mirNode.addValueSocket(groupIndex->second, control->compileMeta()->writtenTo,
                                       control->compileMeta()->readFrom,
                                       control->controlType() == AxiomModel::Control::ControlType::NUM_EXTRACT ||
                                           control->controlType() == AxiomModel::Control::ControlType::MIDI_EXTRACT);
            }

            nodeIndex++;
        } else if (auto groupNode = dynamic_cast<AxiomModel::GroupNode *>(node)) {
            groupNode->setCompileMeta(AxiomModel::NodeCompileMeta(nodeIndex));
            auto groupSurface = *groupNode->nodes().value();
            auto mirNode = mir.addGroupNode(groupSurface->getRuntimeId());
            auto &portalControlGroups = groupSurface->compileMeta()->portals;

            for (const auto &group : portalControlGroups) {
                auto controlGroup = controlGroups.find(group.externalControls[0]);
                assert(controlGroup != controlGroups.end());
                auto groupIndex = valueGroupIndices.find(*controlGroup);
                assert(groupIndex != valueGroupIndices.end());

                mirNode.addValueSocket(groupIndex->second, group.valueWritten, group.valueRead, group.isExtractor);
            }

            nodeIndex++;
        }
    }

    // build root metadata and the updated transaction root
    if (rootSurface) {
        auto mirRoot = transaction->buildRoot();
        std::vector<AxiomModel::RootSurfacePortal> portals;

        for (auto &portal : rootPortals) {
            mirRoot.addSocket(std::move(portal.vartype));
            portals.emplace_back(portal.control->portalId(), portal.control->portalType(), portal.control->wireType(),
                                 portal.control->surface()->node()->name());
        }

        rootSurface->setCompileMeta(AxiomModel::RootSurfaceCompileMeta(std::move(portals)));

        return;
    }

    // build group metadata
    auto groupSurface = dynamic_cast<AxiomModel::GroupSurface *>(surface);
    if (groupSurface) {
        std::vector<AxiomModel::GroupSurfacePortal> portals;
        for (const auto &socketGroup : sockets) {
            std::vector<QUuid> externalControls;
            auto valueWritten = false;
            auto valueRead = false;
            auto isExtractor = false;

            auto valueGroup = valueGroups[socketGroup];
            auto controlPointers =
                AxiomCommon::collect(AxiomModel::findMap(valueGroup->controls, surface->root()->controls().sequence()));
            for (const auto &control : controlPointers) {
                if (!control->exposerUuid().isNull()) {
                    externalControls.push_back(control->exposerUuid());
                }

                if (control->compileMeta()->writtenTo) valueWritten = true;
                if (control->compileMeta()->readFrom) valueRead = true;
                if (control->controlType() == AxiomModel::Control::ControlType::NUM_EXTRACT ||
                    control->controlType() == AxiomModel::Control::ControlType::MIDI_EXTRACT) {
                    isExtractor = true;
                }
            }

            portals.emplace_back(std::move(externalControls), valueWritten, valueRead, isExtractor);
        }

        groupSurface->setCompileMeta(AxiomModel::GroupSurfaceCompileMeta(std::move(portals)));
    }
}
