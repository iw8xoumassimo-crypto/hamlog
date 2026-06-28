#pragma once
#include <QDialog>
#include "types.h"

class QTableWidget;
class QComboBox;
class QSpinBox;
class QCheckBox;

class QslLabelDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QslLabelDialog(QWidget* parent = nullptr);

private slots:
    void onPrint();
    void onPreview();

private:
    void buildQsoList();
    void printLabels(bool preview);

    QTableWidget* m_table     = nullptr;
    QComboBox*    m_filter    = nullptr;
    QSpinBox*     m_cols      = nullptr;
    QSpinBox*     m_rows      = nullptr;
    QList<Qso>    m_qsos;
};
