#include "ModulePropertiesWindow.h"

#include <QtCore/QRegularExpression>
#include <QtGui/QIcon>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

#include "../SpaceCompleter.h"
#include "editor/model/Library.h"
#include "editor/util.h"

using namespace AxiomGui;

ModulePropertiesWindow::ModulePropertiesWindow(AxiomModel::Library *library)
    : QDialog(nullptr, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint) {
    setWindowTitle(tr("Add Module"));
    setStyleSheet(AxiomUtil::loadStylesheet(":/styles/SaveModuleWindow.qss"));
    setWindowIcon(QIcon(":/application.ico"));

    setFixedSize(400, 400);

    auto mainLayout = new QGridLayout();

    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setMargin(10);

    // todo: put a module preview here?

    auto nameLabel = new QLabel(tr("Name:"), this);
    nameLabel->setObjectName("save-label");
    mainLayout->addWidget(nameLabel, 0, 0);

    nameInput = new QLineEdit("New Module", this);
    mainLayout->addWidget(nameInput, 1, 0);

    auto tagsLabel = new QLabel(tr("Tags: (space-separated)"), this);
    tagsLabel->setObjectName("save-label");
    mainLayout->addWidget(tagsLabel, 2, 0);

    // create a completer for current tags
    tagsInput = new QLineEdit(this);
    auto tagList = library->tags();
    auto completer = new SpaceCompleter(tagList, tagsInput, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    mainLayout->addWidget(tagsInput, 3, 0);

    mainLayout->setRowStretch(4, 1);

    auto buttonBox = new QDialogButtonBox();
    auto okButton = buttonBox->addButton(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    auto cancelButton = buttonBox->addButton(QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox, 5, 0);

    setLayout(mainLayout);

    connect(okButton, &QPushButton::clicked, this, &ModulePropertiesWindow::accept);
    connect(cancelButton, &QPushButton::clicked, this, &ModulePropertiesWindow::reject);
}

QString ModulePropertiesWindow::enteredName() const {
    return nameInput->text();
}

QStringList ModulePropertiesWindow::enteredTags() const {
    auto trimmedInput = tagsInput->text().trimmed();
    if (trimmedInput.isEmpty())
        return {};
    else
        return trimmedInput.split(QRegularExpression("(\\s?,\\s?)|(\\s+)"));
}

void ModulePropertiesWindow::setEnteredName(const QString &name) {
    nameInput->setText(name);
}

void ModulePropertiesWindow::setEnteredTags(const QStringList &list) {
    tagsInput->setText(list.join(' '));
}
