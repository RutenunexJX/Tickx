#include "ui_controls.h"
#include "main_window.h"
#include "wave_canvas.h"
#include "wave/project_io.h"
#include "ElaComboBox.h"
#include "ElaDrawerArea.h"
#include "ElaMenu.h"
#include "ElaListView.h"
#include "ElaTreeView.h"
#include "ElaTableView.h"
#include "ElaScrollBar.h"
#include "ElaTabBar.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPointer>
#include <QPropertyAnimation>
#include <QScopeGuard>
#include <QScreen>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QScrollBar>
#include <QSplitter>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTest>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidgetAction>
#include <algorithm>
#include <ctime>
#include <memory>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
class HiddenNativeWindows final : public QObject {
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Polish)
            if (auto* widget = qobject_cast<QWidget*>(object); widget && widget->isWindow())
                widget->setAttribute(Qt::WA_DontShowOnScreen);
        return false;
    }
};
double processCpuMs()
{
#ifdef Q_OS_WIN
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) return -1;
    const auto ticks = [](FILETIME time) {
        return (quint64(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    return double(ticks(kernel) + ticks(user)) / 10000.0;
#else
    return 1000.0 * std::clock() / CLOCKS_PER_SEC;
#endif
}
class EventCounter final : public QObject {
public:
    explicit EventCounter(QWidget* root) : root_(root) { qApp->installEventFilter(this); }
    ~EventCounter() override { qApp->removeEventFilter(this); }
    int paints{}, layouts{};
    bool eventFilter(QObject* object, QEvent* event) override
    {
        auto* widget = qobject_cast<QWidget*>(object);
        if (widget && (widget == root_ || root_->isAncestorOf(widget))) {
            paints += event->type() == QEvent::Paint;
            layouts += event->type() == QEvent::LayoutRequest;
        }
        return false;
    }
private:
    QWidget* root_;
};
void wheel(QWidget* target, int delta, QPoint pixels = {})
{
    const QPoint position = target->rect().center();
    QWheelEvent event(position, target->mapToGlobal(position), pixels, QPoint(0, delta),
        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(target, &event);
}
}

class UiCapabilitiesTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
    HiddenNativeWindows hidden_;
private slots:
    void initTestCase()
    {
        if (QGuiApplication::platformName() == "windows") qApp->installEventFilter(&hidden_);
        QVERIFY(settings_.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_.path());
        qApp->setOrganizationName("WaveCapabilityTests");
        qApp->setApplicationName("IsolatedUi");
        wave::ui::initializeApplicationTheme();
    }
    void interruptibleCombo()
    {
        for (int iteration = 0; iteration < 15; ++iteration) {
            auto host = std::make_unique<QWidget>();
            auto* layout = new QVBoxLayout(host.get());
            auto* combo = wave::ui::comboBox(host.get());
            auto* native = qobject_cast<ElaComboBox*>(combo);
            QVERIFY(native);
            for (int row = 0; row < 100; ++row) combo->addItem(QString::number(row));
            layout->addWidget(combo); host->resize(300, 100); host->show();
            combo->showPopup(); QVERIFY(native->isPopupAnimating());
            const auto animationCount = combo->findChildren<QPropertyAnimation*>().size();
            QTest::keyClick(combo->view(), Qt::Key_Home);
            QVERIFY(!native->isPopupAnimating());
            QTest::keyClick(combo->view(), Qt::Key_Down);
            QTest::keyClick(combo->view(), Qt::Key_Return);
            QCOMPARE(combo->currentIndex(), 1);
            QVERIFY(!combo->view()->isVisible());
            combo->showPopup(); combo->hidePopup(); combo->showPopup();
            QCOMPARE(combo->findChildren<QPropertyAnimation*>().size(), animationCount);
            combo->resize(combo->width() + 10, combo->height());
            QVERIFY(!native->isPopupAnimating());
            combo->hidePopup(); combo->showPopup();
            const QPointer<QAbstractItemView> popup = combo->view();
            host.reset(); QVERIFY(popup.isNull());
        }
    }
    void comboPopupRowsFit_data()
    {
        QTest::addColumn<int>("rows");
        QTest::addColumn<bool>("reducedMotion");
        QTest::addColumn<bool>("nearBottom");
        for (int rows : {1, 3, 5}) for (bool reduced : {false, true})
            for (bool bottom : {false, true}) {
                const auto name = QString("%1-%2-%3").arg(rows)
                    .arg(reduced ? "reduced" : "animated").arg(bottom ? "bottom" : "top");
                QTest::newRow(qPrintable(name)) << rows << reduced << bottom;
            }
    }
    void comboPopupRowsFit()
    {
        QFETCH(int, rows);
        QFETCH(bool, reducedMotion);
        QFETCH(bool, nearBottom);
        const auto previousMotion = qApp->property("waveworkbench.reducedMotion");
        const auto restoreMotion = qScopeGuard([&] {
            qApp->setProperty("waveworkbench.reducedMotion", previousMotion);
        });
        qApp->setProperty("waveworkbench.reducedMotion", reducedMotion);
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        auto* combo = wave::ui::comboBox(&host);
        auto* native = qobject_cast<ElaComboBox*>(combo);
        QVERIFY(native);
        layout->addWidget(combo);
        for (int row = 0; row < rows; ++row) combo->addItem(QString("Option %1").arg(row));
        host.resize(320, 90);
        const QRect available = host.screen()->availableGeometry();
        host.move(available.left() + 60, nearBottom ? available.bottom() - 100 : available.top() + 60);
        host.show(); QTest::qWait(20);
        QSize stableSize;
        for (int cycle = 0; cycle < 4; ++cycle) {
            combo->showPopup();
            if (cycle == 0) QTRY_VERIFY_WITH_TIMEOUT(!native->isPopupAnimating(), 1000);
            else native->finishPopupAnimation();
            QCoreApplication::processEvents();
            auto* view = combo->view();
            auto* popup = view->window();
            QVERIFY(view->isVisible());
            if (cycle == 0) stableSize = popup->size();
            QCOMPARE(popup->size(), stableSize);
            QVERIFY(available.contains(popup->geometry()));
            for (int row = 0; row < rows; ++row) {
                const QRect item = view->visualRect(combo->model()->index(row, 0));
                QVERIFY2(view->viewport()->rect().contains(item),
                    qPrintable(QString("row=%1 item=%2,%3 %4x%5 viewport=%6x%7")
                        .arg(row).arg(item.x()).arg(item.y()).arg(item.width()).arg(item.height())
                        .arg(view->viewport()->width()).arg(view->viewport()->height())));
            }
            const QRect stableGeometry = popup->geometry();
            for (int repeat = 0; repeat < 5; ++repeat) {
                combo->showPopup();
                QVERIFY(!native->isPopupAnimating());
                QCOMPARE(popup->geometry(), stableGeometry);
            }
            QTest::keyClick(view, cycle % 2 ? Qt::Key_End : Qt::Key_Home);
            QTest::keyClick(view, Qt::Key_Return);
            QCOMPARE(combo->currentIndex(), cycle % 2 ? rows - 1 : 0);
            QVERIFY(!view->isVisible());
        }
    }
    void focusAnimationsAreOwned()
    {
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        const QList<QWidget*> fields{wave::ui::lineEdit(&host), wave::ui::spinBox(&host),
            wave::ui::doubleSpinBox(&host)};
        for (auto* field : fields) layout->addWidget(field);
        host.show(); QTest::qWait(20);
        const auto count = host.findChildren<QPropertyAnimation*>().size();
        for (int i = 0; i < 30; ++i) for (auto* field : fields) {
            QFocusEvent in(QEvent::FocusIn, Qt::MouseFocusReason);
            QFocusEvent out(QEvent::FocusOut, Qt::TabFocusReason);
            QApplication::sendEvent(field, &in);
            QApplication::sendEvent(field, &out);
        }
        QCOMPARE(host.findChildren<QPropertyAnimation*>().size(), count);
    }
    void focusedViewDestruction()
    {
        QStandardItemModel model(10, 2);
        for (int i = 0; i < 12; ++i) {
            auto host = std::make_unique<QWidget>();
            auto* layout = new QVBoxLayout(host.get());
            const QList<QAbstractItemView*> views{new ElaListView(host.get()),
                new ElaTreeView(host.get()), new ElaTableView(host.get())};
            for (auto* view : views) { view->setModel(&model); layout->addWidget(view); }
            host->show();
            for (auto* view : views) {
                view->setCurrentIndex(model.index(0, 0));
                view->setFocus(Qt::OtherFocusReason);
                const QPointer<QAbstractItemView> watched = view;
                delete view; QVERIFY(watched.isNull());
            }
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void browsingScrollAndExactTimeline()
    {
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        auto* tree = wave::ui::tree(&host); layout->addWidget(tree);
        for (int row = 0; row < 100; ++row) new QTreeWidgetItem(tree, {QString::number(row)});
        host.resize(400, 240); host.show(); QTest::qWait(20);
        auto* scroll = qobject_cast<ElaScrollBar*>(tree->verticalScrollBar());
        QVERIFY(scroll && scroll->smoothWheelEnabled());
        QVERIFY(tree->isAnimated());
        wheel(scroll, -120); QTest::qWait(45); QVERIFY(scroll->value() > 0);
        scroll->setValue(310); QTest::qWait(200); QCOMPARE(scroll->value(), 310);
        wheel(tree->viewport(), 0, QPoint(0, -17)); QCOMPARE(scroll->value(), 327);
        wheel(scroll, -120); tree->hide();
        const int stopped = scroll->value(); QTest::qWait(180); QCOMPARE(scroll->value(), stopped);
        tree->show();
        auto* parent = tree->topLevelItem(0);
        new QTreeWidgetItem(parent, {QStringLiteral("child")});
        tree->setCurrentItem(parent); tree->scrollToItem(parent); parent->setExpanded(true);
        QTest::keyClick(tree, Qt::Key_Down);
        QCOMPARE(tree->currentItem(), parent->child(0));

        wave::WaveCanvas canvas;
        auto* precise = qobject_cast<ElaScrollBar*>(canvas.horizontalScrollBar());
        QVERIFY(precise); QVERIFY(!precise->smoothWheelEnabled());
        precise->setRange(0, 10000); precise->setSingleStep(5); precise->setPageStep(100);
        wheel(precise, -120); const int immediate = precise->value(); QVERIFY(immediate > 0);
        QTest::qWait(180); QCOMPARE(precise->value(), immediate);
    }
    void replacedOverlayScrollBarOrigin()
    {
        QListView view;
        auto* origin = new QScrollBar(Qt::Vertical, &view);
        view.setVerticalScrollBar(origin);
        auto* overlay = new ElaScrollBar(origin, &view);
        origin->setRange(0, 1000);
        view.show();
        view.setVerticalScrollBar(new QScrollBar(Qt::Vertical, &view));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(overlay->isHidden());
        for (int i = 0; i < 10; ++i) {
            view.resize(300 + i, 200);
            QVERIFY(!view.grab().isNull());
        }
    }
    void tabsKeepQtPageOwnership()
    {
        auto tabs = std::unique_ptr<QTabWidget>(wave::ui::tabs());
        auto* bar = qobject_cast<ElaTabBar*>(tabs->tabBar());
        QVERIFY(bar && bar->nativeTabBehavior() && bar->smoothScrollEnabled());
        QVERIFY(!bar->tabsClosable()); QVERIFY(!bar->isMovable());
        for (int i = 0; i < 12; ++i) tabs->addTab(new QWidget, QStringLiteral("Review page %1").arg(i));
        tabs->resize(420, 200); tabs->show(); QTest::qWait(30);
        wheel(bar, 0, QPoint(-72, 0));
        QVERIFY(bar->tabRect(0).left() < 0);
        tabs->setCurrentIndex(11); QCOMPARE(tabs->currentIndex(), 11);
        QVERIFY(bar->tabRect(11).intersects(bar->rect()));
        const QPointer<QWidget> page = tabs->widget(3);
        tabs->removeTab(3); QVERIFY(page); QCOMPARE(tabs->count(), 11);
        tabs.reset(); QVERIFY(page.isNull());
    }
    void menuKeepsActionsAndLiveEditors()
    {
        QWidget host; host.resize(400, 300); host.show();
        auto* menu = qobject_cast<ElaMenu*>(wave::ui::menu(&host)); QVERIFY(menu);
        auto* disabled = menu->addAction("Unavailable"); disabled->setEnabled(false);
        auto* action = menu->addAction("Enabled"); action->setCheckable(true);
        QSignalSpy triggered(action, &QAction::triggered);
        auto* child = wave::ui::addMenu(menu, "Child"); child->addAction("Leaf");
        for (int i = 0; i < 12; ++i) {
            menu->popup(host.mapToGlobal(QPoint(10, 10)));
            QVERIFY(menu->isPopupAnimating());
            menu->setActiveAction(action); QTest::keyClick(menu, Qt::Key_Return);
            QVERIFY(!menu->isVisible()); QVERIFY(!menu->isPopupAnimating());
        }
        QCOMPARE(triggered.count(), 12); QVERIFY(!action->isChecked());
        menu->popup(host.mapToGlobal(QPoint(10, 10))); action->setEnabled(false);
        QVERIFY(!menu->isPopupAnimating()); menu->hide(); action->setEnabled(true);
        menu->popup(host.mapToGlobal(QPoint(10, 10)));
        menu->setActiveAction(child->menuAction()); QTest::keyClick(menu, Qt::Key_Right);
        QTRY_VERIFY(child->isVisible()); QTest::keyClick(child, Qt::Key_Escape); menu->hide();
        menu->popup(host.mapToGlobal(QPoint(10, 10)));
        QTest::qWait(200); QVERIFY(!menu->isPopupAnimating()); menu->hide();
        auto* widgetAction = new QWidgetAction(menu);
        auto* editor = wave::ui::lineEdit(); widgetAction->setDefaultWidget(editor); menu->addAction(widgetAction);
        menu->popup(host.mapToGlobal(QPoint(10, 10)));
        QVERIFY(!menu->isPopupAnimating()); QTest::keyClicks(editor, "1010");
        QCOMPARE(editor->text(), QStringLiteral("1010")); menu->hide();
    }
    void simulationDrawersPreserveData()
    {
        QSettings{}.remove("waveWorkbench/simulation-layout/v1");
        wave::MainWindow window(wave::makeDemonstrationProject(), {}, nullptr, {}, true);
        window.show(); QTest::qWait(150);
        const auto serialized = wave::serializeProject(window.project());
        auto* panel = window.findChild<ElaDrawerArea*>("SimulationActualPanel");
        auto* split = window.findChild<QSplitter*>("SimulationResultSplitter");
        auto* canvas = window.findChild<wave::WaveCanvas*>();
        QVERIFY(panel && split && canvas); QVERIFY(panel->getIsExpand());
        const QPointer<wave::WaveCanvas> original = canvas;
        EventCounter counter(&window);
        qint64 peak = 0; double preparation = 0; double dispatch = 0;
        for (int i = 0; i < 8; ++i) {
            QElapsedTimer call; call.start(); panel->setExpanded(false);
            dispatch = std::max(dispatch, call.nsecsElapsed() / 1e6);
            peak = std::max(peak, panel->drawerSnapshotBytes());
            preparation = std::max(preparation, panel->drawerPreparationMs());
            QTest::qWait(25); panel->setExpanded(true); QTest::qWait(25);
        }
        panel->setExpanded(false); panel->finishDrawerAnimation();
        QTest::qWait(20); QVERIFY(!panel->getIsExpand());
        QCOMPARE(panel->maximumHeight(), panel->getHeaderHeight());
        auto* label = panel->findChild<QLabel*>("SimulationActualPanelLabel"); QVERIFY(label);
        QTest::keyClick(label->parentWidget(), Qt::Key_Space); QVERIFY(panel->getIsExpand());
        panel->finishDrawerAnimation(); QCOMPARE(panel->drawerSnapshotBytes(), 0);
        panel->setExpanded(false); window.hide();
        QVERIFY(!panel->isDrawerAnimating()); QCOMPARE(panel->drawerSnapshotBytes(), 0);
        window.show(); panel->setExpanded(true, false);
        QVERIFY(original); QCOMPARE(original.data(), window.findChild<wave::WaveCanvas*>());
        QCOMPARE(wave::serializeProject(window.project()), serialized);
        QVERIFY(peak > 0 && peak <= 32 * 1024 * 1024);
        qInfo().noquote() << "DRAWER_PERFORMANCE" << QJsonDocument(QJsonObject{
            {"peakSnapshotBytes", peak}, {"preparationMaxMs", preparation},
            {"dispatchMaxMs", dispatch}, {"paints", counter.paints},
            {"layoutRequests", counter.layouts}}).toJson(QJsonDocument::Compact);
        const auto oldMotion = qApp->property("waveworkbench.reducedMotion");
        const auto restore = qScopeGuard([oldMotion] { qApp->setProperty("waveworkbench.reducedMotion", oldMotion); });
        qApp->setProperty("waveworkbench.reducedMotion", true);
        panel->setExpanded(false); QVERIFY(!panel->isDrawerAnimating());
        QCOMPARE(panel->drawerSnapshotBytes(), 0);
        QMetaObject::invokeMethod(split, "splitterMoved", Q_ARG(int, 350), Q_ARG(int, 1));
        const auto state = split->saveState();
        wave::MainWindow restored(wave::makeDemonstrationProject(), {}, nullptr, {}, true);
        restored.show(); QTest::qWait(50);
        QVERIFY(!restored.findChild<ElaDrawerArea*>("SimulationActualPanel")->getIsExpand());
        QVERIFY(!QSettings{}.value("waveWorkbench/simulation-layout/v1/SimulationResultSplitter").toByteArray().isEmpty());
        Q_UNUSED(state);
    }
    void browsingPerformance()
    {
        QMainWindow window;
        wave::ui::prepareWindow(&window);
        auto* body = new QWidget(&window);
        window.setCentralWidget(body);
        auto* layout = new QVBoxLayout(body);
        auto* combo = wave::ui::comboBox(body);
        for (int i = 0; i < 100; ++i) combo->addItem(QStringLiteral("Signal %1").arg(i));
        layout->addWidget(combo);
        auto* split = new QSplitter(body);
        layout->addWidget(split);
        auto* tree = wave::ui::tree(split);
        tree->setHeaderLabels({QStringLiteral("Simulation signals")});
        for (int group = 0; group < 64; ++group) {
            auto* parent = new QTreeWidgetItem(tree, {QStringLiteral("Module %1").arg(group)});
            for (int row = 0; row < 32; ++row)
                new QTreeWidgetItem(parent, {QStringLiteral("signal_%1[31:0]").arg(row)});
            parent->setExpanded(true);
        }
        auto* tabs = wave::ui::tabs(split);
        auto* table = wave::ui::table(tabs);
        table->setColumnCount(4); table->setRowCount(512);
        for (int row = 0; row < 512; ++row)
            for (int col = 0; col < 4; ++col)
                table->setItem(row, col, new QTableWidgetItem(QString::number(row * 4 + col)));
        tabs->addTab(table, QStringLiteral("Expected / Actual"));
        tabs->addTab(new QWidget, QStringLiteral("Checks"));
        tabs->addTab(new QWidget, QStringLiteral("Batch"));
        window.resize(1200, 800); window.show();
        QTest::qWait(250);
        EventCounter counter(&window);
        QElapsedTimer elapsed; elapsed.start();
        const auto cpuStart = processCpuMs();
        QList<double> dispatch;
        for (int i = 0; i < 12; ++i) {
            QElapsedTimer call; call.start();
            tabs->setCurrentIndex(i % 3);
            wheel(tree->verticalScrollBar(), -120);
            combo->showPopup();
            dispatch.append(call.nsecsElapsed() / 1e6);
            QTest::qWait(220);
            combo->hidePopup();
        }
        const double cpuMs = processCpuMs() - cpuStart;
        const double elapsedMs = elapsed.nsecsElapsed() / 1e6;
        std::sort(dispatch.begin(), dispatch.end());
        const QJsonObject record{
            {"window", "1200x800"}, {"dpr", window.devicePixelRatioF()},
            {"platform", QGuiApplication::platformName()}, {"iterations", 12},
            {"treeRows", 2112}, {"tableCells", 2048},
            {"dispatchMedianMs", dispatch.at(6)}, {"dispatchMaxMs", dispatch.back()},
            {"elapsedMs", elapsedMs}, {"processCpuMs", cpuMs},
            {"paints", counter.paints}, {"layoutRequests", counter.layouts}};
        qInfo().noquote() << "BROWSING_PERFORMANCE" << QJsonDocument(record).toJson(QJsonDocument::Compact);
        QCOMPARE(tree->topLevelItemCount(), 64);
        QCOMPARE(table->item(511, 3)->text(), QStringLiteral("2047"));
        QVERIFY(tree->verticalScrollBar()->value() > 0);
        QVERIFY(counter.paints < 10000);
        QVERIFY(counter.layouts < 10000);
    }
};
QTEST_MAIN(UiCapabilitiesTest)
#include "ui_capabilities_test.moc"
