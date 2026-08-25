#include "waveform_theme.h"

namespace wave {

WaveformTheme waveformTheme(const WaveformColorScheme scheme)
{
    if (scheme == WaveformColorScheme::Dark) {
        return {
            QColor(QStringLiteral("#0B1220")),
            QColor(QStringLiteral("#0F172A")),
            QColor(QStringLiteral("#162033")),
            QColor(QStringLiteral("#1E2A3D")),
            QColor(QStringLiteral("#3B4A62")),
            QColor(QStringLiteral("#26344A")),
            QColor(QStringLiteral("#3C4A60")),
            QColor(QStringLiteral("#E5EDF7")),
            QColor(QStringLiteral("#93A4BA")),
            QColor(QStringLiteral("#3DD6C6")),
            QColor(QStringLiteral("#60A5FA")),
            QColor(QStringLiteral("#C084FC")),
            QColor(QStringLiteral("#F59E0B")),
            QColor(QStringLiteral("#22C55E")),
            QColor(QStringLiteral("#38BDF8")),
            QColor(QStringLiteral("#F87171")),
            QColor(QStringLiteral("#F8D66D")),
            QColor(QStringLiteral("#26466D")),
            QColor(QStringLiteral("#7DD3FC")),
            QColor(QStringLiteral("#34D399")),
            QColor(QStringLiteral("#FB7185")),
        };
    }
    return {
        QColor(QStringLiteral("#EDF2F8")),
        QColor(QStringLiteral("#F8FAFD")),
        QColor(QStringLiteral("#EEF3F8")),
        QColor(QStringLiteral("#FFFFFF")),
        QColor(QStringLiteral("#B8C5D6")),
        QColor(QStringLiteral("#DCE4EE")),
        QColor(QStringLiteral("#C5D0DE")),
        QColor(QStringLiteral("#1F2A3A")),
        QColor(QStringLiteral("#64748B")),
        QColor(QStringLiteral("#0F9D8A")),
        QColor(QStringLiteral("#2563EB")),
        QColor(QStringLiteral("#7C3AED")),
        QColor(QStringLiteral("#D97706")),
        QColor(QStringLiteral("#16845B")),
        QColor(QStringLiteral("#0284C7")),
        QColor(QStringLiteral("#C62828")),
        QColor(QStringLiteral("#B7791F")),
        QColor(QStringLiteral("#DCEBFF")),
        QColor(QStringLiteral("#1D4ED8")),
        QColor(QStringLiteral("#16845B")),
        QColor(QStringLiteral("#C62828")),
    };
}

WaveformColorScheme waveformColorScheme(const QPalette& palette)
{
    return palette.color(QPalette::Window).lightnessF() < 0.45
        ? WaveformColorScheme::Dark
        : WaveformColorScheme::Light;
}

QString waveApplicationStyleSheet(const WaveformColorScheme scheme)
{
    if (scheme == WaveformColorScheme::Dark) {
        return QStringLiteral(R"QSS(
QMainWindow, QDialog { background: #0B1220; color: #E5EDF7; }
QToolBar { background: #111B2C; border: 0; border-bottom: 1px solid #344258; spacing: 5px; padding: 4px; }
QDockWidget::title { background: #162033; color: #DCE7F5; padding: 6px; border-bottom: 1px solid #3C4A60; }
QLabel[waveSectionHeader="true"] { font-weight: 600; color: #DCE7F5; background: #162033; padding: 2px 10px; border-bottom: 1px solid #3C4A60; }
QLabel#SimulationStateLabel { border-radius: 4px; font-weight: 600; }
QLabel#SimulationStateLabel[simulationState="ready"] { color: #C7D2E2; background: #1E2A3D; border: 1px solid #64748B; }
QLabel#SimulationStateLabel[simulationState="compiling"], QLabel#SimulationStateLabel[simulationState="running"] { color: #BAE6FD; background: #153553; border: 1px solid #38BDF8; }
QLabel#SimulationStateLabel[simulationState="current"] { color: #A7F3D0; background: #123A32; border: 1px solid #34D399; }
QLabel#SimulationStateLabel[simulationState="stale"] { color: #FDE68A; background: #493513; border: 1px solid #F59E0B; }
QLabel#SimulationStateLabel[simulationState="failed"] { color: #FECDD3; background: #4A1D2A; border: 1px solid #FB7185; }
QMenu { background: #162033; color: #E5EDF7; border: 1px solid #46556D; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 10px; border-radius: 4px; }
QMenu::item:selected { background: #26466D; }
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QTextEdit {
  min-height: 30px; background: #162033; color: #F1F5F9; border: 1px solid #46556D;
  border-radius: 6px; padding: 2px 8px; selection-background-color: #2563EB;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
QPlainTextEdit:focus, QTextEdit:focus { border: 2px solid #7DD3FC; }
QPushButton, QToolButton { min-height: 30px; color: #E5EDF7; background: #1E2A3D;
  border: 1px solid #46556D; border-radius: 6px; padding: 0 10px; }
QPushButton:hover, QToolButton:hover { background: #293A52; border-color: #708199; }
QPushButton:disabled, QToolButton:disabled { color: #718096; background: #141D2B; border-color: #303C50; }
QTreeView, QTableView, QListView, QTreeWidget, QTableWidget, QListWidget {
  background: #0F172A; alternate-background-color: #131E31; color: #E5EDF7;
  border: 1px solid #3C4A60; outline: 0; selection-background-color: #26466D;
}
QHeaderView::section { background: #1E2A3D; color: #DCE7F5; border: 0; border-right: 1px solid #3C4A60; padding: 7px; }
QTabWidget::pane { border: 1px solid #3C4A60; background: #0F172A; }
QTabBar::tab { background: #162033; color: #AEBED2; padding: 7px 12px; border: 1px solid #344258; }
QTabBar::tab:selected { background: #24334A; color: #FFFFFF; border-bottom: 2px solid #38BDF8; }
QStatusBar { background: #111B2C; color: #AEBED2; border-top: 1px solid #344258; }
QSplitter::handle { background: #344258; }
)QSS");
    }
    return QStringLiteral(R"QSS(
QMainWindow, QDialog { background: #EDF2F8; color: #1F2A3A; }
QToolBar { background: #F6F9FC; border: 0; border-bottom: 1px solid #C5D0DE; spacing: 5px; padding: 4px; }
QDockWidget::title { background: #E7EDF5; color: #304158; padding: 6px; border-bottom: 1px solid #C5D0DE; }
QLabel[waveSectionHeader="true"] { font-weight: 600; color: #304158; background: #E7EDF5; padding: 2px 10px; border-bottom: 1px solid #C5D0DE; }
QLabel#SimulationStateLabel { border-radius: 4px; font-weight: 600; }
QLabel#SimulationStateLabel[simulationState="ready"] { color: #435066; background: #E9EEF5; border: 1px solid #64748B; }
QLabel#SimulationStateLabel[simulationState="compiling"], QLabel#SimulationStateLabel[simulationState="running"] { color: #1659A7; background: #E3EFFF; border: 1px solid #2563EB; }
QLabel#SimulationStateLabel[simulationState="current"] { color: #126442; background: #DFF4E9; border: 1px solid #16845B; }
QLabel#SimulationStateLabel[simulationState="stale"] { color: #815400; background: #FFF0C7; border: 1px solid #B7791F; }
QLabel#SimulationStateLabel[simulationState="failed"] { color: #A52222; background: #FDE7E7; border: 1px solid #C62828; }
QMenu { background: #FFFFFF; color: #1F2A3A; border: 1px solid #B8C5D6; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 10px; border-radius: 4px; }
QMenu::item:selected { background: #DCEBFF; }
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QTextEdit {
  min-height: 30px; background: #FFFFFF; color: #1F2A3A; border: 1px solid #B8C5D6;
  border-radius: 6px; padding: 2px 8px; selection-background-color: #2563EB;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
QPlainTextEdit:focus, QTextEdit:focus { border: 2px solid #1D4ED8; }
QPushButton, QToolButton { min-height: 30px; color: #28384E; background: #FFFFFF;
  border: 1px solid #B8C5D6; border-radius: 6px; padding: 0 10px; }
QPushButton:hover, QToolButton:hover { background: #E7EFF8; border-color: #8FA2BA; }
QPushButton:disabled, QToolButton:disabled { color: #98A5B5; background: #E8EDF3; border-color: #CED7E2; }
QTreeView, QTableView, QListView, QTreeWidget, QTableWidget, QListWidget {
  background: #FFFFFF; alternate-background-color: #F6F8FB; color: #1F2A3A;
  border: 1px solid #C5D0DE; outline: 0; selection-background-color: #DCEBFF;
  selection-color: #1F2A3A;
}
QHeaderView::section { background: #E7EDF5; color: #304158; border: 0; border-right: 1px solid #C5D0DE; padding: 7px; }
QTabWidget::pane { border: 1px solid #C5D0DE; background: #FFFFFF; }
QTabBar::tab { background: #E7EDF5; color: #5A6A80; padding: 7px 12px; border: 1px solid #C5D0DE; }
QTabBar::tab:selected { background: #FFFFFF; color: #1F2A3A; border-bottom: 2px solid #0284C7; }
QStatusBar { background: #E7EDF5; color: #5A6A80; border-top: 1px solid #C5D0DE; }
QSplitter::handle { background: #C5D0DE; }
)QSS");
}

} // namespace wave
