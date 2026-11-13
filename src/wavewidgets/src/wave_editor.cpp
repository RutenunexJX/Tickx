#include "wave/widgets.h"
#include "wave/project_io.h"
#include "wave/commands.h"
#include "wave_canvas.h"
#include "lane_editing.h"
#include "ui_controls.h"

#include <QApplication>
#include <QAction>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QScrollBar>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
constexpr std::size_t MaxInputSize = 8U * 1024U * 1024U;

void require(bool condition, const char* message)
{
    if (!condition) throw std::invalid_argument(message);
}

void assignProject(wave::Project& destination, const wave::Project& source)
{
    // Keep ScenarioRef and the canvas's Project/Scenario addresses valid.
    destination.scenarios.front() = source.scenarios.front();
    destination.schemaVersion = source.schemaVersion;
    destination.name = source.name;
    destination.clockDomains = source.clockDomains;
    destination.importedTraces = source.importedTraces;
    destination.linkedResources = source.linkedResources;
    destination.exportSettings = source.exportSettings;
    destination.extensions = source.extensions;
}

class SyncProjectCommand final : public wave::EditCommand {
public:
    SyncProjectCommand(wave::Project& project, wave::Project next)
        : project_(project), before_(project), after_(std::move(next)) {}
    void redo() override { assignProject(project_, after_); }
    void undo() override { assignProject(project_, before_); }
    bool hasEffect() const noexcept override { return before_ != after_; }
    std::string description() const override { return "Synchronize waveform"; }
private:
    wave::Project& project_;
    wave::Project before_, after_;
};

QJsonObject readObject(const char* data, std::size_t size)
{
    require(data && size && size <= MaxInputSize, "A JSON object of at most 8 MiB is required.");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(QByteArray(data, static_cast<qsizetype>(size)), &error);
    require(error.error == QJsonParseError::NoError && document.isObject(), "Invalid JSON object.");
    return document.object();
}

wave::Project readProject(const char* data, std::size_t size)
{
    require(data && size && size <= MaxInputSize, "A project of at most 8 MiB is required.");
    auto loaded = wave::deserializeProject(QByteArray(data, static_cast<qsizetype>(size)));
    if (!loaded.ok()) throw std::invalid_argument(loaded.error.toStdString());
    require(loaded.project->scenarios.size() == 1 && loaded.project->scenarios.front().duration > 0,
        "Exactly one scenario with a positive duration is required.");
    return std::move(*loaded.project);
}

class WaveEditor final : public QWidget {
public:
    WaveEditor(wave::Project project, QWidget* parent)
        : QWidget(parent), project_(std::move(project))
    {
        wave::ui::initialize();
        setObjectName(QStringLiteral("WaveEditor"));
        setProperty("wavewidgets.contract", QString::fromLatin1(wave::kWaveEditorContract));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setMinimumSize(240, 200);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        canvas_ = new wave::WaveCanvas(this);
        canvas_->setObjectName(QStringLiteral("waveEditorCanvas"));
        canvas_->viewport()->installEventFilter(this);
        canvas_->setDocument(&project_, &scenario(), &history_);
        canvas_->setSignalHeaderWidth(176);

        auto* bar = wave::ui::toolBar(this);
        bar->setObjectName(QStringLiteral("waveEditorTools"));
        bar->setMovable(false);
        bar->setFloatable(false);
        bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        bar->setIconSize(QSize(20, 20));
        const auto action = [this, bar](wave::ui::Icon icon, const QString& title,
                                       const char* name, const char* operation) {
            auto* result = bar->addAction(wave::ui::icon(icon, bar), title);
            result->setObjectName(QString::fromLatin1(name));
            connect(result, &QAction::triggered, this, [this, operation] {
                runUi([&] { perform(QString::fromLatin1(operation)); });
            });
            return result;
        };
        undo_ = action(wave::ui::Icon::Undo, tr("Undo"), "waveEditorUndo", "undo");
        redo_ = action(wave::ui::Icon::Redo, tr("Redo"), "waveEditorRedo", "redo");
        bar->addSeparator();
        action(wave::ui::Icon::ZoomIn, tr("Zoom in"), "waveEditorZoomIn", "zoom-in");
        action(wave::ui::Icon::ZoomOut, tr("Zoom out"), "waveEditorZoomOut", "zoom-out");
        action(wave::ui::Icon::Fit, tr("Fit"), "waveEditorFit", "fit");
        bar->addWidget(canvas_->takeDurationControl(bar));
        layout->addWidget(bar);

        const auto paletteHost = [this, layout](QWidget* palette) {
            auto* host = new QScrollArea(this);
            host->setFrameShape(QFrame::NoFrame);
            host->setWidget(palette);
            host->setWidgetResizable(true);
            host->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            host->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            host->hide();
            layout->addWidget(host);
            return host;
        };
        auto* rangeHost = paletteHost(canvas_->rangeEditPaletteWidget());
        const auto paletteVisibility = [this](QScrollArea* host, bool visible) {
            host->setFixedHeight(host->widget()->sizeHint().height() + 18);
            host->setVisible(visible && editable());
            updatePreferredHeight();
        };
        connect(canvas_, &wave::WaveCanvas::rangeEditPaletteVisibilityChanged, this,
            [rangeHost, paletteVisibility](bool visible) { paletteVisibility(rangeHost, visible); });
        layout->addWidget(canvas_, 1);
        status_ = wave::ui::text(this);
        status_->setObjectName(QStringLiteral("waveEditorStatus"));
        status_->setWordWrap(true);
        status_->hide();
        layout->addWidget(status_);
        connect(canvas_, &wave::WaveCanvas::statusMessage, this, [this](const QString& message) {
            // Keep transient guidance available without reserving a full footer.
            status_->setToolTip(message);
            setToolTip(message);
        });
        connect(canvas_, &wave::WaveCanvas::addLaneRequested, this,
            [this](wave::LaneKind kind) { runUi([&] { addLane(kind); }); });
        connect(canvas_, &wave::WaveCanvas::quickLaneSetupAccepted, this,
            [this](const QString& id, const QString& name, const QString& parameter, const QString& clock) {
                completeLane(id, name, parameter, clock);
            });
        connect(canvas_, &wave::WaveCanvas::quickLaneSetupCanceled, this,
            [this](const QString& id) { runUi([&] { cancelLane(id); }); });
        connect(canvas_, &wave::WaveCanvas::renameLaneRequested, this,
            [this](const QString& id) { runUi([&] { renameLane(id); }); });
        connect(canvas_, &wave::WaveCanvas::laneRenameAccepted, this,
            [this](const QString& id, const QString& name) { completeRename(id, name); });
        connect(canvas_, &wave::WaveCanvas::removeLaneRequested, this,
            [this](const QString& id) { runUi([&] { removeLanes({id}); }); });
        connect(canvas_, &wave::WaveCanvas::removeLanesRequested, this,
            [this](const QStringList& ids) { runUi([&] { removeLanes(ids); }); });
        connect(canvas_, &wave::WaveCanvas::duplicateLaneRequested, this,
            [this](const QString& id) { runUi([&] { duplicateLanes({id}); }); });
        connect(canvas_, &wave::WaveCanvas::duplicateLanesRequested, this,
            [this](const QStringList& ids) { runUi([&] { duplicateLanes(ids); }); });
        connect(canvas_, &wave::WaveCanvas::showHiddenLanesRequested, this, [this] {
            runUi([&] { prepareEdit(); execute(std::make_unique<wave::ShowHiddenLanesCommand>(scenario())); });
        });
        connect(canvas_, &wave::WaveCanvas::showHiddenLaneRequested, this, [this](const QString& id) {
            runUi([&] { prepareEdit(); execute(std::make_unique<wave::ShowLaneCommand>(scenario(), id.toStdString())); });
        });
        connect(canvas_, &wave::WaveCanvas::editLaneParametersRequested, this,
            [this](const QString& id, const QPoint& point) { runUi([&] { laneMenu(id, point); }); });
        connect(canvas_, &wave::WaveCanvas::durationEditRequested, this,
            [this](const QString& value) { changeDuration(value); });
        connect(canvas_, &wave::WaveCanvas::modelEdited, this, [this] { publish(); });
        connect(canvas_, &wave::WaveCanvas::commandAvailabilityChanged, this, [this] { publish(); });
        connect(canvas_, &wave::WaveCanvas::selectionChanged, this, [this] { scheduleView(); });
        connect(canvas_, &wave::WaveCanvas::signalHeaderWidthCommitted, this, [this] { scheduleView(); });
        connect(canvas_->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] { scheduleView(); });
        connect(canvas_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { scheduleView(); });
        canvas_->setReadOnly(true);
        committed_ = wave::serializeProject(project_);
        updateHistory();
        updatePreferredHeight();
        QTimer::singleShot(0, this, [this] {
            if (!viewConfigured_) canvas_->fitScenario();
            scheduleView();
        });
    }

    ~WaveEditor() override
    {
        callback_ = nullptr;
        canvas_->setDocument(nullptr, nullptr, nullptr);
    }

    QSize sizeHint() const override { return {640, preferredHeight_}; }

    QByteArray snapshot()
    {
        if (!notifying_) commit();
        return wave::serializeProject(project_);
    }

    QJsonObject state() const
    {
        return {{"contract", QString::fromLatin1(wave::kWaveEditorContract)},
            {"mode", mode_}, {"readOnly", locked_}, {"editable", editable()},
            {"historyOwner", hostHistory_ ? "host" : "component"},
            {"revision", QString::number(revision_)},
            {"canUndo", canUndo()}, {"canRedo", canRedo()}, {"view", view()},
            {"preferredHeight", preferredHeight_}, {"devicePixelRatio", devicePixelRatioF()}};
    }

    void setCallback(wave::WaveEditorCallback callback, void* context)
    {
        mutationAllowed();
        callback_ = callback;
        callbackContext_ = context;
    }

    void configure(const QJsonObject& config)
    {
        mutationAllowed();
        for (auto it = config.begin(); it != config.end(); ++it)
            require(QStringList{"mode", "readOnly", "historyOwner", "canUndo", "canRedo", "theme", "view"}.contains(it.key()),
                "Unknown wave editor configuration field.");
        auto nextMode = mode_;
        if (config.contains("mode")) {
            require(config["mode"].isString(), "mode must be edit or preview.");
            nextMode = config["mode"].toString();
            require(nextMode == "edit" || nextMode == "preview", "mode must be edit or preview.");
        }
        bool nextLocked = locked_;
        if (config.contains("readOnly")) {
            require(config["readOnly"].isBool(), "readOnly must be a boolean.");
            nextLocked = config["readOnly"].toBool();
        }
        bool nextHost = hostHistory_;
        if (config.contains("historyOwner")) {
            const auto owner = config["historyOwner"].toString();
            require(owner == "host" || owner == "component", "historyOwner must be host or component.");
            nextHost = owner == "host";
            require(!edited_ || nextHost == hostHistory_, "Choose the history owner before editing.");
        }
        for (const auto* key : {"canUndo", "canRedo"})
            if (config.contains(key)) require(config[key].isBool(), "History availability must be boolean.");
        const auto nextTheme = config.contains("theme") ? config["theme"].toString() : theme_;
        require(nextTheme == "system" || nextTheme == "light" || nextTheme == "dark", "Unknown theme.");
        QJsonObject nextView;
        if (config.contains("view")) {
            require(config["view"].isObject(), "view must be an object.");
            nextView = checkedView(config["view"].toObject());
        }
        commit();
        require(canvas_->setReadOnly(nextLocked || nextMode == "preview"),
            "Finish the current gesture before changing mode.");
        hostHistory_ = nextHost;
        mode_ = nextMode;
        locked_ = nextLocked;
        if (config.contains("canUndo")) hostCanUndo_ = config["canUndo"].toBool();
        if (config.contains("canRedo")) hostCanRedo_ = config["canRedo"].toBool();
        if (nextTheme != theme_) setTheme(nextTheme);
        if (!nextView.isEmpty()) {
            restoreView(nextView, config["view"].toObject().contains("selectedLaneIds")
                || config["view"].toObject().contains("activeLaneId"));
            viewConfigured_ = true;
        }
        updateHistory();
        scheduleView();
    }

    void synchronize(wave::Project next)
    {
        mutationAllowed();
        require(next.id == project_.id && next.timeBase == project_.timeBase
            && next.scenarios.size() == 1 && next.scenarios.front().id == scenario().id,
            "Synchronization must retain project, scenario and timebase identities.");
        const auto before = committed_;
        commit();
        require(before == committed_ || next == project_,
            "Local edits were committed; synchronize their saved snapshot before restoring other content.");
        if (next == project_) return;
        const auto oldView = view();
        const QScopedValueRollback<bool> suppress(publishingSuppressed_, true);
        if (hostHistory_) {
            assignProject(project_, next);
            history_.clear();
            canvas_->invalidateRangeSequenceHistoryContext();
        } else {
            history_.execute(std::make_unique<SyncProjectCommand>(project_, std::move(next)));
        }
        canvas_->refreshModel();
        // refreshModel retains surviving stable selections; restore only reading geometry.
        restoreView(oldView, false);
        committed_ = wave::serializeProject(project_);
        publishedState_ = history_.stateId();
        ++revision_;
        updateHistory();
        updatePreferredHeight();
        scheduleView();
    }

    void perform(const QString& action)
    {
        mutationAllowed();
        if (action == "commit") { commit(); return; }
        if (action == "fit" || action == "zoom-in" || action == "zoom-out") {
            commit();
            if (action == "fit") canvas_->fitScenario();
            else if (action == "zoom-in") canvas_->zoomIn();
            else canvas_->zoomOut();
            scheduleView();
            return;
        }
        prepareEdit();
        if (action == "undo" || action == "redo") {
            const auto undo = action == "undo";
            if (!(undo ? canUndo() : canRedo())) return;
            if (hostHistory_) {
                notifyEvent(undo ? "undoRequested" : "redoRequested");
            } else {
                const auto before = history_.stateId();
                if (undo ? history_.undo() : history_.redo()) {
                    canvas_->refreshModel();
                    static_cast<void>(canvas_->restoreSelectionForHistoryTransition(before, history_.stateId()));
                    publish();
                }
            }
        } else if (action == "add-clock") addLane(wave::LaneKind::Clock);
        else if (action == "add-bit") addLane(wave::LaneKind::Bit);
        else if (action == "add-bus") addLane(wave::LaneKind::Bus);
        else throw std::invalid_argument("Unknown wave editor action.");
    }

protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (canvas_ && object == canvas_->viewport()
            && (event->type() == QEvent::Wheel || event->type() == QEvent::Resize
                || event->type() == QEvent::Show || event->type() == QEvent::MouseButtonRelease
                || event->type() == QEvent::MouseMove || event->type() == QEvent::KeyRelease))
            scheduleView();
        return QWidget::eventFilter(object, event);
    }
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        scheduleView();
    }

private:
    wave::Scenario& scenario() { return project_.scenarios.front(); }
    const wave::Scenario& scenario() const { return project_.scenarios.front(); }
    bool editable() const { return mode_ == "edit" && !locked_; }
    bool canUndo() const { return hostHistory_ ? hostCanUndo_ : history_.canUndo(); }
    bool canRedo() const { return hostHistory_ ? hostCanRedo_ : history_.canRedo(); }
    void mutationAllowed() const
    {
        require(!notifying_, "Queue component mutations until after the event callback.");
    }
    void commit()
    {
        require(!dialogOpen_ && !canvas_->hasActiveInteraction(), "Finish the current waveform gesture or dialog first.");
        require(canvas_->commitPendingInlineEdits(), "Correct the highlighted input before continuing.");
        publish();
    }
    void prepareEdit()
    {
        mutationAllowed();
        require(editable(), "Enter Edit mode in a writable document to change the waveform.");
        commit();
    }
    template<typename Function> void runUi(Function&& function)
    {
        try {
            function();
            status_->hide();
        } catch (const std::exception& error) {
            status_->setText(QString::fromUtf8(error.what()));
            status_->setProperty("waveState", QStringLiteral("error"));
            status_->show();
        }
    }
    void execute(std::unique_ptr<wave::EditCommand> command)
    {
        const auto before = history_.stateId();
        canvas_->beginCommandSelectionTransition(before);
        try {
            history_.execute(std::move(command));
        } catch (...) {
            canvas_->cancelCommandSelectionTransition();
            throw;
        }
        canvas_->refreshModel();
        canvas_->finishCommandSelectionTransition(history_.stateId());
        publish();
    }
    void addLane(wave::LaneKind kind)
    {
        prepareEdit();
        const auto draft = wave::makeQuickLane(project_, scenario(), kind);
        pendingLane_ = QString::fromStdString(draft.lane.id);
        // Draft creation must not truncate the user's redo branch before acceptance.
        pendingHistory_ = std::make_unique<wave::CommandStack>(std::move(history_));
        history_ = wave::CommandStack{};
        try {
            history_.execute(wave::quickLaneCommand(project_, scenario(), draft));
        } catch (...) {
            history_ = std::move(*pendingHistory_);
            pendingHistory_.reset();
            pendingLane_.clear();
            throw;
        }
        pendingState_ = history_.stateId();
        canvas_->refreshModel();
        canvas_->revealLocation(pendingLane_, canvas_->cursorTick());
        QStringList labels, ids;
        for (const auto& clock : project_.clockDomains) {
            labels.append(QString::fromStdString(clock.name));
            ids.append(QString::fromStdString(clock.id));
        }
        const auto parameter = draft.clock
            ? QString::fromStdString(wave::formatTick(draft.clock->period, project_.timeBase))
            : kind == wave::LaneKind::Bus ? QString::number(draft.lane.width) : QString{};
        canvas_->beginQuickLaneSetup(pendingLane_, kind, QString::fromStdString(draft.lane.name),
            parameter, labels, ids, QString::fromStdString(draft.lane.clockDomainId));
        updatePreferredHeight();
    }
    void completeLane(const QString& id, const QString& name, const QString& parameter, const QString& clock)
    {
        if (id != pendingLane_ || !editable()) return;
        bool focusParameter = false;
        try {
            require(history_.stateId() == pendingState_, "Signal creation was interrupted.");
            const auto* lane = wave::findLane(scenario(), id.toStdString());
            require(lane, "The pending signal is unavailable.");
            auto command = wave::completeQuickLaneCommand(
                project_, scenario(), *lane, name, parameter, clock, focusParameter);
            wave::QuickLaneDraft draft{*lane, std::nullopt};
            if (lane->kind == wave::LaneKind::Clock)
                draft.clock = *wave::findClock(project_, lane->clockDomainId);
            require(history_.discardLast(), "The pending signal could not be committed.");
            history_ = std::move(*pendingHistory_);
            pendingHistory_.reset();
            try {
                history_.execute(std::move(command));
            } catch (...) {
                pendingHistory_ = std::make_unique<wave::CommandStack>(std::move(history_));
                history_ = wave::CommandStack{};
                history_.execute(wave::quickLaneCommand(project_, scenario(), draft));
                pendingState_ = history_.stateId();
                throw;
            }
            pendingLane_.clear();
            canvas_->finishQuickLaneSetup();
            canvas_->refreshModel();
            publish();
        } catch (const std::exception& error) {
            canvas_->showQuickLaneSetupError(QString::fromUtf8(error.what()), focusParameter);
        }
    }
    void cancelLane(const QString& id)
    {
        if (id != pendingLane_) return;
        require(history_.stateId() == pendingState_, "Signal creation was interrupted.");
        require(history_.discardLast(), "The pending signal could not be canceled.");
        history_ = std::move(*pendingHistory_);
        pendingHistory_.reset();
        pendingLane_.clear();
        canvas_->finishQuickLaneSetup();
        canvas_->refreshModel();
        updateHistory();
        updatePreferredHeight();
    }
    void renameLane(const QString& id)
    {
        prepareEdit();
        const auto* lane = wave::findLane(scenario(), id.toStdString());
        require(lane, "Select an existing signal.");
        canvas_->beginLaneRename(id, QString::fromStdString(lane->name));
    }
    void completeRename(const QString& id, const QString& name)
    {
        if (!editable()) return;
        try {
            const auto* lane = wave::findLane(scenario(), id.toStdString());
            require(lane, "Select an existing signal.");
            wave::validateLaneName(scenario(), lane->id, name);
            auto replacement = *lane;
            replacement.name = name.trimmed().toStdString();
            history_.execute(std::make_unique<wave::ChangeLaneCommand>(
                project_, scenario(), lane->id, std::move(replacement)));
            canvas_->finishLaneRename();
            canvas_->refreshModel();
            publish();
        } catch (const std::exception& error) {
            canvas_->showLaneRenameError(QString::fromUtf8(error.what()));
        }
    }
    void removeLanes(const QStringList& ids)
    {
        prepareEdit();
        std::vector<std::string> names;
        for (const auto& id : ids) names.push_back(id.toStdString());
        execute(std::make_unique<wave::RemoveLanesCommand>(project_, scenario(), std::move(names)));
    }
    void duplicateLanes(const QStringList& ids)
    {
        prepareEdit();
        auto naming = scenario();
        std::vector<wave::Lane> lanes;
        std::vector<wave::ClockDomain> clocks;
        std::size_t insertion = 0;
        QStringList selected;
        for (std::size_t index = 0; index < scenario().lanes.size(); ++index) {
            const auto& original = scenario().lanes[index];
            if (!ids.contains(QString::fromStdString(original.id))) continue;
            require(original.kind != wave::LaneKind::Group, "Select signals to duplicate.");
            auto lane = original;
            lane.id = wave::makeStableId("lane");
            lane.name = wave::uniqueLaneName(naming, lane.name + "_copy");
            lane.color = wave::randomReadableLaneColor(naming, original.color);
            lane.visible = true;
            for (auto& segment : lane.segments) segment.id = wave::makeStableId("segment");
            if (lane.kind == wave::LaneKind::Clock) {
                const auto* originalClock = wave::findClock(project_, lane.clockDomainId);
                require(originalClock, "The clock domain is unavailable.");
                auto clock = *originalClock;
                clock.id = wave::makeStableId("clock");
                clock.name = lane.name;
                lane.clockDomainId = clock.id;
                clocks.push_back(std::move(clock));
            }
            selected.append(QString::fromStdString(lane.id));
            naming.lanes.push_back(lane);
            lanes.push_back(std::move(lane));
            insertion = index + 1;
        }
        require(!lanes.empty(), "Select signals to duplicate.");
        execute(std::make_unique<wave::DuplicateLanesCommand>(
            project_, scenario(), std::move(lanes), std::move(clocks), insertion));
        canvas_->selectLaneHeaders(selected, selected.first());
    }
    void laneMenu(const QString& id, const QPoint& point)
    {
        prepareEdit();
        const auto* lane = wave::findLane(scenario(), id.toStdString());
        require(lane, "Select an existing signal.");
        auto* menu = wave::ui::menu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        const auto add = [this, menu](const QString& text, const char* name, auto handler) {
            auto* action = menu->addAction(text);
            action->setObjectName(QString::fromLatin1(name));
            connect(action, &QAction::triggered, this, [this, handler] { runUi(handler); });
        };
        add(tr("Rename"), "waveEditorRename", [this, id] { renameLane(id); });
        if (lane->kind == wave::LaneKind::Clock)
            add(tr("Clock parameters"), "waveEditorClockParameters", [this, id] {
                prepareEdit();
                const auto* selected = wave::findLane(scenario(), id.toStdString());
                require(selected, "The signal is unavailable.");
                const auto* clock = wave::findClock(project_, selected->clockDomainId);
                require(clock, "The clock domain is unavailable.");
                const auto clockId = clock->id;
                const auto result = [&] {
                    const QScopedValueRollback<bool> modal(dialogOpen_, true);
                    return wave::promptClockParameters(this, project_, *clock);
                }();
                if (result)
                    execute(std::make_unique<wave::ChangeClockCommand>(project_, scenario(), clockId, *result));
            });
        add(tr("Properties"), "waveEditorProperties", [this, id] {
            prepareEdit();
            const auto* selected = wave::findLane(scenario(), id.toStdString());
            require(selected, "The signal is unavailable.");
            const auto result = [&] {
                const QScopedValueRollback<bool> modal(dialogOpen_, true);
                return wave::promptLaneProperties(this, project_, scenario(), *selected, false);
            }();
            if (result)
                execute(std::make_unique<wave::ChangeLaneCommand>(project_, scenario(), id.toStdString(), *result));
        });
        menu->addSeparator();
        for (const auto direction : {-1, 1}) {
            add(direction < 0 ? tr("Move up") : tr("Move down"),
                direction < 0 ? "waveEditorMoveUp" : "waveEditorMoveDown", [this, id, direction] {
                prepareEdit();
                const auto found = std::find_if(scenario().lanes.begin(), scenario().lanes.end(),
                    [&id](const auto& item) { return item.id == id.toStdString(); });
                require(found != scenario().lanes.end(), "The signal is unavailable.");
                const auto position = std::distance(scenario().lanes.begin(), found);
                const auto next = position + direction;
                if (next < 0 || next >= static_cast<std::ptrdiff_t>(scenario().lanes.size())) return;
                execute(std::make_unique<wave::MoveLaneCommand>(scenario(), id.toStdString(),
                    static_cast<std::size_t>(next)));
            });
        }
        add(tr("Delete"), "waveEditorDelete", [this, id] { removeLanes({id}); });
        menu->popup(point);
    }
    void changeDuration(const QString& text)
    {
        if (!editable()) return;
        try {
            QString error;
            std::optional<std::int64_t> cycle;
            const auto duration = wave::parseTimeText(text, project_.timeBase, nullptr, cycle, error);
            if (!duration || *duration <= 0)
                throw std::invalid_argument(error.isEmpty() ? "Timeline end must be greater than zero." : error.toStdString());
            const auto latest = wave::latestWaveContentTick(scenario());
            require(*duration >= latest, "Timeline cannot end before existing content.");
            execute(std::make_unique<wave::ChangeScenarioDurationCommand>(scenario(), *duration));
        } catch (const std::exception& error) {
            canvas_->showDurationEditError(QString::fromUtf8(error.what()));
        }
    }
    void updateHistory()
    {
        undo_->setEnabled(editable() && canUndo());
        redo_->setEnabled(editable() && canRedo());
        setProperty("wavewidgets.canUndo", canUndo());
        setProperty("wavewidgets.canRedo", canRedo());
        setProperty("wavewidgets.stateRevision", QVariant::fromValue<qulonglong>(revision_));
    }
    void publish()
    {
        if (publishingSuppressed_ || !pendingLane_.isEmpty() || canvas_->hasQuickLaneSetup()
            || canvas_->hasLaneRename()) return;
        if (publishedState_ == history_.stateId()) { updateHistory(); return; }
        publishedState_ = history_.stateId();
        const auto current = wave::serializeProject(project_);
        if (current == committed_) { updateHistory(); return; }
        committed_ = current;
        edited_ = true;
        ++revision_;
        updateHistory();
        updatePreferredHeight();
        notifyEvent("changed");
    }
    void updatePreferredHeight()
    {
        int height = 150;
        if (canvas_->rangeEditPaletteWidget()->isVisible())
            height += canvas_->rangeEditPaletteWidget()->sizeHint().height() + 18;
        for (const auto& lane : scenario().lanes)
            if (lane.visible) height += std::clamp(lane.height, 30, 240);
        height = std::clamp(height, 240, 540);
        if (height == preferredHeight_) return;
        preferredHeight_ = height;
        updateGeometry();
        notifyEvent("preferredHeightChanged");
    }
    QJsonObject view() const
    {
        const auto range = canvas_->visibleTimeRange();
        return {{"cursorTick", QString::number(canvas_->cursorTick())},
            {"startTick", QString::number(range.first)}, {"endTick", QString::number(range.second)},
            {"selectedLaneIds", QJsonArray::fromStringList(canvas_->selectedLaneIds())},
            {"activeLaneId", canvas_->selectedLaneId()},
            {"verticalScroll", canvas_->verticalScrollBar()->value()},
            {"headerWidth", canvas_->signalHeaderWidth()}};
    }
    static qint64 tick(const QJsonValue& value)
    {
        bool ok = false;
        const auto result = value.toString().toLongLong(&ok);
        require(value.isString() && ok && result >= 0, "View times must be nonnegative decimal tick strings.");
        return result;
    }
    QJsonObject checkedView(const QJsonObject& input) const
    {
        auto result = view();
        for (auto it = input.begin(); it != input.end(); ++it) {
            require(result.contains(it.key()), "Unknown view field.");
            result[it.key()] = it.value();
        }
        const auto start = tick(result["startTick"]);
        const auto end = tick(result["endTick"]);
        tick(result["cursorTick"]);
        require(end > start && end <= scenario().duration, "Visible range must be within the scenario.");
        for (const auto* field : {"headerWidth", "verticalScroll"}) {
            const auto value = result[field].toDouble(-1);
            require(value >= 0 && value <= std::numeric_limits<int>::max()
                && value == static_cast<int>(value), "Invalid view geometry.");
        }
        require(result["selectedLaneIds"].isArray() && result["activeLaneId"].isString(),
            "Invalid signal selection.");
        for (const auto& id : result["selectedLaneIds"].toArray())
            require(id.isString() && wave::findLane(scenario(), id.toString().toStdString()),
                "Selected signals must exist in the project.");
        require(result["activeLaneId"].toString().isEmpty()
            || wave::findLane(scenario(), result["activeLaneId"].toString().toStdString()),
            "The active signal must exist in the project.");
        return result;
    }
    void restoreView(const QJsonObject& value, bool restoreSelection)
    {
        canvas_->setSignalHeaderWidth(value["headerWidth"].toInt());
        canvas_->goToTick(tick(value["cursorTick"]));
        QStringList ids;
        for (const auto& id : value["selectedLaneIds"].toArray()) ids.append(id.toString());
        if (restoreSelection) {
            if (ids.isEmpty()) canvas_->clearSelection();
            else canvas_->selectLaneHeaders(ids, value["activeLaneId"].toString());
        }
        const auto start = tick(value["startTick"]);
        const auto end = tick(value["endTick"]);
        canvas_->restoreVisibleTimeSpan(end - start, start + (end - start) / 2);
        canvas_->verticalScrollBar()->setValue(value["verticalScroll"].toInt());
    }
    void scheduleView()
    {
        if (viewScheduled_) return;
        viewScheduled_ = true;
        QTimer::singleShot(0, this, [this] {
            viewScheduled_ = false;
            const auto current = view();
            if (current == lastView_) return;
            lastView_ = current;
            notifyEvent("viewChanged");
        });
    }
    void notifyEvent(const char* kind)
    {
        if (!callback_ || notifying_) return;
        auto event = QJsonObject{{"event", QString::fromLatin1(kind)},
            {"revision", QString::number(revision_)}, {"canUndo", canUndo()}, {"canRedo", canRedo()}};
        if (qstrcmp(kind, "viewChanged") == 0) event["view"] = view();
        if (qstrcmp(kind, "preferredHeightChanged") == 0) event["preferredHeight"] = preferredHeight_;
        const auto bytes = QJsonDocument(event).toJson(QJsonDocument::Compact);
        const QScopedValueRollback<bool> guard(notifying_, true);
        try { callback_(callbackContext_, bytes.constData(), static_cast<std::size_t>(bytes.size())); }
        catch (...) { /* Exceptions may not cross the component callback boundary. */ }
    }
    void setTheme(const QString& theme)
    {
        theme_ = theme;
        if (theme == "system") { setPalette(QPalette{}); setStyleSheet({}); return; }
        const auto scheme = theme == "dark" ? wave::WaveformColorScheme::Dark : wave::WaveformColorScheme::Light;
        const auto colors = wave::waveformTheme(scheme);
        auto palette = QApplication::palette();
        palette.setColor(QPalette::Window, colors.application);
        palette.setColor(QPalette::WindowText, colors.text);
        palette.setColor(QPalette::Base, colors.canvas);
        palette.setColor(QPalette::AlternateBase, colors.panel);
        palette.setColor(QPalette::Text, colors.text);
        palette.setColor(QPalette::Button, colors.raised);
        palette.setColor(QPalette::ButtonText, colors.text);
        palette.setColor(QPalette::Highlight, colors.selection);
        palette.setColor(QPalette::HighlightedText, colors.selectionText);
        palette.setColor(QPalette::PlaceholderText, colors.mutedText);
        setPalette(palette);
        setStyleSheet(wave::waveApplicationStyleSheet(scheme));
    }

    wave::Project project_;
    wave::CommandStack history_;
    std::unique_ptr<wave::CommandStack> pendingHistory_;
    wave::WaveCanvas* canvas_{};
    QLabel* status_{};
    QAction* undo_{};
    QAction* redo_{};
    wave::WaveEditorCallback callback_{};
    void* callbackContext_{};
    QByteArray committed_;
    QJsonObject lastView_;
    QString mode_{"preview"}, theme_{"system"}, pendingLane_;
    std::uint64_t revision_{0}, pendingState_{0}, publishedState_{0};
    bool locked_{false}, hostHistory_{false}, hostCanUndo_{false}, hostCanRedo_{false};
    bool edited_{false}, notifying_{false}, publishingSuppressed_{false}, dialogOpen_{false};
    bool viewScheduled_{false}, viewConfigured_{false};
    int preferredHeight_{240};
};

void errorText(const QString& value, char* destination, std::size_t capacity)
{
    if (!destination || !capacity) return;
    const auto bytes = value.toUtf8();
    const auto size = std::min(capacity - 1, static_cast<std::size_t>(bytes.size()));
    std::memcpy(destination, bytes.constData(), size);
    destination[size] = 0;
}

template<typename Function> int api(char* error, std::size_t capacity, Function&& function) noexcept
{
    try {
        require(qApp && QThread::currentThread() == qApp->thread(), "Call the component on the Qt GUI thread.");
        function();
        errorText({}, error, capacity);
        return 0;
    } catch (const std::exception& exception) {
        errorText(QString::fromUtf8(exception.what()), error, capacity);
        return 1;
    } catch (...) {
        errorText(QStringLiteral("The waveform operation failed."), error, capacity);
        return 2;
    }
}
WaveEditor& editor(QWidget* widget)
{
    auto* result = dynamic_cast<WaveEditor*>(widget);
    require(result, "A wave-editor/v1 widget is required.");
    return *result;
}
void writeBytes(const QByteArray& bytes, char* destination, std::size_t capacity, std::size_t* required)
{
    require(required, "required must point to a size.");
    *required = static_cast<std::size_t>(bytes.size()) + 1;
    if (!destination && !capacity) return;
    require(destination && capacity >= *required, "The output buffer is too small.");
    std::memcpy(destination, bytes.constData(), static_cast<std::size_t>(bytes.size()));
    destination[bytes.size()] = 0;
}
} // namespace

int wavewidgets_create_wave_editor_v1(const char* data, std::size_t size, const char* hostQtVersion,
    QWidget* parent, QWidget** result, char* error, std::size_t capacity) noexcept
{
    if (result) *result = nullptr;
    return api(error, capacity, [&] {
        require(result && hostQtVersion && std::strcmp(hostQtVersion, QT_VERSION_STR) == 0,
            "A destination and matching Qt runtime are required.");
        require((data && size) || (!data && !size), "Pass a project or null with zero size.");
        *result = new WaveEditor(data ? readProject(data, size) : wave::makeBlankWaveProject(), parent);
    });
}
int wavewidgets_wave_editor_project_v1(QWidget* widget, char* destination, std::size_t capacity,
    std::size_t* required, char* error, std::size_t errorCapacity) noexcept
{
    if (required) *required = 0;
    return api(error, errorCapacity, [&] { writeBytes(editor(widget).snapshot(), destination, capacity, required); });
}
int wavewidgets_wave_editor_state_v1(QWidget* widget, char* destination, std::size_t capacity,
    std::size_t* required, char* error, std::size_t errorCapacity) noexcept
{
    if (required) *required = 0;
    return api(error, errorCapacity, [&] {
        writeBytes(QJsonDocument(editor(widget).state()).toJson(QJsonDocument::Compact), destination, capacity, required);
    });
}
int wavewidgets_configure_wave_editor_v1(QWidget* widget, const char* json, std::size_t size,
    char* error, std::size_t capacity) noexcept
{
    return api(error, capacity, [&] { editor(widget).configure(readObject(json, size)); });
}
int wavewidgets_sync_wave_editor_v1(QWidget* widget, const char* json, std::size_t size,
    char* error, std::size_t capacity) noexcept
{
    return api(error, capacity, [&] { editor(widget).synchronize(readProject(json, size)); });
}
int wavewidgets_wave_editor_action_v1(QWidget* widget, const char* action, char* error, std::size_t capacity) noexcept
{
    return api(error, capacity, [&] {
        require(action, "An action is required.");
        editor(widget).perform(QString::fromUtf8(action));
    });
}
int wavewidgets_set_wave_editor_callback_v1(QWidget* widget, wave::WaveEditorCallback callback, void* context,
    char* error, std::size_t capacity) noexcept
{
    return api(error, capacity, [&] { editor(widget).setCallback(callback, context); });
}
