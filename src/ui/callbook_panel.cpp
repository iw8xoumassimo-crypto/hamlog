#include "callbook_panel.h"
#include "../services.h"
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>
#include <QGroupBox>

CallbookPanel::CallbookPanel(QWidget* parent) : QWidget(parent)
{
    build();
    m_hamdb  = new HamDbService(this);
    m_hamqth = new HamQthService(this);

    connect(m_hamdb,  &HamDbService::lookupResult, this, [this](const QString& cs, const QHash<QString,QString>& d){
        onResult(cs, d, false);
    });
    connect(m_hamqth, &HamQthService::lookupResult, this, &CallbookPanel::onResult);
}

void CallbookPanel::build()
{
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(4,4,4,4);

    auto* hl = new QHBoxLayout;
    m_input  = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Callsign"));
    auto* btn = new QPushButton(tr("Go"), this);
    hl->addWidget(m_input, 1);
    hl->addWidget(btn);
    vl->addLayout(hl);

    m_call = new QLabel(this);
    QFont f = m_call->font();
    f.setPointSize(f.pointSize() + 3);
    f.setBold(true);
    m_call->setFont(f);
    m_call->setAlignment(Qt::AlignCenter);
    vl->addWidget(m_call);

    auto* grp = new QGroupBox(this);
    m_fl = new QFormLayout(grp);

    m_name    = new QLabel(grp);
    m_qth     = new QLabel(grp);
    m_country = new QLabel(grp);
    m_grid    = new QLabel(grp);
    m_email   = new QLabel(grp);
    m_web     = new QLabel(grp);

    for (QLabel* l : {m_name, m_qth, m_country, m_grid, m_email, m_web})
        l->setWordWrap(true);

    m_fl->addRow(tr("Nome:"),   m_name);
    m_fl->addRow(tr("QTH:"),   m_qth);
    m_fl->addRow(tr("Paese:"), m_country);
    m_fl->addRow(tr("Grid:"),  m_grid);
    m_fl->addRow(tr("Email:"), m_email);
    m_fl->addRow(tr("Web:"),   m_web);

    vl->addWidget(grp);
    vl->addStretch();

    connect(btn,     &QPushButton::clicked,    this, &CallbookPanel::onManualLookup);
    connect(m_input, &QLineEdit::returnPressed, this, &CallbookPanel::onManualLookup);
}

void CallbookPanel::lookup(const QString& callsign)
{
    if (callsign.size() < 3) return;

    QSettings cfg;
    QString user = cfg.value("hamqth/username").toString();
    if (!user.isEmpty()) {
        m_hamqth->setCredentials(user, cfg.value("hamqth/password").toString());
        m_hamqth->lookup(callsign);
    } else {
        m_hamdb->lookup(callsign);
    }
}

void CallbookPanel::onManualLookup()
{
    QString cs = m_input->text().trimmed().toUpper();
    if (!cs.isEmpty()) lookup(cs);
}

void CallbookPanel::onResult(const QString& callsign,
                              const QHash<QString,QString>& data,
                              bool /*fromCache*/)
{
    m_call->setText(callsign);
    if (data.contains("_error")) {
        m_name->setText(tr("Errore: %1").arg(data["_error"]));
        m_qth->clear(); m_country->clear(); m_grid->clear();
        m_email->clear(); m_web->clear();
    } else if (data.isEmpty()) {
        m_name->setText(tr("(nessun risultato)"));
        m_qth->clear(); m_country->clear(); m_grid->clear();
        m_email->clear(); m_web->clear();
    } else {
        showData(callsign, data);
    }
}

void CallbookPanel::showData(const QString& callsign, const QHash<QString,QString>& d)
{
    auto get = [&](const QString& k) { return d.value(k); };

    QString name = get("name");
    if (name.isEmpty()) name = get("fname");

    QString qth = get("addr1");
    if (!get("addr2").isEmpty()) qth += ", " + get("addr2");

    m_name->setText(name);
    m_qth->setText(qth);
    m_country->setText(get("country"));
    m_grid->setText(get("grid"));
    m_email->setText(get("email"));
    m_web->setText(get("web"));

    // Propagate to QSO entry form
    emit callbookData(callsign, name, qth, get("grid"), get("country"));
}

void CallbookPanel::showFromQso(const Qso& qso)
{
    m_call->setText(qso.callsign);
    m_name->setText(qso.name);
    m_qth->setText(qso.qth);
    m_country->setText(qso.country);
    m_grid->setText(qso.gridsquare);
    m_email->clear();
    m_web->clear();
}

void CallbookPanel::clearData()
{
    m_call->clear();
    m_name->clear();
    m_qth->clear();
    m_country->clear();
    m_grid->clear();
    m_email->clear();
    m_web->clear();
}
