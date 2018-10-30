#pragma once

#include <QtCore/QPoint>
#include <QtCore/QUuid>

namespace AxiomModel {

    class ReferenceMapper {
    public:
        virtual ~ReferenceMapper() = default;

        virtual QUuid mapUuid(const QUuid &input) = 0;

        virtual bool isValid(const QUuid &input) = 0;

        virtual QPoint mapPos(const QUuid &parent, const QPoint &input) = 0;
    };
}
