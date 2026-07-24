#include "main_window.h"

#include "wave/export.h"
#include "wave/integration.h"
#include "wave/project_io.h"
#include "wave/trace.h"
#include "wave/validation.h"
#include "trace_canvas.h"
#include "wave_canvas.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <atomic>
#include <array>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace wave {
namespace {

QIcon themedIcon(const QString& name, QStyle* style, const QStyle::StandardPixmap fallback)
{
    const auto icon = QIcon::fromTheme(name);
    return icon.isNull() ? style->standardIcon(fallback) : icon;
}

QString laneKindText(const LaneKind kind)
{
    const auto text = toString(kind);
    return QString::fromLatin1(text.data(), static_cast<qsizetype>(text.size()));
}

QString actionText(const EventAction action)
{
    const auto text = toString(action);
    return QString::fromLatin1(text.data(), static_cast<qsizetype>(text.size()));
}

QString severityText(const Severity severity)
{
    const auto text = toString(severity);
    return QString::fromLatin1(text.data(), static_cast<qsizetype>(text.size()));
}

std::optional<Tick> parseTimeText(
    QString text,
    const TimeBase& timeBase,
    const ClockDomain* clock,
    std::optional<std::int64_t>& cycle,
    QString& error)
{
    text = text.trimmed().toLower();
    const QRegularExpression cycleExpression(
        QStringLiteral(R"(^cycle\s+(-?\d+)$)"));
    const auto cycleMatch = cycleExpression.match(text);
    if (cycleMatch.hasMatch()) {
        if (!clock) {
            error = QObject::tr("A cycle-based time requires a clock domain.");
            return std::nullopt;
        }
        bool valid = false;
        const auto index = cycleMatch.captured(1).toLongLong(&valid);
        if (!valid) {
            error = QObject::tr("Invalid cycle index.");
            return std::nullopt;
        }
        const auto tick = tickAtCycle(*clock, index, clock->activeEdge);
        if (!tick) {
            error = QObject::tr("Cycle time is outside the integer tick range.");
            return std::nullopt;
        }
        cycle = index;
        return *tick;
    }

    const QRegularExpression absoluteExpression(
        QStringLiteral(R"(^(-?\d+)\s*(ps|ns|us|ms|ticks?)?$)"));
    const auto match = absoluteExpression.match(text);
    if (!match.hasMatch()) {
        error = QObject::tr("Use an integer followed by ps, ns, us, ms, tick, or 'cycle N'.");
        return std::nullopt;
    }
    bool valid = false;
    const auto value = match.captured(1).toLongLong(&valid);
    if (!valid) {
        error = QObject::tr("Invalid integer time.");
        return std::nullopt;
    }
    const auto suffix = match.captured(2);
    cycle.reset();
    if (suffix.isEmpty() || suffix.startsWith(QStringLiteral("tick"))) {
        return value;
    }
    const auto unit = suffix == QStringLiteral("ps")
        ? TimeUnit::Picosecond
        : suffix == QStringLiteral("ns")
            ? TimeUnit::Nanosecond
            : suffix == QStringLiteral("us")
                ? TimeUnit::Microsecond
                : TimeUnit::Millisecond;
    const auto tick = toTicks(value, unit, timeBase);
    if (!tick) {
        error = QObject::tr("The time cannot be represented exactly in the project timebase.");
    }
    return tick;
}

std::optional<Tick> parseDelayText(
    QString text,
    const TimeBase& timeBase,
    const ClockDomain* clock,
    QString& error)
{
    text = text.trimmed().toLower();
    const QRegularExpression cycleExpression(
        QStringLiteral(R"(^(-?\d+)\s*cycles?$)"));
    const auto match = cycleExpression.match(text);
    if (match.hasMatch()) {
        if (!clock) {
            error = QObject::tr("A cycle delay requires a clock domain.");
            return std::nullopt;
        }
        bool valid = false;
        const auto cycles = match.captured(1).toLongLong(&valid);
        if (!valid || cycles < 0
            || (cycles > 0 && clock->period > std::numeric_limits<Tick>::max() / cycles)) {
            error = QObject::tr("Cycle delay is outside the integer tick range.");
            return std::nullopt;
        }
        return cycles * clock->period;
    }
    std::optional<std::int64_t> unusedCycle;
    return parseTimeText(text, timeBase, nullptr, unusedCycle, error);
}

std::optional<ExportOptions> requestExportOptions(
    QWidget* parent,
    const Project& project,
    const Scenario& scenario,
    const std::optional<std::pair<Tick, Tick>>& selection)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Export scenario"));
    auto* layout = new QFormLayout(&dialog);
    auto* scope = new QComboBox;
    scope->addItem(QObject::tr("Full scenario"), QStringLiteral("full"));
    scope->addItem(QObject::tr("Current selection"), QStringLiteral("selection"));
    scope->addItem(QObject::tr("Specified time range"), QStringLiteral("range"));
    if (!selection || selection->second <= selection->first) {
        scope->setItemData(1, 0, Qt::UserRole - 1);
    }
    auto* start = new QLineEdit(QString::fromStdString(formatTick(0, project.timeBase)));
    auto* end = new QLineEdit(
        QString::fromStdString(formatTick(scenario.duration, project.timeBase)));
    auto* width = new QSpinBox;
    width->setRange(640, 8000);
    width->setValue(1600);
    auto* dpi = new QSpinBox;
    dpi->setRange(72, 600);
    dpi->setValue(192);
    auto* pdfSpan = new QLineEdit(QStringLiteral("0 tick"));
    pdfSpan->setToolTip(QObject::tr("0 keeps the selected range on one PDF page"));
    auto* relations = new QCheckBox(QObject::tr("Include relations"));
    relations->setChecked(true);
    auto* markers = new QCheckBox(QObject::tr("Include markers"));
    markers->setChecked(true);
    auto* annotations = new QCheckBox(QObject::tr("Include annotations"));
    annotations->setChecked(true);
    layout->addRow(QObject::tr("Scope"), scope);
    layout->addRow(QObject::tr("Start"), start);
    layout->addRow(QObject::tr("End"), end);
    layout->addRow(QObject::tr("Logical width"), width);
    layout->addRow(QObject::tr("PNG DPI"), dpi);
    layout->addRow(QObject::tr("PDF page span"), pdfSpan);
    layout->addRow(relations);
    layout->addRow(markers);
    layout->addRow(annotations);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    const auto updateRangeFields = [=] {
        const auto rangeMode = scope->currentData().toString() == QStringLiteral("range");
        start->setEnabled(rangeMode);
        end->setEnabled(rangeMode);
    };
    QObject::connect(scope, &QComboBox::currentIndexChanged, &dialog, [=](int) {
        updateRangeFields();
    });
    updateRangeFields();
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;

    ExportOptions options;
    options.width = width->value();
    options.pngDpi = dpi->value();
    options.includeRelations = relations->isChecked();
    options.includeMarkers = markers->isChecked();
    options.includeAnnotations = annotations->isChecked();
    QString parseError;
    std::optional<std::int64_t> unusedCycle;
    const auto mode = scope->currentData().toString();
    if (mode == QStringLiteral("selection")) {
        if (!selection || selection->second <= selection->first) {
            QMessageBox::warning(
                parent,
                QObject::tr("Invalid export range"),
                QObject::tr("No non-empty time selection exists."));
            return std::nullopt;
        }
        options.start = selection->first;
        options.end = selection->second;
    } else if (mode == QStringLiteral("range")) {
        options.start = parseTimeText(
            start->text(),
            project.timeBase,
            nullptr,
            unusedCycle,
            parseError);
        if (parseError.isEmpty()) {
            options.end = parseTimeText(
                end->text(),
                project.timeBase,
                nullptr,
                unusedCycle,
                parseError);
        }
        if (!parseError.isEmpty() || !options.start || !options.end
            || *options.end <= *options.start) {
            QMessageBox::warning(
                parent,
                QObject::tr("Invalid export range"),
                parseError.isEmpty()
                    ? QObject::tr("Export end must be greater than start.")
                    : parseError);
            return std::nullopt;
        }
    }
    unusedCycle.reset();
    const auto parsedSpan = parseTimeText(
        pdfSpan->text(),
        project.timeBase,
        nullptr,
        unusedCycle,
        parseError);
    if (!parsedSpan || *parsedSpan < 0) {
        QMessageBox::warning(
            parent,
            QObject::tr("Invalid PDF page span"),
            parseError.isEmpty() ? QObject::tr("PDF page span must be non-negative.") : parseError);
        return std::nullopt;
    }
    options.pdfPageSpanTicks = *parsedSpan;
    return options;
}

std::filesystem::path nativePath(const QString& path)
{
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toUtf8().constData());
#endif
}

bool checkedDifference(const Tick left, const Tick right, Tick& result)
{
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_sub_overflow(left, right, &result);
#else
    if ((right > 0 && left < std::numeric_limits<Tick>::min() + right)
        || (right < 0 && left > std::numeric_limits<Tick>::max() + right)) {
        return false;
    }
    result = left - right;
    return true;
#endif
}

bool checkedSum(const Tick left, const Tick right, Tick& result)
{
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(left, right, &result);
#else
    if ((right > 0 && left > std::numeric_limits<Tick>::max() - right)
        || (right < 0 && left < std::numeric_limits<Tick>::min() - right)) {
        return false;
    }
    result = left + right;
    return true;
#endif
}

QString autosavePathForProject(const QString& projectPath)
{
    return projectPath.isEmpty() ? QString{} : projectPath + QStringLiteral(".autosave");
}

QString projectPathForLoadedFile(const QString& loadedPath)
{
    const auto suffix = QStringLiteral(".autosave");
    return loadedPath.endsWith(suffix, Qt::CaseInsensitive)
        ? loadedPath.first(loadedPath.size() - suffix.size())
        : loadedPath;
}

QString enumMapText(const std::map<std::string, std::string>& enumMap)
{
    QStringList entries;
    for (const auto& [name, value] : enumMap) {
        entries.push_back(
            QString::fromStdString(name) + QStringLiteral("=")
            + QString::fromStdString(value));
    }
    return entries.join(QStringLiteral("; "));
}

std::optional<std::map<std::string, std::string>> parseEnumMapText(
    const QString& text,
    QString& error)
{
    std::map<std::string, std::string> result;
    const auto entries = text.split(
        QRegularExpression(QStringLiteral("[;\\n]")),
        Qt::SkipEmptyParts);
    for (const auto& rawEntry : entries) {
        const auto entry = rawEntry.trimmed();
        const auto equals = entry.indexOf(QLatin1Char('='));
        if (equals <= 0 || equals == entry.size() - 1) {
            error = QObject::tr(
                "Enum entries must use NAME=VALUE, separated by semicolons.");
            return std::nullopt;
        }
        const auto name = entry.first(equals).trimmed().toStdString();
        const auto value = entry.sliced(equals + 1).trimmed().toStdString();
        if (name.empty() || value.empty() || result.contains(name)) {
            error = QObject::tr("Enum names and values must be non-empty and unique.");
            return std::nullopt;
        }
        result.emplace(name, value);
    }
    return result;
}

std::optional<Lane> promptLaneProperties(
    QWidget* parent,
    const Project& project,
    const Scenario& scenario,
    const Lane& initial,
    const bool lockKind)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(
        initial.kind == LaneKind::Group
            ? QObject::tr("Group properties")
            : QObject::tr("Lane properties"));
    auto* layout = new QFormLayout(&dialog);
    auto* stableId = new QLineEdit(QString::fromStdString(initial.id));
    stableId->setReadOnly(true);
    auto* name = new QLineEdit(QString::fromStdString(initial.name));
    auto* kind = new QComboBox;
    for (const auto candidate : {
             LaneKind::Clock,
             LaneKind::Bit,
             LaneKind::Bus,
             LaneKind::Enum,
             LaneKind::Transaction,
             LaneKind::Event,
             LaneKind::Group,
         }) {
        kind->addItem(
            laneKindText(candidate),
            static_cast<int>(candidate));
    }
    kind->setCurrentIndex(kind->findData(static_cast<int>(initial.kind)));
    kind->setEnabled(!lockKind);
    auto* width = new QLineEdit(QString::number(initial.width));
    auto* signedValue = new QCheckBox;
    signedValue->setChecked(initial.isSigned);
    auto* radix = new QComboBox;
    for (const auto candidate : {
             Radix::Binary,
             Radix::Octal,
             Radix::Decimal,
             Radix::Hexadecimal,
         }) {
        const auto text = toString(candidate);
        radix->addItem(
            QString::fromLatin1(text.data(), static_cast<qsizetype>(text.size())),
            static_cast<int>(candidate));
    }
    radix->setCurrentIndex(radix->findData(static_cast<int>(initial.radix)));
    auto* enumMap = new QLineEdit(enumMapText(initial.enumMap));
    enumMap->setPlaceholderText(QObject::tr("IDLE=0; BUSY=1"));
    auto* clock = new QComboBox;
    clock->addItem(QObject::tr("<none>"), QString{});
    for (const auto& domain : project.clockDomains) {
        clock->addItem(
            QString::fromStdString(domain.name)
                + QStringLiteral(" [")
                + QString::fromStdString(domain.id)
                + QLatin1Char(']'),
            QString::fromStdString(domain.id));
    }
    if (!initial.clockDomainId.empty()
        && clock->findData(QString::fromStdString(initial.clockDomainId)) < 0) {
        clock->addItem(
            QObject::tr("%1 (unresolved)")
                .arg(QString::fromStdString(initial.clockDomainId)),
            QString::fromStdString(initial.clockDomainId));
    }
    clock->setCurrentIndex(std::max(
        0,
        clock->findData(QString::fromStdString(initial.clockDomainId))));
    auto* group = new QComboBox;
    group->addItem(QObject::tr("<none>"), QString{});
    for (const auto& candidate : scenario.lanes) {
        if (candidate.kind != LaneKind::Group || candidate.id == initial.id) continue;
        group->addItem(
            QString::fromStdString(candidate.name)
                + QStringLiteral(" [")
                + QString::fromStdString(candidate.id)
                + QLatin1Char(']'),
            QString::fromStdString(candidate.id));
    }
    if (!initial.groupId.empty()
        && group->findData(QString::fromStdString(initial.groupId)) < 0) {
        group->addItem(
            QObject::tr("%1 (unresolved)")
                .arg(QString::fromStdString(initial.groupId)),
            QString::fromStdString(initial.groupId));
    }
    group->setCurrentIndex(std::max(
        0,
        group->findData(QString::fromStdString(initial.groupId))));
    auto* color = new QLineEdit(QString::fromStdString(initial.color));
    auto* height = new QSpinBox;
    height->setRange(30, 240);
    height->setValue(std::clamp(initial.height, 30, 240));
    auto* visible = new QCheckBox;
    visible->setChecked(initial.visible);
    auto* compatibility = new QLabel(QObject::tr(
        "Stable ID and waveform data are preserved. Incompatible type or width changes are rejected."));
    compatibility->setWordWrap(true);

    layout->addRow(QObject::tr("Stable ID"), stableId);
    layout->addRow(QObject::tr("Name"), name);
    layout->addRow(QObject::tr("Kind"), kind);
    layout->addRow(QObject::tr("Width"), width);
    layout->addRow(QObject::tr("Signed"), signedValue);
    layout->addRow(QObject::tr("Radix"), radix);
    layout->addRow(QObject::tr("Enum map"), enumMap);
    layout->addRow(QObject::tr("Clock domain"), clock);
    layout->addRow(QObject::tr("Group"), group);
    layout->addRow(QObject::tr("Color"), color);
    layout->addRow(QObject::tr("Height"), height);
    layout->addRow(QObject::tr("Visible"), visible);
    layout->addRow(compatibility);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    const auto updateControls = [=] {
        const auto selectedKind = static_cast<LaneKind>(kind->currentData().toInt());
        const auto vectorValue =
            selectedKind == LaneKind::Bus || selectedKind == LaneKind::Enum;
        const auto groupValue = selectedKind == LaneKind::Group;
        width->setEnabled(vectorValue);
        signedValue->setEnabled(vectorValue);
        radix->setEnabled(vectorValue);
        enumMap->setEnabled(selectedKind == LaneKind::Enum);
        clock->setEnabled(!groupValue);
        group->setEnabled(!groupValue);
    };
    QObject::connect(kind, &QComboBox::currentIndexChanged, &dialog, [=](int) {
        updateControls();
    });
    updateControls();

    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    Lane result = initial;
    result.name = name->text().trimmed().toStdString();
    if (result.name.empty()) {
        QMessageBox::warning(
            parent,
            QObject::tr("Invalid lane"),
            QObject::tr("Lane name cannot be empty."));
        return std::nullopt;
    }
    result.kind = static_cast<LaneKind>(kind->currentData().toInt());
    if (result.kind == LaneKind::Bus || result.kind == LaneKind::Enum) {
        bool widthOk = false;
        const auto parsedWidth = width->text().trimmed().toULongLong(&widthOk);
        if (!widthOk
            || parsedWidth == 0
            || parsedWidth > std::numeric_limits<std::uint32_t>::max()) {
            QMessageBox::warning(
                parent,
                QObject::tr("Invalid lane"),
                QObject::tr("Width must be an integer from 1 to 4294967295."));
            return std::nullopt;
        }
        result.width = static_cast<std::uint32_t>(parsedWidth);
        result.isSigned = signedValue->isChecked();
    } else {
        result.width = 1;
        result.isSigned = false;
    }
    result.radix = static_cast<Radix>(radix->currentData().toInt());
    if (result.kind == LaneKind::Enum) {
        QString enumError;
        const auto parsedMap = parseEnumMapText(enumMap->text(), enumError);
        if (!parsedMap) {
            QMessageBox::warning(
                parent,
                QObject::tr("Invalid enum map"),
                enumError);
            return std::nullopt;
        }
        result.enumMap = *parsedMap;
    } else {
        result.enumMap.clear();
    }
    result.clockDomainId = result.kind == LaneKind::Group
        ? std::string{}
        : clock->currentData().toString().toStdString();
    result.groupId = result.kind == LaneKind::Group
        ? std::string{}
        : group->currentData().toString().toStdString();
    const QColor parsedColor(color->text().trimmed());
    if (!parsedColor.isValid()) {
        QMessageBox::warning(
            parent,
            QObject::tr("Invalid lane"),
            QObject::tr("Color must be a valid Qt color such as #42A5F5."));
        return std::nullopt;
    }
    result.color = parsedColor.name(QColor::HexRgb).toStdString();
    result.height = height->value();
    result.visible = visible->isChecked();
    return result;
}

void selectLaneItem(QTreeWidget* tree, const QString& laneId)
{
    if (!tree || laneId.isEmpty()) return;
    QSignalBlocker blocker(tree);
    for (QTreeWidgetItemIterator iterator(tree); *iterator; ++iterator) {
        if ((*iterator)->data(0, Qt::UserRole).toString() == laneId) {
            tree->setCurrentItem(*iterator);
            tree->scrollToItem(*iterator);
            return;
        }
    }
}

} // namespace

MainWindow::MainWindow(Project project, QString projectFile, QWidget* parent)
    : QMainWindow(parent)
    , project_(std::move(project))
    , projectFile_(std::move(projectFile))
{
    const auto recoveredSnapshot = projectFile_.endsWith(
        QStringLiteral(".autosave"),
        Qt::CaseInsensitive);
    if (recoveredSnapshot) {
        projectFile_ = projectPathForLoadedFile(projectFile_);
        dirty_ = true;
    }
    setObjectName(QStringLiteral("WaveWorkbenchMainWindow"));
    setMinimumSize(960, 620);
    resize(1440, 900);
    setDockOptions(AllowNestedDocks | AllowTabbedDocks | AnimatedDocks);

    auto* waveformSplitter = new QSplitter(Qt::Vertical, this);
    canvas_ = new WaveCanvas(waveformSplitter);
    compareTraceCanvas_ = new TraceCanvas(waveformSplitter);
    compareTraceCanvas_->setVisible(false);
    waveformSplitter->addWidget(canvas_);
    waveformSplitter->addWidget(compareTraceCanvas_);
    waveformSplitter->setStretchFactor(0, 1);
    waveformSplitter->setStretchFactor(1, 1);
    waveformSplitter->setSizes({620, 300});
    setCentralWidget(waveformSplitter);
    canvas_->setDocument(&project_, activeScenario(), &commandStack_);
    connect(canvas_, &WaveCanvas::addLaneRequested, this, &MainWindow::addLane);
    connect(canvas_, &WaveCanvas::selectionChanged, this, &MainWindow::updateSelection);
    connect(canvas_, &WaveCanvas::modelEdited, this, &MainWindow::markEdited);
    connect(canvas_, &WaveCanvas::commandAvailabilityChanged, this, &MainWindow::updateCommandActions);
    connect(canvas_, &WaveCanvas::eventSelected, this, &MainWindow::selectEventRow);
    connect(canvas_, &WaveCanvas::statusMessage, this, [this](const QString& message) {
        statusBar()->showMessage(message);
    });
    connect(
        compareTraceCanvas_,
        &TraceCanvas::cursorChanged,
        this,
        [this](const qint64 tick, const QString& signalId, const QString& value) {
            statusBar()->showMessage(
                tr("Actual %1  %2 = %3")
                    .arg(QString::fromStdString(formatTick(tick, project_.timeBase)))
                    .arg(signalId, value));
        });

    createActions();
    createToolBars();
    createDocks();
    traceWatcher_ = new QFutureWatcher<std::shared_ptr<TraceParseResult>>(this);
    connect(
        traceWatcher_,
        &QFutureWatcher<std::shared_ptr<TraceParseResult>>::finished,
        this,
        &MainWindow::finishTraceImport);
    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setSingleShot(true);
    autosaveTimer_->setInterval(1'500);
    connect(autosaveTimer_, &QTimer::timeout, this, &MainWindow::startAutosave);
    autosaveWatcher_ = new QFutureWatcher<QPair<quint64, QString>>(this);
    connect(
        autosaveWatcher_,
        &QFutureWatcher<QPair<quint64, QString>>::finished,
        this,
        &MainWindow::finishAutosave);
    populateSignalTree();
    populateClockTree();
    populateLinkedResources();
    populateBottomTables();
    updateCommandActions();
    updateWindowTitle();
    statusBar()->showMessage(
        recoveredSnapshot
            ? tr("Recovery snapshot loaded; save to commit it to %1").arg(projectFile_)
            : tr("Ready"));
    loadFirstTraceReference();
}

const Project& MainWindow::project() const noexcept
{
    return project_;
}

void MainWindow::requestCompareMode()
{
    compareModeRequested_ = true;
    if (compareModeAction_) compareModeAction_->setChecked(true);
    compareTraceCanvas_->setVisible(true);
    if (bottomTabs_ && comparePanel_) bottomTabs_->setCurrentWidget(comparePanel_);
    if (traceIndex_) runCompare();
}

void MainWindow::revealLocation(const QString& laneId, const Tick tick)
{
    pendingRevealLaneId_ = laneId;
    pendingRevealTick_ = tick;
    auto resolvedLaneId = laneId;
    if (resolvedLaneId.isEmpty()) {
        const auto* scenario = activeScenario();
        if (scenario && !scenario->lanes.empty()) {
            resolvedLaneId = QString::fromStdString(scenario->lanes.front().id);
        }
    }
    if (!resolvedLaneId.isEmpty()) {
        canvas_->revealLocation(resolvedLaneId, tick);
        canvas_->setFocus(Qt::OtherFocusReason);
    }
    if (traceIndex_) {
        traceCanvas_->revealTick(tick);
        compareTraceCanvas_->revealTick(tick);
    }
}

void MainWindow::openLanePropertiesPreview(const QString& laneId)
{
    editLaneById(laneId);
}

void MainWindow::openEditMenuPreview()
{
    if (editMenu_) {
        editMenu_->popup(mapToGlobal(QPoint(24, 48)));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (confirmDiscardChanges()) {
        if (traceCancelFlag_) traceCancelFlag_->store(true);
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::openProject()
{
    if (!confirmDiscardChanges()) return;
    const auto path = QFileDialog::getOpenFileName(
        this,
        tr("Open Wave Workbench project"),
        QFileInfo(projectFile_).absolutePath(),
        tr("Wave Workbench project (project.wave.json);;Recovery snapshot (*.autosave);;JSON files (*.json)"));
    if (!path.isEmpty()) loadFromPath(path);
}

void MainWindow::saveProject()
{
    if (projectFile_.isEmpty()) {
        saveProjectAs();
        return;
    }
    writeToPath(projectFile_);
}

void MainWindow::saveProjectAs()
{
    const auto suggested = projectFile_.isEmpty()
        ? QStringLiteral("project.wave.json")
        : projectFile_;
    const auto path = QFileDialog::getSaveFileName(
        this,
        tr("Save Wave Workbench project"),
        suggested,
        tr("Wave Workbench project (project.wave.json);;JSON files (*.json)"));
    if (!path.isEmpty()) writeToPath(path);
}

void MainWindow::undo()
{
    if (commandStack_.undo()) {
        invalidateCompareResult();
        dirty_ = true;
        scheduleAutosave();
        canvas_->refreshModel();
        populateSignalTree();
        populateClockTree();
        populateBottomTables();
        updateCommandActions();
        updateWindowTitle();
    }
}

void MainWindow::redo()
{
    if (commandStack_.redo()) {
        invalidateCompareResult();
        dirty_ = true;
        scheduleAutosave();
        canvas_->refreshModel();
        populateSignalTree();
        populateClockTree();
        populateBottomTables();
        updateCommandActions();
        updateWindowTitle();
    }
}

void MainWindow::markEdited()
{
    invalidateCompareResult();
    dirty_ = true;
    scheduleAutosave();
    populateSignalTree();
    populateClockTree();
    populateBottomTables();
    updateCommandActions();
    updateWindowTitle();
}

void MainWindow::updateSelection(const QString& laneId, const qint64 tick)
{
    inspectorTree_->clear();
    const auto* scenario = activeScenario();
    const auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) {
        updateLaneOrderActions();
        return;
    }

    const auto radixName = toString(lane->radix);
    const std::array<std::pair<QString, QString>, 13> rows{{
        {tr("Stable ID"), QString::fromStdString(lane->id)},
        {tr("Name"), QString::fromStdString(lane->name)},
        {tr("Kind"), laneKindText(lane->kind)},
        {tr("Width"), QString::number(lane->width)},
        {tr("Signed"), lane->isSigned ? tr("yes") : tr("no")},
        {tr("Radix"), QString::fromLatin1(radixName.data(), static_cast<qsizetype>(radixName.size()))},
        {tr("Clock domain"), QString::fromStdString(lane->clockDomainId)},
        {tr("Group"), QString::fromStdString(lane->groupId)},
        {tr("Color"), QString::fromStdString(lane->color)},
        {tr("Height"), QString::number(lane->height)},
        {tr("Visible"), lane->visible ? tr("yes") : tr("no")},
        {tr("Segments"), QString::number(lane->segments.size())},
        {tr("Cursor"), QString::fromStdString(formatTick(tick, project_.timeBase))},
    }};
    for (const auto& [name, value] : rows) {
        inspectorTree_->addTopLevelItem(new QTreeWidgetItem({name, value}));
    }
    inspectorTree_->resizeColumnToContents(0);
    updateLaneOrderActions();
}

void MainWindow::updateCommandActions()
{
    undoAction_->setEnabled(commandStack_.canUndo());
    redoAction_->setEnabled(commandStack_.canRedo());
    undoAction_->setText(
        commandStack_.canUndo()
            ? tr("Undo %1").arg(QString::fromStdString(commandStack_.undoDescription()))
            : tr("Undo"));
    redoAction_->setText(
        commandStack_.canRedo()
            ? tr("Redo %1").arg(QString::fromStdString(commandStack_.redoDescription()))
            : tr("Redo"));
    updateLaneOrderActions();
}

void MainWindow::addLane()
{
    auto* scenario = activeScenario();
    if (!scenario) return;
    Lane lane;
    lane.id = makeStableId("lane");
    lane.name = "signal";
    lane.kind = LaneKind::Bit;
    lane.color = "#4fc3f7";
    const auto replacement = promptLaneProperties(
        this,
        project_,
        *scenario,
        lane,
        false);
    if (!replacement) return;
    if (replacement->kind == LaneKind::Clock
        && replacement->clockDomainId.empty()) {
        QMessageBox::warning(
            this,
            tr("Invalid lane"),
            tr("A clock lane must reference a clock domain."));
        return;
    }
    try {
        commandStack_.execute(std::make_unique<AddLaneCommand>(
            *scenario,
            *replacement));
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot add lane"),
            QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    markEdited();
    canvas_->revealLocation(
        QString::fromStdString(replacement->id),
        canvas_->cursorTick());
    selectLaneItem(signalTree_, QString::fromStdString(replacement->id));
    selectLaneItem(groupTree_, QString::fromStdString(replacement->id));
}

void MainWindow::addGroup()
{
    auto* scenario = activeScenario();
    if (!scenario) return;
    Lane group;
    group.id = makeStableId("group");
    group.name = "Group";
    group.kind = LaneKind::Group;
    group.color = "#90a4ae";
    group.height = 40;
    const auto replacement = promptLaneProperties(
        this,
        project_,
        *scenario,
        group,
        true);
    if (!replacement) return;
    try {
        commandStack_.execute(std::make_unique<AddLaneCommand>(
            *scenario,
            *replacement));
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot add group"),
            QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    markEdited();
    canvas_->revealLocation(
        QString::fromStdString(replacement->id),
        canvas_->cursorTick());
    selectLaneItem(signalTree_, QString::fromStdString(replacement->id));
    selectLaneItem(groupTree_, QString::fromStdString(replacement->id));
}

void MainWindow::editSelectedLane()
{
    editLaneById(selectedLaneIdForEditing());
}

QString MainWindow::selectedLaneIdForEditing() const
{
    const auto itemLaneId = [](const QTreeWidgetItem* item) {
        return item ? item->data(0, Qt::UserRole).toString() : QString{};
    };
    if (groupTree_ && groupTree_->hasFocus()) {
        const auto laneId = itemLaneId(groupTree_->currentItem());
        if (!laneId.isEmpty()) return laneId;
    }
    if (signalTree_ && signalTree_->hasFocus()) {
        const auto laneId = itemLaneId(signalTree_->currentItem());
        if (!laneId.isEmpty()) return laneId;
    }
    if (canvas_) {
        const auto laneId = canvas_->selectedLaneId();
        if (!laneId.isEmpty()) return laneId;
    }
    if (signalTree_) {
        const auto laneId = itemLaneId(signalTree_->currentItem());
        if (!laneId.isEmpty()) return laneId;
    }
    return groupTree_ ? itemLaneId(groupTree_->currentItem()) : QString{};
}

void MainWindow::removeSelectedLane()
{
    auto* scenario = activeScenario();
    const auto laneId = selectedLaneIdForEditing();
    const auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) return;

    const auto message = lane->kind == LaneKind::Group
        ? tr("Remove group \"%1\"? Member lanes will remain and become ungrouped.")
              .arg(QString::fromStdString(lane->name))
        : tr("Remove lane \"%1\"? Its events and related relations will also be removed.")
              .arg(QString::fromStdString(lane->name));
    if (QMessageBox::question(
            this,
            tr("Remove lane or group"),
            message,
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }
    try {
        commandStack_.execute(std::make_unique<RemoveLaneCommand>(
            project_,
            *scenario,
            lane->id));
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot remove lane"),
            QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    updateSelection(QString{}, canvas_->cursorTick());
    markEdited();
}

void MainWindow::moveSelectedLaneUp()
{
    moveSelectedLaneBy(-1);
}

void MainWindow::moveSelectedLaneDown()
{
    moveSelectedLaneBy(1);
}

void MainWindow::moveSelectedLaneBy(const int offset)
{
    auto* scenario = activeScenario();
    const auto laneId = selectedLaneIdForEditing();
    if (!scenario || laneId.isEmpty() || offset == 0) return;
    const auto iterator = std::find_if(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [&laneId](const Lane& lane) {
            return lane.id == laneId.toStdString();
        });
    if (iterator == scenario->lanes.end()) return;
    const auto currentIndex = static_cast<std::ptrdiff_t>(
        std::distance(scenario->lanes.begin(), iterator));
    const auto destinationIndex = currentIndex + offset;
    if (destinationIndex < 0
        || destinationIndex >= static_cast<std::ptrdiff_t>(scenario->lanes.size())) {
        statusBar()->showMessage(tr("The selected lane is already at the display boundary."), 4'000);
        updateLaneOrderActions();
        return;
    }
    try {
        commandStack_.execute(std::make_unique<MoveLaneCommand>(
            *scenario,
            laneId.toStdString(),
            static_cast<std::size_t>(destinationIndex)));
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot reorder lane"),
            QString::fromUtf8(exception.what()));
        return;
    }
    const auto tick = canvas_->cursorTick();
    canvas_->refreshModel();
    markEdited();
    canvas_->revealLocation(laneId, tick);
    selectLaneItem(signalTree_, laneId);
    selectLaneItem(groupTree_, laneId);
    updateLaneOrderActions();
}

void MainWindow::updateLaneOrderActions()
{
    if (!moveLaneUpAction_ || !moveLaneDownAction_) return;
    moveLaneUpAction_->setEnabled(false);
    moveLaneDownAction_->setEnabled(false);
    const auto* scenario = activeScenario();
    const auto laneId = selectedLaneIdForEditing().toStdString();
    if (!scenario || laneId.empty()) return;
    const auto iterator = std::find_if(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [&laneId](const Lane& lane) {
            return lane.id == laneId;
        });
    if (iterator == scenario->lanes.end()) return;
    const auto index = std::distance(scenario->lanes.begin(), iterator);
    moveLaneUpAction_->setEnabled(index > 0);
    moveLaneDownAction_->setEnabled(std::next(iterator) != scenario->lanes.end());
}

void MainWindow::editLaneById(const QString& laneId)
{
    auto* scenario = activeScenario();
    auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) return;
    const auto replacement = promptLaneProperties(
        this,
        project_,
        *scenario,
        *lane,
        false);
    if (!replacement) return;
    try {
        commandStack_.execute(std::make_unique<ChangeLaneCommand>(
            project_,
            *scenario,
            lane->id,
            *replacement));
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot change lane"),
            QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    markEdited();
    canvas_->revealLocation(laneId, canvas_->cursorTick());
    selectLaneItem(signalTree_, laneId);
    selectLaneItem(groupTree_, laneId);
}

void MainWindow::editSelectedClock()
{
    auto* scenario = activeScenario();
    const auto* item = clockTree_ ? clockTree_->currentItem() : nullptr;
    if (!scenario || !item) return;
    const auto clockId = item->data(0, Qt::UserRole).toString().toStdString();
    const auto* original = findClock(project_, clockId);
    if (!original) return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit clock domain"));
    auto* layout = new QFormLayout(&dialog);
    auto* name = new QLineEdit(QString::fromStdString(original->name));
    auto* period = new QLineEdit(
        QString::fromStdString(formatTick(original->period, project_.timeBase)));
    auto* phase = new QLineEdit(
        QString::fromStdString(formatTick(original->phase, project_.timeBase)));
    auto* numerator = new QSpinBox;
    numerator->setRange(1, 1'000'000);
    numerator->setValue(static_cast<int>(std::clamp<std::int64_t>(
        original->dutyCycle.numerator,
        1,
        1'000'000)));
    auto* denominator = new QSpinBox;
    denominator->setRange(2, 1'000'000);
    denominator->setValue(static_cast<int>(std::clamp<std::int64_t>(
        original->dutyCycle.denominator,
        2,
        1'000'000)));
    auto* edge = new QComboBox;
    edge->addItem(tr("Rising"), static_cast<int>(ClockEdge::Rising));
    edge->addItem(tr("Falling"), static_cast<int>(ClockEdge::Falling));
    edge->setCurrentIndex(original->activeEdge == ClockEdge::Rising ? 0 : 1);
    auto* reset = new QLineEdit(QString::fromStdString(original->resetRelation));
    layout->addRow(tr("Name"), name);
    layout->addRow(tr("Period"), period);
    layout->addRow(tr("Phase"), phase);
    layout->addRow(tr("Duty numerator"), numerator);
    layout->addRow(tr("Duty denominator"), denominator);
    layout->addRow(tr("Active edge"), edge);
    layout->addRow(tr("Reset / disable condition"), reset);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    QString parseError;
    std::optional<std::int64_t> unusedCycle;
    const auto parsedPeriod = parseTimeText(
        period->text(),
        project_.timeBase,
        nullptr,
        unusedCycle,
        parseError);
    unusedCycle.reset();
    const auto parsedPhase = parseError.isEmpty()
        ? parseTimeText(
            phase->text(),
            project_.timeBase,
            nullptr,
            unusedCycle,
            parseError)
        : std::optional<Tick>{};
    if (!parseError.isEmpty() || !parsedPeriod || *parsedPeriod <= 0 || !parsedPhase) {
        QMessageBox::warning(
            this,
            tr("Invalid clock"),
            parseError.isEmpty()
                ? tr("Clock period must be positive and phase must be an exact integer tick.")
                : parseError);
        return;
    }

    auto replacement = *original;
    replacement.name = name->text().trimmed().toStdString();
    replacement.period = *parsedPeriod;
    replacement.phase = *parsedPhase;
    replacement.dutyCycle = {numerator->value(), denominator->value()};
    replacement.dutyCycle.normalize();
    replacement.activeEdge = static_cast<ClockEdge>(edge->currentData().toInt());
    replacement.resetRelation = reset->text().trimmed().toStdString();
    try {
        commandStack_.execute(std::make_unique<ChangeClockCommand>(
            project_,
            *scenario,
            clockId,
            std::move(replacement)));
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot change clock"),
            QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    markEdited();
}

void MainWindow::addEvent()
{
    auto* scenario = activeScenario();
    if (!scenario) return;
    auto* lane = findLane(*scenario, canvas_->selectedLaneId().toStdString());
    if (!lane || lane->kind == LaneKind::Clock || lane->kind == LaneKind::Group) {
        const auto iterator = std::find_if(
            scenario->lanes.begin(),
            scenario->lanes.end(),
            [](const Lane& candidate) {
                return candidate.kind != LaneKind::Clock
                    && candidate.kind != LaneKind::Group;
            });
        if (iterator == scenario->lanes.end()) return;
        lane = &*iterator;
    }

    auto value = std::string{"0"};
    const auto covering = std::find_if(
        lane->segments.begin(),
        lane->segments.end(),
        [tick = canvas_->cursorTick()](const Segment& segment) {
            return segment.start <= tick && tick < segment.end;
        });
    if (lane->kind == LaneKind::Bit) {
        value = covering != lane->segments.end() && covering->value == "0" ? "1" : "0";
    } else {
        bool accepted = false;
        const auto entered = QInputDialog::getText(
            this,
            tr("Add event"),
            tr("Value for %1:").arg(QString::fromStdString(lane->name)),
            QLineEdit::Normal,
            covering == lane->segments.end()
                ? QStringLiteral("0")
                : QString::fromStdString(covering->value),
            &accepted);
        if (!accepted) return;
        value = entered.toStdString();
    }

    Event event;
    event.id = makeStableId("event");
    event.laneId = lane->id;
    event.tick = canvas_->cursorTick();
    event.action = EventAction::Drive;
    event.value = std::move(value);
    event.clockDomainId = lane->clockDomainId;
    event.description = "Scenario step";
    try {
        commandStack_.execute(std::make_unique<AddEventCommand>(*scenario, event));
    } catch (const std::exception& exception) {
        QMessageBox::warning(this, tr("Cannot add event"), QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    markEdited();
}

void MainWindow::removeSelectedEvent()
{
    auto* scenario = activeScenario();
    const auto row = eventTable_->currentRow();
    if (!scenario || row < 0 || !eventTable_->item(row, 0)) return;
    const auto eventId = eventTable_->item(row, 0)->data(Qt::UserRole).toString();
    if (eventId.isEmpty()) return;
    try {
        commandStack_.execute(std::make_unique<RemoveEventCommand>(
            *scenario,
            eventId.toStdString()));
    } catch (const std::exception& exception) {
        QMessageBox::warning(this, tr("Cannot remove event"), QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    markEdited();
}

void MainWindow::eventCellChanged(const int row, const int column)
{
    if (populatingTables_) return;
    auto* scenario = activeScenario();
    if (!scenario || row < 0 || !eventTable_->item(row, 0)) return;
    const auto eventId = eventTable_->item(row, 0)->data(Qt::UserRole).toString().toStdString();
    const auto* original = findEvent(*scenario, eventId);
    if (!original) {
        populateBottomTables();
        return;
    }
    auto replacement = *original;
    const auto text = eventTable_->item(row, column)
        ? eventTable_->item(row, column)->text().trimmed()
        : QString{};
    QString error;

    switch (column) {
    case 0: {
        const auto* lane = findLane(*scenario, replacement.laneId);
        const auto* clock = !replacement.clockDomainId.empty()
            ? findClock(project_, replacement.clockDomainId)
            : lane ? findClock(project_, lane->clockDomainId) : nullptr;
        const auto tick = parseTimeText(
            text,
            project_.timeBase,
            clock,
            replacement.cycle,
            error);
        if (tick) replacement.tick = *tick;
        break;
    }
    case 1: {
        auto normalized = text.toLower();
        normalized.replace(QLatin1Char(' '), QLatin1Char('-'));
        const auto action = eventActionFromString(normalized.toStdString());
        if (!action) error = tr("Unknown event action.");
        else replacement.action = *action;
        break;
    }
    case 2: {
        const auto iterator = std::find_if(
            scenario->lanes.begin(),
            scenario->lanes.end(),
            [&text](const Lane& lane) {
                return QString::fromStdString(lane.id) == text
                    || QString::fromStdString(lane.name) == text;
            });
        if (iterator == scenario->lanes.end()) error = tr("Unknown target lane.");
        else replacement.laneId = iterator->id;
        break;
    }
    case 3:
        replacement.value = text.toStdString();
        break;
    case 4:
        replacement.expectedResult = text.toStdString();
        break;
    case 5: {
        if (text.isEmpty()) {
            replacement.clockDomainId.clear();
            replacement.cycle.reset();
            break;
        }
        const auto iterator = std::find_if(
            project_.clockDomains.begin(),
            project_.clockDomains.end(),
            [&text](const ClockDomain& clock) {
                return QString::fromStdString(clock.id) == text
                    || QString::fromStdString(clock.name) == text;
            });
        if (iterator == project_.clockDomains.end()) error = tr("Unknown clock domain.");
        else replacement.clockDomainId = iterator->id;
        break;
    }
    case 6:
        replacement.description = text.toStdString();
        break;
    default:
        return;
    }

    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid event edit"), error);
        populateBottomTables();
        return;
    }
    try {
        commandStack_.execute(std::make_unique<ChangeEventCommand>(
            *scenario,
            eventId,
            std::move(replacement)));
    } catch (const std::exception& exception) {
        QMessageBox::warning(this, tr("Invalid event edit"), QString::fromUtf8(exception.what()));
        populateBottomTables();
        return;
    }
    canvas_->refreshModel();
    markEdited();
    selectEventRow(QString::fromStdString(eventId));
}

void MainWindow::revealSelectedEvent()
{
    const auto row = eventTable_->currentRow();
    const auto* scenario = activeScenario();
    if (!scenario || row < 0 || !eventTable_->item(row, 0)) return;
    const auto id = eventTable_->item(row, 0)->data(Qt::UserRole).toString().toStdString();
    const auto* event = findEvent(*scenario, id);
    if (event && !event->laneId.empty()) {
        canvas_->revealLocation(QString::fromStdString(event->laneId), event->tick);
    }
}

void MainWindow::selectEventRow(const QString& eventId)
{
    for (int row = 0; row < eventTable_->rowCount(); ++row) {
        const auto* item = eventTable_->item(row, 0);
        if (item && item->data(Qt::UserRole).toString() == eventId) {
            eventTable_->selectRow(row);
            eventTable_->scrollToItem(item);
            return;
        }
    }
}

void MainWindow::revealValidationIssue(const int row, const int column)
{
    Q_UNUSED(column)
    if (row < 0 || !validationTable_->item(row, 0)) return;
    const auto* item = validationTable_->item(row, 0);
    const auto laneId = item->data(Qt::UserRole).toString();
    const auto tick = item->data(Qt::UserRole + 1).toLongLong();
    if (!laneId.isEmpty()) canvas_->revealLocation(laneId, tick);
}

void MainWindow::relationCellChanged(const int row, const int column)
{
    if (populatingTables_ || row < 0 || !relationTable_->item(row, 0)) return;
    auto* scenario = activeScenario();
    if (!scenario) return;
    const auto relationId = relationTable_->item(row, 0)
                                ->data(Qt::UserRole + 2)
                                .toString()
                                .toStdString();
    const auto* original = findRelation(*scenario, relationId);
    if (!original) {
        populateRelationTable();
        return;
    }
    if (column < 2) {
        populateRelationTable();
        return;
    }

    auto replacement = *original;
    const auto text = relationTable_->item(row, column)
        ? relationTable_->item(row, column)->text().trimmed()
        : QString{};
    QString error;
    const auto* relationClock = replacement.clockDomainId.empty()
        ? nullptr
        : findClock(project_, replacement.clockDomainId);
    switch (column) {
    case 2: {
        const auto delay = parseDelayText(text, project_.timeBase, relationClock, error);
        if (delay) replacement.minimumDelay = *delay;
        break;
    }
    case 3: {
        const auto delay = parseDelayText(text, project_.timeBase, relationClock, error);
        if (delay) replacement.maximumDelay = *delay;
        break;
    }
    case 4: {
        if (text.isEmpty()) {
            replacement.clockDomainId.clear();
            break;
        }
        const auto iterator = std::find_if(
            project_.clockDomains.begin(),
            project_.clockDomains.end(),
            [&text](const ClockDomain& clock) {
                return QString::fromStdString(clock.id) == text
                    || QString::fromStdString(clock.name) == text;
            });
        if (iterator == project_.clockDomains.end()) error = tr("Unknown clock domain.");
        else replacement.clockDomainId = iterator->id;
        break;
    }
    case 5:
        replacement.condition = text.toStdString();
        break;
    case 6: {
        const auto severity = severityFromString(text.toLower().toStdString());
        if (!severity) error = tr("Severity must be information, warning, or error.");
        else replacement.severity = *severity;
        break;
    }
    case 7:
        replacement.description = text.toStdString();
        break;
    default:
        return;
    }
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid relation edit"), error);
        populateRelationTable();
        return;
    }
    try {
        commandStack_.execute(std::make_unique<ChangeRelationCommand>(
            *scenario,
            relationId,
            std::move(replacement)));
    } catch (const std::exception& exception) {
        QMessageBox::warning(this, tr("Invalid relation edit"), QString::fromUtf8(exception.what()));
        populateRelationTable();
        return;
    }
    canvas_->refreshModel();
    markEdited();
}

void MainWindow::removeSelectedRelation()
{
    const auto row = relationTable_->currentRow();
    auto* scenario = activeScenario();
    if (!scenario || row < 0 || !relationTable_->item(row, 0)) return;
    const auto relationId = relationTable_->item(row, 0)
                                ->data(Qt::UserRole + 2)
                                .toString()
                                .toStdString();
    if (relationId.empty()) return;
    try {
        commandStack_.execute(std::make_unique<RemoveRelationCommand>(
            *scenario,
            relationId));
    } catch (const std::exception& exception) {
        QMessageBox::warning(this, tr("Cannot remove relation"), QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    markEdited();
}

void MainWindow::exportArtifacts()
{
    const auto* scenario = activeScenario();
    if (!scenario) return;
    const auto options = requestExportOptions(
        this,
        project_,
        *scenario,
        canvas_->selectedTimeRange());
    if (!options) return;

    const auto suggestedDirectory = projectFile_.isEmpty()
        ? QDir::current().filePath(QStringLiteral("exports"))
        : QFileInfo(projectFile_).absoluteDir().filePath(QStringLiteral("exports"));
    QDir().mkpath(suggestedDirectory);
    const auto directory = QFileDialog::getExistingDirectory(
        this,
        tr("Export Wave Workbench artifacts"),
        suggestedDirectory,
        QFileDialog::ShowDirsOnly);
    if (directory.isEmpty()) return;

    const auto result = generateArtifactBundle(project_, *scenario, *options);
    QStringList diagnosticLines;
    for (const auto& diagnostic : result.diagnostics) {
        const auto code = toString(diagnostic.code);
        diagnosticLines.append(
            tr("%1 [%2] %3")
                .arg(severityText(diagnostic.severity))
                .arg(QString::fromLatin1(code.data(), static_cast<qsizetype>(code.size())))
                .arg(QString::fromStdString(diagnostic.message)));
    }
    if (!result.ok()) {
        auto message = result.error;
        if (!diagnosticLines.isEmpty()) {
            if (!message.isEmpty()) message += QLatin1Char('\n');
            message += diagnosticLines.join(QLatin1Char('\n'));
        }
        QMessageBox::critical(this, tr("Export failed"), message);
        return;
    }

    auto baseName = QString::fromStdString(sanitizeIdentifier(scenario->name));
    if (baseName.isEmpty()) baseName = QStringLiteral("scenario");
    QString writeError;
    if (!writeArtifactBundleAtomic(
            *result.artifacts,
            directory,
            baseName,
            &writeError)) {
        QMessageBox::critical(this, tr("Export failed"), writeError);
        return;
    }
    const auto suffix = diagnosticLines.isEmpty()
        ? QString{}
        : tr("\n\nDiagnostics:\n%1").arg(diagnosticLines.join(QLatin1Char('\n')));
    QMessageBox::information(
        this,
        tr("Export complete"),
        tr("Generated SystemVerilog, SVA, cocotb, SVG, PNG, PDF, and WaveDrom JSON in:\n%1%2")
            .arg(directory, suffix));
}

void MainWindow::importTrace()
{
    if (traceWatcher_ && traceWatcher_->isRunning()) return;
    const auto initialDirectory = projectFile_.isEmpty()
        ? QDir::currentPath()
        : QFileInfo(projectFile_).absolutePath();
    const auto path = QFileDialog::getOpenFileName(
        this,
        tr("Import simulation trace"),
        initialDirectory,
        tr("Supported traces (*.vcd *.csv);;Value Change Dump (*.vcd);;Timestamped CSV (*.csv)"));
    if (path.isEmpty()) return;
    const auto suffix = QFileInfo(path).suffix().toLower();
    const auto format = suffix == QStringLiteral("vcd")
        ? TraceFormat::Vcd
        : suffix == QStringLiteral("csv")
            ? TraceFormat::Csv
            : TraceFormat::Vcd;
    if (suffix != QStringLiteral("vcd") && suffix != QStringLiteral("csv")) {
        QMessageBox::warning(
            this,
            tr("Unsupported trace"),
            tr("Only standard VCD and timestamped CSV files are supported."));
        return;
    }
    startTraceImport(path, format, makeStableId("trace"), 0, true);
}

void MainWindow::cancelTraceImport()
{
    if (!traceWatcher_ || !traceWatcher_->isRunning() || !traceCancelFlag_) return;
    traceCancelFlag_->store(true);
    cancelTraceAction_->setEnabled(false);
    traceSummary_->setText(tr("Cancelling import…"));
}

void MainWindow::alignImportedTrace()
{
    auto* reference = activeTraceReference();
    const auto* scenario = activeScenario();
    if (!reference || !traceIndex_ || !scenario) return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Align imported trace"));
    auto* layout = new QFormLayout(&dialog);
    auto* mode = new QComboBox;
    mode->addItem(tr("Manual offset"), QStringLiteral("manual"));
    mode->addItem(tr("Match marker start"), QStringLiteral("marker"));
    mode->addItem(tr("Match clock edge"), QStringLiteral("clock"));
    auto* manualOffset = new QLineEdit(
        QString::fromStdString(formatTick(reference->offset, project_.timeBase)));
    auto* actualTime = new QLineEdit(
        QString::fromStdString(formatTick(traceIndex_->startTick, project_.timeBase)));
    actualTime->setToolTip(tr("Currently aligned time of the selected actual marker or edge"));
    auto* marker = new QComboBox;
    for (const auto& candidate : scenario->markers) {
        marker->addItem(
            tr("%1 @ %2")
                .arg(QString::fromStdString(candidate.name))
                .arg(QString::fromStdString(formatTick(candidate.start, project_.timeBase))),
            QString::fromStdString(candidate.id));
    }
    auto* clock = new QComboBox;
    for (const auto& candidate : project_.clockDomains) {
        clock->addItem(
            QString::fromStdString(candidate.name),
            QString::fromStdString(candidate.id));
    }
    auto* cycleIndex = new QSpinBox;
    cycleIndex->setRange(-1'000'000'000, 1'000'000'000);
    cycleIndex->setValue(0);
    layout->addRow(tr("Alignment mode"), mode);
    layout->addRow(tr("Resulting offset"), manualOffset);
    layout->addRow(tr("Actual time"), actualTime);
    layout->addRow(tr("Expected marker"), marker);
    layout->addRow(tr("Clock domain"), clock);
    layout->addRow(tr("Clock cycle"), cycleIndex);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    const auto updateFields = [=] {
        const auto selectedMode = mode->currentData().toString();
        manualOffset->setEnabled(selectedMode == QStringLiteral("manual"));
        actualTime->setEnabled(selectedMode != QStringLiteral("manual"));
        marker->setEnabled(selectedMode == QStringLiteral("marker"));
        clock->setEnabled(selectedMode == QStringLiteral("clock"));
        cycleIndex->setEnabled(selectedMode == QStringLiteral("clock"));
    };
    connect(mode, &QComboBox::currentIndexChanged, &dialog, [=](int) {
        updateFields();
    });
    updateFields();
    if (dialog.exec() != QDialog::Accepted) return;

    QString error;
    std::optional<std::int64_t> cycle;
    std::optional<Tick> offset;
    const auto selectedMode = mode->currentData().toString();
    if (selectedMode == QStringLiteral("manual")) {
        offset = parseTimeText(
            manualOffset->text(),
            project_.timeBase,
            nullptr,
            cycle,
            error);
    } else {
        const auto actual = parseTimeText(
            actualTime->text(),
            project_.timeBase,
            nullptr,
            cycle,
            error);
        std::optional<Tick> expected;
        if (actual && selectedMode == QStringLiteral("marker")) {
            const auto markerId = marker->currentData().toString().toStdString();
            const auto iterator = std::find_if(
                scenario->markers.begin(),
                scenario->markers.end(),
                [&markerId](const Marker& candidate) {
                    return candidate.id == markerId;
                });
            if (iterator == scenario->markers.end()) {
                error = tr("The scenario has no selected marker.");
            } else {
                expected = iterator->start;
            }
        } else if (actual) {
            const auto* domain = findClock(
                project_,
                clock->currentData().toString().toStdString());
            if (!domain) {
                error = tr("The project has no selected clock domain.");
            } else {
                expected = tickAtCycle(*domain, cycleIndex->value(), domain->activeEdge);
                if (!expected) error = tr("The selected clock edge is outside the tick range.");
            }
        }
        if (actual && expected) {
            Tick correction = 0;
            Tick alignedOffset = 0;
            if (!checkedDifference(*expected, *actual, correction)
                || !checkedSum(reference->offset, correction, alignedOffset)) {
                error = tr("The alignment calculation exceeds the integer tick range.");
            } else {
                offset = alignedOffset;
            }
        }
    }
    if (!offset || !error.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Invalid alignment"),
            error.isEmpty() ? tr("The offset is invalid.") : error);
        return;
    }
    Tick delta = 0;
    if (!checkedDifference(*offset, reference->offset, delta)
        || !traceIndex_->shift(delta)) {
        QMessageBox::warning(
            this,
            tr("Invalid alignment"),
            tr("The aligned timestamps exceed the integer tick range."));
        return;
    }
    reference->offset = *offset;
    traceCanvas_->refreshTrace();
    compareTraceCanvas_->refreshTrace();
    markEdited();
    statusBar()->showMessage(
        tr("Trace offset set to %1")
            .arg(QString::fromStdString(formatTick(*offset, project_.timeBase))),
        5'000);
}

void MainWindow::autoMapImportedTrace()
{
    auto* reference = activeTraceReference();
    const auto* scenario = activeScenario();
    if (!reference || !scenario || !traceIndex_) return;
    reference->signalMapping = suggestSignalMapping(*scenario, *traceIndex_);
    populateTraceMappingTable();
    traceCanvas_->refreshTrace();
    markEdited();
}

void MainWindow::traceMappingCellChanged(const int row, const int column)
{
    if (populatingTraceMapping_ || row < 0
        || !traceMappingTable_->item(row, 0)) {
        return;
    }
    const auto signalId = traceMappingTable_->item(row, 0)
                              ->data(Qt::UserRole)
                              .toString()
                              .toStdString();
    if (column == 0) {
        if (traceMappingTable_->item(row, 0)->checkState() == Qt::Checked) {
            traceVisibleSignalIds_.insert(signalId);
        } else {
            traceVisibleSignalIds_.erase(signalId);
        }
        traceCanvas_->setVisibleSignalIds(traceVisibleSignalIds_);
        compareTraceCanvas_->setVisibleSignalIds(traceVisibleSignalIds_);
        return;
    }
    if (column != 1) return;
    auto* reference = activeTraceReference();
    const auto* scenario = activeScenario();
    if (!reference || !scenario) return;
    const auto entered = traceMappingTable_->item(row, 1)
        ? traceMappingTable_->item(row, 1)->text().trimmed()
        : QString{};
    const Lane* selectedLane = nullptr;
    if (!entered.isEmpty()) {
        const auto lane = std::find_if(
            scenario->lanes.begin(),
            scenario->lanes.end(),
            [&entered](const Lane& candidate) {
                return QString::fromStdString(candidate.id) == entered
                    || QString::fromStdString(candidate.name) == entered;
            });
        if (lane == scenario->lanes.end()) {
            QMessageBox::warning(
                this,
                tr("Unknown expected lane"),
                tr("Enter an existing lane name or stable ID."));
            populateTraceMappingTable();
            return;
        }
        selectedLane = &*lane;
    }
    for (auto iterator = reference->signalMapping.begin();
         iterator != reference->signalMapping.end();) {
        if (iterator->second == signalId) iterator = reference->signalMapping.erase(iterator);
        else ++iterator;
    }
    if (selectedLane) reference->signalMapping[selectedLane->id] = signalId;
    traceCanvas_->refreshTrace();
    compareTraceCanvas_->refreshTrace();
    markEdited();
}

void MainWindow::finishTraceImport()
{
    importTraceAction_->setEnabled(true);
    cancelTraceAction_->setEnabled(false);
    traceProgress_->setVisible(false);
    const auto reloadAfter = std::exchange(reloadTraceAfterCurrent_, false);
    const auto reloadIfNeeded = [this, reloadAfter] {
        if (reloadAfter) loadFirstTraceReference();
    };
    const auto parsed = traceWatcher_->result();
    if (!parsed) {
        traceSummary_->setText(tr("Import failed"));
        reloadIfNeeded();
        return;
    }
    if (parsed->cancelled) {
        traceSummary_->setText(tr("Import cancelled"));
        statusBar()->showMessage(tr("Trace import cancelled"), 5'000);
        reloadIfNeeded();
        return;
    }
    if (!parsed->ok()) {
        traceSummary_->setText(tr("Import failed"));
        QMessageBox::critical(
            this,
            tr("Trace import failed"),
            QString::fromStdString(parsed->errorSummary()));
        reloadIfNeeded();
        return;
    }
    if (parsed->index->identity.projectId != project_.id
        || parsed->index->identity.traceId != pendingTraceId_
        || parsed->index->identity.generation != traceGeneration_) {
        statusBar()->showMessage(tr("Discarded an obsolete trace import result"), 5'000);
        reloadIfNeeded();
        return;
    }

    if (pendingTraceAddsReference_) {
        ImportedTrace reference;
        reference.id = pendingTraceId_;
        reference.path = storedTracePath(pendingTracePath_).toUtf8().toStdString();
        reference.format = std::string(toString(pendingTraceFormat_));
        reference.offset = pendingTraceOffset_;
        if (const auto* scenario = activeScenario()) {
            reference.signalMapping = suggestSignalMapping(*scenario, *parsed->index);
        }
        project_.importedTraces.push_back(std::move(reference));
        dirty_ = true;
        scheduleAutosave();
        updateWindowTitle();
    } else if (!activeTraceReference()) {
        statusBar()->showMessage(tr("Imported trace reference no longer exists"), 5'000);
        reloadIfNeeded();
        return;
    }

    traceIndex_ = std::move(*parsed->index);
    activeTraceId_ = pendingTraceId_;
    traceVisibleSignalIds_.clear();
    for (const auto& signal : traceIndex_->traceSignals) {
        traceVisibleSignalIds_.insert(signal.id);
    }
    traceCanvas_->setTrace(&project_, activeScenario(), &*traceIndex_, activeTraceReference());
    traceCanvas_->setVisibleSignalIds(traceVisibleSignalIds_);
    compareTraceCanvas_->setTrace(
        &project_,
        activeScenario(),
        &*traceIndex_,
        activeTraceReference());
    compareTraceCanvas_->setVisibleSignalIds(traceVisibleSignalIds_);
    if (pendingRevealTick_) {
        traceCanvas_->revealTick(*pendingRevealTick_);
        compareTraceCanvas_->revealTick(*pendingRevealTick_);
    }
    invalidateCompareResult();
    populateTraceMappingTable();
    bottomTabs_->setCurrentWidget(tracePanel_);
    traceSummary_->setText(
        tr("%1 signals, %2 transitions")
            .arg(traceIndex_->traceSignals.size())
            .arg(traceIndex_->transitionCount));

    QStringList warnings;
    for (const auto& diagnostic : parsed->diagnostics) {
        if (diagnostic.severity == TraceDiagnosticSeverity::Warning) {
            warnings.append(QString::fromStdString(diagnostic.message));
        }
    }
    statusBar()->showMessage(
        warnings.isEmpty()
            ? tr("Imported %1").arg(pendingTracePath_)
            : tr("Imported with warnings: %1").arg(warnings.join(QStringLiteral("; "))),
        10'000);
    if (compareModeRequested_) runCompare();
    reloadIfNeeded();
}

void MainWindow::startTraceImport(
    const QString& path,
    const TraceFormat format,
    std::string traceId,
    const Tick offset,
    const bool addReference)
{
    if (!traceWatcher_ || traceWatcher_->isRunning()) return;
    pendingTracePath_ = QFileInfo(path).absoluteFilePath();
    pendingTraceId_ = std::move(traceId);
    pendingTraceFormat_ = format;
    pendingTraceOffset_ = offset;
    pendingTraceAddsReference_ = addReference;
    ++traceGeneration_;
    traceCancelFlag_ = std::make_shared<std::atomic_bool>(false);
    const auto cancelFlag = traceCancelFlag_;
    TraceParseOptions options;
    options.projectTimeBase = project_.timeBase;
    options.identity = {project_.id, pendingTraceId_, traceGeneration_};
    options.offset = offset;
    options.isCancelled = [cancelFlag] {
        return cancelFlag->load();
    };
    const auto pathValue = nativePath(pendingTracePath_);
    importTraceAction_->setEnabled(false);
    cancelTraceAction_->setEnabled(true);
    traceProgress_->setVisible(true);
    traceSummary_->setText(tr("Parsing %1…").arg(QFileInfo(path).fileName()));
    const auto future = QtConcurrent::run(
        [pathValue, format, options]() mutable {
            auto result = format == TraceFormat::Vcd
                ? parseVcdFile(pathValue, options)
                : parseCsvFile(pathValue, options);
            return std::make_shared<TraceParseResult>(std::move(result));
        });
    traceWatcher_->setFuture(future);
}

void MainWindow::loadFirstTraceReference()
{
    traceIndex_.reset();
    activeTraceId_.clear();
    traceVisibleSignalIds_.clear();
    populateTraceMappingTable();
    if (traceCanvas_) traceCanvas_->setTrace(&project_, activeScenario(), nullptr, nullptr);
    if (compareTraceCanvas_) {
        compareTraceCanvas_->setTrace(&project_, activeScenario(), nullptr, nullptr);
    }
    invalidateCompareResult();
    if (project_.importedTraces.empty()) {
        if (traceSummary_) traceSummary_->setText(tr("No imported trace"));
        return;
    }
    const auto& reference = project_.importedTraces.front();
    const auto formatText = QString::fromStdString(reference.format).toLower();
    if (formatText != QStringLiteral("vcd") && formatText != QStringLiteral("csv")) {
        traceSummary_->setText(tr("Unsupported trace reference"));
        return;
    }
    const auto path = resolvedTracePath(reference);
    if (!QFileInfo::exists(path)) {
        traceSummary_->setText(tr("Missing trace: %1").arg(path));
        return;
    }
    activeTraceId_ = reference.id;
    startTraceImport(
        path,
        formatText == QStringLiteral("vcd") ? TraceFormat::Vcd : TraceFormat::Csv,
        reference.id,
        reference.offset,
        false);
}

void MainWindow::runCompare()
{
    const auto* scenario = activeScenario();
    const auto* reference = activeTraceReference();
    if (!scenario || !reference || !traceIndex_) {
        QMessageBox::warning(
            this,
            tr("Compare unavailable"),
            tr("Import and map an actual VCD or CSV trace before comparing."));
        return;
    }
    QString parseError;
    std::optional<std::int64_t> cycle;
    const auto tolerance = parseTimeText(
        compareToleranceEdit_->text(),
        project_.timeBase,
        nullptr,
        cycle,
        parseError);
    if (!tolerance || *tolerance < 0) {
        QMessageBox::warning(
            this,
            tr("Invalid edge tolerance"),
            parseError.isEmpty()
                ? tr("Edge tolerance must be a non-negative integer time.")
                : parseError);
        return;
    }

    CompareOptions options;
    options.defaultRule.xHandling = static_cast<XHandling>(
        compareXCombo_->currentData().toInt());
    options.defaultRule.edgeTolerance = *tolerance;
    options.defaultRule.busMask = compareMaskEdit_->text().trimmed().toStdString();
    options.relationOnly = compareRelationOnly_->isChecked();
    if (compareSelectionOnly_->isChecked()) {
        const auto selection = canvas_->selectedTimeRange();
        if (!selection || selection->second <= selection->first) {
            QMessageBox::warning(
                this,
                tr("No compare selection"),
                tr("Create a non-empty time selection on the Expected canvas first."));
            return;
        }
        options.start = selection->first;
        options.end = selection->second;
    }
    compareResult_ = compareScenario(
        project_,
        *scenario,
        *traceIndex_,
        *reference,
        options);
    populateCompareTable();
    std::vector<std::pair<Tick, Tick>> ranges;
    ranges.reserve(compareResult_->differences.size());
    for (const auto& difference : compareResult_->differences) {
        ranges.emplace_back(difference.start, difference.end);
    }
    traceCanvas_->setDifferenceRanges(ranges);
    compareTraceCanvas_->setDifferenceRanges(std::move(ranges));
    compareTraceCanvas_->setVisible(true);
    if (compareModeAction_) compareModeAction_->setChecked(true);
    bottomTabs_->setCurrentWidget(comparePanel_);
}

void MainWindow::revealCompareDifference(const int row, const int column)
{
    Q_UNUSED(column)
    if (!compareResult_ || row < 0
        || row >= static_cast<int>(compareResult_->differences.size())) {
        return;
    }
    const auto& difference = compareResult_->differences.at(static_cast<std::size_t>(row));
    if (!difference.laneId.empty()) {
        canvas_->revealLocation(QString::fromStdString(difference.laneId), difference.start);
    }
    traceCanvas_->revealTick(difference.start);
    compareTraceCanvas_->revealTick(difference.start);
    compareTraceCanvas_->setVisible(true);
    if (compareModeAction_) compareModeAction_->setChecked(true);
    statusBar()->showMessage(
        tr("First selected difference at %1")
            .arg(QString::fromStdString(formatTick(difference.start, project_.timeBase))),
        5'000);
}

void MainWindow::exportCompareReport()
{
    const auto* scenario = activeScenario();
    if (!scenario || !compareResult_) {
        QMessageBox::warning(
            this,
            tr("No compare result"),
            tr("Run Expected/Actual compare before exporting a report."));
        return;
    }
    const auto suggestedDirectory = projectFile_.isEmpty()
        ? QDir::current().filePath(QStringLiteral("compare"))
        : QFileInfo(projectFile_).absoluteDir().filePath(QStringLiteral("compare"));
    QDir().mkpath(suggestedDirectory);
    const auto directory = QFileDialog::getExistingDirectory(
        this,
        tr("Export compare report"),
        suggestedDirectory,
        QFileDialog::ShowDirsOnly);
    if (directory.isEmpty()) return;
    auto baseName = QString::fromStdString(sanitizeIdentifier(scenario->name));
    if (baseName.isEmpty()) baseName = QStringLiteral("scenario");
    const std::array<std::pair<QString, QByteArray>, 3> reports{{
        {
            baseName + QStringLiteral(".compare.json"),
            QByteArray::fromStdString(compareResultJson(*compareResult_)),
        },
        {
            baseName + QStringLiteral(".compare.csv"),
            QByteArray::fromStdString(compareResultCsv(*compareResult_)),
        },
        {
            baseName + QStringLiteral(".compare.html"),
            QByteArray::fromStdString(compareResultHtml(
                *compareResult_,
                project_,
                *scenario)),
        },
    }};
    for (const auto& [name, content] : reports) {
        QSaveFile file(QDir(directory).filePath(name));
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)
            || file.write(content) != content.size()
            || !file.commit()) {
            QMessageBox::critical(
                this,
                tr("Report export failed"),
                tr("Cannot safely write %1: %2").arg(name, file.errorString()));
            return;
        }
    }
    QMessageBox::information(
        this,
        tr("Compare report exported"),
        tr("Generated JSON, CSV, and HTML reports in:\n%1").arg(directory));
}

void MainWindow::exportPinloomEntry()
{
    const auto* scenario = activeScenario();
    if (!scenario) return;
    const auto initialDirectory = projectFile_.isEmpty()
        ? QDir::currentPath()
        : QFileInfo(projectFile_).absoluteDir().filePath(QStringLiteral("exports"));
    const auto artifactDirectory = QFileDialog::getExistingDirectory(
        this,
        tr("Select exported artifacts for Pinloom"),
        initialDirectory,
        QFileDialog::ShowDirsOnly);
    if (artifactDirectory.isEmpty()) return;
    const auto manifestPath = QFileDialog::getSaveFileName(
        this,
        tr("Save Pinloom archive entry"),
        QDir(artifactDirectory).filePath(QStringLiteral("wave-workbench.pinloom.json")),
        tr("Pinloom archive entry (*.json)"));
    if (manifestPath.isEmpty()) return;
    const auto entry = makePinloomEntry(
        project_,
        *scenario,
        projectFile_,
        artifactDirectory,
        manifestPath);
    QSaveFile file(manifestPath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(entry.document) != entry.document.size()
        || !file.commit()) {
        QMessageBox::critical(
            this,
            tr("Pinloom entry failed"),
            tr("Cannot safely write the archive entry: %1").arg(file.errorString()));
        return;
    }
    QMessageBox message(
        QMessageBox::Information,
        tr("Pinloom entry created"),
        tr("Archive entry:\n%1\n\nURI:\n%2")
            .arg(manifestPath, entry.archiveUri.toString(QUrl::FullyEncoded)),
        QMessageBox::NoButton,
        this);
    auto* openButton = message.addButton(tr("Open in Pinloom"), QMessageBox::AcceptRole);
    message.addButton(tr("Close"), QMessageBox::RejectRole);
    message.exec();
    if (message.clickedButton() == openButton
        && !QDesktopServices::openUrl(entry.archiveUri)) {
        QMessageBox::warning(
            this,
            tr("Cannot open Pinloom"),
            tr("No application accepted the pinloom URI. The archive entry remains available."));
    }
}

void MainWindow::scheduleAutosave()
{
    ++autosaveGeneration_;
    if (!dirty_ || projectFile_.isEmpty() || !autosaveTimer_) return;
    if (autosaveWatcher_ && autosaveWatcher_->isRunning()) {
        autosavePending_ = true;
        return;
    }
    autosaveTimer_->start();
}

void MainWindow::startAutosave()
{
    if (!dirty_ || projectFile_.isEmpty() || !autosaveWatcher_) return;
    if (autosaveWatcher_->isRunning()) {
        autosavePending_ = true;
        return;
    }
    autosavePending_ = false;
    const auto generation = autosaveGeneration_;
    auto snapshot = project_;
    const auto path = autosavePathForProject(projectFile_);
    const auto future = QtConcurrent::run(
        [snapshot = std::move(snapshot), path, generation]() mutable {
            QString error;
            if (!saveProjectFileAtomic(snapshot, path, &error)) {
                if (error.isEmpty()) error = QObject::tr("Autosave write failed.");
            }
            return QPair<quint64, QString>{generation, error};
        });
    autosaveWatcher_->setFuture(future);
}

void MainWindow::finishAutosave()
{
    const auto result = autosaveWatcher_->result();
    const auto stale = result.first != autosaveGeneration_;
    if (!result.second.isEmpty()) {
        statusBar()->showMessage(
            tr("Autosave recovery snapshot failed: %1").arg(result.second),
            10'000);
    } else if (!stale) {
        statusBar()->showMessage(tr("Autosaved recovery snapshot"), 3'000);
    }
    if (autosavePending_ || stale) {
        autosavePending_ = false;
        if (dirty_ && !projectFile_.isEmpty()) autosaveTimer_->start(0);
    }
}

void MainWindow::createActions()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* openAction = fileMenu->addAction(
        themedIcon(QStringLiteral("document-open"), style(), QStyle::SP_DialogOpenButton),
        tr("&Open…"),
        QKeySequence::Open,
        this,
        &MainWindow::openProject);
    openAction->setToolTip(tr("Open a project.wave.json file"));
    auto* saveAction = fileMenu->addAction(
        themedIcon(QStringLiteral("document-save"), style(), QStyle::SP_DialogSaveButton),
        tr("&Save"),
        QKeySequence::Save,
        this,
        &MainWindow::saveProject);
    saveAction->setToolTip(tr("Safely save the current project"));
    fileMenu->addAction(tr("Save &As…"), QKeySequence::SaveAs, this, &MainWindow::saveProjectAs);
    fileMenu->addSeparator();
    importTraceAction_ = fileMenu->addAction(
        themedIcon(QStringLiteral("document-import"), style(), QStyle::SP_DialogOpenButton),
        tr("&Import VCD/CSV Trace…"),
        this,
        &MainWindow::importTrace);
    importTraceAction_->setToolTip(tr("Parse a standard VCD or timestamped CSV trace in the background"));
    pinloomAction_ = fileMenu->addAction(
        themedIcon(QStringLiteral("document-export"), style(), QStyle::SP_DirLinkIcon),
        tr("Export and Open in &Pinloom…"),
        this,
        &MainWindow::exportPinloomEntry);
    pinloomAction_->setToolTip(tr("Create a file-based Pinloom archive entry, then open its URI"));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    editMenu_ = menuBar()->addMenu(tr("&Edit"));
    undoAction_ = editMenu_->addAction(
        themedIcon(QStringLiteral("edit-undo"), style(), QStyle::SP_ArrowBack),
        tr("Undo"),
        QKeySequence::Undo,
        this,
        &MainWindow::undo);
    undoAction_->setToolTip(tr("Undo the last edit"));
    redoAction_ = editMenu_->addAction(
        themedIcon(QStringLiteral("edit-redo"), style(), QStyle::SP_ArrowForward),
        tr("Redo"),
        QKeySequence::Redo,
        this,
        &MainWindow::redo);
    redoAction_->setToolTip(tr("Redo the last reverted edit"));
    editMenu_->addSeparator();
    auto* copyAction = editMenu_->addAction(tr("&Copy range"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setToolTip(tr("Copy the selected time range across all selected lanes"));
    connect(copyAction, &QAction::triggered, canvas_, &WaveCanvas::copySelection);
    auto* pasteAction = editMenu_->addAction(tr("&Paste range"));
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setToolTip(tr("Paste the copied range at the current cursor as one undo command"));
    connect(pasteAction, &QAction::triggered, canvas_, &WaveCanvas::pasteAtCursor);
    editMenu_->addSeparator();
    auto* addLaneAction = editMenu_->addAction(
        tr("Add &lane…"),
        this,
        &MainWindow::addLane);
    addLaneAction->setToolTip(tr("Add a signal lane with editable structural properties"));
    auto* addGroupAction = editMenu_->addAction(
        tr("Add &group…"),
        this,
        &MainWindow::addGroup);
    addGroupAction->setToolTip(tr("Add a stable-ID signal group"));
    auto* editLaneAction = editMenu_->addAction(
        tr("Lane / group &properties…"),
        this,
        &MainWindow::editSelectedLane);
    editLaneAction->setToolTip(tr("Edit the selected lane or group without changing its stable ID"));
    moveLaneUpAction_ = editMenu_->addAction(
        themedIcon(QStringLiteral("go-up"), style(), QStyle::SP_ArrowUp),
        tr("Move selected lane &up"),
        this,
        &MainWindow::moveSelectedLaneUp);
    moveLaneUpAction_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    moveLaneUpAction_->setToolTip(
        tr("Move the selected lane or group one position earlier in display order"));
    moveLaneDownAction_ = editMenu_->addAction(
        themedIcon(QStringLiteral("go-down"), style(), QStyle::SP_ArrowDown),
        tr("Move selected lane &down"),
        this,
        &MainWindow::moveSelectedLaneDown);
    moveLaneDownAction_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Down));
    moveLaneDownAction_->setToolTip(
        tr("Move the selected lane or group one position later in display order"));
    auto* removeLaneAction = editMenu_->addAction(
        tr("&Remove selected lane / group…"),
        this,
        &MainWindow::removeSelectedLane);
    removeLaneAction->setToolTip(
        tr("Remove the selected lane and dependent scenario references as one undo command"));
}

void MainWindow::createToolBars()
{
    auto* modeBar = addToolBar(tr("Modes"));
    modeBar->setObjectName(QStringLiteral("ModeToolbar"));
    modeBar->setMovable(false);
    modeBar->setStyleSheet(QStringLiteral(
        "QToolButton{padding:5px 10px;border:1px solid transparent;border-radius:4px;}"
        "QToolButton:checked{background:#245f9e;color:white;border-color:#1b4b7d;}"));
    auto* modeGroup = new QActionGroup(modeBar);
    modeGroup->setExclusive(true);
    const std::array<std::pair<QString, QString>, 4> modes{{
        {tr("Draw"), QStringLiteral("draw")},
        {tr("Scenario"), QStringLiteral("scenario")},
        {tr("Compare"), QStringLiteral("compare")},
        {tr("Export"), QStringLiteral("export")},
    }};
    for (const auto& [label, mode] : modes) {
        auto* action = modeBar->addAction(label);
        action->setCheckable(true);
        action->setData(mode);
        modeGroup->addAction(action);
        if (mode == QStringLiteral("compare")) compareModeAction_ = action;
        connect(action, &QAction::triggered, this, [this, mode](const bool checked) {
            if (!checked) return;
            const auto compareMode = mode == QStringLiteral("compare");
            compareModeRequested_ = compareMode;
            compareTraceCanvas_->setVisible(compareMode);
            if (compareMode) {
                if (bottomTabs_ && comparePanel_) bottomTabs_->setCurrentWidget(comparePanel_);
                if (traceIndex_) runCompare();
            }
        });
    }
    modeGroup->actions().front()->setChecked(true);
    modeBar->addSeparator();

    auto* editBar = addToolBar(tr("Waveform tools"));
    editBar->setObjectName(QStringLiteral("WaveformToolbar"));
    editBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    editBar->addAction(undoAction_);
    editBar->addAction(redoAction_);
    editBar->addSeparator();

    auto* toolGroup = new QActionGroup(editBar);
    toolGroup->setExclusionPolicy(
        QActionGroup::ExclusionPolicy::ExclusiveOptional);
    selectAction_ = editBar->addAction(
        themedIcon(QStringLiteral("edit-select"), style(), QStyle::SP_FileDialogDetailedView),
        tr("Selection"));
    selectAction_->setCheckable(true);
    selectAction_->setChecked(true);
    selectAction_->setToolTip(tr("Select lanes and waveform positions"));
    drawAction_ = editBar->addAction(
        themedIcon(QStringLiteral("draw-freehand"), style(), QStyle::SP_DialogApplyButton),
        tr("Draw"));
    drawAction_->setCheckable(true);
    drawAction_->setToolTip(tr("Draw a clock override or bit/bus/transaction value range"));
    transitionAction_ = editBar->addAction(
        themedIcon(QStringLiteral("transform-move"), style(), QStyle::SP_ArrowRight),
        tr("Transition"));
    transitionAction_->setCheckable(true);
    transitionAction_->setToolTip(tr("Move a waveform transition"));
    markerAction_ = editBar->addAction(
        themedIcon(QStringLiteral("flag"), style(), QStyle::SP_DialogYesButton),
        tr("Cursor"));
    markerAction_->setObjectName(QStringLiteral("CursorToolAction"));
    markerAction_->setCheckable(true);
    markerAction_->setToolTip(tr("Create, measure, lock, move, or remove time cursors"));
    relationAction_ = editBar->addAction(
        themedIcon(QStringLiteral("insert-link"), style(), QStyle::SP_FileLinkIcon),
        tr("Relation"));
    relationAction_->setCheckable(true);
    relationAction_->setToolTip(tr("Create a timing relation"));
    for (auto* action : {selectAction_, drawAction_, transitionAction_, markerAction_, relationAction_}) {
        toolGroup->addAction(action);
    }
    connect(selectAction_, &QAction::triggered, this, [this](const bool checked) {
        if (!checked) {
            selectAction_->setChecked(true);
            return;
        }
        canvas_->setTool(WaveCanvas::Tool::Selection);
    });
    connect(drawAction_, &QAction::triggered, this, [this](const bool checked) {
        if (!checked) {
            drawAction_->setChecked(true);
            return;
        }
        canvas_->setTool(WaveCanvas::Tool::Draw);
    });
    connect(transitionAction_, &QAction::triggered, this, [this](const bool checked) {
        if (!checked) {
            transitionAction_->setChecked(true);
            return;
        }
        canvas_->setTool(WaveCanvas::Tool::Transition);
    });
    connect(markerAction_, &QAction::triggered, this, [this](const bool checked) {
        if (checked) {
            canvas_->setTool(WaveCanvas::Tool::Marker);
        } else {
            selectAction_->setChecked(true);
            canvas_->setTool(WaveCanvas::Tool::Selection);
        }
    });
    connect(relationAction_, &QAction::triggered, this, [this](const bool checked) {
        if (!checked) {
            relationAction_->setChecked(true);
            return;
        }
        canvas_->setTool(WaveCanvas::Tool::Relation);
    });
    auto* pulseAction = editBar->addAction(
        themedIcon(QStringLiteral("insert-object"), style(), QStyle::SP_MediaSeekForward),
        tr("Pulse"),
        canvas_,
        &WaveCanvas::insertPulse);
    pulseAction->setToolTip(tr("Insert a one-clock pulse on the selected bit lane"));

    editBar->addSeparator();
    snapCombo_ = new QComboBox(editBar);
    snapCombo_->setToolTip(tr("Choose waveform snapping mode"));
    const std::array<std::pair<QString, SnapMode>, 7> snapModes{{
        {tr("No snap"), SnapMode::None},
        {tr("Fixed grid"), SnapMode::FixedGrid},
        {tr("Major tick"), SnapMode::MajorTick},
        {tr("Clock rising"), SnapMode::ClockRising},
        {tr("Clock falling"), SnapMode::ClockFalling},
        {tr("Signal edge"), SnapMode::SignalEdge},
        {tr("Marker"), SnapMode::Marker},
    }};
    for (const auto& [label, mode] : snapModes) {
        snapCombo_->addItem(label, static_cast<int>(mode));
    }
    snapCombo_->setCurrentIndex(1);
    connect(snapCombo_, &QComboBox::currentIndexChanged, this, [this](const int index) {
        canvas_->setSnapMode(static_cast<SnapMode>(snapCombo_->itemData(index).toInt()));
    });
    editBar->addWidget(snapCombo_);
    editBar->addSeparator();
    auto* zoomInAction = editBar->addAction(
        themedIcon(QStringLiteral("zoom-in"), style(), QStyle::SP_ArrowUp),
        tr("Zoom in"));
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, canvas_, &WaveCanvas::zoomIn);
    zoomInAction->setToolTip(tr("Zoom in around the viewport center"));
    auto* zoomOutAction = editBar->addAction(
        themedIcon(QStringLiteral("zoom-out"), style(), QStyle::SP_ArrowDown),
        tr("Zoom out"));
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, canvas_, &WaveCanvas::zoomOut);
    zoomOutAction->setToolTip(tr("Zoom out around the viewport center"));
    auto* fitAction = editBar->addAction(
        themedIcon(QStringLiteral("zoom-fit-best"), style(), QStyle::SP_DesktopIcon),
        tr("Fit scenario"),
        canvas_,
        &WaveCanvas::fitScenario);
    fitAction->setToolTip(tr("Fit the complete scenario"));
    auto* fitSelectionAction = editBar->addAction(
        themedIcon(QStringLiteral("zoom-fit-width"), style(), QStyle::SP_TitleBarMaxButton),
        tr("Fit selection"),
        canvas_,
        &WaveCanvas::fitSelection);
    fitSelectionAction->setToolTip(tr("Fit the selected time range, or the complete scenario when empty"));
    exportAction_ = editBar->addAction(
        themedIcon(QStringLiteral("document-export"), style(), QStyle::SP_DialogSaveButton),
        tr("Export"),
        this,
        &MainWindow::exportArtifacts);
    exportAction_->setToolTip(tr("Generate verification code and document waveform artifacts"));
}

void MainWindow::createDocks()
{
    auto* navigatorTabs = new QTabWidget;
    signalTree_ = new QTreeWidget;
    signalTree_->setHeaderLabels({tr("Signal"), tr("Type")});
    signalTree_->setRootIsDecorated(false);
    signalTree_->setAlternatingRowColors(true);
    connect(
        signalTree_,
        &QTreeWidget::itemClicked,
        this,
        [this](QTreeWidgetItem* item, int) {
            const auto laneId = item ? item->data(0, Qt::UserRole).toString() : QString{};
            if (!laneId.isEmpty()) canvas_->revealLocation(laneId, canvas_->cursorTick());
        });
    connect(
        signalTree_,
        &QTreeWidget::itemDoubleClicked,
        this,
        [this](QTreeWidgetItem* item, int) {
            if (item) editLaneById(item->data(0, Qt::UserRole).toString());
        });
    clockTree_ = new QTreeWidget;
    clockTree_->setHeaderLabels({tr("Clock"), tr("Period")});
    clockTree_->setRootIsDecorated(false);
    connect(
        clockTree_,
        &QTreeWidget::itemDoubleClicked,
        this,
        [this](QTreeWidgetItem*, int) { editSelectedClock(); });
    groupTree_ = new QTreeWidget;
    groupTree_->setHeaderLabels({tr("Group / signal"), tr("Type")});
    groupTree_->setRootIsDecorated(true);
    groupTree_->setAlternatingRowColors(true);
    connect(
        groupTree_,
        &QTreeWidget::itemClicked,
        this,
        [this](QTreeWidgetItem* item, int) {
            const auto laneId = item ? item->data(0, Qt::UserRole).toString() : QString{};
            if (!laneId.isEmpty()) canvas_->revealLocation(laneId, canvas_->cursorTick());
        });
    connect(
        groupTree_,
        &QTreeWidget::itemDoubleClicked,
        this,
        [this](QTreeWidgetItem* item, int) {
            if (item) editLaneById(item->data(0, Qt::UserRole).toString());
        });
    navigatorTabs->addTab(signalTree_, tr("Signals"));
    navigatorTabs->addTab(clockTree_, tr("Clocks"));
    navigatorTabs->addTab(groupTree_, tr("Groups"));
    resourceTree_ = new QTreeWidget;
    resourceTree_->setHeaderLabels({tr("Linked resource"), tr("Status")});
    resourceTree_->setRootIsDecorated(false);
    resourceTree_->setAlternatingRowColors(true);
    connect(resourceTree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item) return;
        const auto path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    navigatorTabs->addTab(resourceTree_, tr("Resources"));
    auto* leftDock = new QDockWidget(tr("Project"), this);
    leftDock->setObjectName(QStringLiteral("ProjectDock"));
    leftDock->setWidget(navigatorTabs);
    leftDock->setMinimumWidth(220);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    inspectorTree_ = new QTreeWidget;
    inspectorTree_->setHeaderLabels({tr("Property"), tr("Value")});
    inspectorTree_->setRootIsDecorated(false);
    inspectorTree_->setAlternatingRowColors(true);
    auto* rightDock = new QDockWidget(tr("Inspector"), this);
    rightDock->setObjectName(QStringLiteral("InspectorDock"));
    rightDock->setWidget(inspectorTree_);
    rightDock->setMinimumWidth(260);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

    bottomTabs_ = new QTabWidget;
    auto* bottomTabs = bottomTabs_;
    eventTable_ = new QTableWidget;
    eventTable_->setColumnCount(7);
    eventTable_->setHorizontalHeaderLabels({
        tr("Time / cycle"),
        tr("Action"),
        tr("Target"),
        tr("Value"),
        tr("Expected result"),
        tr("Clock domain"),
        tr("Description"),
    });
    eventTable_->horizontalHeader()->setStretchLastSection(true);
    eventTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    eventTable_->setSortingEnabled(true);
    connect(eventTable_, &QTableWidget::cellChanged, this, &MainWindow::eventCellChanged);
    connect(eventTable_, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        revealSelectedEvent();
    });
    auto* eventPanel = new QWidget;
    auto* eventLayout = new QVBoxLayout(eventPanel);
    eventLayout->setContentsMargins(0, 0, 0, 0);
    eventLayout->setSpacing(0);
    auto* eventBar = new QToolBar;
    eventBar->setIconSize(QSize(16, 16));
    eventBar->addAction(
        themedIcon(QStringLiteral("list-add"), style(), QStyle::SP_FileDialogNewFolder),
        tr("Add event"),
        this,
        &MainWindow::addEvent);
    eventBar->addAction(
        themedIcon(QStringLiteral("list-remove"), style(), QStyle::SP_TrashIcon),
        tr("Remove event"),
        this,
        &MainWindow::removeSelectedEvent);
    auto* eventFilter = new QLineEdit;
    eventFilter->setClearButtonEnabled(true);
    eventFilter->setPlaceholderText(tr("Filter events"));
    eventFilter->setMaximumWidth(240);
    eventBar->addSeparator();
    eventBar->addWidget(eventFilter);
    connect(eventFilter, &QLineEdit::textChanged, this, [this](const QString& filter) {
        for (int row = 0; row < eventTable_->rowCount(); ++row) {
            bool matches = filter.isEmpty();
            for (int column = 0; !matches && column < eventTable_->columnCount(); ++column) {
                const auto* item = eventTable_->item(row, column);
                matches = item && item->text().contains(filter, Qt::CaseInsensitive);
            }
            eventTable_->setRowHidden(row, !matches);
        }
    });
    eventLayout->addWidget(eventBar);
    eventLayout->addWidget(eventTable_);
    bottomTabs->addTab(eventPanel, tr("Events"));

    relationTable_ = new QTableWidget;
    relationTable_->setColumnCount(8);
    relationTable_->setHorizontalHeaderLabels({
        tr("Source"),
        tr("Target"),
        tr("Min delay"),
        tr("Max delay"),
        tr("Clock domain"),
        tr("Condition"),
        tr("Severity"),
        tr("Description"),
    });
    relationTable_->horizontalHeader()->setStretchLastSection(true);
    relationTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(
        relationTable_,
        &QTableWidget::cellChanged,
        this,
        &MainWindow::relationCellChanged);
    connect(relationTable_, &QTableWidget::cellDoubleClicked, this, [this](const int row, int) {
        const auto* item = relationTable_->item(row, 0);
        if (!item) return;
        const auto laneId = item->data(Qt::UserRole).toString();
        const auto tick = item->data(Qt::UserRole + 1).toLongLong();
        if (!laneId.isEmpty()) canvas_->revealLocation(laneId, tick);
    });
    auto* relationPanel = new QWidget;
    auto* relationLayout = new QVBoxLayout(relationPanel);
    relationLayout->setContentsMargins(0, 0, 0, 0);
    relationLayout->setSpacing(0);
    auto* relationBar = new QToolBar;
    relationBar->setIconSize(QSize(16, 16));
    relationBar->addAction(
        themedIcon(QStringLiteral("list-remove"), style(), QStyle::SP_TrashIcon),
        tr("Remove relation"),
        this,
        &MainWindow::removeSelectedRelation);
    relationLayout->addWidget(relationBar);
    relationLayout->addWidget(relationTable_);
    bottomTabs->addTab(relationPanel, tr("Relations"));

    validationTable_ = new QTableWidget;
    validationTable_->setColumnCount(4);
    validationTable_->setHorizontalHeaderLabels({
        tr("Severity"),
        tr("Result"),
        tr("Message"),
        tr("Location"),
    });
    validationTable_->horizontalHeader()->setStretchLastSection(false);
    validationTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    validationTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    validationTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(
        validationTable_,
        &QTableWidget::cellDoubleClicked,
        this,
        &MainWindow::revealValidationIssue);
    bottomTabs->addTab(validationTable_, tr("Validation"));
    tracePanel_ = new QWidget;
    auto* tracePanel = tracePanel_;
    auto* traceLayout = new QVBoxLayout(tracePanel);
    traceLayout->setContentsMargins(0, 0, 0, 0);
    traceLayout->setSpacing(0);
    auto* traceBar = new QToolBar;
    traceBar->setIconSize(QSize(16, 16));
    traceBar->addAction(importTraceAction_);
    cancelTraceAction_ = traceBar->addAction(
        themedIcon(QStringLiteral("process-stop"), style(), QStyle::SP_DialogCancelButton),
        tr("Cancel import"),
        this,
        &MainWindow::cancelTraceImport);
    cancelTraceAction_->setEnabled(false);
    traceBar->addSeparator();
    traceBar->addAction(tr("Auto-map"), this, &MainWindow::autoMapImportedTrace);
    traceBar->addAction(tr("Align…"), this, &MainWindow::alignImportedTrace);
    traceBar->addAction(tr("Fit"), [this] {
        if (traceCanvas_) traceCanvas_->fitTrace();
    });
    traceSummary_ = new QLabel(tr("No imported trace"));
    traceSummary_->setMinimumWidth(220);
    traceBar->addSeparator();
    traceBar->addWidget(traceSummary_);
    traceProgress_ = new QProgressBar;
    traceProgress_->setRange(0, 0);
    traceProgress_->setMaximumWidth(130);
    traceProgress_->setTextVisible(false);
    traceProgress_->setVisible(false);
    traceBar->addWidget(traceProgress_);
    traceLayout->addWidget(traceBar);

    traceMappingTable_ = new QTableWidget;
    traceMappingTable_->setColumnCount(2);
    traceMappingTable_->setHorizontalHeaderLabels({tr("Actual signal"), tr("Expected lane")});
    traceMappingTable_->horizontalHeader()->setStretchLastSection(true);
    traceMappingTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    traceMappingTable_->setMinimumWidth(280);
    connect(
        traceMappingTable_,
        &QTableWidget::cellChanged,
        this,
        &MainWindow::traceMappingCellChanged);
    traceCanvas_ = new TraceCanvas;
    connect(
        traceCanvas_,
        &TraceCanvas::cursorChanged,
        this,
        [this](const qint64 tick, const QString& signalId, const QString& value) {
            statusBar()->showMessage(
                tr("%1  %2 = %3")
                    .arg(QString::fromStdString(formatTick(tick, project_.timeBase)))
                    .arg(signalId, value));
        });
    auto* traceSplitter = new QSplitter(Qt::Horizontal);
    traceSplitter->addWidget(traceMappingTable_);
    traceSplitter->addWidget(traceCanvas_);
    traceSplitter->setStretchFactor(0, 0);
    traceSplitter->setStretchFactor(1, 1);
    traceSplitter->setSizes({320, 900});
    traceLayout->addWidget(traceSplitter);
    bottomTabs->addTab(tracePanel, tr("Imported Trace"));

    comparePanel_ = new QWidget;
    auto* compareLayout = new QVBoxLayout(comparePanel_);
    compareLayout->setContentsMargins(0, 0, 0, 0);
    compareLayout->setSpacing(0);
    auto* compareBar = new QToolBar;
    compareBar->setIconSize(QSize(16, 16));
    compareBar->addAction(
        themedIcon(QStringLiteral("media-playback-start"), style(), QStyle::SP_MediaPlay),
        tr("Run compare"),
        this,
        &MainWindow::runCompare);
    compareXCombo_ = new QComboBox;
    compareXCombo_->setToolTip(tr("Choose how X values participate in comparison"));
    compareXCombo_->addItem(tr("Exact X"), static_cast<int>(XHandling::Exact));
    compareXCombo_->addItem(tr("Ignore any X"), static_cast<int>(XHandling::IgnoreAnyX));
    compareXCombo_->addItem(
        tr("Expected X wildcard"),
        static_cast<int>(XHandling::ExpectedXWildcard));
    compareBar->addWidget(compareXCombo_);
    compareToleranceEdit_ = new QLineEdit(QStringLiteral("0 tick"));
    compareToleranceEdit_->setPlaceholderText(tr("Edge tolerance"));
    compareToleranceEdit_->setToolTip(tr("Maximum accepted skew between matching expected and actual edges"));
    compareToleranceEdit_->setMaximumWidth(110);
    compareBar->addWidget(compareToleranceEdit_);
    compareMaskEdit_ = new QLineEdit;
    compareMaskEdit_->setPlaceholderText(tr("Bus mask"));
    compareMaskEdit_->setToolTip(tr("Global bit mask; 1 compares a bit and 0 ignores it"));
    compareMaskEdit_->setMaximumWidth(105);
    compareBar->addWidget(compareMaskEdit_);
    compareRelationOnly_ = new QCheckBox(tr("Relations only"));
    compareRelationOnly_->setObjectName(QStringLiteral("CompareRelationOnly"));
    compareBar->addWidget(compareRelationOnly_);
    compareSelectionOnly_ = new QCheckBox(tr("Selection window"));
    compareBar->addWidget(compareSelectionOnly_);
    compareBar->addAction(
        themedIcon(QStringLiteral("document-save"), style(), QStyle::SP_DialogSaveButton),
        tr("Export report…"),
        this,
        &MainWindow::exportCompareReport);
    compareSummary_ = new QLabel(tr("Run compare to calculate differences"));
    compareSummary_->setObjectName(QStringLiteral("CompareSummary"));
    compareSummary_->setMinimumWidth(260);
    compareBar->addSeparator();
    compareBar->addWidget(compareSummary_);
    compareLayout->addWidget(compareBar);

    compareTable_ = new QTableWidget;
    compareTable_->setObjectName(QStringLiteral("CompareResultTable"));
    compareTable_->setColumnCount(7);
    compareTable_->setHorizontalHeaderLabels({
        tr("Kind"),
        tr("Lane"),
        tr("Start"),
        tr("End"),
        tr("Expected"),
        tr("Actual"),
        tr("Message"),
    });
    compareTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    compareTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    compareTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(
        compareTable_,
        &QTableWidget::cellDoubleClicked,
        this,
        &MainWindow::revealCompareDifference);
    compareLayout->addWidget(compareTable_);
    bottomTabs->addTab(comparePanel_, tr("Compare Result"));
    auto* bottomDock = new QDockWidget(tr("Scenario"), this);
    bottomDock->setObjectName(QStringLiteral("ScenarioDock"));
    bottomDock->setWidget(bottomTabs);
    bottomDock->setMinimumHeight(190);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);
}

void MainWindow::populateSignalTree()
{
    signalTree_->clear();
    const auto* scenario = activeScenario();
    if (scenario) {
        for (const auto& lane : scenario->lanes) {
            auto* item = new QTreeWidgetItem({
                QString::fromStdString(lane.name),
                laneKindText(lane.kind),
            });
            item->setData(0, Qt::UserRole, QString::fromStdString(lane.id));
            item->setForeground(0, QColor(QString::fromStdString(lane.color)));
            signalTree_->addTopLevelItem(item);
        }
    }
    signalTree_->resizeColumnToContents(0);
    populateGroupTree();
}

void MainWindow::populateClockTree()
{
    clockTree_->clear();
    for (const auto& clock : project_.clockDomains) {
        auto* item = new QTreeWidgetItem({
            QString::fromStdString(clock.name),
            QString::fromStdString(formatTick(clock.period, project_.timeBase)),
        });
        item->setData(0, Qt::UserRole, QString::fromStdString(clock.id));
        item->setToolTip(
            0,
            tr("Double-click to edit period, phase, duty cycle, active edge and reset condition"));
        clockTree_->addTopLevelItem(item);
    }
    clockTree_->resizeColumnToContents(0);
}

void MainWindow::populateGroupTree()
{
    if (!groupTree_) return;
    groupTree_->clear();
    const auto* scenario = activeScenario();
    if (!scenario) return;

    std::map<std::string, QTreeWidgetItem*> groups;
    for (const auto& lane : scenario->lanes) {
        if (lane.kind != LaneKind::Group) continue;
        auto* item = new QTreeWidgetItem({
            QString::fromStdString(lane.name),
            tr("group"),
        });
        item->setData(0, Qt::UserRole, QString::fromStdString(lane.id));
        groupTree_->addTopLevelItem(item);
        groups.emplace(lane.id, item);
    }
    for (const auto& lane : scenario->lanes) {
        if (lane.kind == LaneKind::Group || lane.groupId.empty()) continue;
        auto iterator = groups.find(lane.groupId);
        if (iterator == groups.end()) {
            auto* unresolved = new QTreeWidgetItem({
                QString::fromStdString(lane.groupId),
                tr("unresolved group"),
            });
            unresolved->setForeground(1, QColor(198, 40, 40));
            groupTree_->addTopLevelItem(unresolved);
            iterator = groups.emplace(lane.groupId, unresolved).first;
        }
        auto* child = new QTreeWidgetItem({
            QString::fromStdString(lane.name),
            laneKindText(lane.kind),
        });
        child->setData(0, Qt::UserRole, QString::fromStdString(lane.id));
        child->setForeground(0, QColor(QString::fromStdString(lane.color)));
        iterator->second->addChild(child);
    }
    groupTree_->expandAll();
    groupTree_->resizeColumnToContents(0);
}

void MainWindow::populateLinkedResources()
{
    if (!resourceTree_) return;
    resourceTree_->clear();
    const auto projectDirectory = projectFile_.isEmpty()
        ? QDir::current()
        : QFileInfo(projectFile_).absoluteDir();
    for (const auto& resource : project_.linkedResources) {
        const auto label = resource.summary.empty()
            ? QString::fromStdString(resource.stableId)
            : QString::fromStdString(resource.summary);
        QString status = tr("ID reference");
        QString resolvedPath;
        if (!resource.path.empty()) {
            const auto storedPath = QString::fromUtf8(resource.path);
            resolvedPath = QFileInfo(storedPath).isAbsolute()
                ? QDir::cleanPath(storedPath)
                : projectDirectory.absoluteFilePath(storedPath);
            status = QFileInfo::exists(resolvedPath) ? tr("Resolved") : tr("Unresolved");
        }
        auto* item = new QTreeWidgetItem({label, status});
        item->setData(
            0,
            Qt::UserRole,
            status == tr("Resolved") ? resolvedPath : QString{});
        item->setToolTip(
            0,
            tr("Kind: %1\nStable ID: %2\nHash: %3\nPath: %4")
                .arg(
                    QString::fromStdString(resource.kind),
                    QString::fromStdString(resource.stableId),
                    QString::fromStdString(resource.contentHash),
                    QString::fromUtf8(resource.path)));
        if (status == tr("Unresolved")) item->setForeground(1, QColor(198, 40, 40));
        resourceTree_->addTopLevelItem(item);
    }
    resourceTree_->resizeColumnToContents(0);
}

void MainWindow::populateBottomTables()
{
    populatingTables_ = true;
    const QSignalBlocker eventBlocker(eventTable_);
    const auto sortingEnabled = eventTable_->isSortingEnabled();
    eventTable_->setSortingEnabled(false);
    eventTable_->setRowCount(0);
    const auto* scenario = activeScenario();
    if (!scenario) {
        eventTable_->setSortingEnabled(sortingEnabled);
        populatingTables_ = false;
        populateRelationTable();
        populateValidationTable();
        return;
    }
    eventTable_->setRowCount(static_cast<int>(scenario->events.size()));
    for (int row = 0; row < eventTable_->rowCount(); ++row) {
        const auto& event = scenario->events.at(static_cast<std::size_t>(row));
        const auto* lane = findLane(*scenario, event.laneId);
        const std::array<QString, 7> values{{
            event.cycle
                ? tr("cycle %1").arg(*event.cycle)
                : QString::fromStdString(formatTick(event.tick, project_.timeBase)),
            actionText(event.action),
            lane ? QString::fromStdString(lane->name) : QString::fromStdString(event.laneId),
            QString::fromStdString(event.value),
            QString::fromStdString(event.expectedResult),
            QString::fromStdString(event.clockDomainId),
            QString::fromStdString(event.description),
        }};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));
            if (column == 0) {
                item->setData(Qt::UserRole, QString::fromStdString(event.id));
                item->setData(Qt::UserRole + 1, event.tick);
            }
            eventTable_->setItem(row, column, item);
        }
    }
    eventTable_->setSortingEnabled(sortingEnabled);
    populatingTables_ = false;
    populateRelationTable();
    populateValidationTable();
}

void MainWindow::populateRelationTable()
{
    const QSignalBlocker blocker(relationTable_);
    relationTable_->setRowCount(0);
    const auto* scenario = activeScenario();
    if (!scenario) return;
    relationTable_->setRowCount(static_cast<int>(scenario->relations.size()));
    for (int row = 0; row < relationTable_->rowCount(); ++row) {
        const auto& relation = scenario->relations.at(static_cast<std::size_t>(row));
        const auto* source = findEvent(*scenario, relation.sourceEventId);
        const auto* target = findEvent(*scenario, relation.targetEventId);
        const auto* sourceLane = source ? findLane(*scenario, source->laneId) : nullptr;
        const auto* targetLane = target ? findLane(*scenario, target->laneId) : nullptr;
        const auto describeEvent = [this](const Event* event, const Lane* lane, const std::string& fallback) {
            if (!event) return QString::fromStdString(fallback);
            return tr("%1 @ %2")
                .arg(lane ? QString::fromStdString(lane->name) : QString::fromStdString(event->laneId))
                .arg(QString::fromStdString(formatTick(event->tick, project_.timeBase)));
        };
        const std::array<QString, 8> values{{
            describeEvent(source, sourceLane, relation.sourceEventId),
            describeEvent(target, targetLane, relation.targetEventId),
            QString::fromStdString(formatTick(relation.minimumDelay, project_.timeBase)),
            QString::fromStdString(formatTick(relation.maximumDelay, project_.timeBase)),
            QString::fromStdString(relation.clockDomainId),
            QString::fromStdString(relation.condition),
            severityText(relation.severity),
            QString::fromStdString(relation.description),
        }};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));
            if (column == 0) {
                item->setData(
                    Qt::UserRole,
                    source ? QString::fromStdString(source->laneId) : QString{});
                item->setData(Qt::UserRole + 1, source ? source->tick : 0);
                item->setData(Qt::UserRole + 2, QString::fromStdString(relation.id));
            }
            if (column < 2) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
            relationTable_->setItem(row, column, item);
        }
    }
}

void MainWindow::populateValidationTable()
{
    const QSignalBlocker blocker(validationTable_);
    validationTable_->setRowCount(0);
    const auto* scenario = activeScenario();
    if (!scenario) return;
    const auto issues = validateScenario(project_, *scenario);
    validationTable_->setRowCount(static_cast<int>(issues.size()));
    for (int row = 0; row < validationTable_->rowCount(); ++row) {
        const auto& issue = issues.at(static_cast<std::size_t>(row));
        const auto code = toString(issue.code);
        const auto* lane = findLane(*scenario, issue.laneId);
        const std::array<QString, 4> values{{
            severityText(issue.severity),
            QString::fromLatin1(code.data(), static_cast<qsizetype>(code.size())),
            QString::fromStdString(issue.message),
            issue.laneId.empty()
                ? QString{}
                : tr("%1 @ %2")
                    .arg(lane ? QString::fromStdString(lane->name) : QString::fromStdString(issue.laneId))
                    .arg(QString::fromStdString(formatTick(issue.tick, project_.timeBase))),
        }};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));
            if (column == 0) {
                item->setData(Qt::UserRole, QString::fromStdString(issue.laneId));
                item->setData(Qt::UserRole + 1, issue.tick);
                const QColor color = issue.severity == Severity::Error
                    ? QColor(239, 108, 115)
                    : issue.severity == Severity::Warning
                        ? QColor(255, 183, 77)
                        : QColor(100, 181, 246);
                item->setForeground(color);
            }
            validationTable_->setItem(row, column, item);
        }
    }
}

void MainWindow::populateTraceMappingTable()
{
    if (!traceMappingTable_) return;
    populatingTraceMapping_ = true;
    const QSignalBlocker blocker(traceMappingTable_);
    traceMappingTable_->setRowCount(0);
    const auto* reference = activeTraceReference();
    const auto* scenario = activeScenario();
    if (!traceIndex_ || !reference) {
        populatingTraceMapping_ = false;
        return;
    }
    traceMappingTable_->setRowCount(static_cast<int>(traceIndex_->traceSignals.size()));
    for (int row = 0; row < traceMappingTable_->rowCount(); ++row) {
        const auto& signal = traceIndex_->traceSignals.at(static_cast<std::size_t>(row));
        auto* actual = new QTableWidgetItem(QString::fromStdString(signal.fullName));
        actual->setData(Qt::UserRole, QString::fromStdString(signal.id));
        actual->setFlags(
            (actual->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsUserCheckable);
        actual->setCheckState(
            traceVisibleSignalIds_.contains(signal.id) ? Qt::Checked : Qt::Unchecked);
        traceMappingTable_->setItem(row, 0, actual);

        QString expected;
        const auto mapping = std::find_if(
            reference->signalMapping.begin(),
            reference->signalMapping.end(),
            [&signal](const auto& entry) {
                return entry.second == signal.id;
            });
        if (mapping != reference->signalMapping.end()) {
            const auto* lane = scenario ? findLane(*scenario, mapping->first) : nullptr;
            expected = lane
                ? QString::fromStdString(lane->name)
                : QString::fromStdString(mapping->first);
        }
        traceMappingTable_->setItem(row, 1, new QTableWidgetItem(expected));
    }
    traceMappingTable_->resizeColumnToContents(0);
    populatingTraceMapping_ = false;
}

void MainWindow::populateCompareTable()
{
    if (!compareTable_ || !compareSummary_) return;
    compareTable_->setRowCount(0);
    if (!compareResult_) {
        compareSummary_->setText(tr("Run compare to calculate differences"));
        compareSummary_->setToolTip({});
        compareSummary_->setStyleSheet({});
        return;
    }
    const auto* scenario = activeScenario();
    const auto differenceCount = static_cast<int>(compareResult_->differences.size());
    const auto diagnosticCount = static_cast<int>(compareResult_->diagnostics.size());
    compareTable_->setRowCount(differenceCount + diagnosticCount);
    for (int row = 0; row < differenceCount; ++row) {
        const auto& difference = compareResult_->differences.at(static_cast<std::size_t>(row));
        const auto* lane = scenario ? findLane(*scenario, difference.laneId) : nullptr;
        const std::array<QString, 7> values{{
            QString::fromLatin1(toString(difference.kind).data()),
            lane ? QString::fromStdString(lane->name) : QString::fromStdString(difference.laneId),
            QString::fromStdString(formatTick(difference.start, project_.timeBase)),
            QString::fromStdString(formatTick(difference.end, project_.timeBase)),
            QString::fromStdString(difference.expected),
            QString::fromStdString(difference.actual),
            QString::fromStdString(difference.message),
        }};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));
            if (column == 0) {
                item->setData(Qt::UserRole, QString::fromStdString(difference.laneId));
                item->setData(Qt::UserRole + 1, difference.start);
                item->setForeground(QColor(198, 40, 40));
            }
            compareTable_->setItem(row, column, item);
        }
    }
    for (int index = 0; index < diagnosticCount; ++index) {
        const auto row = differenceCount + index;
        const std::array<QString, 7> values{{
            tr("diagnostic"),
            {},
            {},
            {},
            {},
            {},
            QString::fromStdString(
                compareResult_->diagnostics.at(static_cast<std::size_t>(index))),
        }};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));
            item->setForeground(QColor(89, 101, 121));
            compareTable_->setItem(row, column, item);
        }
    }
    QStringList diagnosticText;
    diagnosticText.reserve(diagnosticCount);
    for (const auto& diagnostic : compareResult_->diagnostics) {
        diagnosticText.push_back(QString::fromStdString(diagnostic));
    }
    compareSummary_->setToolTip(diagnosticText.join(QLatin1Char('\n')));
    if (compareResult_->matches()) {
        compareSummary_->setText(
            tr("Match · %1 tolerated edge intervals · %2 diagnostics")
                .arg(compareResult_->toleratedEdgeCount)
                .arg(diagnosticCount));
        compareSummary_->setStyleSheet(QStringLiteral("color:#16845b;font-weight:600"));
    } else {
        compareSummary_->setText(
            tr("%1 differences · first %2 · offset %3 · %4 diagnostics")
                .arg(compareResult_->differences.size())
                .arg(
                    compareResult_->firstMismatch
                        ? QString::fromStdString(
                            formatTick(*compareResult_->firstMismatch, project_.timeBase))
                        : tr("n/a"))
                .arg(QString::fromStdString(
                    formatTick(compareResult_->traceOffset, project_.timeBase)))
                .arg(diagnosticCount));
        compareSummary_->setStyleSheet(QStringLiteral("color:#c62828;font-weight:600"));
    }
}

void MainWindow::invalidateCompareResult()
{
    compareResult_.reset();
    if (compareTable_) compareTable_->setRowCount(0);
    if (compareSummary_) {
        compareSummary_->setText(tr("Trace or scenario changed; run compare again"));
        compareSummary_->setToolTip({});
        compareSummary_->setStyleSheet({});
    }
    if (traceCanvas_) traceCanvas_->setDifferenceRanges({});
    if (compareTraceCanvas_) compareTraceCanvas_->setDifferenceRanges({});
}

void MainWindow::updateWindowTitle()
{
    const auto name = project_.name.empty()
        ? tr("Untitled")
        : QString::fromStdString(project_.name);
    setWindowTitle(
        tr("%1%2 — Wave Workbench")
            .arg(name)
            .arg(dirty_ ? QStringLiteral(" *") : QString{}));
}

bool MainWindow::loadFromPath(const QString& path)
{
    const auto result = loadProjectFile(path);
    if (!result.ok()) {
        QMessageBox::critical(this, tr("Open failed"), result.error);
        return false;
    }
    if (autosaveTimer_) autosaveTimer_->stop();
    ++autosaveGeneration_;
    autosavePending_ = false;
    const auto importRunning = traceWatcher_ && traceWatcher_->isRunning();
    if (importRunning && traceCancelFlag_) {
        traceCancelFlag_->store(true);
        ++traceGeneration_;
        reloadTraceAfterCurrent_ = true;
    }
    traceIndex_.reset();
    activeTraceId_.clear();
    traceVisibleSignalIds_.clear();
    project_ = *result.project;
    const auto recoveredSnapshot = path.endsWith(
        QStringLiteral(".autosave"),
        Qt::CaseInsensitive);
    projectFile_ = projectPathForLoadedFile(path);
    commandStack_.clear();
    dirty_ = result.migrated || recoveredSnapshot;
    canvas_->setDocument(&project_, activeScenario(), &commandStack_);
    populateSignalTree();
    populateClockTree();
    populateLinkedResources();
    populateBottomTables();
    inspectorTree_->clear();
    updateCommandActions();
    updateWindowTitle();
    if (recoveredSnapshot) {
        statusBar()->showMessage(
            tr("Recovery snapshot loaded; save to commit it to %1").arg(projectFile_),
            10'000);
    } else if (!result.warnings.isEmpty()) {
        statusBar()->showMessage(result.warnings.join(QStringLiteral("; ")), 10'000);
    }
    populateTraceMappingTable();
    traceCanvas_->setTrace(&project_, activeScenario(), nullptr, nullptr);
    compareTraceCanvas_->setTrace(&project_, activeScenario(), nullptr, nullptr);
    invalidateCompareResult();
    if (!importRunning) loadFirstTraceReference();
    if (dirty_) scheduleAutosave();
    return true;
}

bool MainWindow::writeToPath(const QString& path)
{
    QString error;
    if (!saveProjectFileAtomic(project_, path, &error)) {
        QMessageBox::critical(this, tr("Save failed"), error);
        return false;
    }
    projectFile_ = path;
    dirty_ = false;
    if (autosaveTimer_) autosaveTimer_->stop();
    ++autosaveGeneration_;
    autosavePending_ = false;
    populateLinkedResources();
    updateWindowTitle();
    statusBar()->showMessage(tr("Saved %1").arg(path), 5'000);
    return true;
}

bool MainWindow::confirmDiscardChanges()
{
    if (!dirty_) return true;
    const auto choice = QMessageBox::warning(
        this,
        tr("Unsaved changes"),
        tr("The project contains unsaved changes."),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Discard) return true;
    saveProject();
    return !dirty_;
}

Scenario* MainWindow::activeScenario() noexcept
{
    return project_.scenarios.empty() ? nullptr : &project_.scenarios.front();
}

const Scenario* MainWindow::activeScenario() const noexcept
{
    return project_.scenarios.empty() ? nullptr : &project_.scenarios.front();
}

ImportedTrace* MainWindow::activeTraceReference() noexcept
{
    const auto iterator = std::find_if(
        project_.importedTraces.begin(),
        project_.importedTraces.end(),
        [this](const ImportedTrace& trace) {
            return trace.id == activeTraceId_;
        });
    return iterator == project_.importedTraces.end() ? nullptr : &*iterator;
}

const ImportedTrace* MainWindow::activeTraceReference() const noexcept
{
    return const_cast<MainWindow*>(this)->activeTraceReference();
}

QString MainWindow::resolvedTracePath(const ImportedTrace& trace) const
{
    const auto stored = QString::fromUtf8(trace.path);
    if (QFileInfo(stored).isAbsolute() || projectFile_.isEmpty()) {
        return QDir::cleanPath(stored);
    }
    return QDir::cleanPath(
        QFileInfo(projectFile_).absoluteDir().absoluteFilePath(stored));
}

QString MainWindow::storedTracePath(const QString& absolutePath) const
{
    const auto cleaned = QDir::cleanPath(QFileInfo(absolutePath).absoluteFilePath());
    if (projectFile_.isEmpty()) return QDir::fromNativeSeparators(cleaned);
    const auto projectDirectory = QFileInfo(projectFile_).absolutePath();
    const auto relative = QDir(projectDirectory).relativeFilePath(cleaned);
    const auto normalized = QDir::fromNativeSeparators(relative);
    if (normalized != QStringLiteral("..")
        && !normalized.startsWith(QStringLiteral("../"))
        && !QFileInfo(normalized).isAbsolute()) {
        return normalized;
    }
    return QDir::fromNativeSeparators(cleaned);
}

} // namespace wave
