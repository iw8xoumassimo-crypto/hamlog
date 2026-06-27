#include "contest_panel.h"
#include "../database.h"
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QGroupBox>

ContestPanel::ContestPanel(QWidget* parent) : QWidget(parent)
{
    build();
}

void ContestPanel::build()
{
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(4,4,4,4);

    auto* grp = new QGroupBox(tr("Contest"), this);
    auto* fl  = new QFormLayout(grp);

    m_contestId = new QLineEdit(grp);
    m_exchange  = new QLineEdit(grp);
    m_serial    = new QSpinBox(grp);
    m_serial->setRange(1, 9999);
    m_serial->setValue(1);

    fl->addRow(tr("ID Contest:"),        m_contestId);
    fl->addRow(tr("Scambio:"),           m_exchange);
    fl->addRow(tr("Prossimo numero:"),   m_serial);

    vl->addWidget(grp);

    auto* statsGrp = new QGroupBox(tr("Statistiche"), this);
    auto* sl = new QVBoxLayout(statsGrp);
    m_lblCount = new QLabel(tr("QSO: 0"), statsGrp);
    m_lblScore = new QLabel(tr("Punteggio: 0"), statsGrp);
    sl->addWidget(m_lblCount);
    sl->addWidget(m_lblScore);
    vl->addWidget(statsGrp);

    auto* btnReset = new QPushButton(tr("Azzera numerazione"), this);
    connect(btnReset, &QPushButton::clicked, this, &ContestPanel::reset);
    vl->addWidget(btnReset);
    vl->addStretch();

    connect(m_contestId, &QLineEdit::textChanged, this, &ContestPanel::contestChanged);
    connect(&Database::instance(), &Database::qsoSaved, this, [this]{
        int cnt = Database::instance().countQsos();
        m_lblCount->setText(tr("QSO: %1").arg(cnt));
    });
}

QString ContestPanel::contestId()  const { return m_contestId->text(); }
QString ContestPanel::exchange()   const { return m_exchange->text(); }
int     ContestPanel::nextSerial() const { return m_serial->value(); }

void ContestPanel::incrementSerial()
{
    m_serial->setValue(m_serial->value() + 1);
}

void ContestPanel::reset()
{
    m_serial->setValue(1);
}
