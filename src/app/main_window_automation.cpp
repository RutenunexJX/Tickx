#include "main_window.h"
#include "wave_canvas.h"
#include "wave/automation.h"
#include "wave/project_io.h"
#include "wave/session_contract.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace wave {
namespace {

QString modelRevision(const Project& project)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        serializeProject(project), QCryptographicHash::Sha256).toHex());
}

bool equalPath(const QString& left, const QString& right)
{
    auto canonical = [](const QString& path) {
        const QFileInfo info(path);
        return info.canonicalFilePath().isEmpty()
            ? info.absoluteFilePath() : info.canonicalFilePath();
    };
#ifdef Q_OS_WIN
    return canonical(left).compare(canonical(right), Qt::CaseInsensitive) == 0;
#else
    return canonical(left) == canonical(right);
#endif
}

QJsonArray strings(const QStringList& values)
{
    return QJsonArray::fromStringList(values);
}

// Swapping preserves the allocations targeted by earlier/later undo commands.
// Undo restores those exact allocations before an earlier command is invoked.
class AutomationProjectCommand final : public EditCommand {
public:
    AutomationProjectCommand(Project& target, Project replacement)
        : target_(target), alternate_(std::move(replacement)) {}
    void redo() override { std::swap(target_, alternate_); }
    void undo() override { std::swap(target_, alternate_); }
    std::string description() const override { return "CLI automation batch"; }
private:
    Project& target_;
    Project alternate_;
};

bool validRevision(const QJsonValue& value, const bool allowMissing = false)
{
    return value.isString()
        && ((allowMissing && value.toString() == QStringLiteral("missing"))
            || QRegularExpression(QStringLiteral("^[a-fA-F0-9]{64}$"))
                   .match(value.toString()).hasMatch());
}

} // namespace

QJsonObject MainWindow::automationSessionState() const
{
    const auto disk = projectFileRevision(projectFile_);
    const auto activeEditor = hasActiveInlineEditor()
        || (canvas_ && canvas_->hasActiveInteraction());
    QString adoption = QStringLiteral("current");
    if (projectFile_.isEmpty()) adoption = QStringLiteral("unsaved");
    else if (disk.state == ProjectFileRevision::State::Missing)
        adoption = QStringLiteral("missing");
    else if (disk.state == ProjectFileRevision::State::Unreadable)
        adoption = QStringLiteral("unreadable");
    else if (!sameProjectFileRevision(loadedProjectRevision_, disk)) {
        if (pendingExternalProjectUpdate_
            && !pendingExternalProjectUpdate_->project)
            adoption = QStringLiteral("invalid");
        else if (disk.sha256 == ignoredExternalProjectSha_)
            adoption = QStringLiteral("ignored");
        else adoption = dirty_ ? QStringLiteral("conflict") : QStringLiteral("pending");
    }
    auto revision = [](const ProjectFileRevision& value) -> QJsonValue {
        return value.state == ProjectFileRevision::State::Present
            ? QJsonValue(QString::fromLatin1(value.sha256.toHex()))
            : QJsonValue(QJsonValue::Null);
    };
    QJsonObject state{
        {"sessionId", automationSessionId_},
        {"processId", static_cast<double>(QCoreApplication::applicationPid())},
        {"projectPath", projectFile_.isEmpty() ? QString{} : QFileInfo(projectFile_).absoluteFilePath()},
        {"projectId", QString::fromStdString(project_.id)},
        {"modelSha256", modelRevision(project_)},
        {"loadedSha256", revision(loadedProjectRevision_)},
        {"diskSha256", revision(disk)},
        {"adoption", adoption},
        {"dirty", dirty_},
        {"activeEditor", activeEditor},
        {"canUndo", commandStack_.canUndo()},
        {"canRedo", commandStack_.canRedo()},
        {"visible", isVisible()},
    };
    if (pendingExternalProjectUpdate_ && !pendingExternalProjectUpdate_->error.isEmpty())
        state.insert("adoptionError", pendingExternalProjectUpdate_->error);
    QJsonArray scenarios;
    for (const auto& scenario : project_.scenarios) {
        if (scenarios.size() == 1000) break;
        scenarios.append(QJsonObject{{"id", QString::fromStdString(scenario.id)},
            {"name", QString::fromStdString(scenario.name)},
            {"duration", QString::number(scenario.duration)}});
    }
    state.insert("scenarios", scenarios);
    state.insert("scenarioCount", static_cast<double>(project_.scenarios.size()));
    if (canvas_) {
        const auto [start, end] = canvas_->visibleTimeRange();
        QJsonObject view{
            {"cursor", QString::number(canvas_->cursorTick())},
            {"visibleStart", QString::number(start)}, {"visibleEnd", QString::number(end)},
            {"laneIds", strings(canvas_->selectedLaneIds())},
            {"markerIds", strings(canvas_->selectedMarkerIds())},
            {"relationIds", strings(canvas_->selectedRelationIds())},
            {"relationsVisible", canvas_->relationsVisible()},
        };
        if (const auto range = canvas_->selectedTimeRange();
            range && canvas_->hasExplicitRangeSelection())
            view.insert("range", QJsonObject{{"start", QString::number(range->first)},
                                            {"end", QString::number(range->second)}});
        else view.insert("range", QJsonValue::Null);
        if (const auto* scenario = activeScenario()) {
            view.insert("scenarioId", QString::fromStdString(scenario->id));
            QJsonArray lanes;
            for (const auto& lane : scenario->lanes) {
                if (lanes.size() == 1000) break;
                const auto id = QString::fromStdString(lane.id);
                lanes.append(QJsonObject{{"id", id}, {"name", QString::fromStdString(lane.name)},
                    {"visible", lane.visible}, {"displayed", canvas_->isLaneDisplayed(id)},
                    {"group", lane.kind == LaneKind::Group},
                    {"collapsed", canvas_->isGroupCollapsed(id)}});
            }
            view.insert("lanes", lanes);
            view.insert("laneCount", static_cast<double>(scenario->lanes.size()));
        }
        state.insert("view", view);
    }
    return state;
}

QJsonObject MainWindow::executeSessionCommand(
    const QString& command, const QJsonObject& arguments)
{
    auto fail = [&](const QString& code, const QString& message) -> QJsonObject {
        return {{"ok", false}, {"error", QJsonObject{{"code", code}, {"message", message}}},
                {"state", automationSessionState()}};
    };
    auto success = [&](QJsonObject details = {}) -> QJsonObject {
        details.insert("ok", true);
        details.insert("state", automationSessionState());
        return details;
    };
    const auto definitions = describeSessionAutomation().value("arguments").toObject();
    if (!definitions.contains(command))
        return fail("unknown-command", "Unknown session command.");
    const auto allowed = definitions.value(command).toArray();
    for (auto it = arguments.begin(); it != arguments.end(); ++it)
        if (!allowed.contains(it.key()))
            return fail("invalid-arguments", "Unknown field: " + it.key());
    if (QJsonDocument(arguments).toJson(QJsonDocument::Compact).size() > 524288)
        return fail("input-too-large", "Session arguments exceed 512 KiB.");
    for (const auto& key : {"dryRun", "relationsVisible"})
        if (arguments.contains(key) && !arguments.value(key).isBool())
            return fail("invalid-arguments", QString::fromLatin1(key) + " must be boolean.");
    const bool dryRun = arguments.value("dryRun").toBool();
    if (command == "status") return success();

    if (arguments.contains("expectedDiskSha256")
        && !validRevision(arguments.value("expectedDiskSha256"), command == "save"))
        return fail("invalid-arguments", "expectedDiskSha256 must be a SHA-256 digest.");
    if (arguments.contains("expectedModelSha256")
        && !validRevision(arguments.value("expectedModelSha256")))
        return fail("invalid-arguments", "expectedModelSha256 must be a SHA-256 digest.");
    for (const auto& key : {"projectPath", "scenarioId"})
        if (arguments.contains(key)
            && (!arguments.value(key).isString() || arguments.value(key).toString().isEmpty()))
            return fail("invalid-arguments", QString::fromLatin1(key) + " must be a nonempty string.");

    if (command == "open" || command == "refresh") {
        if (command == "open" && !arguments.contains("projectPath"))
            return fail("invalid-arguments", "open requires projectPath.");
        const auto path = command == "open"
            ? QFileInfo(arguments.value("projectPath").toString()).absoluteFilePath()
            : projectFile_;
        if (path.isEmpty()) return fail("no-project-file", "This window has no project file.");
        const auto disk = projectFileRevision(path);
        const auto expected = arguments.value("expectedDiskSha256").toString().toLower();
        if (!expected.isEmpty()
            && (disk.state != ProjectFileRevision::State::Present
                || QString::fromLatin1(disk.sha256.toHex()) != expected))
            return fail("disk-revision-conflict", "Disk revision differs from expectedDiskSha256.");
        if (QFileInfo(path).size() > 64 * 1024 * 1024)
            return fail("input-too-large", "Online open is limited to 64 MiB.");
        if (!projectFile_.isEmpty() && equalPath(path, projectFile_)) {
            scheduleExternalProjectInspection();
            if (projectFileDebounceTimer_) projectFileDebounceTimer_->stop();
            inspectExternalProjectFile();
            auto response = success();
            if (!expected.isEmpty()
                && response.value("state").toObject().value("diskSha256").toString() != expected) {
                response.insert("ok", false);
                response.insert("error", QJsonObject{{"code", "disk-revision-conflict"},
                    {"message", "Disk revision changed during refresh."}});
            }
            return response;
        }
        if (dirty_) return fail("dirty-document", "Save the current project before opening another.");
        if (hasActiveInlineEditor() || canvas_->hasActiveInteraction())
            return fail("active-editor", "Finish the current edit before opening another project.");
        QString error;
        if (!loadFromPath(path, false, &error, expected))
            return fail("project-open-failed", error);
        return success();
    }

    if (hasActiveInlineEditor() || canvas_->hasActiveInteraction())
        return fail("active-editor", "Finish the current edit before changing the session.");
    if (command == "apply" || command == "save" || command == "undo" || command == "redo") {
        const auto expected = arguments.value("expectedModelSha256").toString().toLower();
        if (expected.isEmpty() && !dryRun)
            return fail("revision-required", "expectedModelSha256 is required for online mutations.");
        if (!expected.isEmpty() && expected != modelRevision(project_))
            return fail("model-revision-conflict", "The window model has changed.");
    }

    if (command == "apply") {
        if (!arguments.value("batch").isObject())
            return fail("invalid-arguments", "apply requires a batch object.");
        const auto batch = arguments.value("batch").toObject();
        if (batch.value("operations").toArray().size() > 4096)
            return fail("input-too-large", "A session batch is limited to 4096 operations.");
        std::optional<std::string> scenarioSelector;
        if (arguments.contains("scenarioId")) {
            const auto id = arguments.value("scenarioId").toString().toStdString();
            if (!scenarioIndexByStableId(project_, id))
                return fail("unknown-scenario", "Unknown scenarioId.");
            scenarioSelector = id;
        }
        auto applied = applyAutomationBatch(project_, batch, scenarioSelector);
        if (!applied.ok()) {
            auto result = fail("batch-rejected", applied.error);
            result.insert("batch", applied.json);
            result.insert("failedOperation", applied.failedOperation);
            return result;
        }
        if (QJsonDocument(applied.json).toJson(QJsonDocument::Compact).size() > 524288)
            return fail("report-too-large", "Batch report exceeds 512 KiB; use a smaller batch or offline apply.");
        if (!dryRun && applied.changed) {
            const auto beforeId = activeScenario() ? activeScenario()->id : std::string{};
            const auto afterId = scenarioIndexByStableId(*applied.project, beforeId)
                ? beforeId : applied.project->scenarios.front().id;
            if (!executeScenarioLifecycleCommand(
                    std::make_unique<AutomationProjectCommand>(project_, std::move(*applied.project)),
                    beforeId, afterId, tr("CLI automation batch applied")))
                return fail("batch-rejected", "The session could not commit the batch.");
            // Rebinding the model can reveal a passive range palette. A CLI
            // transaction must not create an editor that blocks its next save.
            canvas_->dismissInlineEditors();
        }
        return success({{"dryRun", dryRun}, {"changed", applied.changed}, {"batch", applied.json}});
    }
    if (command == "save") {
        const auto path = arguments.contains("projectPath")
            ? QFileInfo(arguments.value("projectPath").toString()).absoluteFilePath() : projectFile_;
        if (path.isEmpty()) return fail("invalid-arguments", "save requires projectPath for an untitled project.");
        if (!arguments.contains("expectedDiskSha256"))
            return fail("revision-required", "save requires expectedDiskSha256 (or 'missing').");
        const auto disk = projectFileRevision(path);
        const auto expected = arguments.value("expectedDiskSha256").toString().toLower();
        if ((expected == "missing" && disk.state != ProjectFileRevision::State::Missing)
            || (expected != "missing"
                && (disk.state != ProjectFileRevision::State::Present
                    || expected != QString::fromLatin1(disk.sha256.toHex()))))
            return fail("disk-revision-conflict", "The save destination has changed.");
        if (!projectFile_.isEmpty() && equalPath(path, projectFile_)
            && !sameProjectFileRevision(disk, loadedProjectRevision_))
            return fail("disk-revision-conflict", "The window has not adopted this disk revision.");
        if (dryRun) return success({{"dryRun", true}, {"written", false}});
        QString error;
        if (!writeToPath(path, &error)) return fail("write-failed", error);
        return success({{"written", true}});
    }
    if (command == "undo" || command == "redo") {
        const auto possible = command == "undo" ? commandStack_.canUndo() : commandStack_.canRedo();
        if (!possible) return fail("history-empty", "No matching history entry.");
        if (!dryRun) {
            canvas_->setFocus(Qt::OtherFocusReason);
            if (command == "undo") undo();
            else redo();
            canvas_->dismissInlineEditors();
        }
        return success({{"dryRun", dryRun}});
    }

    // Validate the complete view request against its target scenario before changing anything.
    auto index = activeScenarioIndex_;
    if (arguments.contains("scenarioId")) {
        const auto resolved = scenarioIndexByStableId(
            project_, arguments.value("scenarioId").toString().toStdString());
        if (!resolved) return fail("unknown-scenario", "Unknown scenarioId.");
        index = *resolved;
    }
    if (index >= project_.scenarios.size()) return fail("no-scenario", "The project has no scenario.");
    const auto& scenario = project_.scenarios[index];
    auto time = [&](const QJsonValue& value, Tick& tick) {
        if (!value.isString()) return false;
        const auto parsed = parseAutomationTime(project_, value.toString());
        if (!parsed.ok()) return false;
        tick = *parsed.tick;
        return tick >= 0 && tick <= scenario.duration;
    };
    Tick cursor = 0, start = 0, end = 0, pan = 0;
    if (arguments.contains("cursor") && !time(arguments.value("cursor"), cursor))
        return fail("invalid-time", "cursor must be a time string within the scenario.");
    const bool bounds = arguments.contains("start") || arguments.contains("end");
    if (bounds && (!time(arguments.value("start"), start)
        || !time(arguments.value("end"), end) || start >= end))
        return fail("invalid-time", "start/end must form a nonempty range within the scenario.");
    const auto zoom = arguments.value("zoom").toString();
    if (arguments.contains("zoom") && zoom != "in" && zoom != "out" && zoom != "fit")
        return fail("invalid-arguments", "zoom must be in, out, or fit.");
    if (static_cast<int>(bounds) + static_cast<int>(arguments.contains("zoom"))
        + static_cast<int>(arguments.contains("pan")) > 1)
        return fail("invalid-arguments", "Use only one of start/end, zoom, and pan.");
    if (arguments.contains("pan")) {
        if (!arguments.value("pan").isString())
            return fail("invalid-time", "pan must be a signed time string.");
        auto value = arguments.value("pan").toString();
        const bool negative = value.startsWith('-');
        if (negative) value.remove(0, 1);
        if (!time(value, pan)) return fail("invalid-time", "Invalid pan offset.");
        if (negative) pan = -pan;
    }
    auto ids = [&](const char* key, QStringList& result) {
        if (!arguments.contains(key)) return true;
        if (!arguments.value(key).isArray()) return false;
        const auto values = arguments.value(key).toArray();
        if (values.size() > 1000) return false;
        for (const auto& value : values) {
            if (!value.isString() || value.toString().isEmpty() || result.contains(value.toString()))
                return false;
            result.append(value.toString());
        }
        return true;
    };
    QStringList laneIds, markerIds, relationIds;
    if (!ids("laneIds", laneIds) || !ids("markerIds", markerIds) || !ids("relationIds", relationIds))
        return fail("invalid-arguments", "Selections require at most 1000 unique stable ID strings.");
    QStringList expanded;
    for (const auto& id : laneIds) {
        const auto* lane = findLane(scenario, id.toStdString());
        if (!lane || !lane->visible) return fail("unknown-lane", "Unknown or hidden lane: " + id);
        if (lane->kind == LaneKind::Group) {
            for (const auto& child : scenario.lanes)
                if (child.groupId == lane->id && child.visible && child.kind != LaneKind::Group)
                    expanded.append(QString::fromStdString(child.id));
        } else expanded.append(id);
    }
    expanded.removeDuplicates();
    if (arguments.contains("laneIds") && expanded.isEmpty())
        return fail("invalid-arguments", "laneIds must select at least one visible signal.");
    for (const auto& id : markerIds)
        if (std::none_of(scenario.markers.begin(), scenario.markers.end(),
                [&](const auto& item) { return item.id == id.toStdString(); }))
            return fail("unknown-marker", "Unknown marker: " + id);
    for (const auto& id : relationIds)
        if (std::none_of(scenario.relations.begin(), scenario.relations.end(),
                [&](const auto& item) { return item.id == id.toStdString(); }))
            return fail("unknown-relation", "Unknown relation: " + id);
    std::optional<std::pair<Tick, Tick>> range;
    if (arguments.contains("range") && !arguments.value("range").isNull()) {
        if (!arguments.value("range").isObject())
            return fail("invalid-time", "range requires start/end or null.");
        const auto object = arguments.value("range").toObject();
        Tick from = 0, to = 0;
        if (object.size() != 2 || !time(object.value("start"), from)
            || !time(object.value("end"), to) || from >= to)
            return fail("invalid-time", "Invalid selection range.");
        if (expanded.isEmpty()
            && (index != activeScenarioIndex_ || canvas_->selectedLaneIds().isEmpty()))
            return fail("invalid-arguments", "A range requires selected signals.");
        range = std::pair{from, to};
    }
    QJsonArray groups;
    if (arguments.contains("groups")) {
        if (!arguments.value("groups").isArray()
            || arguments.value("groups").toArray().size() > 1000)
            return fail("invalid-arguments", "groups must be an array of at most 1000 entries.");
        groups = arguments.value("groups").toArray();
        QSet<QString> unique;
        for (const auto& value : groups) {
            const auto item = value.toObject();
            const auto id = item.value("id").toString();
            const auto* lane = findLane(scenario, id.toStdString());
            if (!value.isObject() || item.size() != 2 || !item.value("collapsed").isBool()
                || !lane || lane->kind != LaneKind::Group || !lane->visible || unique.contains(id))
                return fail("unknown-group", "Each group requires a unique visible group ID and collapsed boolean.");
            unique.insert(id);
        }
    }
    if (dryRun) return success({{"dryRun", true}});
    if (index != activeScenarioIndex_ && !switchActiveScenario(index, false))
        return fail("scenario-switch-failed", "The window could not switch scenarios.");
    if (!expanded.isEmpty()) canvas_->selectLaneHeaders(expanded, expanded.front());
    if (arguments.contains("range")) canvas_->setExplicitTimeRange(range);
    if (arguments.contains("markerIds") || arguments.contains("relationIds"))
        canvas_->restoreStableObjectSelections(markerIds, relationIds);
    for (const auto& value : groups) {
        const auto item = value.toObject();
        canvas_->setGroupCollapsed(item.value("id").toString(), item.value("collapsed").toBool());
    }
    if (arguments.contains("relationsVisible"))
        canvas_->setRelationsVisible(arguments.value("relationsVisible").toBool());
    if (arguments.contains("cursor")) canvas_->goToTick(cursor);
    if (bounds) canvas_->setVisibleTimeRange(start, end);
    else if (zoom == "in") canvas_->zoomIn();
    else if (zoom == "out") canvas_->zoomOut();
    else if (zoom == "fit") canvas_->fitScenario();
    else if (arguments.contains("pan")) {
        const auto current = canvas_->visibleTimeRange();
        const Tick span = std::max<Tick>(1, current.second - current.first);
        const Tick offset = std::clamp<Tick>(pan, -current.first,
            std::max<Tick>(0, scenario.duration - current.second));
        canvas_->setVisibleTimeRange(current.first + offset,
            std::min(scenario.duration, current.first + offset + span));
    }
    scheduleActiveScenarioLocationMemory();
    updateCommandActions();
    return success();
}

} // namespace wave
