#include "ModuleBrowserPanel.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTabBar>

#include "ModulePreviewList.h"
#include "editor/model/Library.h"
#include "editor/util.h"

using namespace AxiomGui;

ModuleBrowserPanel::ModuleBrowserPanel(MainWindow *window, AxiomModel::Library *library, QWidget *parent)
    : ads::CDockWidget("Modules", parent), library(library) {
    setStyleSheet(AxiomUtil::loadStylesheet(":/styles/ModuleBrowserPanel.qss"));

    auto mainLayout = new QGridLayout(this);
    auto mainWidget = new QWidget(this);
    mainWidget->setObjectName("mainWidget");

    mainLayout->setContentsMargins(0, 0, 0, 0);

    mainLayout->setColumnStretch(0, 10);
    mainLayout->setColumnStretch(1, 3);
    mainLayout->setColumnMinimumWidth(1, 200);
    mainLayout->setRowStretch(1, 1);

    filterTabs = new QTabBar(this);
    mainLayout->addWidget(filterTabs, 0, 0, Qt::AlignLeft);

    filterTabs->addTab(tr("All"));

    searchBox = new QLineEdit(this);
    searchBox->setObjectName("searchBox");
    searchBox->setPlaceholderText("Search modules...");
    searchBox->setText(library->activeSearch());
    mainLayout->addWidget(searchBox, 0, 1);

    connect(searchBox, &QLineEdit::textChanged, this, &ModuleBrowserPanel::changeSearch);

    auto previewList = new ModulePreviewList(window, library, this);
    mainLayout->addWidget(previewList, 1, 0, 1, 2);

    mainWidget->setLayout(mainLayout);
    setWidget(mainWidget);

    auto tags = library->tags();
    for (const auto &tag : tags) {
        addTag(tag);
    }

    library->tagAdded.connect(this, &ModuleBrowserPanel::addTag);
    library->tagRemoved.connect(this, &ModuleBrowserPanel::removeTag);
    connect(filterTabs, &QTabBar::currentChanged, this, &ModuleBrowserPanel::changeTag);
}

void ModuleBrowserPanel::addTag(const QString &tag) {
    filterTabs->addTab(tag);
    tabValues.push_back(tag);
}

void ModuleBrowserPanel::removeTag(const QString &tag) {
    auto index = std::find(tabValues.begin(), tabValues.end(), tag);
    assert(index != tabValues.end());
    tabValues.erase(index);
    auto tabIndex = index - tabValues.begin() + 1;
    if (filterTabs->currentIndex() == tabIndex) {
        filterTabs->setCurrentIndex(0);
    }
    filterTabs->removeTab(tabIndex);
}

void ModuleBrowserPanel::changeTag(int tag) {
    searchBox->setText("");
    searchBox->clearFocus();
    if (tag == 0)
        library->setActiveTag("");
    else {
        assert(tag <= (int) tabValues.size());
        library->setActiveTag(tabValues[tag - 1]);
    }
}

void ModuleBrowserPanel::changeSearch(const QString &newSearch) {
    library->setActiveSearch(newSearch);
}
