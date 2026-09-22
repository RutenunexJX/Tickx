#pragma once

#include "wave/widgets_export.h"
#include "waveform_theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QIcon>
#include <QLineEdit>
#include <QMessageBox>

class QAbstractItemView;
class QAbstractScrollArea;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QMainWindow;
class QMenu;
class QMenuBar;
class QProgressBar;
class QSpinBox;
class QStatusBar;
class QTabWidget;
class QTableWidget;
class QToolBar;
class QToolButton;
class QTreeWidget;

namespace wave::ui {

// Only the standalone application changes the application palette. Embedded
// workspaces initialize their private Ela runtime without restyling the host.
WAVEWIDGETS_API void initialize();
WAVEWIDGETS_API void applyTheme(WaveformColorScheme scheme);
WAVEWIDGETS_API void initializeApplicationTheme();
WAVEWIDGETS_API void addAppearanceMenu(QMainWindow* window);
WAVEWIDGETS_API void prepareWindow(QMainWindow* window);
WAVEWIDGETS_API void polishDialog(QWidget* dialog);

enum class Icon {
    NewFile, Open, Save, Export, Undo, Redo, Up, Down, Delete,
    Play, Repeat, Stop, Refresh, Compare, Check, Source,
    ZoomIn, ZoomOut, Fit, Measure, Link, Add, Remove, Close, Copy, Paste,
    Success, Information, Warning, Count
};
WAVEWIDGETS_API QIcon icon(Icon name, QWidget* owner = nullptr);

enum class Notice { Success, Information, Warning };
// Low-frequency feedback only; callers retain full status/error details.
WAVEWIDGETS_API void notify(QWidget* owner, Notice kind, const QString& title,
    const QString& message, int milliseconds = 5000);
WAVEWIDGETS_API void dismissNotification(QWidget* owner);

WAVEWIDGETS_API QPushButton* button(QWidget* parent = nullptr);
WAVEWIDGETS_API QPushButton* button(const QString& text, QWidget* parent = nullptr);
WAVEWIDGETS_API QToolButton* toolButton(QWidget* parent = nullptr);
WAVEWIDGETS_API QLineEdit* lineEdit(QWidget* parent = nullptr);
WAVEWIDGETS_API QLineEdit* lineEdit(const QString& text, QWidget* parent = nullptr);
WAVEWIDGETS_API QComboBox* comboBox(QWidget* parent = nullptr);
WAVEWIDGETS_API QSpinBox* spinBox(QWidget* parent = nullptr);
WAVEWIDGETS_API QCheckBox* checkBox(QWidget* parent = nullptr);
WAVEWIDGETS_API QCheckBox* checkBox(const QString& text, QWidget* parent = nullptr);
WAVEWIDGETS_API QDialogButtonBox* buttonBox(
    QDialogButtonBox::StandardButtons buttons, QWidget* parent = nullptr);
WAVEWIDGETS_API QMenu* menu(QWidget* parent = nullptr);
WAVEWIDGETS_API QMenu* addMenu(QMenu* parent, const QString& title);
WAVEWIDGETS_API QMenu* addMenu(QMenuBar* parent, const QString& title);
WAVEWIDGETS_API QToolBar* toolBar(const QString& title, QWidget* parent = nullptr);
WAVEWIDGETS_API QToolBar* toolBar(QWidget* parent = nullptr);
WAVEWIDGETS_API QToolBar* addToolBar(QMainWindow* parent, const QString& title);
WAVEWIDGETS_API QTabWidget* tabs(QWidget* parent = nullptr);
WAVEWIDGETS_API QTreeWidget* tree(QWidget* parent = nullptr);
WAVEWIDGETS_API QTableWidget* table(QWidget* parent = nullptr);
WAVEWIDGETS_API QProgressBar* progressBar(QWidget* parent = nullptr);
WAVEWIDGETS_API QAbstractItemView* completionPopup(QLineEdit* editor);
// Install before connecting valueChanged; Qt retains range/step/value state.
WAVEWIDGETS_API void installScrollBars(QAbstractScrollArea* area);
WAVEWIDGETS_API QLabel* text(QWidget* parent = nullptr);
WAVEWIDGETS_API QLabel* text(const QString& value, QWidget* parent = nullptr);
WAVEWIDGETS_API QWidget* colorField(QLineEdit* editor, QWidget* parent = nullptr);
WAVEWIDGETS_API void installToolTips(QWidget* root);
WAVEWIDGETS_API void showToolTip(const QPoint& position, const QString& value,
    QWidget* owner, const QRect& region = {}, int milliseconds = -1);
WAVEWIDGETS_API void hideToolTip();
WAVEWIDGETS_API QString toolTipText();
WAVEWIDGETS_API bool toolTipVisible();
WAVEWIDGETS_API QRect boundedToolTipGeometry(const QPoint& position,
    const QSize& size, const QRect& available);

class WAVEWIDGETS_API FormLayout : public QFormLayout {
public:
    using QFormLayout::QFormLayout;
    using QFormLayout::addRow;
    void addRow(const QString& label, QWidget* field);
    void addRow(const QString& label, QLayout* field);
};

// Qt owns the dialog roles, default/escape buttons and modal result semantics.
// Their internal controls are explicitly styled on show, not by a global filter.
class WAVEWIDGETS_API Dialog : public QDialog {
public:
    using QDialog::QDialog;
protected:
    void showEvent(QShowEvent* event) override;
};

class WAVEWIDGETS_API MessageBox : public QMessageBox {
public:
    using QMessageBox::QMessageBox;
    static StandardButton warning(QWidget* parent, const QString& title,
        const QString& text, StandardButtons buttons = Ok,
        StandardButton defaultButton = NoButton);
    static StandardButton critical(QWidget* parent, const QString& title,
        const QString& text, StandardButtons buttons = Ok,
        StandardButton defaultButton = NoButton);
    static StandardButton information(QWidget* parent, const QString& title,
        const QString& text, StandardButtons buttons = Ok,
        StandardButton defaultButton = NoButton);
    static StandardButton question(QWidget* parent, const QString& title,
        const QString& text, StandardButtons buttons = StandardButtons(Yes | No),
        StandardButton defaultButton = NoButton);
protected:
    void showEvent(QShowEvent* event) override;
};

class WAVEWIDGETS_API InputDialog : public QInputDialog {
public:
    using QInputDialog::QInputDialog;
    static QString getText(QWidget* parent, const QString& title,
        const QString& label, QLineEdit::EchoMode mode = QLineEdit::Normal,
        const QString& text = {}, bool* ok = nullptr,
        Qt::WindowFlags flags = {},
        Qt::InputMethodHints hints = Qt::ImhNone);
    static QString getItem(QWidget* parent, const QString& title,
        const QString& label, const QStringList& items, int current = 0,
        bool editable = true, bool* ok = nullptr, Qt::WindowFlags flags = {},
        Qt::InputMethodHints hints = Qt::ImhNone);
protected:
    void showEvent(QShowEvent* event) override;
};

} // namespace wave::ui
