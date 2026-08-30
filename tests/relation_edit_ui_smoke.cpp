#include "main_window.h"
#include "wave_canvas.h"

#include "wave/model.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>

#include <algorithm>
#include <array>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] wave::Project makeProject()
{
    wave::Project project;
    project.id = "relation-edit-project";
    project.name = "Relation edit smoke";

    wave::Scenario scenario;
    scenario.id = "relation-edit-scenario";
    scenario.name = "Relation edit";
    scenario.duration = 240'000;
    constexpr std::array<wave::Tick, 8> EventTicks{
        20'000,
        60'000,
        40'000,
        150'000,
        180'000,
        100'000,
        30'000,
        210'000,
    };
    for (int laneIndex = 0; laneIndex < 8; ++laneIndex) {
        wave::Lane lane;
        lane.id = "lane-" + std::to_string(laneIndex);
        lane.name = "Lane " + std::to_string(laneIndex);
        lane.kind = wave::LaneKind::Bit;
        lane.height = 48;
        lane.segments.push_back({
            "segment-" + std::to_string(laneIndex),
            0,
            scenario.duration,
            laneIndex % 2 == 0 ? "0" : "1",
            {},
        });
        scenario.lanes.push_back(std::move(lane));

        wave::Event event;
        event.id = "event-" + std::to_string(laneIndex);
        event.laneId = "lane-" + std::to_string(laneIndex);
        event.tick = EventTicks[static_cast<std::size_t>(laneIndex)];
        event.action = laneIndex % 2 == 0
            ? wave::EventAction::Drive
            : wave::EventAction::Expect;
        event.value = laneIndex % 2 == 0 ? "0" : "1";
        scenario.events.push_back(std::move(event));
    }

    wave::Relation first;
    first.id = "relation-first";
    first.sourceEventId = "event-0";
    first.targetEventId = "event-1";
    first.maximumDelay = 100'000;
    first.description = "First relation";
    scenario.relations.push_back(first);

    wave::Relation second;
    second.id = "relation-second";
    second.sourceEventId = "event-2";
    second.targetEventId = "event-3";
    second.maximumDelay = 100'000;
    second.description = "Second relation";
    scenario.relations.push_back(second);

    project.scenarios.push_back(std::move(scenario));
    return project;
}

void sendMouse(
    wave::WaveCanvas* canvas,
    const QEvent::Type type,
    const QPoint position,
    const Qt::MouseButton button,
    const Qt::MouseButtons buttons,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QMouseEvent event(
        type,
        QPointF(position),
        QPointF(canvas->viewport()->mapToGlobal(position)),
        button,
        buttons,
        modifiers);
    QCoreApplication::sendEvent(canvas->viewport(), &event);
}

void click(
    wave::WaveCanvas* canvas,
    const QPoint position,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendMouse(
        canvas,
        QEvent::MouseButtonPress,
        position,
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    sendMouse(
        canvas,
        QEvent::MouseButtonRelease,
        position,
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QCoreApplication::processEvents();
}

void drag(wave::WaveCanvas* canvas, const QPoint start, const QPoint end)
{
    sendMouse(
        canvas,
        QEvent::MouseButtonPress,
        start,
        Qt::LeftButton,
        Qt::LeftButton);
    sendMouse(
        canvas,
        QEvent::MouseMove,
        end,
        Qt::NoButton,
        Qt::LeftButton);
    sendMouse(
        canvas,
        QEvent::MouseButtonRelease,
        end,
        Qt::LeftButton,
        Qt::NoButton);
    QCoreApplication::processEvents();
}

[[nodiscard]] bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    wave::MainWindow window(makeProject());
    window.resize(1'280, 820);
    window.show();
    application.processEvents();

    auto* canvas = window.findChild<wave::WaveCanvas*>();
    auto* relationAction = window.findChild<QAction*>(
        QStringLiteral("RelationToolAction"));
    if (!canvas || !relationAction) {
        std::cerr << "relation editing controls are missing: canvas="
                  << (canvas != nullptr)
                  << " action=" << (relationAction != nullptr) << '\n';
        return 2;
    }
    if (relationAction->shortcut().matches(
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R))
            != QKeySequence::ExactMatch) {
        return fail("relation editing shortcut is missing")
            ? 0 : 3;
    }

    relationAction->trigger();
    canvas->fitScenario();
    canvas->verticalScrollBar()->setValue(0);
    application.processEvents();
    static_cast<void>(canvas->viewport()->grab());
    if (!relationAction->isChecked()
        || canvas->tool() != wave::WaveCanvas::Tool::Relation
        || !canvas->relationsVisible()) {
        return fail("relation tool did not activate") ? 0 : 4;
    }

    const auto& scenario = window.project().scenarios.front();
    const auto pointForEvent = [canvas, &scenario](const std::string& eventId) {
        const auto event = std::find_if(
            scenario.events.begin(),
            scenario.events.end(),
            [&eventId](const wave::Event& candidate) {
                return candidate.id == eventId;
            });
        const auto laneIndex = static_cast<int>(std::distance(
            scenario.events.begin(),
            event));
        const auto header = canvas->signalHeaderWidth();
        const auto width = canvas->viewport()->width() - header;
        return QPoint(
            header + static_cast<int>(
                         static_cast<double>(event->tick)
                         / static_cast<double>(scenario.duration)
                         * width),
            40 + laneIndex * 48 + 24);
    };
    const auto event0 = pointForEvent("event-0");
    const auto event1 = pointForEvent("event-1");
    const auto event2 = pointForEvent("event-2");
    const auto event3 = pointForEvent("event-3");
    const auto event4 = pointForEvent("event-4");
    const auto event6 = pointForEvent("event-6");
    const auto event7 = pointForEvent("event-7");

    click(canvas, (event0 + event1) / 2);
    if (canvas->selectedRelationIds()
            != QStringList{QStringLiteral("relation-first")}) {
        return fail("relation line selection did not select the stable ID")
            ? 0 : 5;
    }

    drag(canvas, event1, event4);
    const auto retargeted = std::find_if(
        window.project().scenarios.front().relations.begin(),
        window.project().scenarios.front().relations.end(),
        [](const wave::Relation& relation) {
            return relation.id == "relation-first";
        });
    if (retargeted == window.project().scenarios.front().relations.end()
        || retargeted->targetEventId != "event-4") {
        return fail("relation endpoint drag did not retarget") ? 0 : 6;
    }
    if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)) {
        return fail("relation retarget Undo was unavailable") ? 0 : 7;
    }
    auto first = std::find_if(
        window.project().scenarios.front().relations.begin(),
        window.project().scenarios.front().relations.end(),
        [](const wave::Relation& relation) {
            return relation.id == "relation-first";
        });
    if (first == window.project().scenarios.front().relations.end()
        || first->targetEventId != "event-1"
        || !QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)) {
        return fail("relation retarget Undo/Redo was not exact") ? 0 : 8;
    }

    drag(canvas, event6, event7);
    if (window.project().scenarios.front().relations.size() != 3) {
        return fail("relation drag creation did not refresh the model")
            ? 0 : 9;
    }

    const auto beforeBatchDelete = window.project().scenarios.front().relations;
    click(canvas, (event0 + event4) / 2);
    click(canvas, (event2 + event3) / 2, Qt::ControlModifier);
    if (canvas->selectedRelationIds().size() != 2) {
        return fail("Ctrl+click did not multi-select relation lines") ? 0 : 10;
    }
    QKeyEvent remove(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &remove);
    application.processEvents();
    if (window.project().scenarios.front().relations.size() != 1) {
        return fail("relation line multi-delete was not atomic") ? 0 : 10;
    }
    if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
        || window.project().scenarios.front().relations != beforeBatchDelete
        || !QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
        || window.project().scenarios.front().relations.size() != 1
        || !QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
        || window.project().scenarios.front().relations != beforeBatchDelete) {
        return fail("relation batch delete Undo/Redo was not exact")
            ? 0 : 11;
    }

    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &escape);
    application.processEvents();
    if (relationAction->isChecked()
        || canvas->tool() != wave::WaveCanvas::Tool::WaveEdit) {
        return fail("Escape did not leave relation editing") ? 0 : 12;
    }

    window.hide();
    return 0;
}
