#pragma once

#include <QtWidgets/QGraphicsScene>

#include "common/Hookable.h"

namespace AxiomModel {
    class NodeSurface;

    class Node;

    class ConnectionWire;
}

namespace AxiomGui {

    class ModulePreviewCanvas : public QGraphicsScene, public AxiomCommon::Hookable {
    Q_OBJECT

    public:

        explicit ModulePreviewCanvas(AxiomModel::NodeSurface *surface);

    signals:

        void contentChanged();

    private slots:

        void addNode(AxiomModel::Node *node);

        void addWire(AxiomModel::ConnectionWire *wire);

    };

}
