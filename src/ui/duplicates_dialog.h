#pragma once
#include <QDialog>
class QTableWidget;
class QLabel;

class DuplicatesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DuplicatesDialog(QWidget* parent = nullptr);

private slots:
    void onRefresh();
    void onAutoSelect();
    void onDeleteSelected();

private:
    QTableWidget* m_table  = nullptr;
    QLabel*       m_status = nullptr;
};
