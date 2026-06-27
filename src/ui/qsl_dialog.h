#pragma once
#include <QDialog>
class QTableWidget;
class QComboBox;

class QslDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QslDialog(QWidget* parent = nullptr);
private slots:
    void onMarkSent();
    void onMarkRcvd();
    void onFilter();
    void onPrint();
private:
    void build();
    void populate();
    QTableWidget* m_table  = nullptr;
    QComboBox*    m_filter = nullptr;
};
