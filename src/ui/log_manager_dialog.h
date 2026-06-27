#pragma once
#include <QDialog>
class QListWidget;

class LogManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LogManagerDialog(QWidget* parent = nullptr);
signals:
    void logChanged(int id);
private slots:
    void onAdd();
    void onDelete();
    void onSelect();
    void onRename();
private:
    void build();
    void refresh();
    QListWidget* m_list = nullptr;
};
