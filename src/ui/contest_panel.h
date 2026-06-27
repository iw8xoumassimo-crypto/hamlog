#pragma once
#include <QWidget>
class QLineEdit;
class QLabel;
class QComboBox;
class QSpinBox;

class ContestPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ContestPanel(QWidget* parent = nullptr);

    QString contestId() const;
    QString exchange()  const;
    int     nextSerial() const;
    void    incrementSerial();
    void    reset();

signals:
    void contestChanged(const QString& id);

private:
    void build();
    QLineEdit* m_contestId  = nullptr;
    QLineEdit* m_exchange   = nullptr;
    QSpinBox*  m_serial     = nullptr;
    QLabel*    m_lblScore   = nullptr;
    QLabel*    m_lblCount   = nullptr;
};
