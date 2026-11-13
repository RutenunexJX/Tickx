#include "wave/widgets.h"
#include "wave/project_io.h"
#include "wave_canvas.h"
#include "main_window.h"
#include "ui_controls.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>
#include <stdexcept>

namespace {
char error[1024]{};
void checked(int code) { if (code) { qWarning("API error: %s", error); throw std::runtime_error(error); } }
void configure(QWidget* editor, const QJsonObject& config)
{
    const auto bytes = QJsonDocument(config).toJson(QJsonDocument::Compact);
    checked(wavewidgets_configure_wave_editor_v1(editor, bytes.constData(),
        static_cast<std::size_t>(bytes.size()), error, sizeof error));
}
using Getter = int (*)(QWidget*, char*, std::size_t, std::size_t*, char*, std::size_t) noexcept;
QByteArray get(QWidget* editor, Getter getter)
{
    std::size_t size = 0;
    checked(getter(editor, nullptr, 0, &size, error, sizeof error));
    QByteArray result(static_cast<qsizetype>(size), '\0');
    checked(getter(editor, result.data(), size, &size, error, sizeof error));
    result.chop(1);
    return result;
}
QByteArray snapshot(QWidget* editor) { return get(editor, wavewidgets_wave_editor_project_v1); }
QJsonObject state(QWidget* editor) { return QJsonDocument::fromJson(get(editor, wavewidgets_wave_editor_state_v1)).object(); }
wave::Project project(QWidget* editor) { return *wave::deserializeProject(snapshot(editor)).project; }
void action(QWidget* editor, const char* name)
{
    checked(wavewidgets_wave_editor_action_v1(editor, name, error, sizeof error));
}
void sync(QWidget* editor, const QByteArray& data)
{
    checked(wavewidgets_sync_wave_editor_v1(editor, data.constData(),
        static_cast<std::size_t>(data.size()), error, sizeof error));
}
std::unique_ptr<QWidget> create(const QByteArray& data = {})
{
    QWidget* result = nullptr;
    checked(wavewidgets_create_wave_editor_v1(data.isEmpty() ? nullptr : data.constData(),
        static_cast<std::size_t>(data.size()), QT_VERSION_STR, nullptr, &result, error, sizeof error));
    result->resize(760, 390);
    result->show();
    QTest::qWait(25);
    return std::unique_ptr<QWidget>(result);
}
wave::WaveCanvas* canvas(QWidget* editor) { return editor->findChild<wave::WaveCanvas*>(); }
void type(QLineEdit* edit, const QString& text)
{
    if (!edit) throw std::runtime_error("Missing inline editor");
    edit->setFocus();
    edit->selectAll();
    QTest::keyClicks(edit, text);
}
void add(QWidget* editor, const char* button, const QString& name, const QString& parameter = {})
{
    auto* control = editor->findChild<QToolButton*>(QString::fromLatin1(button));
    if (!control || !control->isVisible()) { qWarning() << "Missing add control" << button << (control ? control->geometry() : QRect{}); throw std::runtime_error("Missing visible add control"); }
    QTest::mouseClick(control, Qt::LeftButton);
    type(editor->findChild<QLineEdit*>("QuickLaneNameEdit"), name);
    auto* parameterEdit = editor->findChild<QLineEdit*>("QuickLaneParameterEdit");
    if (!parameter.isEmpty()) type(parameterEdit, parameter);
    QTest::keyClick(parameter.isEmpty() ? editor->findChild<QLineEdit*>("QuickLaneNameEdit") : parameterEdit, Qt::Key_Return);
    QTest::qWait(15);
}
QPoint wavePoint(wave::WaveCanvas* canvas, int lane, wave::Tick tick)
{
    const auto range = canvas->visibleTimeRange();
    const auto width = canvas->viewport()->width() - canvas->signalHeaderWidth();
    const auto x = canvas->signalHeaderWidth()
        + static_cast<int>((tick - range.first) * width / (range.second - range.first));
    return {x, 40 + lane * 56 + 28 - canvas->verticalScrollBar()->value()};
}
struct Events {
    int changed{0}, undo{0}, redo{0}, view{0};
    QWidget* editor{};
    QList<QByteArray> snapshots;
    static void callback(void* context, const char* data, std::size_t size) {
        auto& self = *static_cast<Events*>(context);
        const auto event = QJsonDocument::fromJson(QByteArray(data, static_cast<qsizetype>(size))).object()["event"].toString();
        if (event == "changed") {
            ++self.changed;
            if (self.editor) self.snapshots.append(snapshot(self.editor));
        }
        if (event == "undoRequested") ++self.undo;
        if (event == "redoRequested") ++self.redo;
        if (event == "viewChanged") ++self.view;
    }
};
void listen(QWidget* editor, Events& events)
{
    events.editor = editor;
    checked(wavewidgets_set_wave_editor_callback_v1(editor, Events::callback, &events, error, sizeof error));
}
void capture(QWidget* widget, const QString& name)
{
    const auto directory = qEnvironmentVariable("WAVE_EDITOR_EVIDENCE");
    if (directory.isEmpty()) return;
    QDir().mkpath(directory);
    if (!widget->grab().save(directory + "/" + name + ".png")) throw std::runtime_error("Could not save screenshot");
}
}

class WaveEditorTest final : public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
private slots:
    void initTestCase()
    {
        QVERIFY(settings_.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_.path());
        QCoreApplication::setOrganizationName("WaveEditorTests");
        QCoreApplication::setApplicationName("IsolatedEditor");
        wave::ui::initializeApplicationTheme();
    }
    void completeEditingAndPreview()
    {
        auto editor = create();
        Events events;
        listen(editor.get(), events);
        auto* view = canvas(editor.get());
        QCOMPARE(state(editor.get()).value("mode").toString(), QString("preview"));
        QVERIFY(view->isReadOnly());
        QVERIFY(wavewidgets_wave_editor_action_v1(editor.get(), "add-bit", error, sizeof error) != 0);
        configure(editor.get(), {{"mode", "edit"}});
        add(editor.get(), "CanvasAddClockButton", "clock", "10 ns");
        add(editor.get(), "CanvasAddBitButton", "enable");
        add(editor.get(), "CanvasAddBusButton", "data", "8");
        QCOMPARE(project(editor.get()).scenarios.front().lanes.size(), std::size_t(3));
        QCOMPARE(events.changed, 3);
        QVERIFY(view->setVisibleTimeRange(0, 40'000));
        QTest::qWait(25);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, wavePoint(view, 1, 7'000));
        QVERIFY(!project(editor.get()).scenarios.front().lanes[1].segments.empty());
        QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, wavePoint(view, 2, 7'000));
        auto* value = editor->findChild<QLineEdit*>("BusPresetValueEdit");
        QTRY_VERIFY(value->isVisible());
        type(value, "0x3a");
        QTest::keyClick(value, Qt::Key_Return);
        action(editor.get(), "commit");
        QVERIFY(!project(editor.get()).scenarios.front().lanes[2].segments.empty());
        const auto dataId = QString::fromStdString(project(editor.get()).scenarios.front().lanes[2].id);
        view->renameLaneRequested(dataId);
        type(editor->findChild<QLineEdit*>("LaneRenameEdit"), "payload");
        QTest::keyClick(editor->findChild<QLineEdit*>("LaneRenameEdit"), Qt::Key_Return);
        QCOMPARE(project(editor.get()).scenarios.front().lanes[2].name, std::string("payload"));
        view->goToTick(12'000);
        QTest::qWait(20);
        const auto beforeView = state(editor.get()).value("view");
        const auto content = snapshot(editor.get());
        const auto revision = state(editor.get()).value("revision");
        configure(editor.get(), {{"mode", "preview"}});
        QCOMPARE(canvas(editor.get()), view);
        QCOMPARE(state(editor.get()).value("view"), beforeView);
        QVERIFY(state(editor.get()).value("canUndo").toBool());
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, wavePoint(view, 1, 22'000));
        QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, wavePoint(view, 2, 25'000));
        QTest::keyClick(view, Qt::Key_Delete);
        QTest::keyClick(view, Qt::Key_1);
        QTest::keyClick(view, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(snapshot(editor.get()), content);
        QCOMPARE(state(editor.get()).value("revision"), revision);
        QVERIFY(view->cursorTick() >= 21'000);
        action(editor.get(), "zoom-in");
        QVERIFY(view->visibleTimeSpan() < 40'000);
        const auto scrollBeforePan = view->horizontalScrollBar()->value();
        const auto panPoint = QPoint(view->signalHeaderWidth() + 160, 80);
        QTest::mousePress(view->viewport(), Qt::MiddleButton, Qt::NoModifier, panPoint);
        QTest::mouseMove(view->viewport(), panPoint - QPoint(30, 0));
        QTest::mouseRelease(view->viewport(), Qt::MiddleButton, Qt::NoModifier, panPoint - QPoint(30, 0));
        QVERIFY(view->horizontalScrollBar()->value() > scrollBeforePan);
        QCOMPARE(snapshot(editor.get()), content);
        capture(editor.get(), "embedded-preview");
        configure(editor.get(), {{"mode", "edit"}});
        action(editor.get(), "undo");
        QVERIFY(snapshot(editor.get()) != content);
        action(editor.get(), "redo");
        QCOMPARE(snapshot(editor.get()), content);
        auto reopened = create(content);
        QCOMPARE(snapshot(reopened.get()), content);
        configure(reopened.get(), {{"view", beforeView.toObject()}});
        QTest::qWait(20);
        QCOMPARE(state(reopened.get()).value("view").toObject().value("cursorTick"),
            beforeView.toObject().value("cursorTick"));
        QCOMPARE(state(reopened.get()).value("view").toObject().value("selectedLaneIds"),
            beforeView.toObject().value("selectedLaneIds"));
        qInfo().noquote() << "GUI platform" << QGuiApplication::platformName()
            << "DPR" << editor->devicePixelRatioF() << "font" << view->font().toString()
            << "viewport" << view->viewport()->size();
    }
    void invalidInputAndAtomicConfiguration()
    {
        auto editor = create();
        Events events;
        listen(editor.get(), events);
        configure(editor.get(), {{"mode", "edit"}});
        action(editor.get(), "add-bus");
        type(editor->findChild<QLineEdit*>("QuickLaneParameterEdit"), "0");
        const auto preview = QByteArray(R"({"mode":"preview"})");
        QVERIFY(wavewidgets_configure_wave_editor_v1(editor.get(), preview.constData(),
            static_cast<std::size_t>(preview.size()), error, sizeof error) != 0);
        QVERIFY(canvas(editor.get())->hasQuickLaneSetup());
        QCOMPARE(state(editor.get()).value("mode").toString(), QString("edit"));
        QCOMPARE(events.changed, 0);
        std::size_t size = 0;
        QVERIFY(wavewidgets_wave_editor_project_v1(editor.get(), nullptr, 0, &size, error, sizeof error) != 0);
        QVERIFY(editor->findChild<QLineEdit*>("QuickLaneParameterEdit")->hasFocus());
        type(editor->findChild<QLineEdit*>("QuickLaneParameterEdit"), "16");
        action(editor.get(), "commit");
        QCOMPARE(project(editor.get()).scenarios.front().lanes.front().width, std::uint32_t(16));
        QCOMPARE(events.changed, 1);
        const auto before = state(editor.get());
        const auto invalid = QByteArray(R"({"mode":"preview","view":{"startTick":"9","endTick":"2"}})");
        QVERIFY(wavewidgets_configure_wave_editor_v1(editor.get(), invalid.constData(),
            static_cast<std::size_t>(invalid.size()), error, sizeof error) != 0);
        QCOMPARE(state(editor.get()).value("mode"), before["mode"]);
        QVERIFY(!canvas(editor.get())->isReadOnly());
        action(editor.get(), "add-bit");
        QTest::keyClick(editor->findChild<QLineEdit*>("QuickLaneNameEdit"), Qt::Key_Escape);
        QCOMPARE(project(editor.get()).scenarios.front().lanes.size(), std::size_t(1));
        QCOMPARE(events.changed, 1);
    }
    void hostHistoryAndMultiplePresentations()
    {
        auto first = create();
        configure(first.get(), {{"mode", "edit"}, {"historyOwner", "host"}});
        Events events;
        listen(first.get(), events);
        const auto empty = snapshot(first.get());
        auto second = create(empty);
        configure(second.get(), {{"historyOwner", "host"}});
        add(first.get(), "CanvasAddBitButton", "bit");
        const auto withBit = snapshot(first.get());
        QCOMPARE(events.changed, 1);
        QCOMPARE(events.snapshots.size(), 1);
        sync(first.get(), withBit);
        sync(second.get(), withBit);
        QCOMPARE(events.changed, 1);
        QCOMPARE(snapshot(second.get()), withBit);
        configure(first.get(), {{"canUndo", true}});
        action(first.get(), "undo");
        QCOMPARE(events.undo, 1);
        QCOMPARE(snapshot(first.get()), withBit);
        auto* originalCanvas = canvas(first.get());
        originalCanvas->setVisibleTimeRange(20'000, 70'000);
        originalCanvas->goToTick(35'000);
        QTest::qWait(20);
        const auto oldView = state(first.get()).value("view").toObject();
        sync(first.get(), empty);
        sync(second.get(), empty);
        QCOMPARE(canvas(first.get()), originalCanvas);
        QCOMPARE(snapshot(first.get()), empty);
        QCOMPARE(snapshot(second.get()), empty);
        QCOMPARE(events.changed, 1);
        QCOMPARE(state(first.get()).value("view").toObject()["cursorTick"], oldView["cursorTick"]);
        configure(first.get(), {{"canUndo", false}, {"canRedo", true}});
        action(first.get(), "redo");
        QCOMPARE(events.redo, 1);
        sync(first.get(), withBit);
        QCOMPARE(snapshot(first.get()), withBit);
        auto mismatch = project(first.get());
        mismatch.id = "different-block";
        const auto invalid = wave::serializeProject(mismatch);
        QVERIFY(wavewidgets_sync_wave_editor_v1(first.get(), invalid.constData(),
            static_cast<std::size_t>(invalid.size()), error, sizeof error) != 0);
        QCOMPARE(snapshot(first.get()), withBit);
    }
    void reparentAndComponentHistory()
    {
        auto editor = create();
        configure(editor.get(), {{"mode", "edit"}});
        add(editor.get(), "CanvasAddBitButton", "bit");
        const auto initial = snapshot(editor.get());
        auto next = project(editor.get());
        next.name = "External metadata";
        const auto laneId = QString::fromStdString(next.scenarios.front().lanes.front().id);
        canvas(editor.get())->selectLaneHeaders({laneId}, laneId);
        const auto range = std::pair<wave::Tick, wave::Tick>{0, 10'000};
        QVERIFY(canvas(editor.get())->setExplicitTimeRange(range));
        sync(editor.get(), wave::serializeProject(next));
        QCOMPARE(canvas(editor.get())->selectedTimeRange(), std::optional{range});
        action(editor.get(), "undo");
        QCOMPARE(snapshot(editor.get()), initial);
        action(editor.get(), "undo");
        QVERIFY(project(editor.get()).scenarios.front().lanes.empty());
        action(editor.get(), "redo");
        QVERIFY(state(editor.get()).value("canRedo").toBool());
        action(editor.get(), "add-bit");
        QTest::keyClick(editor->findChild<QLineEdit*>("QuickLaneNameEdit"), Qt::Key_Escape);
        QVERIFY(state(editor.get()).value("canRedo").toBool());
        QCOMPARE(snapshot(editor.get()), initial);
        action(editor.get(), "redo");
        QCOMPARE(project(editor.get()).name, std::string("External metadata"));
        action(editor.get(), "undo");
        auto* originalCanvas = canvas(editor.get());
        QWidget owner;
        auto* layout = new QVBoxLayout(&owner);
        editor->setParent(&owner);
        layout->addWidget(editor.get());
        owner.resize(820, 420);
        owner.show();
        editor->show();
        QTest::qWait(30);
        QCOMPARE(canvas(editor.get()), originalCanvas);
        QCOMPARE(snapshot(editor.get()), initial);
        QVERIFY(state(editor.get()).value("canUndo").toBool());
        editor->setParent(nullptr);
        editor->show();
        owner.close();
    }
    void propertiesOrderingDeletionAndClock()
    {
        auto editor = create();
        configure(editor.get(), {{"mode", "edit"}});
        add(editor.get(), "CanvasAddClockButton", "clk", "10 ns");
        add(editor.get(), "CanvasAddBusButton", "bus", "8");
        auto* view = canvas(editor.get());
        const auto dataId = QString::fromStdString(project(editor.get()).scenarios.front().lanes[1].id);
        view->editLaneParametersRequested(dataId, editor->mapToGlobal(QPoint(80, 80)));
        QTest::qWait(10);
        auto* properties = editor->findChild<QAction*>("waveEditorProperties");
        QVERIFY(properties);
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            type(dialog->findChild<QLineEdit*>("LanePropertiesWidthEdit"), "16");
            QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok), Qt::LeftButton);
        });
        properties->trigger();
        QCOMPARE(project(editor.get()).scenarios.front().lanes[1].width, std::uint32_t(16));
        view->editLaneParametersRequested(dataId, editor->mapToGlobal(QPoint(80, 80)));
        QTest::qWait(10);
        const auto moveActions = editor->findChildren<QAction*>("waveEditorMoveUp");
        QVERIFY(!moveActions.isEmpty());
        moveActions.last()->trigger();
        QCOMPARE(project(editor.get()).scenarios.front().lanes[0].id, dataId.toStdString());
        view->removeLaneRequested(dataId);
        QCOMPARE(project(editor.get()).scenarios.front().lanes.size(), std::size_t(1));
        action(editor.get(), "undo");
        QCOMPARE(project(editor.get()).scenarios.front().lanes.size(), std::size_t(2));
        const auto clockId = QString::fromStdString(project(editor.get()).scenarios.front().lanes[1].id);
        view->editLaneParametersRequested(clockId, editor->mapToGlobal(QPoint(80, 80)));
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            type(dialog->findChild<QLineEdit*>("ClockPhaseEdit"), "2 ns");
            type(dialog->findChild<QLineEdit*>("ClockDutyNumeratorEdit"), "1");
            type(dialog->findChild<QLineEdit*>("ClockDutyDenominatorEdit"), "3");
            QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok), Qt::LeftButton);
        });
        auto* clockAction = editor->findChild<QAction*>("waveEditorClockParameters");
        QVERIFY(clockAction);
        clockAction->trigger();
        QCOMPARE(project(editor.get()).clockDomains.front().phase, wave::Tick(2'000));
        QCOMPARE(project(editor.get()).clockDomains.front().dutyCycle.denominator, std::int64_t(3));
    }
    void nativeGeometryAndRenderingParity()
    {
        auto editor = create();
        configure(editor.get(), {{"mode", "edit"}});
        add(editor.get(), "CanvasAddClockButton", "clk", "10 ns");
        add(editor.get(), "CanvasAddBitButton", "bit");
        add(editor.get(), "CanvasAddBusButton", "bus", "8");
        auto same = project(editor.get());
        wave::setSegmentRange(same.scenarios.front().lanes[1], 0, 10'000, "0");
        wave::setSegmentRange(same.scenarios.front().lanes[1], 10'000, 20'000, "1");
        wave::setSegmentRange(same.scenarios.front().lanes[2], 0, 20'000, "0x5a");
        sync(editor.get(), wave::serializeProject(same));
        configure(editor.get(), {{"mode", "preview"}});
        auto* embedded = canvas(editor.get());
        wave::MainWindow standalone(same);
        standalone.show();
        auto* native = standalone.findChild<wave::WaveCanvas*>();
        QVERIFY(native);
        embedded->setFixedSize(680, 280);
        native->setFixedSize(680, 280);
        native->setFont(embedded->font());
        native->setPalette(embedded->palette());
        native->setSignalHeaderWidth(embedded->signalHeaderWidth());
        native->goToTick(12'000);
        embedded->goToTick(12'000);
        const auto selected = QString::fromStdString(same.scenarios.front().lanes.front().id);
        native->selectLaneHeaders({selected}, selected);
        embedded->selectLaneHeaders({selected}, selected);
        QVERIFY(native->setReadOnly(true));
        QVERIFY(embedded->setVisibleTimeRange(0, 40'000));
        QVERIFY(native->setVisibleTimeRange(0, 40'000));
        QTest::qWait(80);
        QCOMPARE(native->devicePixelRatioF(), embedded->devicePixelRatioF());
        QCOMPARE(native->viewport()->size(), embedded->viewport()->size());
        QCOMPARE(native->visibleTimeRange(), embedded->visibleTimeRange());
        const auto a = embedded->viewport()->grab().toImage().convertToFormat(QImage::Format_ARGB32);
        const auto b = native->viewport()->grab().toImage().convertToFormat(QImage::Format_ARGB32);
        QCOMPARE(a.size(), b.size());
        const auto dpr = embedded->devicePixelRatioF();
        const auto xStart = static_cast<int>((embedded->signalHeaderWidth() + 3) * dpr);
        int different = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = xStart; x < a.width(); ++x)
                if (a.pixel(x, y) != b.pixel(x, y)) ++different;
        capture(embedded->viewport(), "embedded-canvas");
        capture(native->viewport(), "standalone-canvas");
        qInfo() << "Measured DPR" << dpr << "canvas" << embedded->viewport()->size()
            << "physical image" << a.size() << "different waveform pixels" << different;
        QCOMPARE(different, 0);
        embedded->setMinimumSize(0, 0);
        embedded->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        standalone.hide();
        configure(editor.get(), {{"mode", "edit"}});
        for (const int width : {340, 760, 420, 800}) {
            editor->resize(width, editor->sizeHint().height());
            QTest::qWait(20);
            QVERIFY(embedded->viewport()->width() > embedded->signalHeaderWidth());
            auto* addButton = editor->findChild<QToolButton*>("CanvasAddBitButton");
            QVERIFY(addButton->isVisible());
            QVERIFY(embedded->viewport()->rect().contains(addButton->geometry()));
        }
        capture(editor.get(), "embedded-resized");
    }
};

QTEST_MAIN(WaveEditorTest)
#include "wave_editor_test.moc"
