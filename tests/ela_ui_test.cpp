#include "ui_controls.h"
#include "main_window.h"
#include "wave_canvas.h"
#include "trace_canvas.h"
#include "waveform_view.h"
#include "wave/model.h"

#include "ElaCheckBox.h"
#include "ElaColorDialog.h"
#include "ElaAppBar.h"
#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaMenu.h"
#include "ElaMenuBar.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaScrollBar.h"
#include "ElaSpinBox.h"
#include "ElaStatusBar.h"
#include "ElaToolBar.h"
#include "ElaToolButton.h"
#include "ElaText.h"
#include "ElaToolTip.h"

#include <QApplication>
#include <QAbstractAnimation>
#include <QCompleter>
#include <QDir>
#include <QFormLayout>
#include <QFileInfo>
#include <QHelpEvent>
#include <QLabel>
#include <QPointer>
#include <QRawFont>
#include <QSettings>
#include <QScreen>
#include <QSignalSpy>
#include <QStringListModel>
#include <QStyleOptionSlider>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <memory>

namespace {
bool containsColor(const QImage& image, QColor expected)
{
    int matches = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const auto actual = image.pixelColor(x, y);
            if (std::abs(actual.red() - expected.red()) < 15
                && std::abs(actual.green() - expected.green()) < 15
                && std::abs(actual.blue() - expected.blue()) < 15) ++matches;
        }
    return matches >= 8;
}

void savePreview(QWidget* widget, const QString& name)
{
    const auto directory = qEnvironmentVariable("WAVE_ELA_PREVIEW_DIR");
    if (directory.isEmpty()) return;
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(widget->grab().save(directory + QLatin1Char('/') + name + QStringLiteral(".png")));
}

int textPixelHeight(QAbstractItemView* view, const QModelIndex& index, QColor color)
{
    const auto image = view->viewport()->grab().toImage();
    const auto row = view->visualRect(index);
    const auto ratio = image.devicePixelRatio();
    const auto pixels = image.copy(int(row.x() * ratio), int(row.y() * ratio),
        int(row.width() * ratio), int(row.height() * ratio));
    int top = pixels.height(), bottom = -1;
    for (int y = 0; y < pixels.height(); ++y)
        for (int x = 0; x < pixels.width(); ++x) {
            const auto actual = pixels.pixelColor(x, y);
            if (std::abs(actual.red() - color.red()) < 15
                && std::abs(actual.green() - color.green()) < 15
                && std::abs(actual.blue() - color.blue()) < 15) {
                top = std::min(top, y); bottom = std::max(bottom, y);
            }
        }
    return std::max(0, bottom - top + 1);
}
}

class ElaUiTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        const auto palette = qApp->palette();
        const auto font = qApp->font();
        const auto sheet = qApp->styleSheet();
        const bool siblings = qApp->testAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
        wave::ui::initialize();
        QCOMPARE(qApp->palette(), palette);
        QCOMPARE(qApp->font(), font);
        QCOMPARE(qApp->styleSheet(), sheet);
        QCOMPARE(qApp->testAttribute(Qt::AA_DontCreateNativeWidgetSiblings), siblings);
        // The offscreen Windows platform uses CMake's isolated test-fonts dir.
        qApp->setFont(QFont(QStringLiteral("Segoe UI"), 9));
        QTest::failOnWarning(QRegularExpression(QStringLiteral("QPainter::.*|QPaintDevice:.*|.*Recursive repaint.*")));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_.path());
        qApp->setOrganizationName(QStringLiteral("WaveElaTests"));
        qApp->setApplicationName(QStringLiteral("IsolatedUi"));
    }

    void controlsAndKeyboard()
    {
        QWidget host;
        auto* layout = new QFormLayout(&host);
        auto* edit = wave::ui::lineEdit(&host);
        auto* combo = wave::ui::comboBox(&host);
        auto* spin = wave::ui::spinBox(&host);
        auto* check = wave::ui::checkBox(QStringLiteral("&Enabled"), &host);
        auto* button = wave::ui::button(QStringLiteral("&Apply"), &host);
        auto* tool = wave::ui::toolButton(&host);
        QVERIFY(qobject_cast<ElaLineEdit*>(edit));
        QVERIFY(qobject_cast<ElaComboBox*>(combo));
        QVERIFY(qobject_cast<ElaSpinBox*>(spin));
        QVERIFY(qobject_cast<ElaCheckBox*>(check));
        QVERIFY(qobject_cast<ElaPushButton*>(button));
        QVERIFY(qobject_cast<ElaToolButton*>(tool));
        QVERIFY(!host.isVisible());
        QVERIFY(!edit->isVisible());
        combo->addItems({QStringLiteral("First"), QStringLiteral("Second"), QStringLiteral("Third")});
        layout->addRow(QStringLiteral("Lane"), edit);
        layout->addRow(QStringLiteral("Scenario"), combo);
        layout->addRow(QStringLiteral("Width"), spin);
        layout->addRow(check); layout->addRow(button); layout->addRow(tool);
        host.resize(450, 320); host.show(); host.activateWindow();
        edit->setFocus(); QTRY_VERIFY(edit->hasFocus());
        QTest::keyClicks(edit, "valid_name"); QCOMPARE(edit->text(), QStringLiteral("valid_name"));
        QTest::keyClick(edit, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(edit->text(), QString());
        QTest::keyClick(edit, Qt::Key_Y, Qt::ControlModifier); QCOMPARE(edit->text(), QStringLiteral("valid_name"));
        QCOMPARE(edit->font(), host.font());
        QVERIFY(edit->height() >= wave::waveformMetrics().controlHeight);
        QVERIFY(button->height() >= wave::waveformMetrics().controlHeight);
        edit->setProperty("invalidDraft", true);
        QVERIFY(containsColor(edit->grab().toImage(), wave::waveformTheme(wave::waveformColorScheme(edit->palette())).error));
        edit->setProperty("invalidDraft", false);
        QVERIFY(containsColor(edit->grab().toImage(), wave::waveformTheme(wave::waveformColorScheme(edit->palette())).focus));
        QTest::keyClick(edit, Qt::Key_Tab); QTRY_VERIFY(combo->hasFocus());
        QTest::keyClick(combo, Qt::Key_Down); QCOMPARE(combo->currentIndex(), 1);
        combo->showPopup(); QVERIFY(combo->view()->isVisible());
        QTest::keyClick(combo->view(), Qt::Key_Escape);
        QTRY_VERIFY(!combo->view()->isVisible());
        combo->showPopup(); combo->hidePopup();
        QTest::qWait(450); QVERIFY(!combo->view()->isVisible());
        spin->setValue(4); QTest::keyClick(spin, Qt::Key_Up); QCOMPARE(spin->value(), 5);
        check->setTristate(true); check->setCheckState(Qt::PartiallyChecked);
        QTest::keyClick(check, Qt::Key_Space); QCOMPARE(check->checkState(), Qt::Checked);
        QSignalSpy clicked(button, &QPushButton::clicked);
        button->setCheckable(true); QTest::keyClick(button, Qt::Key_Space);
        QCOMPARE(clicked.count(), 1); QVERIFY(button->isChecked());
        button->setEnabled(false); QTest::keyClick(button, Qt::Key_Space); QCOMPARE(clicked.count(), 1);
    }

    void completionPopups()
    {
        wave::MainWindow window(wave::makeDemonstrationProject());
        for (const auto* name : {"BusPresetValueEdit", "RangeEditValueEdit"}) {
            auto* edit = window.findChild<QLineEdit*>(QString::fromLatin1(name));
            QVERIFY(edit); QVERIFY(edit->completer());
            QVERIFY(qobject_cast<ElaListView*>(edit->completer()->popup()));
            QCOMPARE(edit->completer()->completionMode(), QCompleter::PopupCompletion);
            QCOMPARE(edit->completer()->caseSensitivity(), Qt::CaseInsensitive);
        }

        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        auto* edit = wave::ui::lineEdit(&host); layout->addWidget(edit);
        auto* model = new QStringListModel({"IDLE", "WAIT_ACK", "DONE"}, &host);
        auto* completer = new QCompleter(model, &host);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setPopup(wave::ui::completionPopup(edit));
        edit->setCompleter(completer);
        auto* popup = completer->popup();
        QVERIFY(qobject_cast<ElaListView*>(popup));
        QVERIFY(qobject_cast<ElaScrollBar*>(popup->verticalScrollBar()));
        QSignalSpy activated(completer, qOverload<const QString&>(&QCompleter::activated));
        host.resize(420, 100); host.show(); host.activateWindow();
        edit->setFocus(); QTRY_VERIFY(edit->hasFocus());
        QTest::keyClicks(edit, "wa"); QTRY_VERIFY(popup->isVisible());
        QCOMPARE(completer->completionCount(), 1);
        popup->setCurrentIndex(completer->completionModel()->index(0, 0));
        QTest::keyClick(popup, Qt::Key_Return);
        QTRY_VERIFY(!popup->isVisible());
        QCOMPARE(edit->text(), QStringLiteral("WAIT_ACK"));
        QCOMPARE(activated.count(), 1);
        QCOMPARE(activated.at(0).at(0).toString(), QStringLiteral("WAIT_ACK"));

        edit->selectAll(); QTest::keyClicks(edit, "d");
        QTRY_VERIFY(popup->isVisible());
        QTest::keyClick(popup, Qt::Key_Escape);
        QTRY_VERIFY(!popup->isVisible());
        QCOMPARE(edit->text(), QStringLiteral("d"));
        QCOMPARE(activated.count(), 1);
        QTRY_VERIFY(edit->hasFocus());
        model->setStringList({"READY", "RETRY"});
        edit->selectAll(); QTest::keyClicks(edit, "re");
        QTRY_VERIFY(popup->isVisible());
        QCOMPARE(completer->completionCount(), 2);
        const auto textColor = wave::waveformTheme(wave::waveformColorScheme(edit->palette())).text;
        const int normalTextHeight = textPixelHeight(popup, completer->completionModel()->index(0, 0), textColor);
        QVERIFY(normalTextHeight > 0);
        popup->setCurrentIndex(completer->completionModel()->index(0, 0));
        QTest::keyClick(popup, Qt::Key_Down);
        QTest::keyClick(popup, Qt::Key_Return);
        QCOMPARE(edit->text(), QStringLiteral("RETRY"));
        QCOMPARE(activated.count(), 2);
        QTRY_VERIFY(!popup->isVisible());

        edit->setFont(QFont(QStringLiteral("Segoe UI"), 18));
        edit->setText("r"); completer->setCompletionPrefix("r"); completer->complete();
        QTRY_VERIFY(popup->isVisible());
        QCOMPARE(popup->font().pointSize(), edit->font().pointSize());
        QVERIFY(popup->sizeHintForRow(0) >= edit->fontMetrics().height() + 12);
        QVERIFY(textPixelHeight(popup, completer->completionModel()->index(0, 0), textColor)
            >= normalTextHeight * 1.5);
        QVERIFY(popup->visualRect(completer->completionModel()->index(1, 0)).bottom()
            < popup->viewport()->height());
        popup->setCurrentIndex(completer->completionModel()->index(0, 0));
        savePreview(popup, QStringLiteral("completion-popup"));
        QPointer<QAbstractItemView> ownedPopup = popup;
        delete completer;
        QVERIFY(ownedPopup.isNull());
        QVERIFY(!edit->completer());
    }

    void scrollBarContracts()
    {
        QAbstractScrollArea area;
        area.horizontalScrollBar()->setRange(-100, 1'000'000);
        area.horizontalScrollBar()->setSingleStep(17);
        area.horizontalScrollBar()->setPageStep(2300);
        area.horizontalScrollBar()->setValue(456789);
        wave::ui::installScrollBars(&area);
        QCOMPARE(area.horizontalScrollBar()->minimum(), -100);
        QCOMPARE(area.horizontalScrollBar()->maximum(), 1'000'000);
        QCOMPARE(area.horizontalScrollBar()->singleStep(), 17);
        QCOMPARE(area.horizontalScrollBar()->pageStep(), 2300);
        QCOMPARE(area.horizontalScrollBar()->value(), 456789);
        area.resize(600, 300); area.show();
        for (const auto orientation : {Qt::Horizontal, Qt::Vertical}) {
            auto* bar = orientation == Qt::Horizontal
                ? area.horizontalScrollBar() : area.verticalScrollBar();
            auto* ela = qobject_cast<ElaScrollBar*>(bar); QVERIFY(ela);
            QVERIFY(!ela->getIsAnimation());
            QScrollBar reference(orientation);
            for (auto* item : {bar, &reference}) {
                item->setRange(-100, 1'000'000);
                item->setSingleStep(17); item->setPageStep(2300); item->setValue(500000);
            }
            const auto hint = QStyle::SH_ScrollBar_LeftClickAbsolutePosition;
            QCOMPARE(bar->style()->styleHint(hint, nullptr, bar),
                reference.style()->styleHint(hint, nullptr, &reference));
            QSignalSpy values(bar, &QScrollBar::valueChanged);
            for (const auto key : {Qt::Key_PageDown, Qt::Key_PageUp, Qt::Key_End, Qt::Key_Home,
                                   Qt::Key_Right, Qt::Key_Down}) {
                QTest::keyClick(bar, key); QTest::keyClick(&reference, key);
                QCOMPARE(bar->value(), reference.value());
            }
            for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
                bar->setLayoutDirection(direction); reference.setLayoutDirection(direction);
                for (const bool inverted : {false, true}) {
                    bar->setInvertedControls(inverted); reference.setInvertedControls(inverted);
                    bar->setValue(500000); reference.setValue(500000);
                    for (const auto modifiers : {Qt::NoModifier, Qt::ControlModifier, Qt::ShiftModifier}) {
                        for (const auto delta : {QPoint(0, 40), QPoint(0, 40), QPoint(0, 40),
                                                QPoint(0, -120), QPoint(120, 0)}) {
                            for (auto* item : {bar, &reference}) {
                                QWheelEvent wheel(QPointF(4, 4), QPointF(item->mapToGlobal(QPoint(4, 4))),
                                    {}, delta, Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
                                QCoreApplication::sendEvent(item, &wheel);
                            }
                            QCOMPARE(bar->value(), reference.value());
                        }
                    }
                }
            }
            const int finalValue = bar->value();
            const int changes = values.count();
            QTest::qWait(350);
            QCOMPARE(bar->value(), finalValue);
            QCOMPARE(values.count(), changes);
        }

        auto* bar = area.horizontalScrollBar();
        bar->setLayoutDirection(Qt::LeftToRight); bar->setInvertedControls(false);
        bar->setValue(400000);
        QStyleOptionSlider option;
        option.initFrom(bar); option.orientation = bar->orientation();
        option.minimum = bar->minimum(); option.maximum = bar->maximum();
        option.sliderPosition = bar->sliderPosition(); option.sliderValue = bar->value();
        option.pageStep = bar->pageStep(); option.singleStep = bar->singleStep();
        const QPoint handle = bar->style()->subControlRect(QStyle::CC_ScrollBar, &option,
            QStyle::SC_ScrollBarSlider, bar).center();
        QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, handle);
        QTest::mouseMove(bar, handle + QPoint(40, 0));
        QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, handle + QPoint(40, 0));
        QVERIFY(bar->value() > 400000);
        QVERIFY(!bar->isSliderDown());
    }

    void waveformScrollBarThemes()
    {
        const auto appPalette = qApp->palette();
        const auto appFont = qApp->font();
        const auto appSheet = qApp->styleSheet();
        wave::WaveCanvas canvas;
        wave::TraceCanvas trace;
        wave::WaveformView light;
        wave::WaveformView dark;
        for (QAbstractScrollArea* view : std::array<QAbstractScrollArea*, 4>{&canvas, &trace, &light, &dark}) {
            QVERIFY(qobject_cast<ElaScrollBar*>(view->horizontalScrollBar()));
            QVERIFY(qobject_cast<ElaScrollBar*>(view->verticalScrollBar()));
        }
        QVERIFY(light.setThemeName("light")); QVERIFY(dark.setThemeName("dark"));
        for (auto* view : {&light, &dark}) {
            view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            view->resize(600, 200); view->show();
            view->horizontalScrollBar()->setRange(0, 1'000'000);
            view->horizontalScrollBar()->setPageStep(200000);
        }
        for (const auto scheme : {wave::WaveformColorScheme::Dark, wave::WaveformColorScheme::Light}) {
            wave::ui::applyTheme(scheme);
            QVERIFY(containsColor(light.horizontalScrollBar()->grab().toImage(),
                wave::waveformTheme(wave::WaveformColorScheme::Light).mutedText));
            QVERIFY(containsColor(dark.horizontalScrollBar()->grab().toImage(),
                wave::waveformTheme(wave::WaveformColorScheme::Dark).mutedText));
        }
        QVERIFY(light.setThemeName("dark"));
        QVERIFY(containsColor(light.horizontalScrollBar()->grab().toImage(),
            wave::waveformTheme(wave::WaveformColorScheme::Dark).mutedText));
        QCOMPARE(qApp->palette(), appPalette);
        QCOMPARE(qApp->font(), appFont);
        QCOMPARE(qApp->styleSheet(), appSheet);
        wave::ui::applyTheme(wave::waveformColorScheme(appPalette));
    }

    void embeddedHostIsolation()
    {
        QWidget host;
        auto hostPalette = host.palette();
        hostPalette.setColor(QPalette::Window, QColor(20, 22, 30));
        host.setPalette(hostPalette);
        host.setFont(QFont(QStringLiteral("Segoe UI"), 12));
        const auto appPalette = qApp->palette();
        const auto appFont = qApp->font();
        const auto appSheet = qApp->styleSheet();
        {
            wave::MainWindow embedded(wave::makeDemonstrationProject(), {}, &host);
            embedded.setWindowFlag(Qt::Window, false);
            host.resize(1280, 800); host.show(); embedded.show();
            QCoreApplication::processEvents();
            QCOMPARE(host.palette(), hostPalette);
            QCOMPARE(qApp->palette(), appPalette);
            QCOMPARE(qApp->font(), appFont);
            QCOMPARE(qApp->styleSheet(), appSheet);
            auto* edit = embedded.findChild<QLineEdit*>(); QVERIFY(edit);
            QCOMPARE(edit->font().pointSize(), host.font().pointSize());
            QVERIFY(embedded.styleSheet().contains(QStringLiteral("#0b1020")));
            hostPalette.setColor(QPalette::Window, QColor(240, 240, 248));
            host.setPalette(hostPalette); QCoreApplication::processEvents();
            QVERIFY(embedded.styleSheet().contains(QStringLiteral("#eff2f8")));
            host.setFont(QFont(QStringLiteral("Segoe UI"), 14));
            QCoreApplication::processEvents();
            QCOMPARE(edit->font().pointSize(), 14);
            QCOMPARE(qApp->palette(), appPalette);
        }
        QCOMPARE(host.palette(), hostPalette);
    }

    void menuAndToolbarContracts()
    {
        QMainWindow host;
        wave::ui::prepareWindow(&host);
        QVERIFY(qobject_cast<ElaMenuBar*>(host.menuBar()));
        QVERIFY(qobject_cast<ElaStatusBar*>(host.statusBar()));
        auto* bar = wave::ui::addToolBar(&host, QStringLiteral("Actions"));
        QVERIFY(qobject_cast<ElaToolBar*>(bar));
        auto* action = bar->addAction(QStringLiteral("&Toggle"));
        action->setCheckable(true);
        auto* button = qobject_cast<QToolButton*>(bar->widgetForAction(action));
        QVERIFY(button); QVERIFY(button->property("waveEla").toBool());
        QSignalSpy triggered(action, &QAction::triggered);
        button->click(); QCOMPARE(triggered.count(), 1); QVERIFY(action->isChecked());
        auto* popup = wave::ui::addMenu(host.menuBar(), QStringLiteral("&File"));
        QVERIFY(qobject_cast<ElaMenu*>(popup));
        auto* disabled = popup->addAction(QStringLiteral("Disabled")); disabled->setEnabled(false);
        auto* checked = popup->addAction(QStringLiteral("&Checked")); checked->setCheckable(true);
        auto* child = wave::ui::addMenu(popup, QStringLiteral("Submenu"));
        QVERIFY(qobject_cast<ElaMenu*>(child)); child->addAction(QStringLiteral("Child"));
        host.show(); popup->popup(host.mapToGlobal(QPoint(20, 60)));
        popup->setActiveAction(checked); QTest::keyClick(popup, Qt::Key_Return);
        QVERIFY(checked->isChecked()); QTRY_VERIFY(!popup->isVisible());
        popup->popup(host.mapToGlobal(QPoint(20, 60))); popup->setActiveAction(disabled);
        QTest::keyClick(popup, Qt::Key_Escape); QTRY_VERIFY(!popup->isVisible());
        for (int i = 0; i < 5; ++i) {
            std::unique_ptr<QMenu> transient(wave::ui::menu(&host));
            transient->addAction(QStringLiteral("Destroy visible popup"));
            transient->popup(host.mapToGlobal(QPoint(30, 70)));
            transient->grab();
        }
    }

    void standardDialogs()
    {
        QMainWindow styledParent;
        wave::ui::prepareWindow(&styledParent);
        wave::ui::Dialog dialog(&styledParent);
        auto* layout = new QVBoxLayout(&dialog);
        auto* edit = wave::ui::lineEdit(&dialog); layout->addWidget(edit);
        auto* buttons = wave::ui::buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QVERIFY(buttons->button(QDialogButtonBox::Ok));
        buttons->button(QDialogButtonBox::Ok)->setDefault(true);
        QTimer::singleShot(0, &dialog, [&] {
            QVERIFY(buttons->button(QDialogButtonBox::Ok)->property("waveEla").toBool());
            QVERIFY(buttons->button(QDialogButtonBox::Ok)->height() >= wave::waveformMetrics().controlHeight);
            buttons->button(QDialogButtonBox::Ok)->setDown(true);
            QVERIFY(containsColor(buttons->button(QDialogButtonBox::Ok)->grab().toImage(),
                wave::waveformTheme(wave::waveformColorScheme(dialog.palette())).selection));
            buttons->button(QDialogButtonBox::Ok)->setDown(false);
            edit->setFocus(); QTest::keyClick(edit, Qt::Key_Return);
        });
        QCOMPARE(dialog.exec(), int(QDialog::Accepted));
        QTimer::singleShot(0, qApp, [] {
            auto* active = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
            QVERIFY(active);
            auto* edit = active->findChild<QLineEdit*>(); QVERIFY(edit);
            QVERIFY(edit->property("waveEla").toBool());
            edit->selectAll(); QTest::keyClicks(edit, "renamed"); QTest::keyClick(edit, Qt::Key_Return);
        });
        bool accepted = false;
        QCOMPARE(wave::ui::InputDialog::getText(nullptr, "Scenario", "Name", QLineEdit::Normal,
            "old", &accepted), QStringLiteral("renamed"));
        QVERIFY(accepted);
        wave::ui::MessageBox box(QMessageBox::Warning, "Conflict", "Keep external changes?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel); box.setEscapeButton(QMessageBox::Cancel);
        QTimer::singleShot(0, &box, [&] {
            QVERIFY(box.button(QMessageBox::Save)->property("waveEla").toBool());
            QTest::keyClick(&box, Qt::Key_Escape);
        });
        QCOMPARE(box.exec(), int(QMessageBox::Cancel));
        QCOMPARE(box.standardButton(box.clickedButton()), QMessageBox::Cancel);
    }

    void modelRolesAndTabOwnership()
    {
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        auto* tree = wave::ui::tree(&host); layout->addWidget(tree);
        auto* row = new QTreeWidgetItem(tree, {QStringLiteral("semantic foreground")});
        row->setForeground(0, QColor(230, 0, 120));
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable); row->setCheckState(0, Qt::Checked);
        tree->setHeaderLabel(QStringLiteral("Signal"));
        auto* table = wave::ui::table(&host); layout->addWidget(table);
        table->setColumnCount(1); table->setRowCount(1);
        auto* cell = new QTableWidgetItem(QStringLiteral("failed result"));
        cell->setForeground(QColor(230, 0, 120)); table->setItem(0, 0, cell);
        auto* tabs = wave::ui::tabs(&host); layout->addWidget(tabs);
        QPointer<QWidget> first = new QWidget; auto* second = new QWidget;
        tabs->addTab(first, QStringLiteral("Compare")); tabs->addTab(second, QStringLiteral("Checks"));
        tabs->setCurrentIndex(1); QCOMPARE(tabs->currentWidget(), second);
        tabs->removeTab(0); QVERIFY(first); delete first;
        host.resize(520, 500); host.show(); QTest::qWait(30);
        tree->clearSelection(); tree->setCurrentItem(nullptr);
        table->clearSelection(); table->setCurrentItem(nullptr);
        QVERIFY(!row->isSelected());
        QVERIFY(!cell->isSelected());
        QVERIFY(containsColor(tree->viewport()->grab().toImage(), QColor(230, 0, 120)));
        QVERIFY(containsColor(table->viewport()->grab().toImage(), QColor(230, 0, 120)));
        QCOMPARE(row->checkState(0), Qt::Checked);
        const QColor background(20, 210, 90);
        cell->setBackground(background);
        QVERIFY(containsColor(table->viewport()->grab().toImage(), background));
        table->setCurrentCell(0, 0); table->selectRow(0);
        QVERIFY(cell->isSelected());
        QVERIFY(containsColor(table->viewport()->grab().toImage(), table->palette().color(QPalette::Highlight)));
    }

    void textAndFormContracts()
    {
        QWidget host;
        host.setFont(QFont(QStringLiteral("Segoe UI"), 14));
        auto hostPalette = host.palette();
        hostPalette.setColor(QPalette::WindowText, QColor(32, 120, 220));
        host.setPalette(hostPalette);
        auto* form = new wave::ui::FormLayout(&host);
        auto* edit = wave::ui::lineEdit(&host);
        form->addRow(QStringLiteral("&Name"), edit);
        auto* caption = qobject_cast<QLabel*>(form->labelForField(edit));
        QVERIFY(qobject_cast<ElaText*>(caption));
        QCOMPARE(caption->buddy(), edit);
        auto* body = wave::ui::text(QStringLiteral("Normal explanation"), &host);
        form->addRow(body);
        host.show(); QCoreApplication::processEvents();
        QCOMPARE(body->font().pointSize(), 14);
        QCOMPARE(body->palette().color(QPalette::WindowText), hostPalette.color(QPalette::WindowText));
        QCOMPARE(caption->font().pointSize(), 14);
        host.setFont(QFont(QStringLiteral("Segoe UI"), 18));
        QCOMPARE(body->font().pointSize(), 18);
        const QColor custom(210, 10, 90);
        auto palette = body->palette(); palette.setColor(QPalette::WindowText, custom);
        body->setPalette(palette);
        for (const auto mode : {wave::WaveformColorScheme::Dark, wave::WaveformColorScheme::Light}) {
            wave::ui::applyTheme(mode);
            QCOMPARE(body->font().pointSize(), 18);
            QCOMPARE(body->palette().color(QPalette::WindowText), custom);
            QVERIFY(containsColor(body->grab().toImage(), custom));
        }
        wave::MainWindow window(wave::makeDemonstrationProject());
        QVERIFY(qobject_cast<ElaText*>(window.findChild<QLabel*>(QStringLiteral("PointerStatusLabel"))));
        auto* save = window.findChild<QLabel*>(QStringLiteral("SaveStateLabel"));
        QVERIFY(save); QVERIFY(!qobject_cast<ElaText*>(save));
        auto* error = window.findChild<QLabel*>(QStringLiteral("QuickLaneErrorLabel"));
        if (error) QVERIFY(!qobject_cast<ElaText*>(error));
    }

    void iconContracts()
    {
        QWidget owner;
        auto palette = owner.palette();
        const QColor normal(32, 120, 220), disabled(110, 100, 150), selected(210, 30, 90);
        palette.setColor(QPalette::ButtonText, normal);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
        palette.setColor(QPalette::HighlightedText, selected);
        owner.setPalette(palette);
        for (int i = 0; i < int(wave::ui::Icon::Count); ++i) {
            const auto icon = wave::ui::icon(static_cast<wave::ui::Icon>(i), &owner);
            QVERIFY(!icon.isNull());
            QCOMPARE(icon.name(), QStringLiteral("wave-rounded/%1").arg(i));
            for (const auto scale : {1.0, 1.25, 1.5, 2.0}) {
                const auto pixmap = icon.pixmap(QSize(24, 24), scale);
                QCOMPARE(pixmap.size(), QSize(int(24 * scale), int(24 * scale)));
                QCOMPARE(pixmap.devicePixelRatioF(), scale);
                QVERIFY(containsColor(pixmap.toImage(), normal));
            }
            QVERIFY(containsColor(icon.pixmap(QSize(24, 24), QIcon::Disabled).toImage(), disabled));
            QVERIFY(containsColor(icon.pixmap(QSize(24, 24), QIcon::Selected).toImage(), selected));
            QVERIFY(containsColor(icon.pixmap(QSize(24, 24), QIcon::Normal, QIcon::On).toImage(), normal));
        }
        const auto reusable = wave::ui::icon(wave::ui::Icon::Save, &owner);
        palette.setColor(QPalette::ButtonText, Qt::white); owner.setPalette(palette);
        QVERIFY(containsColor(reusable.pixmap(24).toImage(), Qt::white));
        auto* transient = new QWidget;
        const auto detached = wave::ui::icon(wave::ui::Icon::Open, transient);
        delete transient;
        QVERIFY(!detached.pixmap(24).isNull());

        wave::MainWindow window(wave::makeDemonstrationProject());
        for (const auto* name : {"NewProjectAction", "UndoAction", "RedoAction", "MeasureToolAction",
                 "ZoomInAction", "ZoomOutAction", "FitScenarioAction", "RangeEditLoadValuesAction"}) {
            auto* action = window.findChild<QAction*>(QString::fromLatin1(name)); QVERIFY(action);
            QVERIFY(action->icon().name().startsWith(QStringLiteral("wave-rounded/")));
        }
    }

    void compactToolbarAndPersistentNavigation()
    {
        wave::MainWindow window(wave::makeDemonstrationProject());
        window.resize(1100, 720); window.show(); window.activateWindow();
        auto* canvas = window.findChild<wave::WaveCanvas*>(); QVERIFY(canvas);
        auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("WaveformToolbar")); QVERIFY(toolbar);
        auto* search = window.findChild<QLineEdit*>(QStringLiteral("SignalFindEdit")); QVERIFY(search);
        auto* time = window.findChild<QLineEdit*>(QStringLiteral("GoToTimeEdit")); QVERIFY(time);
        auto* searchBar = window.findChild<QWidget*>(QStringLiteral("SignalFindBar")); QVERIFY(searchBar);
        auto* editMenu = window.menuBar()->actions().at(1)->menu(); QVERIFY(editMenu);
        QVERIFY(!window.findChild<QLabel*>(QStringLiteral("WaveTargetLabel")));
        QVERIFY(!window.findChild<QAction*>(QStringLiteral("WaveTargetToolbarAction")));
        for (const auto* name : {"UndoAction", "RedoAction", "RelationToolAction", "FindSignalAction",
                 "GoToTimeAction", "AddGroupAction"}) {
            auto* action = window.findChild<QAction*>(QString::fromLatin1(name)); QVERIFY(action);
            QVERIFY(!editMenu->actions().contains(action));
            if (QString::fromLatin1(name) != QStringLiteral("AddGroupAction")) {
                QVERIFY(window.actions().contains(action) || toolbar->actions().contains(action));
            }
        }
        for (const auto* name : {"MeasureToolAction", "RelationToolAction", "AsyncTimingAction",
                 "ZoomInAction", "ZoomOutAction", "FitScenarioAction"}) {
            auto* action = window.findChild<QAction*>(QString::fromLatin1(name)); QVERIFY(action);
            auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(action)); QVERIFY(button);
            QVERIFY(button->isVisible());
            QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonIconOnly);
            QVERIFY(!button->icon().isNull()); QVERIFY(!button->toolTip().isEmpty());
        }
        QStringList addIcons;
        for (const auto* name : {"CanvasAddClockButton", "CanvasAddBitButton", "CanvasAddBusButton"}) {
            auto* button = window.findChild<QToolButton*>(QString::fromLatin1(name)); QVERIFY(button);
            QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonIconOnly);
            QVERIFY(!button->accessibleName().isEmpty()); QVERIFY(!button->toolTip().isEmpty());
            addIcons.push_back(button->icon().name());
        }
        QCOMPARE(QSet<QString>(addIcons.begin(), addIcons.end()).size(), 3);
        for (const int width : {140, 190, 480}) {
            canvas->setSignalHeaderWidth(width); QCoreApplication::processEvents();
            QVERIFY(search->isVisible()); QVERIFY(time->isVisible());
            QCOMPARE(searchBar->parentWidget(), canvas->viewport());
            QVERIFY(QRect(0, 0, width, 40).contains(searchBar->geometry()));
            QVERIFY(searchBar->rect().contains(search->geometry()));
            const auto before = searchBar->geometry();
            canvas->verticalScrollBar()->setValue(canvas->verticalScrollBar()->maximum());
            QCOMPARE(searchBar->geometry(), before);
        }
        canvas->setSignalHeaderWidth(190);
        canvas->verticalScrollBar()->setValue(0);
        auto* timing = window.findChild<QAction*>(QStringLiteral("AsyncTimingAction"));
        const auto syncIcon = timing->icon().name(); timing->trigger();
        QVERIFY(canvas->asynchronousEditing()); QVERIFY(timing->icon().name() != syncIcon);
        QVERIFY(timing->toolTip().contains(QStringLiteral("Async")));
        timing->trigger(); QVERIFY(!canvas->asynchronousEditing());
        auto* measure = window.findChild<QAction*>(QStringLiteral("MeasureToolAction"));
        auto* relation = window.findChild<QAction*>(QStringLiteral("RelationToolAction"));
        measure->trigger(); QCOMPARE(canvas->tool(), wave::WaveCanvas::Tool::Marker);
        relation->trigger(); QCOMPARE(canvas->tool(), wave::WaveCanvas::Tool::Relation);
        QVERIFY(!measure->isChecked()); QVERIFY(relation->isChecked());
        canvas->setFocus(); QTest::keyClick(canvas, Qt::Key_Escape);
        QCOMPARE(canvas->tool(), wave::WaveCanvas::Tool::WaveEdit);
        QVERIFY(!relation->isChecked()); QVERIFY(search->isVisible()); QVERIFY(time->isVisible());
        // Standalone light/dark previews use the real appearance actions below.
    }

    void persistentNavigationPreservesEditing()
    {
        wave::MainWindow window(wave::makeDemonstrationProject());
        window.resize(1100, 720); window.show(); window.activateWindow();
        auto* canvas = window.findChild<wave::WaveCanvas*>(); QVERIFY(canvas);
        auto* search = window.findChild<QLineEdit*>(QStringLiteral("SignalFindEdit")); QVERIFY(search);
        auto* time = window.findChild<QLineEdit*>(QStringLiteral("GoToTimeEdit")); QVERIFY(time);
        const auto original = window.project();
        canvas->revealLocation(QStringLiteral("lane-request"), 20'000);
        canvas->insertPulse(); QVERIFY(window.project() != original);
        const auto edited = window.project();
        canvas->setFocus(); QTRY_VERIFY(canvas->hasFocus() || canvas->viewport()->hasFocus());
        QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
        QVERIFY(window.project() == original);
        QTest::keyClick(canvas, Qt::Key_Y, Qt::ControlModifier);
        QVERIFY(window.project() == edited);
        QTest::keyClick(canvas, Qt::Key_F, Qt::ControlModifier); QTRY_VERIFY(search->hasFocus());
        QTest::keyClicks(search, "req");
        QCOMPARE(canvas->selectedLaneId(), QStringLiteral("lane-request"));
        QCOMPARE(search->property("searchResultPosition").toString(), QStringLiteral("1/1"));
        QTest::keyClick(search, Qt::Key_Z, Qt::ControlModifier);
        QVERIFY(search->text().isEmpty()); QVERIFY(window.project() == edited);
        QTest::keyClick(search, Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(search->text(), QStringLiteral("req")); QVERIFY(window.project() == edited);
        QTest::keyClick(search, Qt::Key_Escape); QVERIFY(search->isVisible());
        QTest::keyClick(canvas, Qt::Key_G, Qt::ControlModifier); QTRY_VERIFY(time->hasFocus());
        time->setText(QStringLiteral("25 ns")); QTest::keyClick(time, Qt::Key_Return);
        QCOMPARE(canvas->cursorTick(), wave::Tick(25'000)); QVERIFY(window.project() == edited);
        time->setText(QStringLiteral("bad-time")); QTest::keyClick(time, Qt::Key_Return);
        QCOMPARE(canvas->cursorTick(), wave::Tick(25'000)); QVERIFY(time->hasFocus());
        QCOMPARE(time->property("waveState").toString(), QStringLiteral("error"));
        QTest::keyClick(time, Qt::Key_Escape); QVERIFY(time->isVisible());
        QCOMPARE(time->text(), QStringLiteral("25 ns"));

        canvas->selectEntireTimeline();
        const auto range = canvas->selectedTimeRange();
        search->setFocus(); search->setText(QStringLiteral("ack"));
        QCOMPARE(canvas->selectedLaneId(), QStringLiteral("lane-request"));
        QCOMPARE(canvas->selectedTimeRange(), range); QVERIFY(window.project() == edited);
        QTest::keyClick(search, Qt::Key_Escape);
        QTest::keyClick(canvas, Qt::Key_Escape);
        canvas->revealLocation(QStringLiteral("lane-data"), 90'000);
        canvas->selectEntireTimeline();
        auto* rangeValue = window.findChild<QLineEdit*>(QStringLiteral("RangeEditValueEdit")); QVERIFY(rangeValue);
        QTRY_VERIFY(rangeValue->isVisible()); rangeValue->setFocus();
        rangeValue->selectAll(); QTest::keyClicks(rangeValue, "not-a-numeric-value");
        const auto busRange = canvas->selectedTimeRange();
        QTest::mouseClick(time, Qt::LeftButton);
        QTRY_VERIFY(rangeValue->hasFocus());
        QCOMPARE(rangeValue->text(), QStringLiteral("not-a-numeric-value"));
        QCOMPARE(canvas->selectedTimeRange(), busRange); QVERIFY(window.project() == edited);
        QTest::mouseClick(search, Qt::LeftButton);
        QTRY_VERIFY(rangeValue->hasFocus());
        QCOMPARE(canvas->selectedLaneId(), QStringLiteral("lane-data"));
        QCOMPARE(canvas->selectedTimeRange(), busRange); QVERIFY(window.project() == edited);
    }

    void notificationContracts()
    {
        const auto appPalette = qApp->palette();
        const auto appFont = qApp->font();
        const auto appSheet = qApp->styleSheet();
        QMainWindow owner;
        auto* central = new QWidget(&owner); owner.setCentralWidget(central);
        auto* layout = new QVBoxLayout(central);
        auto* edit = wave::ui::lineEdit(central); layout->addWidget(edit); layout->addStretch();
        owner.resize(720, 320); owner.show(); owner.activateWindow();
        edit->setFocus(); QTRY_VERIFY(edit->hasFocus());
        owner.statusBar()->showMessage(QStringLiteral("Pointer status is retained"));
        wave::ui::notify(&owner, wave::ui::Notice::Success, "Saved", "project.wave.json", 5000);
        auto* bar = central->findChild<ElaMessageBar*>(QStringLiteral("WaveNotification")); QVERIFY(bar);
        QVERIFY(bar->isVisible()); QVERIFY(!bar->isWindow());
        QVERIFY(bar->testAttribute(Qt::WA_ShowWithoutActivating));
        QCOMPARE(bar->focusPolicy(), Qt::NoFocus);
        QVERIFY(bar->findChildren<QAbstractAnimation*>().isEmpty());
        QCOMPARE(QApplication::focusWidget(), edit);
        QCOMPARE(owner.statusBar()->currentMessage(), QStringLiteral("Pointer status is retained"));
        QTest::keyClicks(edit, "continued edit"); QCOMPARE(edit->text(), QStringLiteral("continued edit"));
        for (int i = 0; i < 30; ++i)
            wave::ui::notify(&owner, wave::ui::Notice::Success, "Saved", "project.wave.json");
        QCOMPARE(central->findChildren<ElaMessageBar*>().size(), 1);
        auto* close = bar->findChild<QToolButton*>(QStringLiteral("WaveNotificationClose")); QVERIFY(close);
        QSignalSpy closed(bar, &ElaMessageBar::closeRequested);
        close->click(); QCOMPARE(closed.count(), 1); QVERIFY(!bar->isVisible());
        QCOMPARE(QApplication::focusWidget(), edit);

        wave::ui::notify(&owner, wave::ui::Notice::Success, "Old", "first", 40);
        wave::ui::notify(&owner, wave::ui::Notice::Information, "Latest", "second", 1000);
        QTest::qWait(70);
        QVERIFY(bar->isVisible()); QCOMPARE(bar->accessibleDescription(), QStringLiteral("second"));
        wave::ui::notify(&owner, wave::ui::Notice::Success, "Duplicate", "no timeout extension", 80);
        wave::ui::notify(&owner, wave::ui::Notice::Success, "Duplicate", "no timeout extension", 4000);
        QTRY_VERIFY_WITH_TIMEOUT(!bar->isVisible(), 500);

        const QString longMessage = QStringLiteral("<b>literal path</b> ") + QString(500, 'X');
        wave::ui::notify(&owner, wave::ui::Notice::Warning, "Results available", longMessage);
        auto* body = bar->findChild<QLabel*>(QStringLiteral("WaveNotificationText")); QVERIFY(body);
        QCOMPARE(body->textFormat(), Qt::PlainText);
        QCOMPARE(body->accessibleName(), longMessage);
        QCOMPARE(body->toolTip(), Qt::convertFromPlainText(longMessage));
        owner.resize(340, 220); owner.setFont(QFont("Segoe UI", 18));
        QCoreApplication::processEvents();
        QVERIFY(bar->isVisible()); QVERIFY(central->rect().contains(bar->geometry()));
        QVERIFY(bar->rect().contains(QRect(close->mapTo(bar, QPoint()), close->size())));
        auto palette = owner.palette(); palette.setColor(QPalette::Window, QColor(20, 22, 30));
        owner.setPalette(palette); QCoreApplication::processEvents();
        QCOMPARE(bar->palette().color(QPalette::Window),
            wave::waveformTheme(wave::WaveformColorScheme::Dark).warningSurface);
        owner.setLayoutDirection(Qt::RightToLeft); QCoreApplication::processEvents();
        QVERIFY(central->rect().contains(bar->geometry()));
        owner.hide(); QVERIFY(!bar->isVisible());
        owner.show(); QCoreApplication::processEvents(); QVERIFY(!bar->isVisible());

        wave::ui::notify(&owner, wave::ui::Notice::Information, "Primary", "independent owner");
        auto* transient = new QWidget; transient->resize(420, 220); transient->show();
        wave::ui::notify(transient, wave::ui::Notice::Information, "Transient", "destroy while visible");
        QPointer<ElaMessageBar> doomed = transient->findChild<ElaMessageBar*>(); QVERIFY(doomed);
        delete transient; QVERIFY(doomed.isNull());
        QCoreApplication::processEvents();
        QVERIFY(bar->isVisible()); QCOMPARE(bar->accessibleDescription(), QStringLiteral("independent owner"));
        QCOMPARE(qApp->palette(), appPalette); QCOMPARE(qApp->font(), appFont); QCOMPARE(qApp->styleSheet(), appSheet);

        QTemporaryDir projectDirectory; QVERIFY(projectDirectory.isValid());
        const auto path = projectDirectory.filePath("saved.wave.json");
        wave::MainWindow workspace(wave::makeDemonstrationProject(), path);
        workspace.show(); QCoreApplication::processEvents();
        QVERIFY(QMetaObject::invokeMethod(&workspace, "saveProject", Qt::DirectConnection));
        QVERIFY(QFileInfo::exists(path));
        auto* saved = workspace.findChild<ElaMessageBar*>(QStringLiteral("WaveNotification")); QVERIFY(saved);
        QVERIFY(saved->isVisible()); QCOMPARE(saved->accessibleName(), QStringLiteral("Project saved"));
    }

    void toolTipContracts()
    {
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        auto* editor = wave::ui::lineEdit(&host); layout->addWidget(editor);
        auto* button = wave::ui::button(QStringLiteral("Hover target"), &host);
        button->setToolTip(QStringLiteral("Current control help")); layout->addWidget(button);
        wave::ui::installToolTips(&host);
        host.resize(450, 180); host.show(); host.activateWindow();
        editor->setFocus(); QTRY_VERIFY(editor->hasFocus());
        QHelpEvent help(QEvent::ToolTip, {4, 4}, button->mapToGlobal(QPoint(4, 4)));
        QCoreApplication::sendEvent(button, &help);
        QVERIFY(wave::ui::toolTipVisible());
        QCOMPARE(wave::ui::toolTipText(), button->toolTip());
        QVERIFY(!QToolTip::isVisible());
        QCOMPARE(QApplication::focusWidget(), editor);
        auto* tip = host.findChild<ElaToolTip*>(QStringLiteral("WaveElaToolTip")); QVERIFY(tip);
        QVERIFY(tip->testAttribute(Qt::WA_ShowWithoutActivating));
        QEvent leave(QEvent::Leave); QCoreApplication::sendEvent(button, &leave);
        QTest::qWait(350); QVERIFY(!wave::ui::toolTipVisible());

        const auto point = button->mapToGlobal(QPoint(10, 10));
        wave::ui::showToolTip(point, "Old", button, {}, 30);
        wave::ui::showToolTip(point, "Latest", button, {}, 1000);
        QTest::qWait(60); QVERIFY(wave::ui::toolTipVisible());
        QCOMPARE(wave::ui::toolTipText(), QStringLiteral("Latest"));
        wave::ui::showToolTip(point, "Expires", button, {}, 20);
        QTest::qWait(50); QVERIFY(!wave::ui::toolTipVisible());
        wave::ui::showToolTip(point, "Region", button, QRect(0, 0, 10, 10));
        QMouseEvent move(QEvent::MouseMove, QPointF(40, 40), QPointF(button->mapToGlobal(QPoint(40, 40))),
            Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(button, &move);
        QVERIFY(!wave::ui::toolTipVisible());

        const QRect negativeScreen(-1920, -200, 1920, 1080);
        for (const auto position : {negativeScreen.topLeft(), negativeScreen.bottomRight(), QPoint(-100, 400)}) {
            QVERIFY(negativeScreen.contains(wave::ui::boundedToolTipGeometry(position, {520, 220}, negativeScreen)));
        }
        QCOMPARE(wave::ui::boundedToolTipGeometry({0, 0}, {4000, 4000}, negativeScreen), negativeScreen);
        QVERIFY(wave::ui::boundedToolTipGeometry({}, {20, 20}, {}).isEmpty());
        const auto available = host.screen()->availableGeometry();
        editor->setFont(QFont(QStringLiteral("Segoe UI"), 18));
        wave::ui::showToolTip(available.bottomRight(), QStringLiteral("Long help ") + QString(160, 'X'), editor);
        QVERIFY(wave::ui::toolTipVisible());
        tip = host.findChild<ElaToolTip*>(QStringLiteral("WaveElaToolTip")); QVERIFY(tip);
        QVERIFY(available.contains(tip->geometry()));
        savePreview(tip, QStringLiteral("tooltip"));
        QEvent deactivate(QEvent::WindowDeactivate);
        QCoreApplication::sendEvent(&host, &deactivate);
        QVERIFY(!wave::ui::toolTipVisible());

        auto* late = wave::ui::button(QStringLiteral("Late child"), &host);
        layout->addWidget(late); late->setToolTip("Late help");
        QCoreApplication::processEvents();
        QHelpEvent lateHelp(QEvent::ToolTip, {3, 3}, late->mapToGlobal(QPoint(3, 3)));
        QCoreApplication::sendEvent(late, &lateHelp);
        QCOMPARE(wave::ui::toolTipText(), QStringLiteral("Late help"));
        QPointer<ElaToolTip> ownedTip = late->findChild<ElaToolTip*>(); QVERIFY(ownedTip);
        delete late;
        QVERIFY(ownedTip.isNull()); QVERIFY(!wave::ui::toolTipVisible());
        QCOMPARE(wave::ui::toolTipText(), QString());

        QLabel foreign(QStringLiteral("Unmanaged host control"));
        foreign.setToolTip("Native host tooltip"); foreign.show();
        QHelpEvent foreignHelp(QEvent::ToolTip, {2, 2}, foreign.mapToGlobal(QPoint(2, 2)));
        QCoreApplication::sendEvent(&foreign, &foreignHelp);
        QCOMPARE(QToolTip::text(), QStringLiteral("Native host tooltip"));
        QVERIFY(!wave::ui::toolTipVisible());
        QToolTip::hideText();
    }

    void colorFieldContracts()
    {
        const auto palette = qApp->palette();
        const auto font = qApp->font();
        const auto sheet = qApp->styleSheet();
        wave::ui::Dialog host;
        auto* layout = new wave::ui::FormLayout(&host);
        auto* editor = wave::ui::lineEdit("#804fc3f7", &host);
        editor->setObjectName(QStringLiteral("TestColor"));
        layout->addRow(QStringLiteral("Color"), wave::ui::colorField(editor, &host));
        auto* choose = host.findChild<QPushButton*>(QStringLiteral("TestColorChooseButton")); QVERIFY(choose);
        host.show(); host.activateWindow(); choose->setFocus();
        QSignalSpy edited(editor, &QLineEdit::textEdited);
        QTimer::singleShot(2000, &host, [] {
            if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
        });
        QTimer::singleShot(0, &host, [&] {
            auto* dialog = qobject_cast<ElaColorDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
            QVERIFY(!dialog->findChild<ElaAppBar*>());
            QVERIFY(!(dialog->windowFlags() & Qt::FramelessWindowHint));
            QCOMPARE(dialog->getCurrentColor().rgba(), QColor("#804fc3f7").rgba());
            auto* hex = dialog->findChild<QLineEdit*>(QStringLiteral("ColorDialogHexEdit")); QVERIFY(hex);
            hex->setFocus(); QTRY_VERIFY(hex->hasFocus());
            hex->selectAll(); QTest::keyClicks(hex, "#12abef");
            QCOMPARE(dialog->getCurrentColor().name(), QStringLiteral("#12abef"));
            QVERIFY(dialog->width() <= dialog->screen()->availableGeometry().width());
            QVERIFY(dialog->height() <= dialog->screen()->availableGeometry().height());
            savePreview(dialog, QStringLiteral("color-picker"));
            auto* accept = dialog->findChild<QPushButton*>(QStringLiteral("ColorDialogAccept")); QVERIFY(accept);
            QVERIFY(dialog->rect().contains(QRect(accept->mapTo(dialog, QPoint()), accept->size())));
            accept->click();
        });
        choose->click();
        QCOMPARE(editor->text(), QStringLiteral("#8012abef"));
        QCOMPARE(edited.count(), 1);
        QTest::keyClick(editor, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(editor->text(), QStringLiteral("#804fc3f7"));
        QTest::keyClick(editor, Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(editor->text(), QStringLiteral("#8012abef"));

        editor->setText("invalid color draft"); edited.clear();
        QTimer::singleShot(0, &host, [] {
            auto* dialog = qobject_cast<ElaColorDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
            QCOMPARE(dialog->getCurrentColor().toRgb(), QColor(Qt::black));
            dialog->setCurrentColor(Qt::red);
            QTest::keyClick(dialog, Qt::Key_Escape);
        });
        choose->click();
        QCOMPARE(editor->text(), QStringLiteral("invalid color draft"));
        QCOMPARE(edited.count(), 0);
        QTimer::singleShot(0, &host, [] {
            auto* dialog = qobject_cast<ElaColorDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
            dialog->setCurrentColor(QColor("#654321"));
            auto* accept = dialog->findChild<QPushButton*>(QStringLiteral("ColorDialogAccept")); QVERIFY(accept);
            accept->click();
        });
        choose->click();
        QCOMPARE(editor->text(), QStringLiteral("#654321"));
        QVERIFY(QColor(editor->text()).isValid());
        QCOMPARE(edited.count(), 1);
        QCOMPARE(qApp->palette(), palette); QCOMPARE(qApp->font(), font); QCOMPARE(qApp->styleSheet(), sheet);
    }

    void applicationThemesAndPreviews()
    {
        wave::ui::initializeApplicationTheme();
        wave::MainWindow window(wave::makeDemonstrationProject());
        auto* canvas = window.findChild<wave::WaveCanvas*>(); QVERIFY(canvas);
        const auto projectId = window.project().id;
        window.resize(1280, 800); window.show();
        for (const auto& mode : {QStringLiteral("light"), QStringLiteral("dark")}) {
            auto* action = window.findChild<QAction*>(QStringLiteral("Appearance-") + mode);
            QVERIFY(action); action->trigger(); QTest::qWait(30);
            QCOMPARE(qApp->property("waveworkbench.colorScheme").toString(), mode);
            QCOMPARE(window.project().id, projectId);
            QCOMPARE(window.findChild<wave::WaveCanvas*>(), canvas);
            savePreview(&window, QStringLiteral("workspace-") + mode);
            auto* fileMenu = window.menuBar()->actions().front()->menu(); QVERIFY(fileMenu);
            fileMenu->popup(window.mapToGlobal(QPoint(8, 24)));
            QTest::qWait(20);
            savePreview(fileMenu, QStringLiteral("file-menu-") + mode);
            fileMenu->hide();
            wave::ui::notify(&window, wave::ui::Notice::Success, QStringLiteral("Export complete"),
                QStringLiteral("Generated waveform artifacts in C:/workspace/exports"));
            savePreview(&window, QStringLiteral("notification-") + mode);
            wave::ui::dismissNotification(&window);

            bool inspectedLaneDialog = false;
            QTimer dialogGuard;
            dialogGuard.setSingleShot(true);
            connect(&dialogGuard, &QTimer::timeout, &window, [] {
                if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
            });
            dialogGuard.start(2000);
            QTimer::singleShot(0, &window, [&] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
                auto* edit = dialog->findChild<QLineEdit*>(QStringLiteral("LanePropertiesColorEdit")); QVERIFY(edit);
                auto* choose = dialog->findChild<QPushButton*>(QStringLiteral("LanePropertiesColorEditChooseButton"));
                QVERIFY(qobject_cast<ElaPushButton*>(choose));
                auto* form = qobject_cast<QFormLayout*>(dialog->layout()); QVERIFY(form);
                auto* caption = qobject_cast<ElaText*>(form->labelForField(edit->parentWidget())); QVERIFY(caption);
                QCOMPARE(caption->buddy(), edit->parentWidget());
                auto* error = dialog->findChild<QLabel*>(QStringLiteral("LanePropertiesError")); QVERIFY(error);
                QVERIFY(!qobject_cast<ElaText*>(error));
                savePreview(dialog, QStringLiteral("lane-properties-") + mode);
                inspectedLaneDialog = true;
                dialog->reject();
            });
            const auto beforeColor = wave::findLane(window.project().scenarios.front(), "lane-data")->color;
            window.openLanePropertiesPreview(QStringLiteral("lane-data"));
            dialogGuard.stop();
            QVERIFY(inspectedLaneDialog);
            QCOMPARE(wave::findLane(window.project().scenarios.front(), "lane-data")->color, beforeColor);

            wave::ui::Dialog gallery(&window);
            gallery.setWindowTitle(QStringLiteral("Parameter controls — ") + mode);
            auto* layout = new wave::ui::FormLayout(&gallery);
            auto* name = wave::ui::lineEdit(QStringLiteral("data_bus"), &gallery);
            auto* invalid = wave::ui::lineEdit(QStringLiteral("invalid tick"), &gallery);
            invalid->setProperty("invalidDraft", true);
            auto* value = wave::ui::spinBox(&gallery); value->setRange(1, 256); value->setValue(32);
            auto* kind = wave::ui::comboBox(&gallery); kind->addItems({"Bus", "Bit", "Clock"});
            auto* enabled = wave::ui::checkBox(QStringLiteral("Active lane"), &gallery); enabled->setChecked(true);
            layout->addRow(QStringLiteral("Signal name"), name);
            layout->addRow(QStringLiteral("Kind"), kind); layout->addRow(QStringLiteral("Width"), value);
            layout->addRow(QStringLiteral("Start tick"), invalid); layout->addRow(enabled);
            auto* error = new QLabel(QStringLiteral("Enter a valid time value; the draft is not applied."), &gallery);
            error->setProperty("waveState", QStringLiteral("error")); layout->addRow(error);
            layout->addRow(wave::ui::buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &gallery));
            gallery.resize(540, 350); gallery.show(); name->setFocus(); QTest::qWait(30);
            savePreview(&gallery, QStringLiteral("parameters-") + mode);
            std::unique_ptr<QMenu> popup(wave::ui::menu(&gallery));
            auto* checked = popup->addAction(QStringLiteral("&Snap to grid")); checked->setCheckable(true); checked->setChecked(true);
            popup->addAction(QStringLiteral("&Save\tCtrl+S"));
            popup->addAction(QStringLiteral("Unavailable action"))->setEnabled(false);
            wave::ui::addMenu(popup.get(), QStringLiteral("Selection"))->addAction(QStringLiteral("All lanes"));
            popup->popup(gallery.mapToGlobal(QPoint(20, 40))); QTest::qWait(30);
            savePreview(popup.get(), QStringLiteral("menu-") + mode);
            popup->hide(); gallery.hide();
        }
        window.findChild<QAction*>(QStringLiteral("Appearance-system"))->trigger();
        QCOMPARE(QSettings().value("appearance/colorScheme").toString(), QStringLiteral("system"));
    }

private:
    QTemporaryDir settings_;
};

QTEST_MAIN(ElaUiTest)
#include "ela_ui_test.moc"
