#include "export_adif_dialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QDateEdit>
#include <QDate>
#include <QLabel>

ExportAdifDialog::ExportAdifDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Esporta ADIF con filtri"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumWidth(380);

    auto* root = new QVBoxLayout(this);

    // Date range
    auto* dateGrp = new QGroupBox(tr("Periodo"), this);
    auto* dateForm = new QFormLayout(dateGrp);
    m_useDates = new QCheckBox(tr("Filtra per date"), this);
    m_dateFrom = new QDateEdit(QDate::currentDate().addYears(-1), this);
    m_dateTo   = new QDateEdit(QDate::currentDate(), this);
    m_dateFrom->setCalendarPopup(true);
    m_dateTo->setCalendarPopup(true);
    m_dateFrom->setDisplayFormat("yyyy-MM-dd");
    m_dateTo->setDisplayFormat("yyyy-MM-dd");
    m_dateFrom->setEnabled(false);
    m_dateTo->setEnabled(false);
    connect(m_useDates, &QCheckBox::toggled, m_dateFrom, &QDateEdit::setEnabled);
    connect(m_useDates, &QCheckBox::toggled, m_dateTo,   &QDateEdit::setEnabled);
    dateForm->addRow(m_useDates);
    dateForm->addRow(tr("Dal:"), m_dateFrom);
    dateForm->addRow(tr("Al:"),  m_dateTo);
    root->addWidget(dateGrp);

    // Band / Mode / Country
    auto* fltGrp  = new QGroupBox(tr("Filtri"), this);
    auto* fltForm = new QFormLayout(fltGrp);

    m_band = new QComboBox(this);
    m_band->addItems({"(tutte)", "160M","80M","60M","40M","30M","20M","17M",
                      "15M","12M","10M","6M","4M","2M","70CM"});
    m_mode = new QComboBox(this);
    m_mode->addItems({"(tutti)", "FT8","FT4","SSB","CW","AM","FM","RTTY",
                      "JS8","PSK31","WSPR","SSTV"});
    m_country = new QLineEdit(this);
    m_country->setPlaceholderText(tr("es. Italy"));

    fltForm->addRow(tr("Banda:"),  m_band);
    fltForm->addRow(tr("Modo:"),   m_mode);
    fltForm->addRow(tr("Paese:"),  m_country);
    root->addWidget(fltGrp);

    // QSL state
    auto* qslGrp  = new QGroupBox(tr("Stato QSL"), this);
    auto* qslLay  = new QVBoxLayout(qslGrp);
    m_lotwNot = new QCheckBox(tr("Solo QSO non inviati a LoTW"), this);
    m_eqslNot = new QCheckBox(tr("Solo QSO non inviati a eQSL"), this);
    m_clubNot = new QCheckBox(tr("Solo QSO non inviati a ClubLog"), this);
    qslLay->addWidget(m_lotwNot);
    qslLay->addWidget(m_eqslNot);
    qslLay->addWidget(m_clubNot);
    root->addWidget(qslGrp);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

QsoFilter ExportAdifDialog::filter() const
{
    QsoFilter f;
    if (m_useDates->isChecked()) {
        f.dateFrom = m_dateFrom->date().toString("yyyy-MM-dd");
        f.dateTo   = m_dateTo->date().toString("yyyy-MM-dd");
    }
    if (m_band->currentIndex() > 0) f.band = m_band->currentText().toLower();
    if (m_mode->currentIndex() > 0) f.mode = m_mode->currentText();
    f.country        = m_country->text().trimmed();
    f.lotwNotSent    = m_lotwNot->isChecked();
    f.eqslNotSent    = m_eqslNot->isChecked();
    f.clublogNotSent = m_clubNot->isChecked();
    return f;
}
