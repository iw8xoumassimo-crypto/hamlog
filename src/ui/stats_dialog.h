#pragma once
#include <QDialog>

class StatsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StatsDialog(QWidget* parent = nullptr);
private:
    void build();
    void populate();
};
