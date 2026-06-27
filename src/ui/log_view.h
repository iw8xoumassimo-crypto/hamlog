#pragma once
#include <QWidget>
#include <QAbstractTableModel>
#include "types.h"

class QTableView;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QSortFilterProxyModel;
class QMenu;

// ---------------------------------------------------------------------------
// Model
// ---------------------------------------------------------------------------
class LogModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit LogModel(QObject* parent = nullptr);

    int rowCount   (const QModelIndex& = {}) const override;
    int columnCount(const QModelIndex& = {}) const override;
    QVariant data       (const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData (int section, Qt::Orientation, int role = Qt::DisplayRole) const override;

    void          refresh();
    void          setFilter(const QsoFilter& f) { m_filter = f; }
    Qso           qsoAt(int row) const;
    const QList<Qso>& allQsos() const { return m_qsos; }

    static QStringList columnNames();

private:
    QList<Qso>   m_qsos;
    QsoFilter    m_filter;
};

// ---------------------------------------------------------------------------
// Flag delegate (coloured country-worked squares)
// ---------------------------------------------------------------------------
class FlagDelegate;

// ---------------------------------------------------------------------------
// LogView widget
// ---------------------------------------------------------------------------
class LogView : public QWidget
{
    Q_OBJECT
public:
    explicit LogView(QWidget* parent = nullptr);

    Qso  selectedQso() const;
    void refresh();
    void openSearchBar();

signals:
    void selectionChanged(const QString& callsign);
    void qsoDoubleClicked(const Qso& qso);

private slots:
    void onFilterChanged();
    void onDoubleClick(const QModelIndex& idx);
    void onSelectionChanged();
    void onContextMenu(const QPoint& pos);
    void onCopyCall();
    void onDeleteSelected();

private:
    void buildToolbar();
    Qso  qsoFromProxy(const QModelIndex& proxyIdx) const;

    QTableView*              m_table      = nullptr;
    LogModel*                m_model      = nullptr;
    QSortFilterProxyModel*   m_proxy      = nullptr;
    QLineEdit*               m_search     = nullptr;
    QComboBox*               m_bandFilter = nullptr;
    QComboBox*               m_modeFilter = nullptr;
    QLabel*                  m_countLabel = nullptr;
    QMenu*                   m_ctxMenu    = nullptr;
};
