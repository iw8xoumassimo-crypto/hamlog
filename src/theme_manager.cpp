#include "theme_manager.h"
#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace ThemeManager {

static Theme s_current = Default;

// ---------------------------------------------------------------------------
// QSS stylesheets
// ---------------------------------------------------------------------------
static const char* QSS_DEFAULT = R"(
QMainWindow { background: #eef1f5; }
QDialog { background: #f5f6fa; }
QMenuBar {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2a4d78, stop:1 #1e3a5f);
    color: #d8e8f8;
    padding: 2px 0;
    font-weight: 500;
}
QMenuBar::item { padding: 5px 14px; }
QMenuBar::item:selected { background: #2c5a8a; color: #ffffff; }
QMenu {
    background: #ffffff;
    border: 1px solid #c0ccd8;
    padding: 3px 0;
}
QMenu::item { padding: 6px 22px 6px 30px; color: #2c3e50; }
QMenu::item:selected { background: #2980b9; color: white; }
QMenu::separator { background: #e0e8f0; height: 1px; margin: 3px 0; }
QToolBar {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2c5282, stop:1 #1e3a5f);
    border-bottom: 2px solid #152d4a;
    spacing: 3px;
    padding: 3px 8px;
}
QToolBar QToolButton {
    color: #cce0f8;
    background: transparent;
    border: 1px solid transparent;
    border-radius: 5px;
    padding: 4px 9px;
}
QToolBar QToolButton:hover { background: rgba(255,255,255,0.20); border-color: rgba(255,255,255,0.40); color: #ffffff; }
QToolBar QToolButton:pressed { background: rgba(255,255,255,0.35); }
QToolBar::separator { background: rgba(255,255,255,0.22); width: 1px; margin: 4px 3px; }
QStatusBar {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #243f60, stop:1 #1e3a5f);
    color: #90b8e0;
}
QStatusBar QLabel {
    color: #c8ddf0;
    border: 1px solid #2c5282;
    border-radius: 3px;
    padding: 2px 9px;
    background: #2a4d78;
    margin: 1px 2px;
}
QTableView {
    background: #ffffff;
    alternate-background-color: #f0f5fb;
    gridline-color: #d8e4f0;
    selection-background-color: #2980b9;
    selection-color: #ffffff;
    border: none;
}
QTableView QHeaderView::section {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #3a80c4, stop:1 #2c6fad);
    color: #ffffff;
    border: none;
    border-right: 1px solid #2469a0;
    border-bottom: 1px solid #2469a0;
    padding: 5px 5px;
    font-weight: bold;
}
QTableView::item { padding: 2px 3px; }
QPushButton {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #3a9ad9, stop:1 #2980b9);
    color: white;
    border: 1px solid #2268a0;
    border-radius: 5px;
    padding: 5px 16px;
    min-width: 64px;
    font-weight: 500;
}
QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #5ab0e8, stop:1 #3498db); }
QPushButton:pressed { background: #1a6090; }
QPushButton:default {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #34c468, stop:1 #27ae60);
    border-color: #1e8449;
}
QPushButton:default:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4dd680, stop:1 #2ecc71); }
QPushButton:disabled { background: #c8cdd4; color: #f0f0f0; border-color: #b0b5bc; }
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: #ffffff;
    border: 1.5px solid #b8c8d8;
    border-radius: 4px;
    padding: 3px 6px;
    color: #2c3e50;
    selection-background-color: #2980b9;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus { border-color: #2980b9; background: #f5faff; }
QComboBox::drop-down { border: none; width: 22px; }
QGroupBox {
    border: 1px solid #c0ccd8;
    border-radius: 6px;
    margin-top: 12px;
    background: transparent;
    font-weight: bold;
    color: #1e3a5f;
    padding-top: 4px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 6px;
    color: #2980b9;
    background: transparent;
}
QTabWidget::pane { border: 1px solid #c0ccd8; border-radius: 0 4px 4px 4px; background: #ffffff; }
QTabBar { qproperty-drawBase: 0; }
QTabBar::tab {
    background: #d8e4f0;
    border: 1px solid #b8c8dc;
    border-bottom: none;
    padding: 7px 20px;
    color: #4a6080;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
    margin-right: 2px;
    font-size: 10pt;
}
QTabBar::tab:selected {
    background: #ffffff;
    color: #1e3a5f;
    border-bottom: 3px solid #2980b9;
    font-weight: bold;
}
QTabBar::tab:hover:!selected { background: #c0d8f0; color: #1e3a5f; }
QScrollBar:vertical { width: 8px; background: #eef1f5; border: none; }
QScrollBar::handle:vertical { background: #a8b8cc; border-radius: 4px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #7890a8; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { height: 8px; background: #eef1f5; border: none; }
QScrollBar::handle:horizontal { background: #a8b8cc; border-radius: 4px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #7890a8; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QProgressBar {
    border: none; border-radius: 4px;
    background: #d8e4f0; height: 8px; color: transparent; text-align: center;
}
QProgressBar::chunk { background: #2980b9; border-radius: 4px; }
QSplitter::handle { background: #ccd8e8; }
QSplitter::handle:hover { background: #2980b9; }
QDockWidget::title {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #3a80c4, stop:1 #2c6fad);
    color: #ffffff;
    padding: 5px 8px;
    font-weight: bold;
}
)";

static const char* QSS_DARK = R"(
QWidget { color: #e0e0e0; background: #2b2b2b; }
QMainWindow, QDialog { background: #2b2b2b; }
QMenuBar { background: #3c3c3c; color: #e0e0e0; }
QMenuBar::item:selected { background: #555555; }
QMenu { background: #3c3c3c; border: 1px solid #555555; color: #e0e0e0; }
QMenu::item:selected { background: #555555; }
QToolBar { background: #383838; border-bottom: 1px solid #505050; spacing: 2px; }
QStatusBar { background: #383838; color: #c0c0c0; }
QTableView {
    background: #2b2b2b;
    alternate-background-color: #333333;
    gridline-color: #444444;
    selection-background-color: #4a6fa5;
    selection-color: #ffffff;
    color: #e0e0e0;
}
QTableView QHeaderView::section {
    background: #3c3c3c;
    border: 1px solid #505050;
    color: #d0d0d0;
    padding: 4px;
    font-weight: bold;
}
QPushButton {
    background: #4a4a4a;
    border: 1px solid #666666;
    border-radius: 3px;
    color: #e0e0e0;
    padding: 4px 10px;
    min-width: 60px;
}
QPushButton:hover   { background: #4a6fa5; border-color: #7090c0; }
QPushButton:pressed { background: #3a5580; }
QPushButton:default { border: 2px solid #4a6fa5; }
QLineEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: #3c3c3c;
    border: 1px solid #555555;
    border-radius: 2px;
    color: #e0e0e0;
    padding: 2px 4px;
}
QLineEdit:focus, QTextEdit:focus { border-color: #4a6fa5; }
QTabWidget::pane { border: 1px solid #555555; top: -1px; }
QTabBar { qproperty-drawBase: 0; }
QTabBar::tab {
    background: #3c3c3c;
    border: 1px solid #555555;
    border-bottom: none;
    color: #a0a0a0;
    padding: 8px 20px;
    min-width: 90px;
    font-size: 10pt;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
}
QTabBar::tab:selected {
    background: #2b2b2b;
    border-color: #666666;
    border-bottom: 3px solid #5a9fd4;
    color: #5a9fd4;
    font-weight: bold;
}
QTabBar::tab:!selected:hover { background: #4a5a6a; color: #c0d8f0; }
QGroupBox {
    border: 1px solid #555555;
    border-radius: 4px;
    margin-top: 8px;
    font-weight: bold;
    color: #c0c0c0;
}
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
QScrollBar:vertical {
    width: 12px; background: #3c3c3c;
    border: 1px solid #555555; border-radius: 3px;
}
QScrollBar::handle:vertical {
    background: #606060; border-radius: 3px; min-height: 20px;
}
QScrollBar::handle:vertical:hover { background: #808080; }
QProgressBar {
    border: 1px solid #555555; border-radius: 3px;
    text-align: center; background: #3c3c3c; color: #e0e0e0;
}
QProgressBar::chunk { background: #4a6fa5; border-radius: 2px; }
QSplitter::handle { background: #505050; }
QLabel { color: #e0e0e0; }
QCheckBox { color: #e0e0e0; }
QRadioButton { color: #e0e0e0; }
)";

static const char* QSS_NIGHT = R"(
QWidget { color: #ff4500; background: #0a0a0a; }
QMainWindow, QDialog { background: #0a0a0a; }
QMenuBar { background: #1a0000; color: #ff4500; }
QMenuBar::item:selected { background: #3a0000; }
QMenu { background: #1a0000; border: 1px solid #3a0000; color: #ff4500; }
QMenu::item:selected { background: #3a0000; }
QToolBar { background: #0f0000; border-bottom: 1px solid #3a0000; }
QStatusBar { background: #0f0000; color: #cc3300; }
QTableView {
    background: #0a0a0a;
    alternate-background-color: #120000;
    gridline-color: #2a0000;
    selection-background-color: #5a0000;
    selection-color: #ff6600;
    color: #ff4500;
}
QTableView QHeaderView::section {
    background: #1a0000;
    border: 1px solid #3a0000;
    color: #cc3300;
    padding: 4px;
    font-weight: bold;
}
QPushButton {
    background: #1a0000;
    border: 1px solid #5a0000;
    border-radius: 3px;
    color: #ff4500;
    padding: 4px 10px;
    min-width: 60px;
}
QPushButton:hover   { background: #3a0000; border-color: #cc3300; }
QPushButton:pressed { background: #5a0000; }
QPushButton:default { border: 2px solid #cc3300; }
QLineEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: #120000;
    border: 1px solid #3a0000;
    color: #ff4500;
    padding: 2px 4px;
}
QLineEdit:focus, QTextEdit:focus { border-color: #cc3300; }
QTabWidget::pane { border: 1px solid #3a0000; top: -1px; }
QTabBar { qproperty-drawBase: 0; }
QTabBar::tab {
    background: #1a0000;
    border: 1px solid #3a0000;
    border-bottom: none;
    color: #993300;
    padding: 8px 20px;
    min-width: 90px;
    font-size: 10pt;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
}
QTabBar::tab:selected {
    background: #0a0a0a;
    border-color: #5a0000;
    border-bottom: 3px solid #ff4500;
    color: #ff4500;
    font-weight: bold;
}
QTabBar::tab:!selected:hover { background: #2a0000; color: #cc3300; }
QGroupBox {
    border: 1px solid #3a0000;
    border-radius: 4px;
    margin-top: 8px;
    color: #cc3300;
}
QScrollBar:vertical { width: 12px; background: #0f0000; border: 1px solid #2a0000; }
QScrollBar::handle:vertical { background: #4a0000; min-height: 20px; }
QProgressBar {
    border: 1px solid #3a0000; background: #120000; color: #ff4500; text-align: center;
}
QProgressBar::chunk { background: #990000; }
QSplitter::handle { background: #2a0000; }
QLabel { color: #ff4500; }
QCheckBox { color: #ff4500; }
QRadioButton { color: #ff4500; }
)";

static const char* QSS_SEPIA = R"(
QWidget { color: #3b2a1a; background: #f4e8d0; }
QMainWindow, QDialog { background: #f4e8d0; }
QMenuBar { background: #e8d8b8; color: #3b2a1a; }
QMenuBar::item:selected { background: #d0b880; }
QMenu { background: #f8eed8; border: 1px solid #c8a870; color: #3b2a1a; }
QMenu::item:selected { background: #d0b880; }
QToolBar { background: #e8d0a8; border-bottom: 1px solid #c8a870; }
QStatusBar { background: #e8d0a8; color: #5a3a20; }
QTableView {
    background: #fdf8f0;
    alternate-background-color: #f8eed8;
    gridline-color: #d8c090;
    selection-background-color: #c89040;
    selection-color: #ffffff;
    color: #3b2a1a;
}
QTableView QHeaderView::section {
    background: #e8d8b8;
    border: 1px solid #c8a870;
    color: #3b2a1a;
    padding: 4px;
    font-weight: bold;
}
QPushButton {
    background: #e8d0a8;
    border: 1px solid #c8a870;
    border-radius: 3px;
    color: #3b2a1a;
    padding: 4px 10px;
    min-width: 60px;
}
QPushButton:hover   { background: #d0b880; border-color: #a07840; }
QPushButton:pressed { background: #c0a060; }
QPushButton:default { border: 2px solid #c89040; }
QLineEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: #fdf8f0;
    border: 1px solid #c8a870;
    color: #3b2a1a;
    padding: 2px 4px;
}
QLineEdit:focus, QTextEdit:focus { border-color: #a07840; }
QTabWidget::pane { border: 1px solid #c8a870; top: -1px; }
QTabBar { qproperty-drawBase: 0; }
QTabBar::tab {
    background: #e0c898;
    border: 1px solid #c8a870;
    border-bottom: none;
    color: #7a5030;
    padding: 8px 20px;
    min-width: 90px;
    font-size: 10pt;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
}
QTabBar::tab:selected {
    background: #fdf8f0;
    border-color: #b09060;
    border-bottom: 3px solid #a07840;
    color: #5a2800;
    font-weight: bold;
}
QTabBar::tab:!selected:hover { background: #d0b070; color: #3b2010; }
QGroupBox {
    border: 1px solid #c8a870;
    border-radius: 4px;
    margin-top: 8px;
    color: #5a3a20;
}
QScrollBar:vertical { width: 12px; background: #ecdfc0; border: 1px solid #c8a870; }
QScrollBar::handle:vertical { background: #c0a060; min-height: 20px; }
QProgressBar {
    border: 1px solid #c8a870; background: #f8eed8; color: #3b2a1a; text-align: center;
}
QProgressBar::chunk { background: #c89040; }
QSplitter::handle { background: #d8c090; }
QLabel { color: #3b2a1a; }
QCheckBox { color: #3b2a1a; }
QRadioButton { color: #3b2a1a; }
)";

// ---------------------------------------------------------------------------
// QPalette for each theme (complements QSS for controls that ignore QSS)
// ---------------------------------------------------------------------------
static QPalette makePalette(Theme theme)
{
    QPalette p;
    switch (theme) {
    case Dark: {
        p.setColor(QPalette::Window,          QColor(0x2b, 0x2b, 0x2b));
        p.setColor(QPalette::WindowText,      QColor(0xe0, 0xe0, 0xe0));
        p.setColor(QPalette::Base,            QColor(0x3c, 0x3c, 0x3c));
        p.setColor(QPalette::AlternateBase,   QColor(0x33, 0x33, 0x33));
        p.setColor(QPalette::Text,            QColor(0xe0, 0xe0, 0xe0));
        p.setColor(QPalette::Button,          QColor(0x4a, 0x4a, 0x4a));
        p.setColor(QPalette::ButtonText,      QColor(0xe0, 0xe0, 0xe0));
        p.setColor(QPalette::Highlight,       QColor(0x4a, 0x6f, 0xa5));
        p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
        p.setColor(QPalette::ToolTipBase,     QColor(0x3c, 0x3c, 0x3c));
        p.setColor(QPalette::ToolTipText,     QColor(0xe0, 0xe0, 0xe0));
        p.setColor(QPalette::Link,            QColor(0x70, 0xb0, 0xff));
        p.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x70, 0x70, 0x70));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x70, 0x70, 0x70));
        break;
    }
    case Night: {
        p.setColor(QPalette::Window,          QColor(0x0a, 0x0a, 0x0a));
        p.setColor(QPalette::WindowText,      QColor(0xff, 0x45, 0x00));
        p.setColor(QPalette::Base,            QColor(0x12, 0x00, 0x00));
        p.setColor(QPalette::AlternateBase,   QColor(0x0a, 0x00, 0x00));
        p.setColor(QPalette::Text,            QColor(0xff, 0x45, 0x00));
        p.setColor(QPalette::Button,          QColor(0x1a, 0x00, 0x00));
        p.setColor(QPalette::ButtonText,      QColor(0xff, 0x45, 0x00));
        p.setColor(QPalette::Highlight,       QColor(0x5a, 0x00, 0x00));
        p.setColor(QPalette::HighlightedText, QColor(0xff, 0x66, 0x00));
        break;
    }
    case Sepia: {
        p.setColor(QPalette::Window,          QColor(0xf4, 0xe8, 0xd0));
        p.setColor(QPalette::WindowText,      QColor(0x3b, 0x2a, 0x1a));
        p.setColor(QPalette::Base,            QColor(0xfd, 0xf8, 0xf0));
        p.setColor(QPalette::AlternateBase,   QColor(0xf8, 0xee, 0xd8));
        p.setColor(QPalette::Text,            QColor(0x3b, 0x2a, 0x1a));
        p.setColor(QPalette::Button,          QColor(0xe8, 0xd0, 0xa8));
        p.setColor(QPalette::ButtonText,      QColor(0x3b, 0x2a, 0x1a));
        p.setColor(QPalette::Highlight,       QColor(0xc8, 0x90, 0x40));
        p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
        break;
    }
    default:
        p = QApplication::style()->standardPalette();
        break;
    }
    return p;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void apply(QApplication* app, Theme theme)
{
    s_current = theme;

    const char* qss = QSS_DEFAULT;
    switch (theme) {
    case Dark:  qss = QSS_DARK;  break;
    case Night: qss = QSS_NIGHT; break;
    case Sepia: qss = QSS_SEPIA; break;
    default:    qss = QSS_DEFAULT; break;
    }

    app->setStyle(QStyleFactory::create("Fusion"));
    app->setPalette(makePalette(theme));
    app->setStyleSheet(QString::fromLatin1(qss));
}

void apply(QApplication* app, const QString& themeName)
{
    apply(app, fromName(themeName));
}

Theme       current()     { return s_current; }
QString     currentName() { return availableThemes().value(static_cast<int>(s_current)); }
QStringList availableThemes() { return {"Default","Dark","Night","Sepia"}; }

Theme fromName(const QString& name)
{
    if (name.toLower() == "dark")  return Dark;
    if (name.toLower() == "night") return Night;
    if (name.toLower() == "sepia") return Sepia;
    return Default;
}

} // namespace ThemeManager
