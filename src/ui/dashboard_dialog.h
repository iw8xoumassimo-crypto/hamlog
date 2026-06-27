#pragma once
#include <QDialog>
class QLabel;

class DashboardDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DashboardDialog(QWidget* parent = nullptr);
private:
    void build();
    void populate();
};
