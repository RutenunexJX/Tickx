#include "wave_canvas.h"
#include "wave/commands.h"
#include "wave/model.h"

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

namespace {
wave::Lane lane(std::string id, wave::LaneKind kind, std::string group = {}, bool visible = true)
{
    wave::Lane result;
    result.id = result.name = std::move(id);
    result.kind = kind;
    result.groupId = std::move(group);
    result.visible = visible;
    return result;
}

wave::Project fixture()
{
    wave::Project project;
    project.id = "group-regression";
    wave::Scenario scenario;
    scenario.id = "first";
    scenario.duration = 1'000;
    // Parents may follow members; hidden, dangling and non-group references
    // must retain their existing display semantics.
    scenario.lanes = {
        lane("member-a", wave::LaneKind::Bit, "a"),
        lane("member-b", wave::LaneKind::Bit, "b"),
        lane("hidden-member", wave::LaneKind::Bit, "a", false),
        lane("a", wave::LaneKind::Group),
        lane("b", wave::LaneKind::Group),
        lane("empty", wave::LaneKind::Group),
        lane("hidden-parent", wave::LaneKind::Group, {}, false),
        lane("orphan", wave::LaneKind::Bit, "missing"),
        lane("hidden-parent-member", wave::LaneKind::Bit, "hidden-parent"),
        lane("plain", wave::LaneKind::Bit),
        lane("wrong-kind-member", wave::LaneKind::Bit, "plain")
    };
    project.scenarios.push_back(std::move(scenario));
    return project;
}

bool referenceDisplayed(const wave::Scenario& scenario, const wave::WaveCanvas& canvas, const wave::Lane& item)
{
    if (!item.visible) return false;
    if (item.kind == wave::LaneKind::Group || item.groupId.empty()) return true;
    const auto* parent = wave::findLane(scenario, item.groupId);
    return !parent || !parent->visible || parent->kind != wave::LaneKind::Group
        || !canvas.isGroupCollapsed(QString::fromStdString(parent->id));
}
}

class CanvasGroupRegressionTest final : public QObject {
    Q_OBJECT
private:
    static void verifyDisplay(const wave::Scenario& scenario, const wave::WaveCanvas& canvas)
    {
        for (const auto& item : scenario.lanes)
            QCOMPARE(canvas.isLaneDisplayed(QString::fromStdString(item.id)),
                referenceDisplayed(scenario, canvas, item));
    }

private slots:
    void parentOrderAndVisibility()
    {
        auto project = fixture();
        auto& scenario = project.scenarios.front();
        wave::CommandStack commands;
        wave::WaveCanvas canvas;
        canvas.setDocument(&project, &scenario, &commands);
        QVERIFY(!canvas.setGroupCollapsed("empty", true));
        QVERIFY(!canvas.setGroupCollapsed("hidden-parent", true));
        QVERIFY(!canvas.setGroupCollapsed("plain", true));
        QVERIFY(canvas.setGroupCollapsed("a", true));
        QVERIFY(canvas.setGroupCollapsed("b", true));
        verifyDisplay(scenario, canvas);
        QVERIFY(canvas.setGroupCollapsed("a", false));
        verifyDisplay(scenario, canvas);
        canvas.resize(900, 600);
        canvas.show();
        QApplication::processEvents();
        QVERIFY(!canvas.viewport()->grab().isNull());
    }

    void equalSizeReplacementAndReallocation()
    {
        auto project = fixture();
        auto& scenario = project.scenarios.front();
        wave::CommandStack commands;
        wave::WaveCanvas canvas;
        canvas.setDocument(&project, &scenario, &commands);
        QVERIFY(canvas.setGroupCollapsed("a", true));
        std::reverse(scenario.lanes.begin(), scenario.lanes.end());
        canvas.refreshModel();
        verifyDisplay(scenario, canvas);
        scenario.lanes.reserve(scenario.lanes.capacity() + 64);
        verifyDisplay(scenario, canvas);
        wave::findLane(scenario, "member-a")->groupId = "empty";
        canvas.refreshModel();
        QVERIFY(!canvas.isGroupCollapsed("a"));
        QVERIFY(canvas.setGroupCollapsed("empty", true));
        verifyDisplay(scenario, canvas);
        wave::findLane(scenario, "empty")->visible = false;
        canvas.refreshModel();
        QVERIFY(!canvas.isGroupCollapsed("empty"));
        verifyDisplay(scenario, canvas);
        auto* parent = wave::findLane(scenario, "b");
        parent->kind = wave::LaneKind::Bit;
        parent->groupId.clear();
        canvas.refreshModel();
        QVERIFY(!canvas.setGroupCollapsed("b", true));
        verifyDisplay(scenario, canvas);
        auto* member = wave::findLane(scenario, "member-a");
        member->groupId = "a";
        member->visible = false;
        canvas.refreshModel();
        QVERIFY(!canvas.setGroupCollapsed("a", true));
        verifyDisplay(scenario, canvas);
    }

    void commandHistoryAndLaneLifetime()
    {
        auto project = fixture();
        auto& scenario = project.scenarios.front();
        wave::CommandStack commands;
        wave::WaveCanvas canvas;
        canvas.setDocument(&project, &scenario, &commands);
        const auto original = scenario;
        QVERIFY(commands.execute(std::make_unique<wave::SetLanesGroupCommand>(
            scenario, std::vector<std::string>{"member-a", "member-b"}, "empty")));
        canvas.refreshModel();
        QVERIFY(canvas.setGroupCollapsed("empty", true));
        verifyDisplay(scenario, canvas);
        commands.undo();
        canvas.refreshModel();
        QCOMPARE(scenario, original);
        QVERIFY(!canvas.isGroupCollapsed("empty"));
        verifyDisplay(scenario, canvas);
        commands.redo();
        canvas.refreshModel();
        QVERIFY(canvas.setGroupCollapsed("empty", true));
        verifyDisplay(scenario, canvas);
        QVERIFY(commands.execute(std::make_unique<wave::AddLaneCommand>(
            scenario, lane("added", wave::LaneKind::Bit, "empty"), 0)));
        canvas.refreshModel();
        QVERIFY(!canvas.isLaneDisplayed("added"));
        QVERIFY(commands.execute(std::make_unique<wave::RemoveLaneCommand>(project, scenario, "empty")));
        canvas.refreshModel();
        QVERIFY(canvas.isLaneDisplayed("added"));
        verifyDisplay(scenario, canvas);
        commands.undo();
        canvas.refreshModel();
        QVERIFY(canvas.setGroupCollapsed("empty", true));
        verifyDisplay(scenario, canvas);
    }

    void documentSwitchRestoresGroupsAndSelection()
    {
        auto project = fixture();
        auto second = project.scenarios.front();
        second.id = "second";
        wave::findLane(second, "member-a")->groupId = "empty";
        project.scenarios.push_back(std::move(second));
        wave::CommandStack commands;
        wave::WaveCanvas canvas;
        canvas.setDocument(&project, &project.scenarios[0], &commands);
        QVERIFY(canvas.setGroupCollapsed("a", true));
        canvas.selectLaneHeaders({"member-b"}, "member-b");
        canvas.setDocument(&project, &project.scenarios[1], &commands);
        QVERIFY(!canvas.setGroupCollapsed("a", true));
        QVERIFY(canvas.setGroupCollapsed("empty", true));
        canvas.setDocument(&project, &project.scenarios[0], &commands);
        QVERIFY(canvas.isGroupCollapsed("a"));
        QVERIFY(!canvas.isGroupCollapsed("empty"));
        verifyDisplay(project.scenarios[0], canvas);
        canvas.setDocument(nullptr, nullptr, &commands);
        QVERIFY(!canvas.isLaneDisplayed("member-a"));
        QVERIFY(!canvas.setGroupCollapsed("a", true));
        canvas.setDocument(&project, &project.scenarios[1], &commands);
        QVERIFY(canvas.isGroupCollapsed("empty"));
        verifyDisplay(project.scenarios[1], canvas);
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir profile;
    if (!profile.isValid()) return 2;
    qputenv("WAVEWORKBENCH_SETTINGS_DIR", profile.path().toUtf8());
    qputenv("WAVEWORKBENCH_RECOVERY_DIR", (profile.path() + "/recovery").toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, profile.path());
    QApplication application(argc, argv);
    CanvasGroupRegressionTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "canvas_group_regression_test.moc"
