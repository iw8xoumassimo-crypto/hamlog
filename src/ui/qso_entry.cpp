#include "qso_entry.h"
#include "../database.h"
#include "../dxcc.h"
#include "../locator.h"
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFrame>
#include <QDateTime>
#include <QMessageBox>
#include <QSettings>
#include <QKeyEvent>
#include <QShortcut>

static const QStringList BANDS = {
    "160m","80m","60m","40m","30m","20m","17m","15m",
    "12m","10m","6m","4m","2m","70cm","23cm"
};

static const QStringList MODES = {
    "SSB","USB","LSB","CW","AM","FM","FT8","FT4","FT2","JS8",
    "RTTY","PSK31","MSK144","WSPR","JT65","JT9","SSTV","OTHER"
};

static const QStringList QSL_STATES = {"N","Y","Q","R","I"};

QsoEntry::QsoEntry(QWidget* parent) : QWidget(parent)
{
    setWindowTitle(tr("Nuovo QSO"));
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setMinimumSize(520, 460);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    build();
    resize(580, 560);
}

void QsoEntry::build()
{
    auto* vl = new QVBoxLayout(this);

    // ---- Callsign header frame ----
    auto* headerFrame = new QFrame(this);
    headerFrame->setObjectName("qsoHeader");
    headerFrame->setStyleSheet(
        "QFrame#qsoHeader{"
        "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #1e3564,stop:1 #12233e);"
        "border-radius:8px;border:1px solid #2c5282;}");
    auto* hfl = new QVBoxLayout(headerFrame);
    hfl->setContentsMargins(10,8,10,6);
    hfl->setSpacing(4);

    // Row 1: callsign + date/time
    auto* top = new QHBoxLayout;
    top->setSpacing(6);

    m_call = new QLineEdit(this);
    m_call->setPlaceholderText(tr("NOMINATIVO"));
    QFont bigFont = m_call->font();
    bigFont.setPointSize(bigFont.pointSize() + 6);
    bigFont.setBold(true);
    m_call->setFont(bigFont);
    m_call->setMinimumHeight(40);
    m_call->setMinimumWidth(180);
    m_call->setStyleSheet(
        "QLineEdit{background:#0a1a2e;color:#40e0ff;border:2px solid #2c6fad;"
        "border-radius:6px;padding:4px 14px;letter-spacing:2px;}"
        "QLineEdit:focus{border-color:#3a9ad9;}");

    m_date   = new QDateEdit(QDate::currentDate(), this);
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat("yyyy-MM-dd");

    m_timeOn  = new QTimeEdit(QTime::currentTime(), this);
    m_timeOff = new QTimeEdit(QTime::currentTime(), this);
    m_timeOn->setDisplayFormat("HH:mm");
    m_timeOff->setDisplayFormat("HH:mm");

    auto* btnNow = new QPushButton(tr("UTC"), this);
    btnNow->setMaximumWidth(46);
    btnNow->setStyleSheet(
        "QPushButton{background:#2c5282;color:#c8ddf0;border:1px solid #3a80c4;"
        "border-radius:4px;padding:2px 8px;min-width:0;font-size:8pt;}");
    connect(btnNow, &QPushButton::clicked, this, &QsoEntry::onNow);

    auto darkLbl = [this](const QString& t) {
        auto* l = new QLabel(t, this);
        l->setStyleSheet("color:#90b8e0;background:transparent;border:none;");
        return l;
    };

    top->addWidget(m_call, 3);
    top->addSpacing(6);
    top->addWidget(darkLbl(tr("Data:")));
    top->addWidget(m_date, 2);
    top->addWidget(darkLbl(tr("ON:")));
    top->addWidget(m_timeOn, 1);
    top->addWidget(darkLbl(tr("OFF:")));
    top->addWidget(m_timeOff, 1);
    top->addWidget(btnNow);
    hfl->addLayout(top);

    // Row 2: DXCC badge strip + dupe alert (always visible)
    auto* infoRow = new QHBoxLayout;
    infoRow->setSpacing(6);

    m_lblCountry   = new QLabel(tr("—"), this);
    m_lblContinent = new QLabel(tr("—"), this);
    m_lblCqZone    = new QLabel(tr("CQ —"), this);

    auto badge = [](QLabel* l, const QString& hex) {
        l->setStyleSheet(QString(
            "QLabel{background:%1;color:white;border-radius:3px;"
            "padding:1px 8px;font-size:8pt;font-weight:bold;border:none;}").arg(hex));
    };
    badge(m_lblCountry,   "#1565c0");
    badge(m_lblContinent, "#00695c");
    badge(m_lblCqZone,    "#6a1b9a");

    m_dupeInfo = new QLabel(this);
    m_dupeInfo->setAlignment(Qt::AlignCenter);
    m_dupeInfo->setTextFormat(Qt::PlainText);
    m_dupeInfo->setStyleSheet(
        "QLabel{background:#c62828;color:white;font-weight:bold;"
        "border-radius:3px;padding:2px 12px;font-size:9pt;border:none;}");
    m_dupeInfo->hide();

    infoRow->addWidget(m_lblCountry);
    infoRow->addWidget(m_lblContinent);
    infoRow->addWidget(m_lblCqZone);
    infoRow->addStretch();
    infoRow->addWidget(m_dupeInfo);
    hfl->addLayout(infoRow);

    vl->addWidget(headerFrame);

    // ---- Band / Freq / Mode row ----
    auto* mid = new QHBoxLayout;

    m_band = new QComboBox(this);
    m_band->addItems(BANDS);

    m_freq = new QDoubleSpinBox(this);
    m_freq->setRange(0.001, 50000.0);
    m_freq->setDecimals(3);
    m_freq->setSuffix(" MHz");
    m_freq->setMinimumWidth(110);

    m_mode    = new QComboBox(this);
    m_mode->addItems(MODES);

    m_submode = new QComboBox(this);
    m_submode->setEditable(true);
    m_submode->setMaximumWidth(80);

    m_rstSent = new QLineEdit("59", this);
    m_rstSent->setMaximumWidth(45);
    m_rstRcvd = new QLineEdit("59", this);
    m_rstRcvd->setMaximumWidth(45);

    mid->addWidget(new QLabel(tr("Banda:")));
    mid->addWidget(m_band);
    mid->addWidget(new QLabel(tr("Freq:")));
    mid->addWidget(m_freq);
    mid->addWidget(new QLabel(tr("Modo:")));
    mid->addWidget(m_mode);
    mid->addWidget(m_submode);
    mid->addWidget(new QLabel(tr("RST↑:")));
    mid->addWidget(m_rstSent);
    mid->addWidget(new QLabel(tr("RST↓:")));
    mid->addWidget(m_rstRcvd);
    mid->addStretch();
    vl->addLayout(mid);

    // ---- Tabs ----
    auto* tabs = new QTabWidget(this);

    // -- Basic tab --
    auto* basicW = new QWidget;
    auto* gl = new QGridLayout(basicW);
    int row = 0;

    m_name  = new QLineEdit(basicW);
    m_qth   = new QLineEdit(basicW);
    m_grid  = new QLineEdit(basicW); m_grid->setMaximumWidth(80);
    m_power = new QLineEdit(basicW); m_power->setMaximumWidth(60);

    gl->addWidget(new QLabel(tr("Nome:")),        row,0);
    gl->addWidget(m_name,                          row,1);
    gl->addWidget(new QLabel(tr("QTH:")),          row,2);
    gl->addWidget(m_qth,                           row,3, 1, 3); ++row;

    gl->addWidget(new QLabel(tr("Loc.:")),         row,0);
    gl->addWidget(m_grid,                          row,1);
    gl->addWidget(new QLabel(tr("Potenza (W):")),  row,2);
    gl->addWidget(m_power,                         row,3); ++row;

    gl->setRowStretch(row, 1);   // spazio vuoto che assorbe l'espansione verticale

    tabs->addTab(basicW, tr("Base"));

    // -- Extra tab --
    auto* extraW = new QWidget;
    auto* fl = new QFormLayout(extraW);
    m_iota    = new QLineEdit(extraW);
    m_sota    = new QLineEdit(extraW);
    m_pota    = new QLineEdit(extraW);
    m_contest = new QLineEdit(extraW);
    m_srx     = new QLineEdit(extraW);
    m_stx     = new QLineEdit(extraW);
    m_manager = new QLineEdit(extraW);
    m_comment = new QPlainTextEdit(extraW);
    m_comment->setMinimumHeight(50);
    m_notes   = new QPlainTextEdit(extraW);
    m_notes->setMinimumHeight(50);

    fl->addRow(tr("IOTA:"),    m_iota);
    fl->addRow(tr("SOTA:"),    m_sota);
    fl->addRow(tr("POTA:"),    m_pota);
    fl->addRow(tr("Contest:"), m_contest);
    fl->addRow(tr("Nr. Ricevuto:"), m_srx);
    fl->addRow(tr("Nr. Inviato:"), m_stx);
    fl->addRow(tr("Manager:"), m_manager);
    fl->addRow(tr("Commento:"), m_comment);
    fl->addRow(tr("Note:"),   m_notes);
    tabs->addTab(extraW, tr("Extra"));

    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vl->addWidget(tabs, 1);   // stretch=1: il tab cresce quando la finestra si allarga

    // ---- QSL Panel (always visible) ----
    auto* qslGrp = new QGroupBox(tr("QSL"), this);
    auto* qslHl  = new QHBoxLayout(qslGrp);
    qslHl->setSpacing(12);
    qslHl->setContentsMargins(8, 6, 8, 6);

    // Helper: received-status combo with color feedback
    auto makeRcvdCombo = [](QWidget* p) -> QComboBox* {
        auto* cb = new QComboBox(p);
        cb->addItems(QSL_STATES);
        cb->setMaximumWidth(62);
        auto applyColor = [cb](const QString& v) {
            if (v == "Y" || v == "R")
                cb->setStyleSheet(
                    "QComboBox{background:#c8f8c8;color:#155724;font-weight:bold;"
                    "border:1px solid #28a745;border-radius:3px;}");
            else if (v == "N")
                cb->setStyleSheet("");
            else
                cb->setStyleSheet(
                    "QComboBox{background:#fff3cd;color:#856404;font-weight:bold;"
                    "border:1px solid #ffc107;border-radius:3px;}");
        };
        QObject::connect(cb, &QComboBox::currentTextChanged, cb, applyColor);
        return cb;
    };

    // Helper: one service block — checkbox + "Conf.:" combo
    auto makeServiceBlock = [&](const QString& label,
                                QCheckBox*& sentOut,
                                QComboBox*& rcvdOut) {
        auto* box = new QGroupBox(qslGrp);
        box->setFlat(true);
        auto* vb = new QVBoxLayout(box);
        vb->setSpacing(4);
        vb->setContentsMargins(4, 2, 4, 2);

        sentOut = new QCheckBox(label, box);
        sentOut->setChecked(true);
        QFont f = sentOut->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 1);
        sentOut->setFont(f);

        auto* hl2 = new QHBoxLayout;
        auto* lbl = new QLabel(tr("Conferma:"), box);
        rcvdOut   = makeRcvdCombo(box);
        hl2->addWidget(lbl);
        hl2->addWidget(rcvdOut);
        hl2->addStretch();

        vb->addWidget(sentOut);
        vb->addLayout(hl2);
        qslHl->addWidget(box, 1);
    };

    makeServiceBlock(tr("Invia QSL Card"), m_qslSent,  m_qslRcvd);
    makeServiceBlock(tr("Invia LoTW"),     m_lotwSent, m_lotwRcvd);
    makeServiceBlock(tr("Invia eQSL"),     m_eqslSent, m_eqslRcvd);

    vl->addWidget(qslGrp);

    // ---- Buttons ----
    auto* hl = new QHBoxLayout;
    m_btnSave  = new QPushButton(tr("Salva QSO"), this);
    m_btnClear = new QPushButton(tr("Pulisci"),    this);
    m_btnSave->setDefault(true);

    auto* btnLookup = new QPushButton(tr("Cerca"), this);

    hl->addStretch();
    hl->addWidget(btnLookup);
    hl->addWidget(m_btnClear);
    hl->addWidget(m_btnSave);
    vl->addLayout(hl);

    // Connections
    connect(m_call,   &QLineEdit::textChanged,          this, &QsoEntry::onCallsignChanged);
    connect(m_freq,   QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &QsoEntry::onFreqChanged);
    connect(m_mode,   &QComboBox::currentTextChanged,   this, &QsoEntry::onModeChanged);
    connect(m_btnSave,  &QPushButton::clicked,          this, &QsoEntry::onSave);
    connect(m_btnClear, &QPushButton::clicked,          this, &QsoEntry::onClear);
    connect(btnLookup,  &QPushButton::clicked,          this, &QsoEntry::onLookup);

    // Band change → update dupe strip
    connect(m_band, &QComboBox::currentIndexChanged, this, [this](int){ updateDupeInfo(); });

    // Ctrl+S = Save; F2 = Save; F3 = Clear
    auto* sc = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(sc, &QShortcut::activated, this, &QsoEntry::onSave);
    auto* scF2 = new QShortcut(Qt::Key_F2, this);
    connect(scF2, &QShortcut::activated, this, &QsoEntry::onSave);
    auto* scF3 = new QShortcut(Qt::Key_F3, this);
    connect(scF3, &QShortcut::activated, this, &QsoEntry::onClear);
}

void QsoEntry::reset()
{
    m_editId = 0;
    m_origQslSent.clear();
    m_origLotwSent.clear();
    m_origEqslSent.clear();
    setWindowTitle(tr("Nuovo QSO"));

    m_dupeInfo->hide();
    m_dupeInfo->clear();

    m_call->clear();
    m_date->setDate(QDate::currentDate());
    m_timeOn->setTime(QTime::currentTime());
    m_timeOff->setTime(QTime::currentTime());

    m_rstSent->setText("59");
    m_rstRcvd->setText("59");
    m_name->clear();  m_qth->clear();  m_grid->clear();  m_power->clear();
    m_lblCountry->setText(tr("—")); m_lblContinent->setText(tr("—")); m_lblCqZone->setText(tr("CQ —"));
    m_iota->clear();  m_sota->clear();  m_pota->clear();
    m_contest->clear(); m_srx->clear(); m_stx->clear(); m_manager->clear();
    m_comment->clear(); m_notes->clear();

    QSettings cfg;
    bool lotwEnabled = !cfg.value("lotw/username").toString().isEmpty();

    // Default: tutti e tre i servizi QSL spuntati per default
    m_qslSent->setChecked(true);
    m_lotwSent->setChecked(true);
    m_eqslSent->setChecked(true);

    auto setCombo = [](QComboBox* cb, const QString& val){
        int idx = cb->findText(val);
        if (idx >= 0) cb->setCurrentIndex(idx);
        else cb->setCurrentIndex(0);
    };
    setCombo(m_qslRcvd,  "N");
    setCombo(m_lotwRcvd, "N");
    setCombo(m_eqslRcvd, "N");

    m_call->setFocus();
}

void QsoEntry::applyQslDefaults(const QString& callsign)
{
    if (callsign.size() < 3 || m_editId) return;

    QSettings cfg;
    bool lotwEnabled   = !cfg.value("lotw/username").toString().isEmpty();
    bool alreadyWorked = !Database::instance().searchQsos(callsign).isEmpty();

    // Se già lavorato: deseleziona (QSL già inviata in precedenza)
    m_qslSent->setChecked(!alreadyWorked);
    m_lotwSent->setChecked(!alreadyWorked);
    m_eqslSent->setChecked(!alreadyWorked);
}

void QsoEntry::loadQso(const Qso& q)
{
    m_editId = q.id;
    setWindowTitle(q.id ? tr("Modifica QSO – %1").arg(q.callsign) : tr("Nuovo QSO"));
    fillFrom(q);
}

void QsoEntry::fillFrom(const Qso& q)
{
    m_call->setText(q.callsign);
    m_date->setDate(QDate::fromString(q.date, "yyyy-MM-dd"));

    auto parseTime = [](const QString& s){
        if (s.size() >= 4)
            return QTime(s.left(2).toInt(), s.mid(2,2).toInt());
        return QTime::currentTime();
    };
    m_timeOn->setTime(parseTime(q.timeOn));
    m_timeOff->setTime(parseTime(q.timeOff.isEmpty() ? q.timeOn : q.timeOff));

    int bandIdx = m_band->findText(q.band);
    if (bandIdx >= 0) m_band->setCurrentIndex(bandIdx);
    if (q.freq > 0) m_freq->setValue(q.freq);

    int modeIdx = m_mode->findText(q.mode, Qt::MatchFixedString);
    if (modeIdx >= 0) m_mode->setCurrentIndex(modeIdx);
    m_submode->setCurrentText(q.submode);

    m_rstSent->setText(q.rstSent.isEmpty() ? "59" : q.rstSent);
    m_rstRcvd->setText(q.rstRcvd.isEmpty() ? "59" : q.rstRcvd);
    m_name->setText(q.name);
    m_qth->setText(q.qth);
    m_grid->setText(q.gridsquare);
    m_power->setText(q.power > 0 ? QString::number(q.power,'f',0) : QString());

    m_lblCountry->setText(q.country.isEmpty()   ? tr("—") : q.country);
    m_lblContinent->setText(q.continent.isEmpty() ? tr("—") : q.continent);
    m_lblCqZone->setText(q.cqZone ? tr("CQ %1").arg(q.cqZone) : tr("CQ —"));

    m_iota->setText(q.iota);
    m_sota->setText(q.sotaRef);
    m_pota->setText(q.potaRef);
    m_contest->setText(q.contestId);
    m_srx->setText(q.srx);
    m_stx->setText(q.stx);
    m_manager->setText(q.manager);
    m_comment->setPlainText(q.comment);
    m_notes->setPlainText(q.notes);

    // Salva i valori originali per preservare stati intermedi (Q/R/I) al salvataggio
    m_origQslSent  = q.qslSent;
    m_origLotwSent = q.lotwSent;
    m_origEqslSent = q.eqslSent;
    m_qslSent->setChecked(q.qslSent  != "N");
    m_lotwSent->setChecked(q.lotwSent != "N");
    m_eqslSent->setChecked(q.eqslSent != "N");

    auto setQslCombo = [](QComboBox* cb, const QString& val){
        int i = cb->findText(val);
        cb->setCurrentIndex(i >= 0 ? i : 0);
    };
    setQslCombo(m_qslRcvd,  q.qslRcvd);
    setQslCombo(m_lotwRcvd, q.lotwRcvd);
    setQslCombo(m_eqslRcvd, q.eqslRcvd);
}

Qso QsoEntry::collectQso() const
{
    Qso q;
    q.id        = m_editId;
    q.callsign  = m_call->text().trimmed().toUpper();
    q.date      = m_date->date().toString("yyyy-MM-dd");
    q.timeOn    = m_timeOn->time().toString("HHmm");
    q.timeOff   = m_timeOff->time().toString("HHmm");
    q.band      = m_band->currentText();
    q.freq      = m_freq->value();
    q.mode      = m_mode->currentText();
    q.submode   = m_submode->currentText().trimmed();
    q.rstSent   = m_rstSent->text().trimmed();
    q.rstRcvd   = m_rstRcvd->text().trimmed();
    q.name      = m_name->text().trimmed();
    q.qth       = m_qth->text().trimmed();
    q.gridsquare= m_grid->text().trimmed().toUpper();
    q.power     = m_power->text().trimmed().toDouble();
    q.country   = (m_lblCountry->text()   == tr("—")) ? QString() : m_lblCountry->text();
    q.continent = (m_lblContinent->text() == tr("—")) ? QString() : m_lblContinent->text();
    q.cqZone    = m_lblCqZone->text().split(' ').last().toInt();  // "CQ 14" → 14
    q.iota      = m_iota->text().trimmed();
    q.sotaRef   = m_sota->text().trimmed();
    q.potaRef   = m_pota->text().trimmed();
    q.contestId = m_contest->text().trimmed();
    q.srx       = m_srx->text().trimmed();
    q.stx       = m_stx->text().trimmed();
    q.manager   = m_manager->text().trimmed();
    q.comment   = m_comment->toPlainText().trimmed();
    q.notes     = m_notes->toPlainText().trimmed();
    // Preserva stati intermedi Q/R/I: se la checkbox è ancora spuntata e il valore
    // originale non era né Y né N (es. Q=queued, R=requested, I=invalid), lo mantiene.
    auto sentVal = [](bool checked, const QString& orig) -> QString {
        if (!checked) return "N";
        static const QStringList intermediate{"Q","R","I"};
        return intermediate.contains(orig) ? orig : "Y";
    };
    q.qslSent   = sentVal(m_qslSent->isChecked(),  m_origQslSent);
    q.qslRcvd   = m_qslRcvd->currentText();
    q.lotwSent  = sentVal(m_lotwSent->isChecked(), m_origLotwSent);
    q.lotwRcvd  = m_lotwRcvd->currentText();
    q.eqslSent  = sentVal(m_eqslSent->isChecked(), m_origEqslSent);
    q.eqslRcvd  = m_eqslRcvd->currentText();

    QSettings cfg;
    q.stationCall  = cfg.value("station/callsign","").toString();
    q.operatorCall = cfg.value("station/operator","").toString();

    return q;
}

void QsoEntry::onSave()
{
    Qso q = collectQso();
    if (!q.isValid()) {
        QMessageBox::warning(this, tr("Incompleto"), tr("Nominativo, data, banda e modo sono obbligatori."));
        return;
    }

    bool wasEdit = (q.id != 0);
    bool ok = q.id ? Database::instance().updateQso(q)
                   : Database::instance().saveQso(q);
    if (ok) {
        emit qsoSaved(q);
        reset();
        if (wasEdit) hide();
    }
}

void QsoEntry::onClear() { reset(); }

void QsoEntry::onLookup()
{
    QString cs = m_call->text().trimmed().toUpper();
    if (!cs.isEmpty()) emit callsignForLookup(cs);
}

void QsoEntry::onNow()
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    m_date->setDate(now.date());
    m_timeOn->setTime(now.time());
    m_timeOff->setTime(now.time());
}

void QsoEntry::onCallsignChanged(const QString& text)
{
    QString cs = text.trimmed().toUpper();
    applyDxccInfo(cs);
    applyQslDefaults(cs);
    updateDupeInfo();
    emit callsignChanged(cs);

    if (cs.size() >= 3) {
        // 1) Fill from log history (instant — previous QSOs with this callsign)
        fillFromLog(cs);

        // 2) Fill from callbook cache; if not cached trigger async lookup
        auto cached = Database::instance().getCallbookCache(cs);
        if (!cached.isEmpty()) {
            QString name = cached.value("name");
            if (name.isEmpty()) name = cached.value("fname");
            QString qth = cached.value("addr1");
            if (!cached.value("addr2").isEmpty()) qth += ", " + cached.value("addr2");
            fillFromCallbook(cs, name, qth, cached.value("grid"), cached.value("country"));
        } else {
            emit callsignForLookup(cs);
        }
    }
}

void QsoEntry::applyDxccInfo(const QString& callsign)
{
    if (callsign.size() < 3) {
        m_lblCountry->setText(tr("—"));
        m_lblContinent->setText(tr("—"));
        m_lblCqZone->setText(tr("CQ —"));
        return;
    }
    DxccInfo info = Dxcc::lookup(callsign);
    m_lblCountry->setText(info.entity.isEmpty()    ? tr("—") : info.entity);
    m_lblContinent->setText(info.continent.isEmpty() ? tr("—") : info.continent);
    m_lblCqZone->setText(info.cqZone ? tr("CQ %1").arg(info.cqZone) : tr("CQ —"));
}

void QsoEntry::onFreqChanged(double mhz)
{
    populateBandFromFreq(mhz);
}

void QsoEntry::populateBandFromFreq(double mhz)
{
    QString band = Qso::freqToBand(mhz);
    int idx = m_band->findText(band);
    if (idx >= 0) m_band->setCurrentIndex(idx);
}

void QsoEntry::onModeChanged(const QString& mode)
{
    // Auto-set default RST for CW/digital
    if (mode == "CW") {
        if (m_rstSent->text() == "59") m_rstSent->setText("599");
        if (m_rstRcvd->text() == "59") m_rstRcvd->setText("599");
    } else if (mode == "FT8" || mode == "FT4" || mode == "FT2" || mode == "JS8" ||
               mode == "WSPR" || mode == "JT65" || mode == "JT9" || mode == "MSK144") {
        m_rstSent->setText("+00");
        m_rstRcvd->setText("+00");
    } else {
        if (m_rstSent->text() == "599") m_rstSent->setText("59");
        if (m_rstRcvd->text() == "599") m_rstRcvd->setText("59");
    }
    updateDupeInfo();
}

void QsoEntry::prefill(const QString& callsign, double freqMhz, const QString& mode)
{
    if (!callsign.isEmpty()) m_call->setText(callsign.toUpper());
    if (freqMhz > 0 && !m_catLocked) {
        m_freq->setValue(freqMhz);
        populateBandFromFreq(freqMhz);
    }
    if (!mode.isEmpty()) {
        int idx = m_mode->findText(mode, Qt::MatchFixedString);
        if (idx >= 0) m_mode->setCurrentIndex(idx);
    }
}

void QsoEntry::fillFromSpot(const QString& callsign, double freqMhz, const QString& mode)
{
    prefill(callsign, freqMhz, mode);
    m_call->setFocus();
}

void QsoEntry::setRigState(const RigState& state)
{
    if (state.freq > 0) {
        m_freq->setValue(state.freq / 1e6);
        populateBandFromFreq(state.freq / 1e6);
    }
    if (!state.mode.isEmpty()) {
        int idx = m_mode->findText(state.mode, Qt::MatchFixedString);
        if (idx >= 0) m_mode->setCurrentIndex(idx);
    }
}

void QsoEntry::onCatConnected() { m_catLocked = true; }

void QsoEntry::fillFromLog(const QString& callsign)
{
    if (callsign.size() < 3) return;
    QList<Qso> prev = Database::instance().searchQsos(callsign);
    if (prev.isEmpty()) return;
    const Qso& last = prev.first(); // sorted DESC — most recent first
    if (m_name->text().isEmpty() && !last.name.isEmpty())
        m_name->setText(last.name);
    if (m_qth->text().isEmpty() && !last.qth.isEmpty())
        m_qth->setText(last.qth);
    if (m_grid->text().isEmpty() && !last.gridsquare.isEmpty())
        m_grid->setText(last.gridsquare.toUpper());
}

void QsoEntry::updateDupeInfo()
{
    QString cs = m_call->text().trimmed().toUpper();
    if (cs.size() < 3) {
        m_dupeInfo->hide();
        return;
    }

    DxccInfo dxInfo = Dxcc::lookup(cs);
    QString band = m_band->currentText();
    QString mode = m_mode->currentText();

    QList<Qso> prev = Database::instance().searchQsos(cs);
    int totalCount = prev.size();
    int bandCount = 0;
    for (const Qso& q : prev) {
        if (q.band == band) ++bandCount;
    }

    // Zone / continent info for the strip
    QString zoneInfo;
    if (dxInfo.cqZone)  zoneInfo = tr("CQ%1").arg(dxInfo.cqZone);
    if (dxInfo.ituZone) zoneInfo += (zoneInfo.isEmpty() ? "" : " ") + tr("ITU%1").arg(dxInfo.ituZone);
    if (!dxInfo.continent.isEmpty()) zoneInfo += (zoneInfo.isEmpty() ? "" : "  ") + dxInfo.continent;
    QString prefix = zoneInfo.isEmpty() ? QString() : QString("[%1]  ").arg(zoneInfo);

    // New DXCC / new band on DXCC
    bool newDxcc = false, newBand = false;
    if (!dxInfo.entity.isEmpty()) {
        auto worked = Database::instance().workedEntities();
        newDxcc = !worked.contains(dxInfo.entity);
        if (!newDxcc && !band.isEmpty()) {
            auto workedBand = Database::instance().workedEntities(band);
            newBand = !workedBand.contains(dxInfo.entity);
        }
    }

    QString text, style;

    if (newDxcc) {
        text  = tr("★  NUOVO DXCC: %1  %2%3 QSO precedenti").arg(dxInfo.entity).arg(prefix).arg(totalCount);
        style = "background:#27ae60; color:white; font-weight:bold; padding:2px 6px;";
    } else if (newBand) {
        text  = tr("★  DXCC su nuova banda %1  %2%3 QSO totali").arg(band.toUpper()).arg(prefix).arg(totalCount);
        style = "background:#e67e22; color:white; font-weight:bold; padding:2px 6px;";
    } else if (bandCount == 0 && totalCount > 0) {
        text  = tr("★  Nuova banda %1  %2%3 QSO totali").arg(band.toUpper()).arg(prefix).arg(totalCount);
        style = "background:#f39c12; color:white; padding:2px 6px;";
    } else if (bandCount > 0) {
        text  = tr("DUPE: %1 su %2/%3  %4%5 QSO totali").arg(bandCount).arg(band.toUpper()).arg(mode).arg(prefix).arg(totalCount);
        style = "background:#c0392b; color:white; font-weight:bold; padding:2px 6px;";
    } else {
        // First ever contact, entity unknown or dxcc lookup failed
        text  = tr("Primo QSO con %1  %2").arg(cs).arg(prefix);
        style = "background:#2980b9; color:white; padding:2px 6px;";
    }

    m_dupeInfo->setText(text);
    m_dupeInfo->setStyleSheet(style);
    m_dupeInfo->show();
}

void QsoEntry::fillFromCallbook(const QString& callsign,
                                 const QString& name,
                                 const QString& qth,
                                 const QString& grid,
                                 const QString& country)
{
    // Only auto-fill if this callsign matches what's currently in the form
    // and the field is still empty (don't overwrite user edits)
    if (m_call->text().trimmed().toUpper() != callsign.toUpper()) return;

    if (m_name->text().isEmpty() && !name.isEmpty())
        m_name->setText(name);
    if (m_qth->text().isEmpty() && !qth.isEmpty())
        m_qth->setText(qth);
    if (m_grid->text().isEmpty() && !grid.isEmpty())
        m_grid->setText(grid.toUpper());
    // Update DXCC label only if not already filled
    if ((m_lblCountry->text() == tr("—") || m_lblCountry->text().isEmpty()) && !country.isEmpty())
        m_lblCountry->setText(country);
}
