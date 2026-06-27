#pragma once
#include <QDialog>
class QTableWidget;
class QLabel;
class QPushButton;

class IntegrityDialog : public QDialog
{
    Q_OBJECT
public:
    explicit IntegrityDialog(QWidget* parent = nullptr);
private slots:
    void onFindDuplicates();
    void onRemoveDuplicates();
    void onEnrichDxcc();
    void onCheckMissingFields();
    void onSummary();
private:
    QTableWidget* m_table  = nullptr;
    QLabel*       m_status = nullptr;
    QLabel*       m_summary = nullptr;
};
