#pragma once

#include <QtWidgets/QWidget>

class QComboBox;
class QRadioButton;
class QLineEdit;

namespace AxiomGui {

    class CodeConfigWidget : public QWidget {
        Q_OBJECT

    public:
        CodeConfigWidget();

    signals:

        void instrumentPrefixChanged(const QString &oldSafePrefix, const QString &safePrefix);

    private slots:
        void processPrefixChange(const QString &newPrefix);
        void ensureInstrumentPrefixSafe();

    private:
        QString oldSafePrefix = "axiom_";

        QComboBox *optimizationSelect;

        QRadioButton *instrumentAndLibraryContent;
        QRadioButton *instrumentContent;
        QRadioButton *libraryContent;

        QLineEdit *instrumentPrefixEdit;
    };
}
