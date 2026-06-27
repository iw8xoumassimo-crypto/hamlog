#include "log_manager_dialog.h"
#include "../database.h"
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QListWidgetItem>

LogManagerDialog::LogManagerDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Gestisci Log"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumSize(380, 300);
    build();
    refresh();
}

void LogManagerDialog::build()
{
    auto* vl = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    vl->addWidget(m_list);

    auto* hl = new QHBoxLayout;
    auto* btnAdd    = new QPushButton(tr("Aggiungi"), this);
    auto* btnRename = new QPushButton(tr("Rinomina"), this);
    auto* btnDel    = new QPushButton(tr("Elimina"),  this);
    auto* btnSelect = new QPushButton(tr("Passa a"),  this);
    hl->addWidget(btnAdd);
    hl->addWidget(btnRename);
    hl->addWidget(btnDel);
    hl->addStretch();
    hl->addWidget(btnSelect);
    vl->addLayout(hl);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    connect(btnAdd,    &QPushButton::clicked, this, &LogManagerDialog::onAdd);
    connect(btnRename, &QPushButton::clicked, this, &LogManagerDialog::onRename);
    connect(btnDel,    &QPushButton::clicked, this, &LogManagerDialog::onDelete);
    connect(btnSelect, &QPushButton::clicked, this, &LogManagerDialog::onSelect);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &LogManagerDialog::onSelect);
}

void LogManagerDialog::refresh()
{
    m_list->clear();
    int current = Database::instance().currentLogId();
    for (const LogInfo& l : Database::instance().getLogs()) {
        auto* item = new QListWidgetItem(
            QString("%1 — %2 (%3)").arg(l.name, l.callsign.isEmpty() ? "—" : l.callsign, l.qth));
        item->setData(Qt::UserRole, l.id);
        if (l.id == current) {
            QFont f = item->font(); f.setBold(true); item->setFont(f);
        }
        m_list->addItem(item);
    }
}

void LogManagerDialog::onAdd()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Nuovo Log"), tr("Nome log:"),
                                         QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    Database::instance().addLog(name, QString(), QString(), QString());
    refresh();
}

void LogManagerDialog::onRename()
{
    auto* item = m_list->currentItem();
    if (!item) return;
    bool ok;
    QString name = QInputDialog::getText(this, tr("Rinomina"), tr("Nuovo nome:"),
                                         QLineEdit::Normal, item->text().split(" — ").first(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    int id = item->data(Qt::UserRole).toInt();
    LogInfo l;
    l.id   = id;
    l.name = name;
    Database::instance().updateLog(l);
    refresh();
}

void LogManagerDialog::onDelete()
{
    auto* item = m_list->currentItem();
    if (!item) return;
    int id = item->data(Qt::UserRole).toInt();
    if (id == 1) {
        QMessageBox::warning(this, tr("Elimina"), tr("Impossibile eliminare il log predefinito."));
        return;
    }
    if (QMessageBox::question(this, tr("Elimina log"),
            tr("Eliminare il log '%1'? I QSO saranno spostati nel log predefinito.").arg(item->text()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
    Database::instance().deleteLog(id, false);
    refresh();
}

void LogManagerDialog::onSelect()
{
    auto* item = m_list->currentItem();
    if (!item) return;
    int id = item->data(Qt::UserRole).toInt();
    Database::instance().setCurrentLog(id);
    emit logChanged(id);
    refresh();
}
