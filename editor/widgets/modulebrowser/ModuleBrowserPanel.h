#pragma once

#include <map>

#include "../dock/DockWidget.h"
#include "common/TrackedObject.h"

class QTabBar;
class QLineEdit;

namespace AxiomModel {
    class Library;
}

namespace AxiomGui {

    class MainWindow;

    class ModuleBrowserPanel : public ads::CDockWidget, public AxiomCommon::TrackedObject {
        Q_OBJECT

    public:
        explicit ModuleBrowserPanel(MainWindow *window, AxiomModel::Library *library, QWidget *parent = nullptr);

    private slots:

        void changeTag(int tag);

        void changeSearch(const QString &newSearch);

    private:
        AxiomModel::Library *library;
        QLineEdit *searchBox;
        QTabBar *filterTabs;
        std::vector<QString> tabValues;

        void addTag(const QString &tag);

        void removeTag(const QString &tag);
    };
}
