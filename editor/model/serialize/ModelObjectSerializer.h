#pragma once

#include "../ModelObject.h"
#include "common/SequenceOperators.h"
#include <QtCore/QDataStream>
#include <memory>

namespace AxiomModel {

    class ModelRoot;
    class ReferenceMapper;

    namespace ModelObjectSerializer {
        std::vector<ModelObject *> deserializeChunk(QDataStream &stream, uint32_t version, ModelRoot *root,
                                                    const QUuid &parent, ReferenceMapper *ref, bool isLibrary);

        void serializeRoot(ModelRoot *root, bool includeHistory, QDataStream &stream);

        std::unique_ptr<ModelRoot> deserializeRoot(QDataStream &stream, bool includeHistory, bool isLibrary,
                                                   uint32_t version);

        void serialize(ModelObject *obj, QDataStream &stream, const QUuid &parent);

        std::unique_ptr<ModelObject> deserialize(QDataStream &stream, uint32_t version, ModelRoot *root,
                                                 const QUuid &parent, ReferenceMapper *ref, bool isLibrary);

        void serializeInner(ModelObject *obj, QDataStream &stream);

        std::unique_ptr<ModelObject> deserializeInner(QDataStream &stream, uint32_t version, ModelRoot *root,
                                                      ModelObject::ModelType type, const QUuid &uuid,
                                                      const QUuid &parent, ReferenceMapper *ref, bool isLibrary);

        template<class T>
        void serializeChunk(QDataStream &stream, const QUuid &parent, T objects) {
            stream << (uint32_t) AxiomCommon::refSequence(&objects).size();
            for (const auto &obj : objects) {
                QByteArray objectBuffer;
                QDataStream objectStream(&objectBuffer, QIODevice::WriteOnly);
                serialize(obj, objectStream, parent);
                stream << objectBuffer;
            }
        }
    }
}
