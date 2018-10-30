#include "ProjectSerializer.h"

#include "../Library.h"
#include "../ModelRoot.h"
#include "../Project.h"
#include "LibrarySerializer.h"
#include "ModelObjectSerializer.h"

using namespace AxiomModel;

void ProjectSerializer::writeHeader(QDataStream &stream, uint64_t magic) {
    stream << static_cast<quint64>(magic);
    stream << schemaVersion;
}

bool ProjectSerializer::readHeader(QDataStream &stream, uint64_t expectedMagic, uint32_t *versionOut) {
    quint64 magic;
    stream >> magic;
    if (magic != expectedMagic) return false;

    uint32_t version;
    stream >> version;
    if (versionOut) *versionOut = version;
    return version >= minSchemaVersion && version <= schemaVersion;
}

void ProjectSerializer::serialize(AxiomModel::Project *project, QDataStream &stream,
                                  std::function<void(QDataStream &)> writeLinkedFile) {
    writeHeader(stream, projectSchemaMagic);
    writeLinkedFile(stream);
    ModelObjectSerializer::serializeRoot(&project->mainRoot(), true, stream);
}

std::unique_ptr<Project> ProjectSerializer::deserialize(QDataStream &stream, uint32_t *versionOut,
                                                        std::function<void(Library *)> importLibrary,
                                                        std::function<QString(QDataStream &, uint32_t)> getLinkedFile) {
    uint32_t version;
    if (!readHeader(stream, projectSchemaMagic, &version)) {
        if (versionOut) *versionOut = version;
        return nullptr;
    }
    if (versionOut) *versionOut = version;

    auto linkedFile = getLinkedFile(stream, version);
    auto modelRoot = ModelObjectSerializer::deserializeRoot(stream, true, false, version);
    auto project = std::make_unique<Project>(linkedFile, std::move(modelRoot));

    // Before schema version 5, the module library was included in the project file. To ensure modules aren't lost,
    // merge the library in.
    if (version < 5) {
        auto library = LibrarySerializer::deserialize(stream, version);
        importLibrary(library.get());
    }

    return project;
}
