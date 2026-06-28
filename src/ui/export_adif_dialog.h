#pragma once
#include <QDialog>
#include "types.h"

class QLineEdit;
class QComboBox;
class QCheckBox;
class QDateEdit;

class ExportAdifDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportAdifDialog(QWidget* parent = nullptr);
    QsoFilter filter() const;

private:
    QDateEdit*  m_dateFrom   = nullptr;
    QDateEdit*  m_dateTo     = nullptr;
    QComboBox*  m_band       = nullptr;
    QComboBox*  m_mode       = nullptr;
    QLineEdit*  m_country    = nullptr;
    QCheckBox*  m_lotwNot    = nullptr;
    QCheckBox*  m_eqslNot    = nullptr;
    QCheckBox*  m_clubNot    = nullptr;
    QCheckBox*  m_useDates   = nullptr;
};
