#include "qsl_label_dialog.h"
#include "../database.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QPainter>
#include <QPageLayout>
#include <QMarginsF>
#include <QSettings>

QslLabelDialog::QslLabelDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Stampa etichette QSL"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    resize(700, 500);

    auto* vl = new QVBoxLayout(this);

    // Filtro
    auto* fltLay = new QHBoxLayout;
    fltLay->addWidget(new QLabel(tr("Mostra QSO:"), this));
    m_filter = new QComboBox(this);
    m_filter->addItem(tr("QSL cartacea non inviata"),     "paper_not_sent");
    m_filter->addItem(tr("LoTW non inviato"),             "lotw_not_sent");
    m_filter->addItem(tr("eQSL non inviato"),             "eqsl_not_sent");
    m_filter->addItem(tr("Tutti i QSO"),                  "all");
    fltLay->addWidget(m_filter);
    auto* btnRefresh = new QPushButton(tr("Aggiorna"), this);
    fltLay->addWidget(btnRefresh);
    fltLay->addStretch();
    vl->addLayout(fltLay);

    // Tabella QSO selezionabili
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({tr("Data"), tr("Ora"), tr("Nominativo"),
                                        tr("Banda"), tr("Modo"), tr("Paese")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    vl->addWidget(m_table, 1);

    // Impostazioni layout etichette
    auto* layoutGrp = new QGroupBox(tr("Layout foglio etichette (Avery)"), this);
    auto* layoutForm = new QFormLayout(layoutGrp);
    m_cols = new QSpinBox(this); m_cols->setRange(1, 10); m_cols->setValue(3);
    m_rows = new QSpinBox(this); m_rows->setRange(1, 30); m_rows->setValue(8);
    layoutForm->addRow(tr("Colonne:"), m_cols);
    layoutForm->addRow(tr("Righe per pagina:"), m_rows);
    vl->addWidget(layoutGrp);

    // Pulsanti
    auto* btnLay = new QHBoxLayout;
    auto* btnPreview = new QPushButton(tr("Anteprima…"), this);
    auto* btnPrint   = new QPushButton(tr("Stampa…"),    this);
    auto* btnClose   = new QPushButton(tr("Chiudi"),     this);
    btnLay->addWidget(btnPreview);
    btnLay->addWidget(btnPrint);
    btnLay->addStretch();
    btnLay->addWidget(btnClose);
    vl->addLayout(btnLay);

    connect(btnRefresh, &QPushButton::clicked, this, &QslLabelDialog::buildQsoList);
    connect(m_filter,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QslLabelDialog::buildQsoList);
    connect(btnPreview, &QPushButton::clicked, this, &QslLabelDialog::onPreview);
    connect(btnPrint,   &QPushButton::clicked, this, &QslLabelDialog::onPrint);
    connect(btnClose,   &QPushButton::clicked, this, &QDialog::reject);

    buildQsoList();
}

void QslLabelDialog::buildQsoList()
{
    QString flt = m_filter->currentData().toString();
    QsoFilter f;
    if (flt == "paper_not_sent")  { f.clublogNotSent = false; /* paper handled below */ }
    if (flt == "lotw_not_sent")   f.lotwNotSent    = true;
    if (flt == "eqsl_not_sent")   f.eqslNotSent    = true;

    QList<Qso> all = Database::instance().getAllQsos(f);

    // Filtro QSL cartacea
    if (flt == "paper_not_sent") {
        m_qsos.clear();
        for (const Qso& q : all)
            if (q.qslSent != "Y") m_qsos.append(q);
    } else {
        m_qsos = all;
    }

    m_table->setRowCount(0);
    for (const Qso& q : m_qsos) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(q.date));
        m_table->setItem(row, 1, new QTableWidgetItem(q.timeOn));
        m_table->setItem(row, 2, new QTableWidgetItem(q.callsign));
        m_table->setItem(row, 3, new QTableWidgetItem(q.band));
        m_table->setItem(row, 4, new QTableWidgetItem(q.mode));
        m_table->setItem(row, 5, new QTableWidgetItem(q.country));
    }
    m_table->resizeColumnsToContents();
}

void QslLabelDialog::onPreview()  { printLabels(true);  }
void QslLabelDialog::onPrint()    { printLabels(false); }

void QslLabelDialog::printLabels(bool preview)
{
    // Raccogli QSO selezionati (o tutti se nessuno selezionato)
    QList<Qso> selected;
    auto sel = m_table->selectedItems();
    QSet<int> selRows;
    for (auto* it : sel) selRows.insert(it->row());
    if (selRows.isEmpty()) {
        selected = m_qsos;
    } else {
        for (int r : selRows) if (r < m_qsos.size()) selected.append(m_qsos[r]);
    }
    if (selected.isEmpty()) return;

    QSettings cfg;
    QString myCall = cfg.value("station/callsign", "N0CALL").toString();
    QString myName = cfg.value("station/name").toString();
    QString myQth  = cfg.value("station/qth").toString();
    QString myLoc  = cfg.value("station/locator").toString();

    int cols = m_cols->value();
    int rows = m_rows->value();

    auto doPrint = [&](QPrinter* printer) {
        QPainter p(printer);
        QRectF page = printer->pageLayout().paintRectPixels(printer->resolution());

        double labelW = page.width()  / cols;
        double labelH = page.height() / rows;
        double margin = 6.0;

        int idx = 0;
        int total = selected.size();
        while (idx < total) {
            if (idx > 0) printer->newPage();
            for (int row = 0; row < rows && idx < total; ++row) {
                for (int col = 0; col < cols && idx < total; ++col, ++idx) {
                    const Qso& q = selected[idx];
                    QRectF r(page.left() + col * labelW + margin,
                             page.top()  + row * labelH + margin,
                             labelW - 2*margin,
                             labelH - 2*margin);

                    // Bordo (facoltativo - commentare se non si usa foglio pretagliato)
                    // p.drawRect(r);

                    QFont big   = p.font(); big.setPointSizeF(9); big.setBold(true);
                    QFont small = p.font(); small.setPointSizeF(7);

                    // Callsign destinatario grande
                    p.setFont(big);
                    p.drawText(r, Qt::AlignTop | Qt::AlignHCenter, q.callsign);

                    // Corpo etichetta
                    p.setFont(small);
                    QString body = QString("%1  %2  %3 UTC\n%4  %5")
                                       .arg(q.date).arg(q.timeOn).arg(q.band).arg(q.mode)
                                       .arg(q.rstRcvd.isEmpty() ? "59" : q.rstRcvd);
                    QRectF bodyR = r.adjusted(0, r.height()*0.35, 0, -r.height()*0.25);
                    p.drawText(bodyR, Qt::AlignCenter | Qt::TextWordWrap, body);

                    // Mittente in basso
                    QString sender = QString("%1  %2  %3  %4")
                                         .arg(myCall).arg(myName).arg(myQth).arg(myLoc);
                    QRectF footR = r.adjusted(0, r.height()*0.72, 0, 0);
                    p.drawText(footR, Qt::AlignBottom | Qt::AlignHCenter, sender);
                }
            }
        }
        p.end();
    };

    if (preview) {
        QPrinter printer(QPrinter::HighResolution);
        printer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4),
                                          QPageLayout::Portrait, QMarginsF(10,10,10,10)));
        QPrintPreviewDialog dlg(&printer, this);
        connect(&dlg, &QPrintPreviewDialog::paintRequested, doPrint);
        dlg.exec();
    } else {
        QPrinter printer(QPrinter::HighResolution);
        printer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4),
                                          QPageLayout::Portrait, QMarginsF(10,10,10,10)));
        if (printer.outputFormat() == QPrinter::NativeFormat ||
            printer.outputFormat() == QPrinter::PdfFormat) {
            doPrint(&printer);
        }
    }
}
