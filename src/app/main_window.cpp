#include "main_window.h"

#include "wave/export.h"
#include "wave/integration.h"
#include "wave/project_io.h"
#include "wave/simulation_session.h"
#include "wave/simulation_scenario_store.h"
#include "wave/stimulus_scenario.h"
#include "wave/trace.h"
#include "wave/validation.h"
#include "trace_canvas.h"
#include "trace_signal_browser.h"
#include "wave_canvas.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QColor>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QInputDialog>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QValidator>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace wave {
namespace {

constexpr int ScenarioLocationMemoryDelayMs = 400;
constexpr std::size_t DefaultFstVisibleSignals = 32;

QString simulationBatchStateLabel(const SimulationBatchScenarioState state)
{
    switch (state) {
    case SimulationBatchScenarioState::Pending: return QObject::tr("Pending");
    case SimulationBatchScenarioState::Running: return QObject::tr("Running");
    case SimulationBatchScenarioState::Succeeded: return QObject::tr("Passed");
    case SimulationBatchScenarioState::Failed: return QObject::tr("Failed");
    case SimulationBatchScenarioState::Cancelled: return QObject::tr("Cancelled");
    }
    return QObject::tr("Pending");
}

QString simulationStageDetail(const SimulationRunStage stage)
{
    switch (stage) {
    case SimulationRunStage::ValidateInputs:
        return QObject::tr("Validating simulation inputs");
    case SimulationRunStage::ProbeToolchain:
        return QObject::tr("Checking Verilator and the C++ toolchain");
    case SimulationRunStage::GenerateHarness:
        return QObject::tr("Preparing the runtime stimulus");
    case SimulationRunStage::ResolveBuildCache:
        return QObject::tr("Checking the compiled model cache");
    case SimulationRunStage::BuildModel:
        return QObject::tr("Compiling the simulation model");
    case SimulationRunStage::RunModel:
        return QObject::tr("Running the graphical stimulus");
    case SimulationRunStage::ImportTrace:
        return QObject::tr("Importing the generated waveform");
    case SimulationRunStage::MaterializeProject:
        return QObject::tr("Updating the result workspace");
    case SimulationRunStage::Completed:
        return QObject::tr("Simulation completed");
    }
    return QObject::tr("Running simulation");
}

QString defaultWellenReaderPath()
{
#ifdef Q_OS_WIN
    constexpr auto executable = "wave-wellen-reader.exe";
#else
    constexpr auto executable = "wave-wellen-reader";
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(
        QString::fromLatin1(executable));
}

class PositiveInt64Validator final : public QValidator {
public:
    explicit PositiveInt64Validator(
        const std::int64_t minimum,
        QObject* parent = nullptr)
        : QValidator(parent)
        , minimum_(minimum)
    {
    }

    State validate(QString& input, int&) const override
    {
        if (input.isEmpty()) return Intermediate;
        if (std::any_of(
                input.cbegin(),
                input.cend(),
                [](const QChar character) { return !character.isDigit(); })) {
            return Invalid;
        }
        bool valid = false;
        const auto value = input.toLongLong(&valid);
        if (!valid) return Invalid;
        return value >= minimum_ ? Acceptable : Intermediate;
    }

private:
    std::int64_t minimum_;
};

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

std::string uniqueLaneName(const Scenario& scenario, const std::string_view base)
{
    const auto available = [&scenario](const std::string& candidate) {
        return std::none_of(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&candidate](const Lane& lane) {
                return QString::compare(
                           QString::fromStdString(lane.name),
                           QString::fromStdString(candidate),
                           Qt::CaseInsensitive)
                    == 0;
            });
    };
    auto candidate = std::string(base);
    if (available(candidate)) return candidate;
    for (std::size_t suffix = 2;; ++suffix) {
        candidate = std::string(base) + "_" + std::to_string(suffix);
        if (available(candidate)) return candidate;
    }
}

std::string randomReadableLaneColor(
    const Scenario& scenario,
    const std::string_view excludedColor = {})
{
    static constexpr std::array<std::string_view, 12> palette{
        "#4fc3f7",
        "#81c784",
        "#ffb74d",
        "#ba68c8",
        "#e57373",
        "#64b5f6",
        "#ffd54f",
        "#4db6ac",
        "#f06292",
        "#a1887f",
        "#90a4ae",
        "#7986cb",
    };
    const auto excluded = QString::fromLatin1(
        excludedColor.data(),
        static_cast<qsizetype>(excludedColor.size()));
    std::vector<std::size_t> candidates;
    std::vector<std::size_t> unused;
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const auto color = QString::fromLatin1(
            palette.at(index).data(),
            static_cast<qsizetype>(palette.at(index).size()));
        if (!excluded.isEmpty()
            && QString::compare(color, excluded, Qt::CaseInsensitive) == 0) {
            continue;
        }
        candidates.push_back(index);
        const auto used = std::any_of(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&color](const Lane& lane) {
                return QString::compare(
                           QString::fromStdString(lane.color),
                           color,
                           Qt::CaseInsensitive)
                    == 0;
            });
        if (!used) unused.push_back(index);
    }
    const auto& pool = unused.empty() ? candidates : unused;
    const auto paletteIndex = pool.empty()
        ? std::size_t{0}
        : pool.at(static_cast<std::size_t>(
              QRandomGenerator::global()->bounded(static_cast<int>(pool.size()))));
    return std::string(palette.at(paletteIndex));
}

std::optional<std::string> promptNewGroupName(
    QWidget* parent,
    const Scenario& scenario,
    const QString& title,
    const QString& description,
    const std::string_view defaultName)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("GroupNameDialog"));
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(380);
    dialog.setAccessibleName(title);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);
    auto* prompt = new QLabel(description, &dialog);
    prompt->setObjectName(QStringLiteral("GroupNamePromptLabel"));
    prompt->setWordWrap(true);
    layout->addWidget(prompt);

    auto* name = new QLineEdit(
        QString::fromStdString(std::string(defaultName)),
        &dialog);
    name->setObjectName(QStringLiteral("GroupNameEdit"));
    name->setAccessibleName(QObject::tr("Group name"));
    name->setPlaceholderText(QObject::tr("Group name"));
    name->setMaxLength(128);
    layout->addWidget(name);

    auto* error = new QLabel(&dialog);
    error->setObjectName(QStringLiteral("GroupNameErrorLabel"));
    error->setAccessibleName(QObject::tr("Group name validation"));
    error->setMinimumHeight(error->fontMetrics().height());
    error->setStyleSheet(QStringLiteral("color: #ef9a9a;"));
    layout->addWidget(error);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    buttons->setObjectName(QStringLiteral("GroupNameButtonBox"));
    layout->addWidget(buttons);
    auto* accept = buttons->button(QDialogButtonBox::Ok);
    accept->setText(QObject::tr("Create"));
    accept->setDefault(true);

    const auto validate = [&scenario, name, error, accept] {
        const auto candidate = name->text().trimmed();
        if (candidate.isEmpty()) {
            error->setText(QObject::tr("Enter a group name."));
            accept->setEnabled(false);
            return;
        }
        const auto duplicate = std::any_of(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&candidate](const Lane& lane) {
                return QString::compare(
                           candidate,
                           QString::fromStdString(lane.name),
                           Qt::CaseInsensitive)
                    == 0;
            });
        error->setText(
            duplicate
                ? QObject::tr("A signal or group already uses this name.")
                : QString{});
        accept->setEnabled(!duplicate);
    };
    QObject::connect(
        name,
        &QLineEdit::textChanged,
        &dialog,
        [validate] { validate(); });
    QObject::connect(
        buttons,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept);
    QObject::connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject);
    validate();
    name->selectAll();
    name->setFocus(Qt::OtherFocusReason);

    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    return name->text().trimmed().toStdString();
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
        QStringLiteral(
            R"(^(-?(?:\d+(?:\.\d*)?|\.\d+))\s*(ps|ns|us|ms|ticks?)?$)"));
    const auto match = absoluteExpression.match(text);
    if (!match.hasMatch()) {
        error = QObject::tr(
            "Use a number followed by ps, ns, us, ms, tick, or 'cycle N'.");
        return std::nullopt;
    }
    const auto valueText = match.captured(1);
    const auto suffix = match.captured(2);
    cycle.reset();
    if (suffix.isEmpty() || suffix.startsWith(QStringLiteral("tick"))) {
        bool valid = false;
        const auto value = valueText.toLongLong(&valid);
        if (!valid) {
            error = QObject::tr("Tick values must be integers.");
            return std::nullopt;
        }
        return value;
    }
    const auto unit = suffix == QStringLiteral("ps")
        ? TimeUnit::Picosecond
        : suffix == QStringLiteral("ns")
            ? TimeUnit::Nanosecond
            : suffix == QStringLiteral("us")
                ? TimeUnit::Microsecond
                : TimeUnit::Millisecond;
    const auto valueBytes = valueText.toLatin1();
    const auto tick = toTicks(
        std::string_view{
            valueBytes.constData(),
            static_cast<std::size_t>(valueBytes.size())},
        unit,
        timeBase);
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

std::optional<Tick> parseRangeWidthText(
    QString text,
    const TimeBase& timeBase,
    const ClockDomain* clock,
    std::optional<std::int64_t>& cycles,
    QString& error)
{
    text = text.trimmed().toLower();
    const QRegularExpression cycleExpression(
        QStringLiteral(R"(^(?:cycle\s+(\d+)|(\d+)\s*cycles?)$)"));
    const auto cycleMatch = cycleExpression.match(text);
    if (cycleMatch.hasMatch()) {
        if (!clock) {
            error = QObject::tr("A cycle-based width requires a clock domain.");
            return std::nullopt;
        }
        bool valid = false;
        const auto countText = cycleMatch.captured(1).isEmpty()
            ? cycleMatch.captured(2)
            : cycleMatch.captured(1);
        const auto count = countText.toLongLong(&valid);
        if (!valid || count < 0 || clock->period <= 0
            || (count > 0
                && clock->period > std::numeric_limits<Tick>::max() / count)) {
            error = QObject::tr("Cycle width is outside the integer tick range.");
            return std::nullopt;
        }
        cycles = count;
        return count * clock->period;
    }

    cycles.reset();
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
    dialog.setObjectName(QStringLiteral("ExportOptionsDialog"));
    dialog.setWindowTitle(QObject::tr("Export scenario"));
    auto* layout = new QFormLayout(&dialog);
    auto* scope = new QComboBox;
    scope->setObjectName(QStringLiteral("ExportScopeCombo"));
    scope->addItem(QObject::tr("Full scenario"), QStringLiteral("full"));
    scope->addItem(QObject::tr("Current selection"), QStringLiteral("selection"));
    scope->addItem(QObject::tr("Specified time range"), QStringLiteral("range"));
    if (!selection || selection->second <= selection->first) {
        scope->setItemData(1, 0, Qt::UserRole - 1);
    }
    auto* start = new QLineEdit(QString::fromStdString(formatTick(0, project.timeBase)));
    start->setObjectName(QStringLiteral("ExportStartEdit"));
    auto* end = new QLineEdit(
        QString::fromStdString(formatTick(scenario.duration, project.timeBase)));
    end->setObjectName(QStringLiteral("ExportEndEdit"));
    auto* width = new QSpinBox;
    width->setObjectName(QStringLiteral("ExportLogicalWidthSpin"));
    width->setRange(640, 8000);
    width->setValue(1600);
    auto* dpi = new QSpinBox;
    dpi->setObjectName(QStringLiteral("ExportPngDpiSpin"));
    dpi->setRange(72, 600);
    dpi->setValue(192);
    auto* pdfSpan = new QLineEdit(QStringLiteral("0 tick"));
    pdfSpan->setObjectName(QStringLiteral("ExportPdfSpanEdit"));
    pdfSpan->setToolTip(QObject::tr("0 keeps the selected range on one PDF page"));
    auto* relations = new QCheckBox(QObject::tr("Include relations"));
    relations->setObjectName(QStringLiteral("ExportRelationsCheck"));
    relations->setChecked(true);
    auto* markers = new QCheckBox(QObject::tr("Include markers"));
    markers->setObjectName(QStringLiteral("ExportMarkersCheck"));
    markers->setChecked(true);
    auto* annotations = new QCheckBox(QObject::tr("Include annotations"));
    annotations->setObjectName(QStringLiteral("ExportAnnotationsCheck"));
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
    auto* error = new QLabel;
    error->setObjectName(QStringLiteral("ExportOptionsError"));
    error->setWordWrap(true);
    error->setStyleSheet(QStringLiteral("color: #ff9d9a;"));
    error->hide();
    layout->addRow(error);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    const auto updateRangeFields = [=] {
        const auto rangeMode = scope->currentData().toString() == QStringLiteral("range");
        start->setEnabled(rangeMode);
        end->setEnabled(rangeMode);
    };
    QObject::connect(scope, &QComboBox::currentIndexChanged, &dialog, [=](int) {
        error->hide();
        updateRangeFields();
    });
    QObject::connect(start, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(end, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(pdfSpan, &QLineEdit::textEdited, error, &QWidget::hide);
    updateRangeFields();

    std::optional<ExportOptions> acceptedOptions;
    const auto showError = [error](const QString& message, QWidget* field) {
        error->setText(message);
        error->show();
        if (!field) return;
        field->setFocus(Qt::OtherFocusReason);
        if (auto* edit = qobject_cast<QLineEdit*>(field)) edit->selectAll();
    };
    QObject::connect(
        buttons,
        &QDialogButtonBox::accepted,
        &dialog,
        [&] {
            ExportOptions options;
            options.width = width->value();
            options.pngDpi = dpi->value();
            options.includeRelations = relations->isChecked();
            options.includeMarkers = markers->isChecked();
            options.includeAnnotations = annotations->isChecked();

            const auto mode = scope->currentData().toString();
            if (mode == QStringLiteral("selection")) {
                if (!selection || selection->second <= selection->first) {
                    showError(
                        QObject::tr("No non-empty time selection exists."),
                        scope);
                    return;
                }
                options.start = selection->first;
                options.end = selection->second;
            } else if (mode == QStringLiteral("range")) {
                QString parseError;
                std::optional<std::int64_t> unusedCycle;
                const auto parsedStart = parseTimeText(
                    start->text(),
                    project.timeBase,
                    nullptr,
                    unusedCycle,
                    parseError);
                if (!parsedStart) {
                    showError(
                        parseError.isEmpty()
                            ? QObject::tr("Enter a valid export start time.")
                            : parseError,
                        start);
                    return;
                }
                parseError.clear();
                unusedCycle.reset();
                const auto parsedEnd = parseTimeText(
                    end->text(),
                    project.timeBase,
                    nullptr,
                    unusedCycle,
                    parseError);
                if (!parsedEnd) {
                    showError(
                        parseError.isEmpty()
                            ? QObject::tr("Enter a valid export end time.")
                            : parseError,
                        end);
                    return;
                }
                if (*parsedEnd <= *parsedStart) {
                    showError(
                        QObject::tr("Export end must be greater than start."),
                        end);
                    return;
                }
                options.start = *parsedStart;
                options.end = *parsedEnd;
            }

            QString parseError;
            std::optional<std::int64_t> unusedCycle;
            const auto parsedSpan = parseTimeText(
                pdfSpan->text(),
                project.timeBase,
                nullptr,
                unusedCycle,
                parseError);
            if (!parsedSpan || *parsedSpan < 0) {
                showError(
                    parseError.isEmpty()
                        ? QObject::tr("PDF page span must be non-negative.")
                        : parseError,
                    pdfSpan);
                return;
            }
            options.pdfPageSpanTicks = *parsedSpan;
            acceptedOptions = std::move(options);
            error->hide();
            dialog.accept();
        });

    if (dialog.exec() != QDialog::Accepted || !acceptedOptions) {
        return std::nullopt;
    }
    return acceptedOptions;
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

constexpr qsizetype maximumRecentProjects = 5;

QString normalizedProjectPath(const QString& path)
{
    return path.isEmpty()
        ? QString{}
        : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool sameProjectPath(const QString& left, const QString& right)
{
    const auto normalizedLeft = normalizedProjectPath(left);
    const auto normalizedRight = normalizedProjectPath(right);
    return !normalizedLeft.isEmpty()
        && !normalizedRight.isEmpty()
        && QString::compare(normalizedLeft, normalizedRight, Qt::CaseInsensitive) == 0;
}

std::optional<QString> droppedProjectPath(const QMimeData* mimeData)
{
    if (!mimeData || !mimeData->hasUrls()) return std::nullopt;
    const auto urls = mimeData->urls();
    if (urls.size() != 1 || !urls.front().isLocalFile()) return std::nullopt;

    const auto path = normalizedProjectPath(urls.front().toLocalFile());
    const auto lowerPath = path.toLower();
    if (!QFileInfo(path).isFile()
        || (!lowerPath.endsWith(QStringLiteral(".json"))
            && !lowerPath.endsWith(QStringLiteral(".autosave")))) {
        return std::nullopt;
    }
    return path;
}

QStringList storedRecentProjectPaths()
{
    const auto stored = QSettings{}.value(
        QStringLiteral("files/recentProjects")).toStringList();
    QStringList result;
    for (const auto& rawPath : stored) {
        const auto path = normalizedProjectPath(rawPath);
        if (path.isEmpty()) continue;
        const auto duplicate = std::any_of(
            result.cbegin(),
            result.cend(),
            [&path](const QString& existing) {
                return sameProjectPath(existing, path);
            });
        if (!duplicate) result.push_back(path);
        if (result.size() == maximumRecentProjects) break;
    }
    return result;
}

void storeRecentProjectPaths(const QStringList& paths)
{
    QSettings settings;
    settings.setValue(QStringLiteral("files/recentProjects"), paths);
}

QString autosavePathForProject(const QString& projectPath)
{
    return projectPath.isEmpty()
        ? untitledRecoveryPath()
        : projectPath + QStringLiteral(".autosave");
}

QString projectPathForLoadedFile(const QString& loadedPath)
{
    if (QString::compare(
            QFileInfo(loadedPath).absoluteFilePath(),
            QFileInfo(untitledRecoveryPath()).absoluteFilePath(),
            Qt::CaseInsensitive)
        == 0) {
        return {};
    }
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
    Scenario& scenario,
    const Lane& initial,
    const bool lockKind)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("LanePropertiesDialog"));
    dialog.setWindowTitle(
        initial.kind == LaneKind::Group
            ? QObject::tr("Group properties")
            : QObject::tr("Lane properties"));
    auto* layout = new QFormLayout(&dialog);
    auto* stableId = new QLineEdit(QString::fromStdString(initial.id));
    stableId->setObjectName(QStringLiteral("LanePropertiesStableId"));
    stableId->setReadOnly(true);
    auto* name = new QLineEdit(QString::fromStdString(initial.name));
    name->setObjectName(QStringLiteral("LanePropertiesNameEdit"));
    auto* kind = new QComboBox;
    kind->setObjectName(QStringLiteral("LanePropertiesKindCombo"));
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
    width->setObjectName(QStringLiteral("LanePropertiesWidthEdit"));
    auto* signedValue = new QCheckBox;
    signedValue->setObjectName(QStringLiteral("LanePropertiesSignedCheck"));
    signedValue->setChecked(initial.isSigned);
    auto* radix = new QComboBox;
    radix->setObjectName(QStringLiteral("LanePropertiesRadixCombo"));
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
    enumMap->setObjectName(QStringLiteral("LanePropertiesEnumMapEdit"));
    enumMap->setPlaceholderText(QObject::tr("IDLE=0; BUSY=1"));
    auto* clock = new QComboBox;
    clock->setObjectName(QStringLiteral("LanePropertiesClockCombo"));
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
    group->setObjectName(QStringLiteral("LanePropertiesGroupCombo"));
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
    color->setObjectName(QStringLiteral("LanePropertiesColorEdit"));
    auto* height = new QSpinBox;
    height->setObjectName(QStringLiteral("LanePropertiesHeightSpin"));
    height->setRange(30, 240);
    height->setValue(std::clamp(initial.height, 30, 240));
    auto* visible = new QCheckBox;
    visible->setObjectName(QStringLiteral("LanePropertiesVisibleCheck"));
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
    auto* error = new QLabel;
    error->setObjectName(QStringLiteral("LanePropertiesError"));
    error->setWordWrap(true);
    error->setStyleSheet(QStringLiteral("color: #ff9d9a;"));
    error->hide();
    layout->addRow(error);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
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
        error->hide();
        updateControls();
    });
    QObject::connect(name, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(width, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(enumMap, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(color, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(clock, &QComboBox::currentIndexChanged, error, &QWidget::hide);
    QObject::connect(group, &QComboBox::currentIndexChanged, error, &QWidget::hide);
    updateControls();

    std::optional<Lane> acceptedLane;
    const auto showError = [error](const QString& message, QWidget* field) {
        error->setText(message);
        error->show();
        if (!field) return;
        field->setFocus(Qt::OtherFocusReason);
        if (auto* edit = qobject_cast<QLineEdit*>(field)) edit->selectAll();
    };
    QObject::connect(
        buttons,
        &QDialogButtonBox::accepted,
        &dialog,
        [&] {
            Lane result = initial;
            result.name = name->text().trimmed().toStdString();
            if (result.name.empty()) {
                showError(QObject::tr("Lane name cannot be empty."), name);
                return;
            }
            const auto duplicateName = std::any_of(
                scenario.lanes.begin(),
                scenario.lanes.end(),
                [&initial, &result](const Lane& candidate) {
                    return candidate.id != initial.id
                        && QString::compare(
                               QString::fromStdString(candidate.name),
                               QString::fromStdString(result.name),
                               Qt::CaseInsensitive)
                            == 0;
                });
            if (duplicateName) {
                showError(
                    QObject::tr("Another signal or group already uses this name."),
                    name);
                return;
            }

            result.kind = static_cast<LaneKind>(kind->currentData().toInt());
            if (result.kind == LaneKind::Bus || result.kind == LaneKind::Enum) {
                bool widthOk = false;
                const auto parsedWidth = width->text().trimmed().toULongLong(&widthOk);
                if (!widthOk
                    || parsedWidth == 0
                    || parsedWidth > std::numeric_limits<std::uint32_t>::max()) {
                    showError(
                        QObject::tr("Width must be an integer from 1 to 4294967295."),
                        width);
                    return;
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
                    showError(enumError, enumMap);
                    return;
                }
                result.enumMap = *parsedMap;
            } else {
                result.enumMap.clear();
            }

            result.clockDomainId = result.kind == LaneKind::Group
                ? std::string{}
                : clock->currentData().toString().toStdString();
            if (!result.clockDomainId.empty()
                && !findClock(project, result.clockDomainId)) {
                showError(QObject::tr("Select an existing clock domain."), clock);
                return;
            }
            if (result.kind == LaneKind::Clock && result.clockDomainId.empty()) {
                showError(QObject::tr("A clock lane must reference a clock domain."), clock);
                return;
            }

            result.groupId = result.kind == LaneKind::Group
                ? std::string{}
                : group->currentData().toString().toStdString();
            if (!result.groupId.empty()) {
                const auto* selectedGroup = findLane(scenario, result.groupId);
                if (!selectedGroup
                    || selectedGroup->kind != LaneKind::Group
                    || selectedGroup->id == initial.id) {
                    showError(QObject::tr("Select an existing signal group."), group);
                    return;
                }
            }

            const QColor parsedColor(color->text().trimmed());
            if (!parsedColor.isValid()) {
                showError(
                    QObject::tr("Color must be a valid Qt color such as #42A5F5."),
                    color);
                return;
            }
            result.color = parsedColor.name(QColor::HexRgb).toStdString();
            result.height = height->value();
            result.visible = visible->isChecked();
            if (findLane(scenario, initial.id)) {
                try {
                    [[maybe_unused]] const ChangeLaneCommand validation(
                        project,
                        scenario,
                        initial.id,
                        result);
                } catch (const std::exception& exception) {
                    const auto message = QString::fromUtf8(exception.what());
                    QWidget* field = kind;
                    if (result.kind == LaneKind::Enum
                        && message.contains(
                            QStringLiteral("enum"),
                            Qt::CaseInsensitive)) {
                        field = enumMap;
                    } else if ((result.kind == LaneKind::Bus
                                || result.kind == LaneKind::Enum)
                               && (message.contains(
                                       QStringLiteral("width"),
                                       Qt::CaseInsensitive)
                                   || message.contains(
                                       QStringLiteral("value"),
                                       Qt::CaseInsensitive))) {
                        field = width;
                    }
                    showError(
                        QObject::tr("These properties cannot be applied: %1")
                            .arg(message),
                        field);
                    return;
                }
            }
            acceptedLane = std::move(result);
            error->hide();
            dialog.accept();
        });

    if (dialog.exec() != QDialog::Accepted || !acceptedLane) {
        return std::nullopt;
    }
    return acceptedLane;
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

MainWindow::ProjectFileRevision MainWindow::projectFileRevision(
    const QString& path)
{
    ProjectFileRevision revision;
    if (path.isEmpty() || !QFileInfo::exists(path)) return revision;

    QFile file(path);
    if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly)) {
        revision.state = ProjectFileRevision::State::Unreadable;
        return revision;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        revision.state = ProjectFileRevision::State::Unreadable;
        return revision;
    }
    revision.state = ProjectFileRevision::State::Present;
    revision.sha256 = hash.result();
    return revision;
}

QString untitledRecoveryPath()
{
    auto directory = qEnvironmentVariable("WAVEWORKBENCH_RECOVERY_DIR");
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
        if (!directory.isEmpty()) directory += QStringLiteral("/recovery");
    }
    if (directory.isEmpty()) {
        directory = QDir::tempPath() + QStringLiteral("/WaveWorkbench/recovery");
    }
    return QDir::cleanPath(
        directory + QStringLiteral("/untitled.wave.json.autosave"));
}

QString preferredProjectLoadPath(const QString& requestedPath)
{
    if (requestedPath.isEmpty()) {
        const auto recoveryPath = untitledRecoveryPath();
        const QFileInfo recoveryInfo(recoveryPath);
        return recoveryInfo.exists()
                && recoveryInfo.isFile()
                && loadProjectFile(recoveryPath).ok()
            ? recoveryPath
            : QString{};
    }
    if (requestedPath.endsWith(
            QStringLiteral(".autosave"),
            Qt::CaseInsensitive)) {
        return requestedPath;
    }
    const auto recoveryPath = autosavePathForProject(requestedPath);
    const QFileInfo recoveryInfo(recoveryPath);
    if (!recoveryInfo.exists() || !recoveryInfo.isFile()) return requestedPath;

    const QFileInfo projectInfo(requestedPath);
    if (projectInfo.exists()
        && recoveryInfo.lastModified() <= projectInfo.lastModified()) {
        return requestedPath;
    }
    return loadProjectFile(recoveryPath).ok() ? recoveryPath : requestedPath;
}

MainWindow::MainWindow(
    Project project,
    QString projectFile,
    QWidget* parent,
    const std::optional<std::size_t> initialScenarioIndex,
    const bool loadFirstTrace,
    QString wellenReaderExecutable)
    : QMainWindow(parent)
    , project_(std::move(project))
    , projectFile_(std::move(projectFile))
    , wellenReaderExecutable_(
          wellenReaderExecutable.isEmpty()
              ? defaultWellenReaderPath()
              : QFileInfo(std::move(wellenReaderExecutable)).absoluteFilePath())
    , simulationResultMode_(loadFirstTrace)
{
    const auto recoveredSnapshot = projectFile_.endsWith(
        QStringLiteral(".autosave"),
        Qt::CaseInsensitive);
    recoveryLoaded_ = recoveredSnapshot;
    if (recoveredSnapshot) {
        projectFile_ = projectPathForLoadedFile(projectFile_);
        dirty_ = true;
    }
    loadedProjectRevision_ = projectFileRevision(projectFile_);
    if (!project_.scenarios.empty()) {
        activeScenarioIndex_ = initialScenarioIndex
            ? std::min(*initialScenarioIndex, project_.scenarios.size() - 1)
            : rememberedActiveScenarioIndex().value_or(std::size_t{0});
    }
    resetEditTracking(!recoveredSnapshot);
    if (simulationResultMode_) configureSimulationSession();
    setObjectName(QStringLiteral("WaveWorkbenchMainWindow"));
    setMinimumSize(960, 620);
    resize(1440, 900);

    canvas_ = new WaveCanvas(this);
    compareTraceCanvas_ = new TraceCanvas(this);
    if (simulationResultMode_) {
        simulationResultSplitter_ = new QSplitter(Qt::Vertical, this);
        simulationResultSplitter_->setObjectName(
            QStringLiteral("SimulationResultSplitter"));
        simulationResultSplitter_->setChildrenCollapsible(false);

        const auto section = [this](
                                 const QString& title,
                                 QWidget* content,
                                 const QString& objectName) {
            auto* panel = new QWidget(simulationResultSplitter_);
            panel->setObjectName(objectName);
            auto* layout = new QVBoxLayout(panel);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            auto* label = new QLabel(title, panel);
            label->setObjectName(objectName + QStringLiteral("Label"));
            label->setMinimumHeight(24);
            label->setContentsMargins(10, 2, 10, 2);
            label->setStyleSheet(QStringLiteral(
                "font-weight:600;color:#39465a;background:#f3f6fa;"
                "border-bottom:1px solid #d7dee8;"));
            layout->addWidget(label);
            layout->addWidget(content, 1);
            return panel;
        };

        canvas_->setProperty("simulationStimulusCanvas", true);
        compareTraceCanvas_->setProperty("simulationResultCanvas", true);
        traceSignalBrowser_ = new TraceSignalBrowser(this);
        simulationActualSplitter_ = new QSplitter(Qt::Horizontal, this);
        simulationActualSplitter_->setObjectName(
            QStringLiteral("SimulationActualSplitter"));
        simulationActualSplitter_->setChildrenCollapsible(false);
        simulationActualSplitter_->addWidget(traceSignalBrowser_);
        simulationActualSplitter_->addWidget(compareTraceCanvas_);
        simulationActualSplitter_->setStretchFactor(0, 0);
        simulationActualSplitter_->setStretchFactor(1, 1);
        simulationActualSplitter_->setSizes({260, 1'040});

        comparePanel_ = new QWidget(this);
        comparePanel_->setObjectName(QStringLiteral("SimulationComparisonContent"));
        auto* compareLayout = new QVBoxLayout(comparePanel_);
        compareLayout->setContentsMargins(0, 0, 0, 0);
        compareLayout->setSpacing(0);
        auto* compareBar = new QToolBar(comparePanel_);
        compareBar->setObjectName(QStringLiteral("SimulationComparisonToolbar"));
        compareBar->setIconSize(QSize(16, 16));
        compareXCombo_ = new QComboBox(compareBar);
        compareXCombo_->setObjectName(QStringLiteral("SimulationCompareXHandling"));
        compareXCombo_->setToolTip(
            tr("Choose how X values participate in expected/actual comparison"));
        compareXCombo_->addItem(
            tr("Exact X"), static_cast<int>(XHandling::Exact));
        compareXCombo_->addItem(
            tr("Ignore any X"), static_cast<int>(XHandling::IgnoreAnyX));
        compareXCombo_->addItem(
            tr("Expected X wildcard"),
            static_cast<int>(XHandling::ExpectedXWildcard));
        compareBar->addWidget(compareXCombo_);
        compareToleranceEdit_ = new QLineEdit(QStringLiteral("0 tick"), compareBar);
        compareToleranceEdit_->setObjectName(
            QStringLiteral("SimulationCompareEdgeTolerance"));
        compareToleranceEdit_->setPlaceholderText(tr("Edge tolerance"));
        compareToleranceEdit_->setToolTip(
            tr("Maximum accepted skew between matching expected and actual edges"));
        compareToleranceEdit_->setMaximumWidth(120);
        compareBar->addWidget(compareToleranceEdit_);
        auto* compareSpacer = new QWidget(compareBar);
        compareSpacer->setSizePolicy(
            QSizePolicy::Expanding, QSizePolicy::Preferred);
        compareBar->addWidget(compareSpacer);
        compareSummary_ = new QLabel(
            tr("Add an expected waveform to an output lane, then compare"),
            compareBar);
        compareSummary_->setObjectName(QStringLiteral("CompareSummary"));
        compareSummary_->setContentsMargins(8, 0, 8, 0);
        compareBar->addWidget(compareSummary_);
        compareLayout->addWidget(compareBar);

        compareTable_ = new QTableWidget(comparePanel_);
        compareTable_->setObjectName(QStringLiteral("CompareResultTable"));
        compareTable_->setColumnCount(7);
        compareTable_->setHorizontalHeaderLabels({
            tr("Kind"), tr("Lane"), tr("Start"), tr("End"),
            tr("Expected"), tr("Actual"), tr("Message")});
        compareTable_->horizontalHeader()->setSectionResizeMode(
            6, QHeaderView::Stretch);
        compareTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        compareTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        compareTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        connect(
            compareTable_, &QTableWidget::cellClicked,
            this, &MainWindow::revealCompareDifference);
        connect(
            compareTable_, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::revealCompareDifference);
        compareLayout->addWidget(compareTable_, 1);

        simulationCheckPanel_ = new QWidget(this);
        simulationCheckPanel_->setObjectName(
            QStringLiteral("SimulationChecksContent"));
        auto* checkLayout = new QVBoxLayout(simulationCheckPanel_);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setSpacing(0);
        auto* checkBar = new QToolBar(simulationCheckPanel_);
        checkBar->setObjectName(QStringLiteral("SimulationChecksToolbar"));
        checkBar->setIconSize(QSize(16, 16));
        auto* addValue = checkBar->addAction(
            tr("Value at time"), this, &MainWindow::addValueSimulationCheck);
        addValue->setObjectName(QStringLiteral("AddValueSimulationCheckAction"));
        auto* addStable = checkBar->addAction(
            tr("Stable range"), this, &MainWindow::addStableSimulationCheck);
        addStable->setObjectName(QStringLiteral("AddStableSimulationCheckAction"));
        auto* addResponse = checkBar->addAction(
            tr("Edge response"), this, &MainWindow::addEdgeResponseSimulationCheck);
        addResponse->setObjectName(
            QStringLiteral("AddEdgeResponseSimulationCheckAction"));
        checkBar->addSeparator();
        editSimulationCheckAction_ = checkBar->addAction(
            tr("Edit"), this, &MainWindow::editSelectedSimulationCheck);
        editSimulationCheckAction_->setObjectName(
            QStringLiteral("EditSimulationCheckAction"));
        removeSimulationCheckAction_ = checkBar->addAction(
            themedIcon(QStringLiteral("edit-delete"), style(), QStyle::SP_TrashIcon),
            tr("Remove"),
            this,
            &MainWindow::removeSelectedSimulationCheck);
        removeSimulationCheckAction_->setObjectName(
            QStringLiteral("RemoveSimulationCheckAction"));
        checkBar->addSeparator();
        auto* runChecks = checkBar->addAction(
            themedIcon(
                QStringLiteral("media-playback-start"),
                style(),
                QStyle::SP_MediaPlay),
            tr("Run checks"),
            this,
            &MainWindow::runSimulationChecks);
        runChecks->setObjectName(
            QStringLiteral("RunSimulationChecksPanelAction"));
        auto* checkSpacer = new QWidget(checkBar);
        checkSpacer->setSizePolicy(
            QSizePolicy::Expanding, QSizePolicy::Preferred);
        checkBar->addWidget(checkSpacer);
        simulationCheckSummary_ = new QLabel(
            tr("Add a lightweight check to the current scenario"), checkBar);
        simulationCheckSummary_->setObjectName(
            QStringLiteral("SimulationCheckSummary"));
        simulationCheckSummary_->setContentsMargins(8, 0, 8, 0);
        checkBar->addWidget(simulationCheckSummary_);
        checkLayout->addWidget(checkBar);

        simulationCheckTable_ = new QTableWidget(simulationCheckPanel_);
        simulationCheckTable_->setObjectName(
            QStringLiteral("SimulationCheckResultTable"));
        simulationCheckTable_->setColumnCount(7);
        simulationCheckTable_->setHorizontalHeaderLabels({
            tr("Status"), tr("Check"), tr("Kind"), tr("Source"),
            tr("Target"), tr("Window"), tr("Result")});
        simulationCheckTable_->horizontalHeader()->setSectionResizeMode(
            6, QHeaderView::Stretch);
        simulationCheckTable_->setSelectionBehavior(
            QAbstractItemView::SelectRows);
        simulationCheckTable_->setSelectionMode(
            QAbstractItemView::SingleSelection);
        simulationCheckTable_->setEditTriggers(
            QAbstractItemView::NoEditTriggers);
        connect(
            simulationCheckTable_, &QTableWidget::cellClicked,
            this, &MainWindow::revealSimulationCheckOutcome);
        connect(
            simulationCheckTable_, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::revealSimulationCheckOutcome);
        connect(
            simulationCheckTable_, &QTableWidget::itemSelectionChanged,
            this,
            [this] {
                const auto selected = selectedSimulationCheckIndex().has_value();
                if (editSimulationCheckAction_)
                    editSimulationCheckAction_->setEnabled(selected);
                if (removeSimulationCheckAction_)
                    removeSimulationCheckAction_->setEnabled(selected);
            });
        checkLayout->addWidget(simulationCheckTable_, 1);

        auto* simulationBatchPanel = new QWidget(this);
        simulationBatchPanel->setObjectName(
            QStringLiteral("SimulationBatchContent"));
        auto* batchLayout = new QVBoxLayout(simulationBatchPanel);
        batchLayout->setContentsMargins(0, 0, 0, 0);
        batchLayout->setSpacing(0);
        auto* batchBar = new QToolBar(simulationBatchPanel);
        batchBar->setObjectName(QStringLiteral("SimulationBatchToolbar"));
        batchBar->setIconSize(QSize(16, 16));
        simulationBatchSummary_ = new QLabel(
            tr("Run all stored scenarios to compare their simulation status"),
            batchBar);
        simulationBatchSummary_->setObjectName(
            QStringLiteral("SimulationBatchSummary"));
        simulationBatchSummary_->setContentsMargins(8, 0, 8, 0);
        batchBar->addWidget(simulationBatchSummary_);
        batchLayout->addWidget(batchBar);

        simulationBatchTable_ = new QTableWidget(simulationBatchPanel);
        simulationBatchTable_->setObjectName(
            QStringLiteral("SimulationBatchResultTable"));
        simulationBatchTable_->setColumnCount(5);
        simulationBatchTable_->setHorizontalHeaderLabels({
            tr("Status"), tr("Scenario"), tr("Result"), tr("Model"),
            tr("Duration")});
        simulationBatchTable_->horizontalHeader()->setSectionResizeMode(
            2, QHeaderView::Stretch);
        simulationBatchTable_->setSelectionBehavior(
            QAbstractItemView::SelectRows);
        simulationBatchTable_->setSelectionMode(
            QAbstractItemView::SingleSelection);
        simulationBatchTable_->setEditTriggers(
            QAbstractItemView::NoEditTriggers);
        connect(
            simulationBatchTable_, &QTableWidget::cellClicked,
            this, &MainWindow::revealSimulationBatchResult);
        connect(
            simulationBatchTable_, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::revealSimulationBatchResult);
        batchLayout->addWidget(simulationBatchTable_, 1);

        simulationReviewTabs_ = new QTabWidget(this);
        simulationReviewTabs_->setObjectName(
            QStringLiteral("SimulationReviewTabs"));
        simulationReviewTabs_->addTab(comparePanel_, tr("Expected / Actual"));
        simulationReviewTabs_->addTab(simulationCheckPanel_, tr("Checks"));
        simulationReviewTabs_->addTab(simulationBatchPanel, tr("Batch"));

        simulationResultSplitter_->addWidget(section(
            tr("Stimulus / Expected"),
            canvas_,
            QStringLiteral("SimulationStimulusPanel")));
        simulationResultSplitter_->addWidget(section(
            tr("Actual"),
            simulationActualSplitter_,
            QStringLiteral("SimulationActualPanel")));
        simulationResultSplitter_->addWidget(section(
            tr("Review"),
            simulationReviewTabs_,
            QStringLiteral("SimulationComparisonPanel")));
        simulationResultSplitter_->setStretchFactor(0, 1);
        simulationResultSplitter_->setStretchFactor(1, 1);
        simulationResultSplitter_->setStretchFactor(2, 0);
        simulationResultSplitter_->setSizes({340, 360, 170});
        setCentralWidget(simulationResultSplitter_);
    } else {
        setCentralWidget(canvas_);
        compareTraceCanvas_->hide();
    }
    canvas_->installEventFilter(this);
    canvas_->viewport()->installEventFilter(this);
    canvas_->setSignalHeaderWidth(QSettings{}.value(
        QStringLiteral("canvas/signalHeaderWidth"),
        canvas_->signalHeaderWidth()).toInt());
    canvas_->setDocument(&project_, activeScenario(), &commandStack_);
    connect(
        canvas_,
        &WaveCanvas::signalHeaderWidthCommitted,
        this,
        [](const int width) {
            QSettings settings;
            settings.setValue(QStringLiteral("canvas/signalHeaderWidth"), width);
        });
    connect(canvas_, &WaveCanvas::addLaneRequested, this, [this](const LaneKind kind) {
        addQuickLane(kind);
    });
    connect(
        canvas_,
        &WaveCanvas::showHiddenLanesRequested,
        this,
        &MainWindow::showHiddenLanes);
    connect(
        canvas_,
        &WaveCanvas::showHiddenLaneRequested,
        this,
        &MainWindow::showHiddenLane);
    connect(
        canvas_,
        &WaveCanvas::quickLaneSetupAccepted,
        this,
        &MainWindow::completeQuickLaneSetup);
    connect(
        canvas_,
        &WaveCanvas::quickLaneSetupCanceled,
        this,
        &MainWindow::cancelQuickLaneSetup);
    connect(
        canvas_,
        &WaveCanvas::laneRenameAccepted,
        this,
        &MainWindow::completeLaneRename);
    connect(
        canvas_,
        &WaveCanvas::durationEditRequested,
        this,
        &MainWindow::changeScenarioDuration);
    connect(canvas_, &WaveCanvas::measureModeExitRequested, this, [this] {
        if (markerAction_) markerAction_->setChecked(false);
        canvas_->setTool(WaveCanvas::Tool::WaveEdit);
        updateWaveContext();
        updateSegmentActions();
        statusBar()->showMessage(tr("Direct waveform editing active"), 3'000);
    });
    connect(
        canvas_,
        &WaveCanvas::duplicateLaneRequested,
        this,
        &MainWindow::duplicateLaneById);
    connect(
        canvas_,
        &WaveCanvas::duplicateLanesRequested,
        this,
        &MainWindow::duplicateLanesById);
    connect(canvas_, &WaveCanvas::renameLaneRequested, this, &MainWindow::renameLaneById);
    connect(canvas_, &WaveCanvas::removeLaneRequested, this, &MainWindow::removeLaneById);
    connect(canvas_, &WaveCanvas::removeLanesRequested, this, &MainWindow::removeLanesById);
    connect(
        canvas_,
        &WaveCanvas::editLaneParametersRequested,
        this,
        &MainWindow::showLaneContextMenu);
    connect(canvas_, &WaveCanvas::selectionChanged, this, &MainWindow::updateSelection);
    connect(canvas_, &WaveCanvas::modelEdited, this, &MainWindow::markEdited);
    connect(canvas_, &WaveCanvas::commandAvailabilityChanged, this, &MainWindow::updateCommandActions);
    connect(canvas_, &WaveCanvas::eventSelected, this, &MainWindow::selectEventRow);
    connect(canvas_, &WaveCanvas::statusMessage, this, [this](const QString& message) {
        updateWaveContext();
        scheduleActiveScenarioLocationMemory();
        statusBar()->showMessage(message);
    });
    connect(
        canvas_,
        &WaveCanvas::pointerStatusMessage,
        this,
        [this](const QString& message) {
            updateWaveContext();
            scheduleActiveScenarioLocationMemory();
            if (pointerStatusLabel_) {
                pointerStatusLabel_->setText(message);
                pointerStatusLabel_->setToolTip(message);
            }
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
    if (traceSignalBrowser_) {
        connect(
            traceSignalBrowser_,
            &TraceSignalBrowser::visibleSignalIdsChanged,
            this,
            [this](const QStringList& ids) {
                traceVisibleSignalIds_.clear();
                for (const auto& id : ids) {
                    traceVisibleSignalIds_.insert(id.toStdString());
                }
                traceVisibilityCustomized_ = true;
                if (traceCanvas_) {
                    traceCanvas_->setVisibleSignalIds(traceVisibleSignalIds_);
                }
                compareTraceCanvas_->setVisibleSignalIds(
                    traceVisibleSignalIds_);
                fstSignalLoadPaused_ = false;
                startPendingFstSignalLoad();
            });
        connect(
            traceSignalBrowser_,
            &TraceSignalBrowser::signalActivated,
            compareTraceCanvas_,
            &TraceCanvas::revealSignal);
    }

    createActions();
    if (!projectFile_.isEmpty() && QFileInfo(projectFile_).isFile()) {
        rememberProjectPath(projectFile_);
    }
    createToolBars();
    if (simulationResultMode_) {
        if (auto* waveformToolbar = findChild<QToolBar*>(
                QStringLiteral("WaveformToolbar"))) {
            waveformToolbar->hide();
        }
        auto* resultToolbar = addToolBar(tr("Simulation result"));
        resultToolbar->setObjectName(QStringLiteral("SimulationResultToolbar"));
        resultToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        resultToolbar->setMovable(false);
        resultToolbar->setFloatable(false);
        resultToolbar->setAllowedAreas(Qt::TopToolBarArea);
        resultToolbar->toggleViewAction()->setEnabled(false);
        resultToolbar->toggleViewAction()->setVisible(false);
        runSimulationAction_ = resultToolbar->addAction(
            themedIcon(
                QStringLiteral("media-playback-start"),
                style(),
                QStyle::SP_MediaPlay),
            tr("Run"),
            this,
            &MainWindow::runSimulation);
        runSimulationAction_->setObjectName(QStringLiteral("RunSimulationAction"));
        runAllSimulationScenariosAction_ = resultToolbar->addAction(
            themedIcon(
                QStringLiteral("media-playlist-repeat"),
                style(),
                QStyle::SP_MediaPlay),
            tr("Run all"),
            this,
            &MainWindow::runAllSimulationScenarios);
        runAllSimulationScenariosAction_->setObjectName(
            QStringLiteral("RunAllSimulationScenariosAction"));
        stopSimulationAction_ = resultToolbar->addAction(
            themedIcon(
                QStringLiteral("media-playback-stop"),
                style(),
                QStyle::SP_MediaStop),
            tr("Stop"),
            this,
            &MainWindow::stopSimulation);
        stopSimulationAction_->setObjectName(QStringLiteral("StopSimulationAction"));
        rerunSimulationAction_ = resultToolbar->addAction(
            themedIcon(
                QStringLiteral("view-refresh"),
                style(),
                QStyle::SP_BrowserReload),
            tr("Rerun"),
            this,
            &MainWindow::rerunSimulation);
        rerunSimulationAction_->setObjectName(QStringLiteral("RerunSimulationAction"));
        runSimulationCompareAction_ = resultToolbar->addAction(
            themedIcon(
                QStringLiteral("view-statistics"),
                style(),
                QStyle::SP_DialogApplyButton),
            tr("Compare"),
            this,
            &MainWindow::runCompare);
        runSimulationCompareAction_->setObjectName(
            QStringLiteral("RunSimulationCompareAction"));
        runSimulationChecksAction_ = resultToolbar->addAction(
            themedIcon(
                QStringLiteral("task-complete"),
                style(),
                QStyle::SP_DialogApplyButton),
            tr("Checks"),
            this,
            &MainWindow::runSimulationChecks);
        runSimulationChecksAction_->setObjectName(
            QStringLiteral("RunSimulationChecksAction"));
        resultToolbar->addSeparator();
        createSimulationScenarioAction_ = resultToolbar->addAction(
            themedIcon(QStringLiteral("document-new"), style(), QStyle::SP_FileIcon),
            tr("New scenario"),
            this,
            &MainWindow::createSimulationScenario);
        createSimulationScenarioAction_->setObjectName(
            QStringLiteral("CreateSimulationScenarioAction"));
        renameSimulationScenarioAction_ = resultToolbar->addAction(
            tr("Rename scenario"),
            this,
            &MainWindow::renameSimulationScenario);
        renameSimulationScenarioAction_->setObjectName(
            QStringLiteral("RenameSimulationScenarioAction"));
        deleteSimulationScenarioAction_ = resultToolbar->addAction(
            themedIcon(QStringLiteral("edit-delete"), style(), QStyle::SP_TrashIcon),
            tr("Delete scenario"),
            this,
            &MainWindow::deleteSimulationScenario);
        deleteSimulationScenarioAction_->setObjectName(
            QStringLiteral("DeleteSimulationScenarioAction"));
        resultToolbar->addSeparator();
        simulationStubButton_ = new QToolButton(resultToolbar);
        simulationStubButton_->setObjectName(
            QStringLiteral("SimulationStubDependenciesButton"));
        simulationStubButton_->setPopupMode(QToolButton::InstantPopup);
        simulationStubButton_->setToolButtonStyle(
            Qt::ToolButtonTextBesideIcon);
        simulationStubButton_->setMenu(
            new QMenu(simulationStubButton_));
        resultToolbar->addWidget(simulationStubButton_);
        simulationClockButton_ = new QToolButton(resultToolbar);
        simulationClockButton_->setObjectName(
            QStringLiteral("SimulationClockDomainsButton"));
        simulationClockButton_->setPopupMode(QToolButton::InstantPopup);
        simulationClockButton_->setToolButtonStyle(
            Qt::ToolButtonTextBesideIcon);
        simulationClockButton_->setMenu(
            new QMenu(simulationClockButton_));
        resultToolbar->addWidget(simulationClockButton_);
        resultToolbar->addAction(asyncTimingAction_);
        resultToolbar->addSeparator();
        resultToolbar->addAction(
            themedIcon(QStringLiteral("zoom-in"), style(), QStyle::SP_ArrowUp),
            tr("Zoom in"),
            compareTraceCanvas_,
            &TraceCanvas::zoomIn);
        resultToolbar->addAction(
            themedIcon(QStringLiteral("zoom-out"), style(), QStyle::SP_ArrowDown),
            tr("Zoom out"),
            compareTraceCanvas_,
            &TraceCanvas::zoomOut);
        resultToolbar->addAction(
            themedIcon(
                QStringLiteral("zoom-fit-best"),
                style(),
                QStyle::SP_DesktopIcon),
            tr("Fit trace"),
            compareTraceCanvas_,
            &TraceCanvas::fitTrace);
        auto* spacer = new QWidget(resultToolbar);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        resultToolbar->addWidget(spacer);
        simulationStateLabel_ = new QLabel(resultToolbar);
        simulationStateLabel_->setObjectName(QStringLiteral("SimulationStateLabel"));
        simulationStateLabel_->setMinimumWidth(92);
        simulationStateLabel_->setAlignment(Qt::AlignCenter);
        simulationStateLabel_->setContentsMargins(10, 3, 10, 3);
        resultToolbar->addWidget(simulationStateLabel_);
        updateSimulationStubButton();
        updateSimulationClockButton();
        updateSimulationBatchView();
        updateSimulationControls(simulationSessionError_);
    }
    rememberActiveScenario();
    const auto restoredLocation = restoreActiveScenarioLocation();
    updateWaveContext();
    if (simulationResultMode_) populateSimulationCheckTable();
    scenarioLocationMemoryTimer_ = new QTimer(this);
    scenarioLocationMemoryTimer_->setObjectName(
        QStringLiteral("ScenarioLocationMemoryTimer"));
    scenarioLocationMemoryTimer_->setSingleShot(true);
    scenarioLocationMemoryTimer_->setInterval(
        ScenarioLocationMemoryDelayMs);
    connect(
        scenarioLocationMemoryTimer_,
        &QTimer::timeout,
        this,
        &MainWindow::rememberActiveScenarioLocation);
    connect(
        canvas_->horizontalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        [this] {
            scheduleActiveScenarioLocationMemory();
        });
    if (simulationResultMode_) {
        QString scenarioError;
        if (!persistActiveSimulationScenario(&scenarioError)
            && !scenarioError.isEmpty()) {
            simulationStateDetail_ = scenarioError;
        }
        updateSimulationScenarioActions();
    }
    connect(
        canvas_->horizontalScrollBar(),
        &QScrollBar::rangeChanged,
        this,
        [this](const int, const int) {
            scheduleActiveScenarioLocationMemory();
        });
    traceWatcher_ = new QFutureWatcher<std::shared_ptr<TraceParseResult>>(this);
    connect(
        traceWatcher_,
        &QFutureWatcher<std::shared_ptr<TraceParseResult>>::finished,
        this,
        &MainWindow::finishTraceImport);
    fstSignalLoadWatcher_ =
        new QFutureWatcher<std::shared_ptr<TraceSignalLoadResult>>(this);
    connect(
        fstSignalLoadWatcher_,
        &QFutureWatcher<std::shared_ptr<TraceSignalLoadResult>>::finished,
        this,
        &MainWindow::finishFstSignalLoad);
    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setSingleShot(true);
    autosaveTimer_->setInterval(1'500);
    connect(autosaveTimer_, &QTimer::timeout, this, &MainWindow::startAutosave);
    autosaveWatcher_ = new QFutureWatcher<QPair<quint64, QString>>(this);
    autosaveWatcher_->setObjectName(QStringLiteral("AutosaveWatcher"));
    connect(
        autosaveWatcher_,
        &QFutureWatcher<QPair<quint64, QString>>::finished,
        this,
        &MainWindow::finishAutosave);
    pointerStatusLabel_ = new QLabel(this);
    pointerStatusLabel_->setObjectName(QStringLiteral("PointerStatusLabel"));
    pointerStatusLabel_->setMinimumWidth(210);
    pointerStatusLabel_->setMaximumWidth(520);
    pointerStatusLabel_->setText(tr("Pointer: move over a signal"));
    pointerStatusLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusBar()->addPermanentWidget(pointerStatusLabel_, 1);
    pointerStatusLabel_->setVisible(!simulationResultMode_);
    saveStateLabel_ = new QLabel(this);
    saveStateLabel_->setObjectName(QStringLiteral("SaveStateLabel"));
    saveStateLabel_->setMinimumWidth(118);
    saveStateLabel_->setAlignment(Qt::AlignCenter);
    statusBar()->addPermanentWidget(saveStateLabel_);
    updateCommandActions();
    updateWindowTitle();
    if (simulationResultMode_) {
        setWindowTitle(
            tr("%1 - Simulation Result - Wave Workbench")
                .arg(QString::fromStdString(project_.name)));
    }
    auto initialStatus = recoveredSnapshot
        ? (projectFile_.isEmpty()
               ? tr("Untitled recovery snapshot loaded; use Save to choose a project file")
               : tr("Recovery snapshot loaded; save to commit it to %1").arg(projectFile_))
        : (restoredLocation
               ? tr("Resumed %1 · no edit range restored")
                     .arg(*restoredLocation)
               : tr("Ready"));
    if (recoveredSnapshot && restoredLocation) {
        initialStatus.append(
            tr(" · resumed %1 · no edit range restored")
                .arg(*restoredLocation));
    }
    statusBar()->showMessage(initialStatus);
    if (loadFirstTrace) {
        QTimer::singleShot(
            0, this, &MainWindow::loadFirstTraceReference);
    }
}

MainWindow::~MainWindow()
{
    if (traceCancelFlag_) traceCancelFlag_->store(true);
    if (fstSignalLoadCancelFlag_) fstSignalLoadCancelFlag_->store(true);
}

void MainWindow::configureSimulationSession()
{
    simulationManifest_.reset();
    const auto parsed = simulationSessionFromProject(project_);
    if (parsed.ok()) {
        simulationRequest_ = *parsed.request;
        simulationScenarioDirectory_ = simulationRequest_->scenarioDirectory;
        QFile manifestFile(simulationRequest_->manifestPath);
        if (manifestFile.open(QIODevice::ReadOnly)) {
            const auto manifest = parseZeroSlackModuleManifest(
                manifestFile.readAll());
            if (manifest.ok()) {
                simulationManifest_ = *manifest.manifest;
            } else {
                simulationStateDetail_ = manifest.error;
            }
        } else {
            simulationStateDetail_ = tr(
                "Cannot read the module manifest: %1")
                                         .arg(manifestFile.errorString());
        }
        loadStoredSimulationScenarios();
        simulationRunner_ = std::make_unique<VerilatorSimulationRunner>();
        simulationStateMachine_.configure(true, false);
        return;
    }

    simulationStateMachine_.configure(false, false);
    if (parsed.found) {
        simulationSessionError_ = parsed.error.isEmpty()
            ? tr("Simulation session metadata is invalid")
            : parsed.error;
        simulationStateMachine_.markFailed();
    } else {
        simulationSessionError_ = tr(
            "This trace has no rerun session; Run controls are unavailable");
    }
    simulationStateDetail_ = simulationSessionError_;
}

void MainWindow::loadStoredSimulationScenarios()
{
    if (!simulationRequest_ || !simulationManifest_
        || simulationScenarioDirectory_.trimmed().isEmpty()) {
        return;
    }
    const auto stored = loadSimulationScenarioStore(simulationScenarioDirectory_);
    if (!stored.ok()) {
        simulationStateDetail_ = stored.error;
        return;
    }
    std::vector<Scenario> restoredScenarios;
    std::vector<ClockDomain> restoredClocks;
    std::map<std::string, StimulusScenarioViewState> restoredViews;
    QStringList diagnostics = stored.diagnostics;
    for (const auto& entry : stored.scenarios) {
        const auto restored = restoreZeroSlackStimulusScenario(
            *simulationManifest_, entry.scenario);
        diagnostics.append(restored.diagnostics);
        if (!restored.ok() || restored.project->scenarios.empty()) {
            diagnostics.append(
                tr("Ignored saved scenario %1: %2")
                    .arg(QString::fromStdString(entry.scenario.name),
                         restored.error));
            continue;
        }
        auto scenario = restored.project->scenarios.front();
        restoredViews[scenario.id] = restored.view;
        if (entry.isDefault) defaultSimulationScenarioId_ = scenario.id;
        restoredScenarios.push_back(std::move(scenario));
        for (auto clock : restored.project->clockDomains) {
            if (std::none_of(
                    restoredClocks.begin(), restoredClocks.end(),
                    [&clock](const ClockDomain& candidate) {
                        return candidate.id == clock.id;
                    })) {
                restoredClocks.push_back(std::move(clock));
            }
        }
    }
    if (restoredScenarios.empty()) {
        if (!diagnostics.isEmpty()) simulationStateDetail_ = diagnostics.join('\n');
        return;
    }
    if (defaultSimulationScenarioId_.empty()) {
        defaultSimulationScenarioId_ = restoredScenarios.front().id;
    }
    project_.scenarios = std::move(restoredScenarios);
    project_.clockDomains = std::move(restoredClocks);
    simulationScenarioViews_ = std::move(restoredViews);
    activeScenarioIndex_ = 0;
    const auto defaultScenario = std::find_if(
        project_.scenarios.begin(), project_.scenarios.end(),
        [this](const Scenario& scenario) {
            return scenario.id == defaultSimulationScenarioId_;
        });
    if (defaultScenario != project_.scenarios.end()) {
        activeScenarioIndex_ = static_cast<std::size_t>(
            std::distance(project_.scenarios.begin(), defaultScenario));
    }
    if (!diagnostics.isEmpty()) simulationStateDetail_ = diagnostics.join('\n');
}

StimulusScenarioViewState MainWindow::activeSimulationViewState() const
{
    StimulusScenarioViewState view;
    const auto* scenario = activeScenario();
    if (!canvas_ || !scenario) return view;
    const auto laneId = canvas_->selectedLaneId().toStdString();
    if (const auto* lane = findLane(*scenario, laneId);
        lane && lane->kind != LaneKind::Group) {
        view.selectedPortName = lane->name;
    }
    view.cursorTick = std::clamp<Tick>(
        canvas_->cursorTick(), 0, std::max<Tick>(0, scenario->duration));
    view.visibleSpanTicks = std::clamp<Tick>(
        canvas_->visibleTimeSpan(), 0, std::max<Tick>(0, scenario->duration));
    return view;
}

bool MainWindow::persistActiveSimulationScenario(QString* error)
{
    if (!simulationResultMode_ || simulationScenarioDirectory_.trimmed().isEmpty()) {
        return true;
    }
    const auto* scenario = activeScenario();
    if (!scenario) return true;
    if (defaultSimulationScenarioId_.empty()) {
        defaultSimulationScenarioId_ = scenario->id;
    }
    const auto view = activeSimulationViewState();
    const auto exported = exportZeroSlackStimulusScenario(project_, *scenario, view);
    if (!exported.ok()) {
        if (error) *error = exported.error;
        return false;
    }
    const auto saved = saveSimulationScenario(
        simulationScenarioDirectory_,
        *exported.scenario,
        scenario->id == defaultSimulationScenarioId_);
    if (!saved.ok()) {
        if (error) *error = saved.error;
        return false;
    }
    simulationScenarioViews_[scenario->id] = view;
    return true;
}

void MainWindow::createSimulationScenario()
{
    const auto* source = activeScenario();
    if (!source || simulationScenarioDirectory_.isEmpty()) return;
    QString error;
    if (!persistActiveSimulationScenario(&error)) {
        QMessageBox::warning(this, tr("New scenario"), error);
        return;
    }
    bool accepted = false;
    const auto name = QInputDialog::getText(
        this,
        tr("New scenario"),
        tr("Scenario name"),
        QLineEdit::Normal,
        tr("Scenario %1").arg(project_.scenarios.size() + 1),
        &accepted).trimmed();
    if (!accepted || name.isEmpty()) return;
    const auto duplicate = std::any_of(
        project_.scenarios.begin(), project_.scenarios.end(),
        [&name](const Scenario& candidate) {
            return QString::compare(
                       QString::fromStdString(candidate.name), name,
                       Qt::CaseInsensitive) == 0;
        });
    if (duplicate) {
        QMessageBox::warning(
            this, tr("New scenario"), tr("A scenario with this name already exists."));
        return;
    }

    auto scenario = *source;
    scenario.id = makeStableId("simulation-scenario");
    scenario.name = name.toStdString();
    std::map<std::string, std::string> clonedClockIds;
    std::vector<ClockDomain> clonedClocks;
    for (auto& lane : scenario.lanes) {
        if (lane.clockDomainId.empty()) continue;
        const auto existing = clonedClockIds.find(lane.clockDomainId);
        if (existing != clonedClockIds.end()) {
            lane.clockDomainId = existing->second;
            continue;
        }
        const auto* sourceClock = findClock(project_, lane.clockDomainId);
        if (!sourceClock) continue;
        auto clone = *sourceClock;
        const auto oldId = lane.clockDomainId;
        clone.id = makeStableId("simulation-clock");
        clonedClockIds.emplace(oldId, clone.id);
        lane.clockDomainId = clone.id;
        clonedClocks.push_back(std::move(clone));
    }
    project_.clockDomains.insert(
        project_.clockDomains.end(), clonedClocks.begin(), clonedClocks.end());
    project_.scenarios.push_back(std::move(scenario));
    const auto targetIndex = project_.scenarios.size() - 1;
    simulationScenarioViews_[project_.scenarios.back().id] =
        activeSimulationViewState();
    populateScenarioSelector();
    switchActiveScenario(targetIndex, false);
    markEdited();
    if (!persistActiveSimulationScenario(&error)) {
        QMessageBox::warning(this, tr("New scenario"), error);
    }
    updateSimulationScenarioActions();
}

void MainWindow::renameSimulationScenario()
{
    auto* scenario = activeScenario();
    if (!scenario || scenario->id == defaultSimulationScenarioId_) return;
    bool accepted = false;
    const auto name = QInputDialog::getText(
        this,
        tr("Rename scenario"),
        tr("Scenario name"),
        QLineEdit::Normal,
        QString::fromStdString(scenario->name),
        &accepted).trimmed();
    if (!accepted || name.isEmpty()
        || name == QString::fromStdString(scenario->name)) {
        return;
    }
    const auto duplicate = std::any_of(
        project_.scenarios.begin(), project_.scenarios.end(),
        [scenario, &name](const Scenario& candidate) {
            return &candidate != scenario
                && QString::compare(
                       QString::fromStdString(candidate.name), name,
                       Qt::CaseInsensitive) == 0;
        });
    if (duplicate) {
        QMessageBox::warning(
            this, tr("Rename scenario"), tr("A scenario with this name already exists."));
        return;
    }
    scenario->name = name.toStdString();
    markEdited();
    QString error;
    if (!persistActiveSimulationScenario(&error)) {
        QMessageBox::warning(this, tr("Rename scenario"), error);
    }
    populateScenarioSelector();
    updateSimulationScenarioActions();
}

void MainWindow::deleteSimulationScenario()
{
    const auto* scenario = activeScenario();
    if (!scenario || scenario->id == defaultSimulationScenarioId_
        || project_.scenarios.size() <= 1) {
        return;
    }
    const auto scenarioId = scenario->id;
    const auto name = QString::fromStdString(scenario->name);
    if (QMessageBox::question(
            this,
            tr("Delete scenario"),
            tr("Delete scenario '%1'?").arg(name)) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!removeSimulationScenario(
            simulationScenarioDirectory_, scenarioId, &error)) {
        QMessageBox::warning(this, tr("Delete scenario"), error);
        return;
    }
    project_.scenarios.erase(
        project_.scenarios.begin() + static_cast<std::ptrdiff_t>(activeScenarioIndex_));
    simulationScenarioViews_.erase(scenarioId);
    std::set<std::string> usedClockIds;
    for (const auto& candidate : project_.scenarios) {
        for (const auto& lane : candidate.lanes) {
            if (!lane.clockDomainId.empty()) usedClockIds.insert(lane.clockDomainId);
        }
    }
    project_.clockDomains.erase(
        std::remove_if(
            project_.clockDomains.begin(), project_.clockDomains.end(),
            [&usedClockIds](const ClockDomain& clock) {
                return !usedClockIds.contains(clock.id);
            }),
        project_.clockDomains.end());
    activeScenarioIndex_ = std::min(
        activeScenarioIndex_, project_.scenarios.size() - 1);
    commandStack_.clear();
    resetEditTracking(false);
    canvas_->setDocument(&project_, activeScenario(), &commandStack_);
    populateScenarioSelector();
    static_cast<void>(restoreActiveScenarioLocation());
    markEdited();
    updateSimulationScenarioActions();
}

void MainWindow::updateSimulationScenarioActions()
{
    const auto* scenario = activeScenario();
    const auto state = simulationStateMachine_.state();
    const auto editable = state != SimulationSessionState::Compiling
        && state != SimulationSessionState::Running;
    const auto available = simulationResultMode_
        && !simulationScenarioDirectory_.isEmpty()
        && scenario && editable;
    if (createSimulationScenarioAction_) {
        createSimulationScenarioAction_->setEnabled(available);
    }
    const auto named = available
        && scenario->id != defaultSimulationScenarioId_;
    if (renameSimulationScenarioAction_) {
        renameSimulationScenarioAction_->setEnabled(named);
    }
    if (deleteSimulationScenarioAction_) {
        deleteSimulationScenarioAction_->setEnabled(
            named && project_.scenarios.size() > 1);
    }
    updateSimulationClockButton();
    updateSimulationStubButton();
    if (runAllSimulationScenariosAction_) {
        runAllSimulationScenariosAction_->setText(
            tr("Run all (%1)")
                .arg(static_cast<qulonglong>(project_.scenarios.size())));
    }
}

void MainWindow::updateSimulationClockButton()
{
    if (!simulationClockButton_ || !simulationClockButton_->menu()) return;

    auto* menu = simulationClockButton_->menu();
    menu->clear();
    std::set<std::string> clockIds;
    if (const auto* scenario = activeScenario()) {
        for (const auto& lane : scenario->lanes) {
            if (lane.kind == LaneKind::Clock && !lane.clockDomainId.empty())
                clockIds.insert(lane.clockDomainId);
        }
    }

    std::size_t count = 0;
    for (const auto& clock : project_.clockDomains) {
        if (!clockIds.contains(clock.id)) continue;
        ++count;
        const QString period = QString::fromStdString(
            formatTick(clock.period, project_.timeBase));
        const QString phase = QString::fromStdString(
            formatTick(clock.phase, project_.timeBase));
        auto* action = menu->addAction(
            tr("%1  ·  %2  ·  phase %3")
                .arg(QString::fromStdString(clock.name), period, phase));
        action->setObjectName(QStringLiteral("EditSimulationClockAction"));
        action->setToolTip(tr("Edit period, phase, duty cycle, and active edge"));
        const auto clockId = clock.id;
        connect(action, &QAction::triggered, this, [this, clockId] {
            editClockById(clockId);
        });
    }
    if (count == 0) {
        auto* unavailable = menu->addAction(tr("No clock domains in this scenario"));
        unavailable->setEnabled(false);
    }
    simulationClockButton_->setText(
        tr("Clocks (%1)").arg(static_cast<qulonglong>(count)));
    simulationClockButton_->setToolTip(
        count == 0
            ? tr("No semantic clock candidate was found for this scenario")
            : tr("Edit each independent simulation clock domain"));
}

void MainWindow::updateSimulationStubButton()
{
    if (!simulationStubButton_ || !simulationStubButton_->menu()) return;

    auto* menu = simulationStubButton_->menu();
    menu->clear();
    const auto dependencyCount = simulationManifest_
        ? simulationManifest_->unresolvedDependencies.size()
        : std::size_t{0};
    const std::set<std::string> selected = [&] {
        std::set<std::string> modules;
        if (!simulationRequest_) return modules;
        for (const QString& module : simulationRequest_->stubbedModules) {
            modules.insert(module.toUtf8().toStdString());
        }
        return modules;
    }();

    std::size_t selectedCount = 0;
    if (simulationManifest_) {
        for (const auto& dependency :
             simulationManifest_->unresolvedDependencies) {
            if (selected.contains(dependency.moduleName)) ++selectedCount;
            auto* action = menu->addAction(
                tr("%1 (%2 instance(s))")
                    .arg(QString::fromStdString(dependency.moduleName))
                    .arg(static_cast<qulonglong>(dependency.instances.size())));
            action->setObjectName(
                QStringLiteral("SimulationStubDependencyAction"));
            action->setProperty(
                "moduleName", QString::fromStdString(dependency.moduleName));
            action->setCheckable(dependency.stubSupported);
            action->setChecked(
                dependency.stubSupported
                && selected.contains(dependency.moduleName));
            action->setEnabled(dependency.stubSupported);
            const QString explanation = dependency.stubSupported
                ? tr("Generate a passive input-only stub for this unresolved module. Dependency behavior is not modeled.")
                : tr("Stub unavailable: %1")
                      .arg(QString::fromStdString(
                          dependency.stubUnsupportedReason));
            action->setToolTip(explanation);
            action->setStatusTip(explanation);
            if (!dependency.stubSupported) {
                action->setText(
                    tr("%1 (unavailable)")
                        .arg(QString::fromStdString(dependency.moduleName)));
                continue;
            }
            const std::string moduleName = dependency.moduleName;
            connect(action, &QAction::triggered, this,
                    [this, moduleName](const bool checked) {
                if (!simulationRequest_ || !simulationManifest_) return;
                std::set<std::string> next;
                for (const QString& module : simulationRequest_->stubbedModules)
                    next.insert(module.toUtf8().toStdString());
                if (checked) next.insert(moduleName);
                else next.erase(moduleName);

                simulationRequest_->stubbedModules.clear();
                for (const auto& dependency :
                     simulationManifest_->unresolvedDependencies) {
                    if (dependency.stubSupported
                        && next.contains(dependency.moduleName)) {
                        simulationRequest_->stubbedModules.append(
                            QString::fromStdString(dependency.moduleName));
                    }
                }
                attachSimulationSession(project_, *simulationRequest_);
                markEdited();
                simulationStubButton_->setText(
                    tr("Stubs (%1/%2)")
                        .arg(simulationRequest_->stubbedModules.size())
                        .arg(static_cast<qulonglong>(
                            simulationManifest_->unresolvedDependencies.size())));
                updateSimulationControls(
                    tr("Stub selection changed. Selected stubs are passive inputs only; dependency behavior is not modeled."));
                QTimer::singleShot(
                    0, this, &MainWindow::updateSimulationStubButton);
            });
        }
    }
    if (dependencyCount == 0) {
        auto* unavailable = menu->addAction(
            tr("No unresolved module dependencies"));
        unavailable->setEnabled(false);
    }
    simulationStubButton_->setText(
        tr("Stubs (%1/%2)")
            .arg(static_cast<qulonglong>(selectedCount))
            .arg(static_cast<qulonglong>(dependencyCount)));
    simulationStubButton_->setEnabled(
        dependencyCount > 0
        && simulationStateMachine_.state()
            != SimulationSessionState::Compiling
        && simulationStateMachine_.state()
            != SimulationSessionState::Running);
    simulationStubButton_->setToolTip(
        dependencyCount == 0
            ? tr("The selected design has no unresolved module dependencies")
            : tr("Explicitly select passive input-only stubs. They do not model dependency behavior."));
}

void MainWindow::updateSimulationControls(const QString& detail)
{
    if (!simulationResultMode_) return;
    if (!detail.isNull()) simulationStateDetail_ = detail;

    const auto state = simulationStateMachine_.state();
    const auto key = QString::fromLatin1(toString(state).data());
    const auto previous = property("simulationResultState").toString();
    setProperty("simulationResultState", key);

    QString label;
    QString foreground;
    QString background;
    switch (state) {
    case SimulationSessionState::Ready:
        label = tr("Ready");
        foreground = QStringLiteral("#435066");
        background = QStringLiteral("#e9eef5");
        break;
    case SimulationSessionState::Compiling:
        label = tr("Compiling");
        foreground = QStringLiteral("#1659a7");
        background = QStringLiteral("#e3efff");
        break;
    case SimulationSessionState::Running:
        label = tr("Running");
        foreground = QStringLiteral("#1659a7");
        background = QStringLiteral("#e3efff");
        break;
    case SimulationSessionState::Current:
        label = tr("Current");
        foreground = QStringLiteral("#126442");
        background = QStringLiteral("#dff4e9");
        break;
    case SimulationSessionState::Stale:
        label = tr("Stale");
        foreground = QStringLiteral("#815400");
        background = QStringLiteral("#fff0c7");
        break;
    case SimulationSessionState::Failed:
        label = tr("Failed");
        foreground = QStringLiteral("#a52222");
        background = QStringLiteral("#fde7e7");
        break;
    }

    if (simulationStateLabel_) {
        simulationStateLabel_->setText(label);
        simulationStateLabel_->setProperty("simulationState", key);
        simulationStateLabel_->setToolTip(simulationStateDetail_);
        simulationStateLabel_->setStyleSheet(QStringLiteral(
            "QLabel{color:%1;background:%2;border:1px solid %1;"
            "border-radius:3px;font-weight:600;}")
            .arg(foreground, background));
    }

    const auto actions = simulationStateMachine_.actions();
    if (runSimulationAction_) {
        runSimulationAction_->setEnabled(actions.runEnabled);
        runSimulationAction_->setToolTip(
            actions.runEnabled ? tr("Run the current graphical stimulus")
                               : simulationSessionError_);
    }
    if (runAllSimulationScenariosAction_) {
        const auto batchEnabled = actions.runEnabled || actions.rerunEnabled;
        runAllSimulationScenariosAction_->setEnabled(
            batchEnabled && !project_.scenarios.empty());
        runAllSimulationScenariosAction_->setText(
            tr("Run all (%1)")
                .arg(static_cast<qulonglong>(project_.scenarios.size())));
        runAllSimulationScenariosAction_->setToolTip(
            batchEnabled
                ? tr("Run every stored scenario sequentially")
                : simulationSessionError_);
    }
    if (stopSimulationAction_) {
        stopSimulationAction_->setEnabled(
            actions.stopEnabled && !simulationStopRequested_);
        stopSimulationAction_->setToolTip(
            simulationStopRequested_ ? tr("Cancellation requested")
                                     : tr("Stop the active simulation"));
    }
    if (rerunSimulationAction_) {
        rerunSimulationAction_->setEnabled(actions.rerunEnabled);
        rerunSimulationAction_->setToolTip(
            actions.rerunEnabled ? tr("Run the current stimulus again")
                                 : simulationSessionError_);
    }
    if (runSimulationCompareAction_) {
        const auto ready = state == SimulationSessionState::Current
            && traceIndex_.has_value()
            && activeTraceReference() != nullptr;
        runSimulationCompareAction_->setEnabled(ready);
        runSimulationCompareAction_->setToolTip(
            ready
                ? tr("Compare expected output ranges with the current simulation trace")
                : tr("Run the current scenario before comparing expected and actual waveforms"));
    }
    if (runSimulationChecksAction_) {
        const auto ready = state == SimulationSessionState::Current
            && traceIndex_.has_value()
            && activeTraceReference() != nullptr;
        runSimulationChecksAction_->setEnabled(ready);
        runSimulationChecksAction_->setToolTip(
            ready
                ? tr("Evaluate lightweight checks against the current actual trace")
                : tr("Run the current scenario before evaluating lightweight checks"));
    }
    if (canvas_) {
        canvas_->setEnabled(
            state != SimulationSessionState::Compiling
            && state != SimulationSessionState::Running);
    }
    if (simulationStubButton_) {
        simulationStubButton_->setEnabled(
            simulationManifest_
            && !simulationManifest_->unresolvedDependencies.empty()
            && state != SimulationSessionState::Compiling
            && state != SimulationSessionState::Running);
    }
    updateSimulationScenarioActions();
    if (previous != key) emit simulationSessionStateChanged(key);
}

bool MainWindow::exportSimulationStimulus(QString& error)
{
    if (!simulationRequest_) {
        error = simulationSessionError_.isEmpty()
            ? tr("Simulation session is unavailable")
            : simulationSessionError_;
        return false;
    }
    const auto* scenario = activeScenario();
    if (!scenario) {
        error = tr("No active graphical stimulus scenario");
        return false;
    }
    return exportSimulationStimulus(
        *scenario, activeSimulationViewState(), error);
}

bool MainWindow::exportSimulationStimulus(
    const Scenario& scenario,
    const StimulusScenarioViewState& view,
    QString& error)
{
    if (!simulationRequest_) {
        error = simulationSessionError_.isEmpty()
            ? tr("Simulation session is unavailable")
            : simulationSessionError_;
        return false;
    }
    const auto exported = exportZeroSlackStimulusScenario(
        project_, scenario, view);
    if (!exported.ok()) {
        error = exported.error.isEmpty()
            ? tr("The graphical stimulus could not be exported")
            : exported.error;
        return false;
    }

    const QFileInfo outputInfo(simulationRequest_->stimulusPath);
    auto outputDirectory = outputInfo.absoluteDir();
    if (!outputDirectory.mkpath(QStringLiteral("."))) {
        error = tr("Cannot create the stimulus directory: %1")
                    .arg(outputDirectory.absolutePath());
        return false;
    }
    const auto document = serializeZeroSlackStimulusScenario(*exported.scenario);
    QSaveFile file(outputInfo.absoluteFilePath());
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(document) != document.size()
        || !file.commit()) {
        error = tr("Cannot save the graphical stimulus: %1")
                    .arg(file.errorString());
        return false;
    }
    if (!simulationScenarioDirectory_.isEmpty()) {
        const auto saved = saveSimulationScenario(
            simulationScenarioDirectory_,
            *exported.scenario,
            scenario.id == defaultSimulationScenarioId_);
        if (!saved.ok()) {
            error = saved.error;
            return false;
        }
        simulationScenarioViews_[scenario.id] = view;
    }
    return true;
}

void MainWindow::runSimulation()
{
    if (!simulationResultMode_ || !simulationRequest_ || !simulationRunner_)
        return;
    if (simulationBatchRun_.running()) return;
    if (!commitPendingEdits()) return;
    simulationBatchRun_.reset();
    simulationBatchReports_.clear();
    updateSimulationBatchView();

    if (!projectFile_.isEmpty()) {
        simulationRequest_->resultProjectPath =
            QFileInfo(projectFile_).absoluteFilePath();
    }
    attachSimulationSession(project_, *simulationRequest_);

    QString error;
    if (!exportSimulationStimulus(error)) {
        simulationStateMachine_.markFailed();
        updateSimulationControls(error);
        statusBar()->showMessage(error, 10'000);
        return;
    }
    if (!simulationStateMachine_.beginRun()) return;
    invalidateCompareResult();

    const auto generation = nextSimulationGeneration();
    simulationGeneration_ = generation;
    simulationRequest_->generation = generation;
    simulationStopRequested_ = false;
    updateSimulationControls(tr("Preparing the simulation inputs"));
    statusBar()->showMessage(tr("Checking simulation model"));
    const auto started = simulationRunner_->start(
        *simulationRequest_,
        [this, generation](SimulationRunReport report) {
            if (report.generation != generation
                || generation != simulationGeneration_) {
                return;
            }
            finishSimulationRun(std::move(report));
        },
        [this, generation](
            const quint64 reportedGeneration,
            const SimulationRunStage stage) {
            if (reportedGeneration != generation
                || generation != simulationGeneration_) {
                return;
            }
            simulationStateMachine_.observeStage(stage);
            const auto detail = simulationStageDetail(stage);
            updateSimulationControls(detail);
            statusBar()->showMessage(detail);
        });
    if (!started) {
        simulationStateMachine_.finish(SimulationRunStatus::RunFailed);
        updateSimulationControls(tr("Simulation runner is already active"));
        statusBar()->showMessage(tr("Simulation could not be started"), 10'000);
    }
}

void MainWindow::runAllSimulationScenarios()
{
    if (!simulationResultMode_ || !simulationRequest_ || !simulationRunner_
        || project_.scenarios.empty() || simulationBatchRun_.running()) {
        return;
    }
    if (!commitPendingEdits()) return;

    if (!projectFile_.isEmpty()) {
        simulationRequest_->resultProjectPath =
            QFileInfo(projectFile_).absoluteFilePath();
    }
    attachSimulationSession(project_, *simulationRequest_);

    QString error;
    if (!persistActiveSimulationScenario(&error)) {
        simulationStateMachine_.markFailed();
        updateSimulationControls(error);
        statusBar()->showMessage(error, 10'000);
        return;
    }

    std::vector<SimulationBatchScenarioDescriptor> scenarios;
    scenarios.reserve(project_.scenarios.size());
    for (std::size_t index = 0; index < project_.scenarios.size(); ++index) {
        const auto& scenario = project_.scenarios.at(index);
        scenarios.push_back({
            index,
            scenario.id,
            scenario.name.empty()
                ? tr("Scenario %1").arg(static_cast<qulonglong>(index + 1))
                : QString::fromStdString(scenario.name),
        });
    }

    simulationBatchGeneration_ = nextSimulationGeneration();
    simulationBatchResultDirectory_ = QDir(simulationRequest_->artifactDirectory)
        .filePath(
            QStringLiteral("batch-%1")
                .arg(simulationBatchGeneration_));
    if (!QDir().mkpath(simulationBatchResultDirectory_)) {
        error = tr("Cannot create the batch result directory: %1")
                    .arg(simulationBatchResultDirectory_);
        simulationStateMachine_.markFailed();
        updateSimulationControls(error);
        statusBar()->showMessage(error, 10'000);
        return;
    }
    if (!simulationBatchRun_.begin(std::move(scenarios))
        || !simulationStateMachine_.beginRun()) {
        simulationBatchRun_.reset();
        updateSimulationControls(tr("The simulation batch could not be started"));
        return;
    }

    simulationBatchReports_.clear();
    simulationBatchReturnScenarioIndex_ = activeScenarioIndex_;
    simulationStopRequested_ = false;
    invalidateCompareResult();
    if (simulationReviewTabs_) simulationReviewTabs_->setCurrentIndex(2);
    updateSimulationBatchView();
    updateSimulationControls(tr("Preparing all stored scenarios"));
    statusBar()->showMessage(
        tr("Running %1 simulation scenarios")
            .arg(static_cast<qulonglong>(project_.scenarios.size())));
    startNextSimulationBatchScenario();
}

void MainWindow::startNextSimulationBatchScenario()
{
    const auto* item = simulationBatchRun_.current();
    if (!item || !simulationRequest_ || !simulationRunner_) {
        finishSimulationBatch();
        return;
    }
    const auto scenarioIndex = item->scenario.scenarioIndex;
    if (scenarioIndex >= project_.scenarios.size()) {
        SimulationRunReport report;
        report.status = SimulationRunStatus::InvalidStimulus;
        report.diagnostic = tr("A queued scenario no longer exists");
        finishSimulationBatchScenario(
            scenarioIndex, simulationBatchGeneration_, std::move(report));
        return;
    }

    const auto& scenario = project_.scenarios.at(scenarioIndex);
    const auto viewIterator = simulationScenarioViews_.find(scenario.id);
    const auto view = viewIterator == simulationScenarioViews_.end()
        ? StimulusScenarioViewState{}
        : viewIterator->second;
    QString error;
    if (!exportSimulationStimulus(scenario, view, error)) {
        SimulationRunReport report;
        report.status = SimulationRunStatus::InvalidStimulus;
        report.diagnostic = error;
        finishSimulationBatchScenario(
            scenarioIndex, simulationBatchGeneration_, std::move(report));
        return;
    }

    auto request = *simulationRequest_;
    const auto generation = nextSimulationGeneration();
    simulationGeneration_ = generation;
    request.generation = generation;
    request.resultProjectPath = QDir(simulationBatchResultDirectory_)
        .filePath(
            QStringLiteral("scenario-%1.wave.json")
                .arg(static_cast<qulonglong>(scenarioIndex + 1)));
    const auto batchGeneration = simulationBatchGeneration_;
    updateSimulationBatchView();
    const auto preparing = tr("Scenario %1/%2: %3")
        .arg(static_cast<qulonglong>(scenarioIndex + 1))
        .arg(static_cast<qulonglong>(project_.scenarios.size()))
        .arg(item->scenario.scenarioName);
    updateSimulationControls(preparing);
    statusBar()->showMessage(preparing);

    const auto started = simulationRunner_->start(
        std::move(request),
        [this, scenarioIndex, batchGeneration, generation](
            SimulationRunReport report) {
            if (batchGeneration != simulationBatchGeneration_
                || generation != simulationGeneration_) {
                return;
            }
            finishSimulationBatchScenario(
                scenarioIndex, batchGeneration, std::move(report));
        },
        [this, scenarioIndex, batchGeneration, generation](
            const quint64 reportedGeneration,
            const SimulationRunStage stage) {
            if (batchGeneration != simulationBatchGeneration_
                || reportedGeneration != generation
                || generation != simulationGeneration_) {
                return;
            }
            simulationStateMachine_.observeStage(stage);
            const auto detail = tr("Scenario %1/%2: %3")
                .arg(static_cast<qulonglong>(scenarioIndex + 1))
                .arg(static_cast<qulonglong>(project_.scenarios.size()))
                .arg(simulationStageDetail(stage));
            updateSimulationControls(detail);
            statusBar()->showMessage(detail);
        });
    if (!started) {
        SimulationRunReport report;
        report.generation = generation;
        report.status = SimulationRunStatus::RunFailed;
        report.diagnostic = tr("Simulation runner is already active");
        finishSimulationBatchScenario(
            scenarioIndex, batchGeneration, std::move(report));
    }
}

void MainWindow::finishSimulationBatchScenario(
    const std::size_t scenarioIndex,
    const quint64 batchGeneration,
    SimulationRunReport report)
{
    if (batchGeneration != simulationBatchGeneration_) return;
    const auto* current = simulationBatchRun_.current();
    if (!current || current->scenario.scenarioIndex != scenarioIndex) return;

    const auto succeeded = report.ok();
    if (!simulationBatchRun_.completeCurrent(report)) return;
    if (succeeded) {
        simulationBatchReports_.insert_or_assign(
            scenarioIndex, std::move(report));
    }
    updateSimulationBatchView();
    if (simulationBatchRun_.running()) {
        QTimer::singleShot(
            0, this, &MainWindow::startNextSimulationBatchScenario);
        return;
    }
    finishSimulationBatch();
}

void MainWindow::finishSimulationBatch()
{
    if (simulationBatchRun_.running()) return;
    simulationStopRequested_ = false;
    const auto summary = simulationBatchRun_.summary();
    const auto terminalStatus = simulationBatchRun_.state()
            == SimulationBatchState::Cancelled
        ? SimulationRunStatus::Cancelled
        : summary.failed > 0 ? SimulationRunStatus::RunFailed
                             : SimulationRunStatus::Succeeded;
    simulationStateMachine_.finish(terminalStatus);

    std::optional<std::size_t> resultIndex;
    if (simulationBatchReturnScenarioIndex_
        && simulationBatchReports_.contains(
            *simulationBatchReturnScenarioIndex_)) {
        resultIndex = *simulationBatchReturnScenarioIndex_;
    } else if (!simulationBatchReports_.empty()) {
        resultIndex = simulationBatchReports_.begin()->first;
    }
    QString resultError;
    const auto resultShown = resultIndex
        && showSimulationBatchResult(*resultIndex, resultError);
    if (resultShown) {
        simulationStateMachine_.markCurrent();
    } else {
        traceIndex_.reset();
        activeTraceId_.clear();
        traceVisibleSignalIds_.clear();
        traceVisibilityCustomized_ = false;
        project_.importedTraces.clear();
        refreshTraceViews();
        compareTraceCanvas_->hide();
        invalidateCompareResult();
        simulationStateMachine_.markFailed();
    }

    auto message = tr("Batch complete: %1 passed, %2 failed, %3 cancelled")
        .arg(static_cast<qulonglong>(summary.succeeded))
        .arg(static_cast<qulonglong>(summary.failed))
        .arg(static_cast<qulonglong>(summary.cancelled));
    if (!resultError.isEmpty()) message.append(tr(" · %1").arg(resultError));
    updateSimulationBatchView();
    updateSimulationControls(message);
    statusBar()->showMessage(message, 10'000);
    emit simulationBatchFinished(
        static_cast<int>(summary.succeeded),
        static_cast<int>(summary.failed),
        static_cast<int>(summary.cancelled));
}

void MainWindow::updateSimulationBatchView()
{
    if (!simulationBatchTable_ || !simulationBatchSummary_) return;
    const auto& scenarios = simulationBatchRun_.scenarios();
    simulationBatchTable_->setRowCount(static_cast<int>(scenarios.size()));
    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        const auto& result = scenarios.at(index);
        const auto row = static_cast<int>(index);
        auto* status = new QTableWidgetItem(
            simulationBatchStateLabel(result.state));
        status->setData(
            Qt::UserRole,
            QVariant::fromValue<qulonglong>(
                static_cast<qulonglong>(result.scenario.scenarioIndex)));
        switch (result.state) {
        case SimulationBatchScenarioState::Succeeded:
            status->setForeground(QColor(QStringLiteral("#126442")));
            break;
        case SimulationBatchScenarioState::Failed:
            status->setForeground(QColor(QStringLiteral("#a52222")));
            break;
        case SimulationBatchScenarioState::Running:
            status->setForeground(QColor(QStringLiteral("#1659a7")));
            break;
        case SimulationBatchScenarioState::Cancelled:
            status->setForeground(QColor(QStringLiteral("#815400")));
            break;
        case SimulationBatchScenarioState::Pending:
            break;
        }
        simulationBatchTable_->setItem(row, 0, status);
        simulationBatchTable_->setItem(
            row, 1, new QTableWidgetItem(result.scenario.scenarioName));
        const auto resultText = result.state
                == SimulationBatchScenarioState::Succeeded
            ? tr("Waveform available")
            : result.diagnostic;
        auto* resultItem = new QTableWidgetItem(resultText);
        resultItem->setToolTip(resultText);
        simulationBatchTable_->setItem(row, 2, resultItem);
        const auto modelText = result.state
                == SimulationBatchScenarioState::Succeeded
            ? result.buildCacheHit ? tr("Cached") : tr("Built")
            : QString{};
        simulationBatchTable_->setItem(
            row, 3, new QTableWidgetItem(modelText));
        const auto durationText = result.durationMs > 0
            ? tr("%1 ms").arg(result.durationMs)
            : QString{};
        simulationBatchTable_->setItem(
            row, 4, new QTableWidgetItem(durationText));
    }

    const auto summary = simulationBatchRun_.summary();
    if (summary.total == 0) {
        simulationBatchSummary_->setText(
            tr("Run all stored scenarios to compare their simulation status"));
        return;
    }
    simulationBatchSummary_->setText(
        tr("%1/%2 complete · %3 passed · %4 failed · %5 cancelled")
            .arg(static_cast<qulonglong>(
                summary.succeeded + summary.failed + summary.cancelled))
            .arg(static_cast<qulonglong>(summary.total))
            .arg(static_cast<qulonglong>(summary.succeeded))
            .arg(static_cast<qulonglong>(summary.failed))
            .arg(static_cast<qulonglong>(summary.cancelled)));
}

bool MainWindow::showSimulationBatchResult(
    const std::size_t scenarioIndex,
    QString& error)
{
    const auto report = simulationBatchReports_.find(scenarioIndex);
    if (report == simulationBatchReports_.end()) {
        error = tr("This scenario has no successful waveform result");
        return false;
    }
    if (!switchActiveScenario(scenarioIndex, false)) {
        error = tr("The scenario could not be activated");
        return false;
    }
    auto selectedReport = report->second;
    invalidateCompareResult();
    if (!applySimulationResult(selectedReport, error)) return false;
    populateSimulationCheckTable();
    return true;
}

void MainWindow::revealSimulationBatchResult(const int row, const int)
{
    if (!simulationBatchTable_ || row < 0
        || row >= simulationBatchTable_->rowCount()) {
        return;
    }
    const auto* statusItem = simulationBatchTable_->item(row, 0);
    if (!statusItem) return;
    const auto scenarioIndex = static_cast<std::size_t>(
        statusItem->data(Qt::UserRole).toULongLong());
    const auto& scenarios = simulationBatchRun_.scenarios();
    const auto result = std::find_if(
        scenarios.begin(), scenarios.end(),
        [scenarioIndex](const SimulationBatchScenarioResult& candidate) {
            return candidate.scenario.scenarioIndex == scenarioIndex;
        });
    if (result == scenarios.end()
        || result->state == SimulationBatchScenarioState::Pending
        || result->state == SimulationBatchScenarioState::Running) {
        return;
    }

    if (result->state == SimulationBatchScenarioState::Succeeded) {
        QString error;
        if (!showSimulationBatchResult(scenarioIndex, error)) {
            simulationStateMachine_.markFailed();
            updateSimulationControls(error);
            statusBar()->showMessage(error, 10'000);
            return;
        }
        simulationStateMachine_.markCurrent();
        const auto message = tr("Showing batch result: %1")
            .arg(result->scenario.scenarioName);
        updateSimulationControls(message);
        statusBar()->showMessage(message, 5'000);
        return;
    }

    static_cast<void>(switchActiveScenario(scenarioIndex, false));
    traceIndex_.reset();
    activeTraceId_.clear();
    traceVisibleSignalIds_.clear();
    traceVisibilityCustomized_ = false;
    project_.importedTraces.clear();
    refreshTraceViews();
    compareTraceCanvas_->hide();
    invalidateCompareResult();
    simulationStateMachine_.markFailed();
    const auto message = result->diagnostic.isEmpty()
        ? tr("This scenario did not produce a waveform")
        : result->diagnostic;
    updateSimulationControls(message);
    statusBar()->showMessage(message, 10'000);
}

void MainWindow::rerunSimulation()
{
    runSimulation();
}

void MainWindow::stopSimulation()
{
    if (!simulationRunner_ || !simulationRunner_->running()
        || simulationStopRequested_) {
        return;
    }
    if (simulationBatchRun_.running()) {
        static_cast<void>(simulationBatchRun_.requestCancel());
        updateSimulationBatchView();
    }
    simulationStopRequested_ = simulationRunner_->cancel();
    updateSimulationControls(
        simulationStopRequested_ ? tr("Cancellation requested")
                                 : tr("The active simulation could not be cancelled"));
    statusBar()->showMessage(
        simulationStopRequested_ ? tr("Stopping simulation")
                                 : tr("Simulation stop request failed"),
        5'000);
}

void MainWindow::initializeTraceVisibility(
    const std::optional<std::set<std::string>>& preferred)
{
    traceVisibleSignalIds_.clear();
    if (!traceIndex_) return;

    std::set<std::string> available;
    for (const auto& signal : traceIndex_->traceSignals) {
        available.insert(signal.id);
    }
    if (preferred) {
        for (const auto& id : *preferred) {
            if (available.contains(id)) traceVisibleSignalIds_.insert(id);
        }
        return;
    }

    if (simulationResultMode_) {
        if (const auto* reference = activeTraceReference()) {
            for (const auto& [laneId, signalId] : reference->signalMapping) {
                static_cast<void>(laneId);
                if (available.contains(signalId)) {
                    traceVisibleSignalIds_.insert(signalId);
                }
            }
        }
    }
    if (traceVisibleSignalIds_.empty()) {
        if (traceIndex_->format == TraceFormat::Fst) {
            for (const auto& signal : traceIndex_->traceSignals) {
                traceVisibleSignalIds_.insert(signal.id);
                if (traceVisibleSignalIds_.size() >= DefaultFstVisibleSignals) {
                    break;
                }
            }
        } else {
            traceVisibleSignalIds_ = std::move(available);
        }
    }
}

void MainWindow::refreshTraceViews()
{
    const auto* trace = traceIndex_ ? &*traceIndex_ : nullptr;
    if (traceCanvas_) {
        traceCanvas_->setTrace(
            &project_, activeScenario(), trace, activeTraceReference());
        traceCanvas_->setVisibleSignalIds(traceVisibleSignalIds_);
    }
    if (compareTraceCanvas_) {
        compareTraceCanvas_->setTrace(
            &project_, activeScenario(), trace, activeTraceReference());
        compareTraceCanvas_->setVisibleSignalIds(traceVisibleSignalIds_);
    }
    if (traceSignalBrowser_) {
        traceSignalBrowser_->setTrace(trace, traceVisibleSignalIds_);
    }
}

std::set<std::string> MainWindow::requiredFstSignalIds() const
{
    std::set<std::string> required = traceVisibleSignalIds_;
    if (const auto* reference = activeTraceReference()) {
        for (const auto& [laneId, signalId] : reference->signalMapping) {
            static_cast<void>(laneId);
            required.insert(signalId);
        }
    }
    return required;
}

bool MainWindow::requiredFstSignalsLoaded() const
{
    if (!traceIndex_ || traceIndex_->format != TraceFormat::Fst) return true;
    for (const auto& id : requiredFstSignalIds()) {
        const auto* signal = traceIndex_->findSignal(id);
        if (signal && !signal->transitionsLoaded) return false;
    }
    return true;
}

void MainWindow::updateFstLoadStatus()
{
    if (!traceIndex_ || traceIndex_->format != TraceFormat::Fst) return;
    const auto loaded = std::count_if(
        traceIndex_->traceSignals.begin(),
        traceIndex_->traceSignals.end(),
        [](const TraceSignal& signal) { return signal.transitionsLoaded; });
    const auto required = requiredFstSignalIds();
    const auto pending = std::count_if(
        required.begin(),
        required.end(),
        [this](const std::string& id) {
            const auto* signal = traceIndex_->findSignal(id);
            return signal && !signal->transitionsLoaded;
        });
    setProperty("wavewidgets.fstLoadedSignalCount", loaded);
    setProperty("wavewidgets.fstSignalCount", traceIndex_->traceSignals.size());
    setProperty("wavewidgets.fstRequiredSignalsLoaded", pending == 0);
    if (traceSummary_) {
        traceSummary_->setText(
            pending > 0
                ? tr("FST: %1/%2 loaded · loading %3 selected")
                      .arg(loaded)
                      .arg(traceIndex_->traceSignals.size())
                      .arg(pending)
                : tr("FST: %1/%2 signals loaded")
                      .arg(loaded)
                      .arg(traceIndex_->traceSignals.size()));
    }
}

void MainWindow::cancelActiveFstSignalLoad(const bool pause)
{
    fstSignalLoadPaused_ = pause;
    if (fstSignalLoadCancelFlag_) fstSignalLoadCancelFlag_->store(true);
}

void MainWindow::startPendingFstSignalLoad()
{
    if (fstSignalLoadPaused_ || !traceIndex_
        || traceIndex_->format != TraceFormat::Fst
        || !fstSignalLoadWatcher_ || fstSignalLoadWatcher_->isRunning()) {
        return;
    }
    std::vector<std::string> batch;
    batch.reserve(FstTraceReaderLimits{}.maxSignalsPerRequest);
    for (const auto& id : requiredFstSignalIds()) {
        const auto* signal = traceIndex_->findSignal(id);
        if (!signal || signal->transitionsLoaded) continue;
        batch.push_back(id);
        if (batch.size() >= FstTraceReaderLimits{}.maxSignalsPerRequest) break;
    }
    if (batch.empty()) {
        if (traceProgress_ && (!traceWatcher_ || !traceWatcher_->isRunning())) {
            traceProgress_->setVisible(false);
        }
        if (cancelTraceAction_ && (!traceWatcher_ || !traceWatcher_->isRunning())) {
            cancelTraceAction_->setEnabled(false);
        }
        updateFstLoadStatus();
        statusBar()->showMessage(
            tr("Loaded %1 of %2 FST signals")
                .arg(property("wavewidgets.fstLoadedSignalCount").toULongLong())
                .arg(property("wavewidgets.fstSignalCount").toULongLong()),
            3'000);
        if (std::exchange(fstComparePending_, false)) {
            QTimer::singleShot(0, this, &MainWindow::runCompare);
        }
        if (std::exchange(fstChecksPending_, false)) {
            QTimer::singleShot(0, this, &MainWindow::runSimulationChecks);
        }
        return;
    }

    const auto* reference = activeTraceReference();
    const auto path = reference ? resolvedTracePath(*reference) : pendingTracePath_;
    const auto offset = reference ? reference->offset : pendingTraceOffset_;
    const auto identity = traceIndex_->identity;
    fstSignalLoadCancelFlag_ = std::make_shared<std::atomic_bool>(false);
    const auto cancelFlag = fstSignalLoadCancelFlag_;
    TraceParseOptions options;
    options.projectTimeBase = traceIndex_->projectTimeBase;
    options.identity = identity;
    options.offset = offset;
    options.isCancelled = [cancelFlag] { return cancelFlag->load(); };
    if (traceProgress_) traceProgress_->setVisible(true);
    if (cancelTraceAction_) cancelTraceAction_->setEnabled(true);
    updateFstLoadStatus();
    statusBar()->showMessage(
        tr("Loading %1 selected FST signals…").arg(batch.size()));
    const auto executable = wellenReaderExecutable_;
    const auto future = QtConcurrent::run(
        [path, options, executable, batch = std::move(batch)]() mutable {
            auto result = loadFstSignalsFile(
                path, options, executable, std::span<const std::string>(batch));
            return std::make_shared<TraceSignalLoadResult>(std::move(result));
        });
    fstSignalLoadWatcher_->setFuture(future);
}

void MainWindow::finishFstSignalLoad()
{
    const auto loaded = fstSignalLoadWatcher_->result();
    if (!loaded) {
        if (traceSummary_) traceSummary_->setText(tr("FST signal load returned no result"));
        return;
    }
    if (loaded->cancelled) {
        if (fstSignalLoadPaused_) {
            if (traceSummary_) traceSummary_->setText(tr("FST signal loading paused"));
            if (traceProgress_) traceProgress_->setVisible(false);
            if (cancelTraceAction_) cancelTraceAction_->setEnabled(false);
        } else {
            startPendingFstSignalLoad();
        }
        return;
    }
    if (!traceIndex_ || traceIndex_->format != TraceFormat::Fst
        || traceIndex_->identity != loaded->identity) {
        startPendingFstSignalLoad();
        return;
    }
    if (!loaded->ok()) {
        const auto message = tr("FST signal load failed: %1")
            .arg(QString::fromStdString(loaded->errorSummary()));
        if (traceSummary_) traceSummary_->setText(message);
        statusBar()->showMessage(message, 10'000);
        if (traceProgress_) traceProgress_->setVisible(false);
        if (cancelTraceAction_) cancelTraceAction_->setEnabled(false);
        fstComparePending_ = false;
        fstChecksPending_ = false;
        return;
    }
    std::string error;
    if (!mergeLoadedTraceSignals(*traceIndex_, *loaded, &error)) {
        const auto message = tr("FST signal load was rejected: %1")
            .arg(QString::fromStdString(error));
        if (traceSummary_) traceSummary_->setText(message);
        statusBar()->showMessage(message, 10'000);
        return;
    }
    refreshTraceViews();
    updateFstLoadStatus();
    startPendingFstSignalLoad();
}

bool MainWindow::applySimulationResult(
    SimulationRunReport& report,
    QString& error)
{
    if (!report.trace) {
        error = tr("Simulation completed without a waveform index");
        return false;
    }
    const auto resultPath = report.artifacts.resultProjectPath.isEmpty()
        ? simulationRequest_->resultProjectPath
        : report.artifacts.resultProjectPath;
    const auto loaded = loadProjectFile(resultPath);
    if (!loaded.ok() || loaded.project->importedTraces.size() != 1) {
        error = loaded.error.isEmpty()
            ? tr("Simulation result project has no unique trace reference")
            : loaded.error;
        return false;
    }

    auto traceReference = loaded.project->importedTraces.front();
    const auto storedTrace = QString::fromStdString(traceReference.path);
    const auto absoluteTrace = QFileInfo(storedTrace).isAbsolute()
        ? QFileInfo(storedTrace).absoluteFilePath()
        : QFileInfo(resultPath).absoluteDir().absoluteFilePath(storedTrace);
    traceReference.path = storedTracePath(absoluteTrace).toStdString();
    project_.importedTraces.clear();
    project_.importedTraces.push_back(std::move(traceReference));
    attachSimulationSession(project_, *simulationRequest_);

    const auto preferredSignals = traceVisibilityCustomized_
        ? std::optional{traceVisibleSignalIds_}
        : std::nullopt;
    traceIndex_ = std::move(*report.trace);
    activeTraceId_ = project_.importedTraces.front().id;
    initializeTraceVisibility(preferredSignals);
    refreshTraceViews();
    compareTraceCanvas_->show();
    compareTraceCanvas_->fitTrace();

    const auto targetPath = projectFile_.isEmpty() ? resultPath : projectFile_;
    loadedProjectRevision_ = projectFileRevision(targetPath);
    if (!writeToPath(targetPath)) {
        error = tr("The updated simulation result could not be saved");
        return false;
    }
    return true;
}

void MainWindow::finishSimulationRun(SimulationRunReport report)
{
    simulationStopRequested_ = false;
    simulationStateMachine_.finish(report.status);
    if (report.ok()) {
        QString error;
        if (!applySimulationResult(report, error)) {
            simulationStateMachine_.markFailed();
            updateSimulationControls(error);
            statusBar()->showMessage(error, 10'000);
            return;
        }
        simulationStateMachine_.markCurrent();
        const auto message = report.buildCache.hit
            ? tr("Simulation result is current · cached model · %1 ms")
                  .arg(report.durationMs)
            : tr("Simulation result is current · model built · %1 ms")
                  .arg(report.durationMs);
        updateSimulationControls(message);
        statusBar()->showMessage(message, 8'000);
        return;
    }

    const auto stopped = report.status == SimulationRunStatus::Cancelled;
    const auto superseded = report.status == SimulationRunStatus::Superseded;
    const auto message = stopped
        ? tr("Simulation stopped; the previous result was retained")
        : superseded
            ? tr("A newer simulation retained ownership of the result")
            : report.diagnostic.isEmpty()
                ? tr("Simulation failed during %1")
                      .arg(QString::fromLatin1(toString(report.stage).data()))
                : report.diagnostic;
    updateSimulationControls(message);
    statusBar()->showMessage(message, 10'000);
}

const Project& MainWindow::project() const noexcept
{
    return project_;
}

void MainWindow::requestCompareMode()
{
    compareModeRequested_ = false;
    if (compareTraceCanvas_) compareTraceCanvas_->hide();
    statusBar()->showMessage(tr("Waveform-only workspace active"), 3'000);
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
        rememberActiveScenarioLocation();
    }
    if (traceIndex_) {
        if (traceCanvas_) traceCanvas_->revealTick(tick);
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

QStringList MainWindow::matchingVisibleSignals(const QString& query) const
{
    QStringList matches;
    const auto* scenario = activeScenario();
    const auto normalized = query.trimmed();
    if (!scenario || normalized.isEmpty()) return matches;

    for (const auto& lane : scenario->lanes) {
        if (!lane.visible || lane.kind == LaneKind::Group) continue;
        const auto name = QString::fromStdString(lane.name);
        const auto id = QString::fromStdString(lane.id);
        if (name.contains(normalized, Qt::CaseInsensitive)
            || id.contains(normalized, Qt::CaseInsensitive)) {
            matches.append(id);
        }
    }
    return matches;
}

void MainWindow::showSignalFind()
{
    if (!canvas_ || !signalFindWidgetAction_ || !signalFindEdit_
        || !signalFindResultLabel_) {
        return;
    }
    if (canvas_->hasExplicitRangeSelection()) {
        statusBar()->showMessage(
            tr("Esc clears the selected range before Ctrl+F finds another signal"),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;
    if (markerAction_ && markerAction_->isChecked()) {
        markerAction_->setChecked(false);
    }
    closeGoToTime(false);

    signalFindWidgetAction_->setVisible(true);
    signalFindMatchIndex_ = -1;
    signalFindEdit_->setFocus(Qt::ShortcutFocusReason);
    signalFindEdit_->selectAll();
    updateSignalFind();
}

void MainWindow::closeSignalFind(const bool announce)
{
    if (!signalFindWidgetAction_ || !signalFindWidgetAction_->isVisible()) return;
    signalFindWidgetAction_->setVisible(false);
    signalFindMatchIndex_ = -1;
    if (canvas_ && canvas_->viewport()) {
        canvas_->viewport()->setFocus(Qt::OtherFocusReason);
    }
    if (!announce) return;

    QString selectedName;
    const auto selectedId = canvas_ ? canvas_->selectedLaneId() : QString{};
    if (const auto* scenario = activeScenario(); scenario && !selectedId.isEmpty()) {
        const auto selected = std::find_if(
            scenario->lanes.begin(),
            scenario->lanes.end(),
            [&selectedId](const Lane& lane) {
                return QString::fromStdString(lane.id) == selectedId;
            });
        if (selected != scenario->lanes.end()) {
            selectedName = QString::fromStdString(selected->name);
        }
    }
    statusBar()->showMessage(
        selectedName.isEmpty()
            ? tr("Signal search closed · Ctrl+F opens it again")
            : tr("Signal search closed · %1 remains selected · Ctrl+F finds another")
                  .arg(selectedName),
        5'000);
}

void MainWindow::activateSignalFindMatch(
    const QStringList& matches,
    const int index,
    const bool wrapped)
{
    if (!canvas_ || !signalFindEdit_ || !signalFindResultLabel_
        || index < 0 || index >= matches.size()) {
        return;
    }
    const auto* scenario = activeScenario();
    if (!scenario) return;
    const auto laneId = matches.at(index);
    const auto lane = std::find_if(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [&laneId](const Lane& candidate) {
            return QString::fromStdString(candidate.id) == laneId;
        });
    if (lane == scenario->lanes.end() || !lane->visible
        || lane->kind == LaneKind::Group) {
        updateSignalFind();
        return;
    }

    const auto horizontalScroll = canvas_->horizontalScrollBar()->value();
    const auto cursor = canvas_->cursorTick();
    canvas_->revealLocation(laneId, cursor);
    canvas_->horizontalScrollBar()->setValue(horizontalScroll);
    signalFindMatchIndex_ = index;
    signalFindResultLabel_->setText(
        tr("%1/%2").arg(index + 1).arg(matches.size()));
    signalFindEdit_->setStyleSheet({});
    if (signalFindPreviousButton_) signalFindPreviousButton_->setEnabled(true);
    if (signalFindNextButton_) signalFindNextButton_->setEnabled(true);

    auto message = tr("Found signal %1 · %2 of %3 · Enter next · Shift+Enter previous · Esc closes")
                       .arg(QString::fromStdString(lane->name))
                       .arg(index + 1)
                       .arg(matches.size());
    if (wrapped) message.append(tr(" · wrapped"));
    statusBar()->showMessage(message);
}

void MainWindow::updateSignalFind()
{
    if (!signalFindWidgetAction_ || !signalFindWidgetAction_->isVisible()
        || !signalFindEdit_ || !signalFindResultLabel_) {
        return;
    }
    const auto query = signalFindEdit_->text().trimmed();
    if (query.isEmpty()) {
        signalFindMatchIndex_ = -1;
        signalFindResultLabel_->setText(QStringLiteral("0/0"));
        signalFindEdit_->setStyleSheet({});
        if (signalFindPreviousButton_) signalFindPreviousButton_->setEnabled(false);
        if (signalFindNextButton_) signalFindNextButton_->setEnabled(false);
        statusBar()->showMessage(
            tr("Find visible signal · type a name or ID · Enter next · Shift+Enter previous · Esc closes"));
        return;
    }

    const auto matches = matchingVisibleSignals(query);
    if (matches.isEmpty()) {
        signalFindMatchIndex_ = -1;
        signalFindResultLabel_->setText(QStringLiteral("0/0"));
        signalFindEdit_->setStyleSheet(
            QStringLiteral("QLineEdit { border: 1px solid #c96d6d; }"));
        if (signalFindPreviousButton_) signalFindPreviousButton_->setEnabled(false);
        if (signalFindNextButton_) signalFindNextButton_->setEnabled(false);
        statusBar()->showMessage(
            tr("No visible signal matches “%1” · edit the query or press Esc")
                .arg(query),
            5'000);
        return;
    }

    const auto index = signalFindMatchIndex_ >= 0
            && signalFindMatchIndex_ < matches.size()
        ? signalFindMatchIndex_
        : 0;
    activateSignalFindMatch(matches, index, false);
}

void MainWindow::stepSignalFind(const int direction)
{
    if (!signalFindWidgetAction_ || !signalFindWidgetAction_->isVisible()
        || !signalFindEdit_) {
        return;
    }
    const auto matches = matchingVisibleSignals(signalFindEdit_->text());
    if (matches.isEmpty()) {
        updateSignalFind();
        signalFindEdit_->setFocus(Qt::OtherFocusReason);
        return;
    }

    auto current = matches.indexOf(canvas_ ? canvas_->selectedLaneId() : QString{});
    if (current < 0) current = signalFindMatchIndex_;
    auto next = 0;
    auto wrapped = false;
    if (current < 0) {
        next = direction < 0 ? matches.size() - 1 : 0;
    } else {
        const auto candidate = current + (direction < 0 ? -1 : 1);
        wrapped = candidate < 0 || candidate >= matches.size();
        next = (candidate % matches.size() + matches.size()) % matches.size();
    }
    activateSignalFindMatch(matches, next, wrapped && matches.size() > 1);
    signalFindEdit_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::showGoToTime()
{
    if (!canvas_ || !goToTimeWidgetAction_ || !goToTimeEdit_
        || !goToTimeLabel_ || !goToTimeRangeLabel_
        || !goToTimeGoButton_ || !goToTimeOtherEdgeButton_) {
        return;
    }
    if (!commitPendingEdits()) return;
    goToTimeEditsRange_ = canvas_->hasExplicitRangeSelection();
    goToTimeEditsRangeWidth_ = false;
    if (!goToTimeEditsRange_
        && markerAction_
        && markerAction_->isChecked()) {
        markerAction_->setChecked(false);
    }
    canvas_->dismissInlineValueEditor();
    closeSignalFind(false);

    const auto* scenario = activeScenario();
    if (!scenario) return;
    if (goToTimeEditsRange_ && rangeEditPaletteAction_) {
        rangeEditPaletteAction_->setVisible(false);
    }
    goToTimeWidgetAction_->setVisible(true);
    goToTimeEdit_->setStyleSheet({});
    syncGoToTimeEditor(true);
    goToTimeEdit_->setFocus(Qt::ShortcutFocusReason);
    goToTimeEdit_->selectAll();
    if (goToTimeEditsRange_) {
        const auto anchor = canvas_->explicitRangeAnchorTick();
        statusBar()->showMessage(
            tr("Set exact active range edge · anchor %1 · click Range edge to enter a width · Enter applies · Esc returns")
                .arg(anchor
                         ? QString::fromStdString(
                               formatTick(*anchor, project_.timeBase))
                         : QString{}));
    } else {
        statusBar()->showMessage(
            tr("Go to time · enter decimal ps/ns/us/ms, integer tick, or cycle N · Enter jumps · Esc closes"));
    }
}

void MainWindow::syncGoToTimeEditor(const bool replaceInput)
{
    if (!canvas_ || !goToTimeLabel_ || !goToTimeEdit_
        || !goToTimeRangeLabel_ || !goToTimeOtherEdgeButton_
        || !goToTimeGoButton_) {
        return;
    }
    const auto* scenario = activeScenario();
    if (!scenario) return;
    const auto format = [this](const Tick tick) {
        return QString::fromStdString(formatTick(tick, project_.timeBase));
    };

    const auto anchor = canvas_->explicitRangeAnchorTick();
    const auto active = canvas_->explicitRangeActiveTick();
    if (goToTimeEditsRange_ && (!anchor || !active)) {
        goToTimeEditsRange_ = false;
        goToTimeEditsRangeWidth_ = false;
    }

    const QSignalBlocker blocker(goToTimeEdit_);
    if (goToTimeEditsRange_) {
        goToTimeLabel_->setCursor(Qt::PointingHandCursor);
        if (goToTimeEditsRangeWidth_) {
            const auto width = *active > *anchor
                ? *active - *anchor
                : *anchor - *active;
            const auto towardEnd = *active > *anchor;
            goToTimeLabel_->setText(tr("Range width ▾"));
            goToTimeLabel_->setAccessibleName(
                tr("Range width mode; click to edit the active edge"));
            goToTimeLabel_->setToolTip(
                tr("Click to switch from exact range width to exact active edge"));
            goToTimeEdit_->setPlaceholderText(tr("25 ns or cycle 3"));
            goToTimeEdit_->setAccessibleName(tr("Exact selected range width"));
            goToTimeEdit_->setToolTip(
                tr("Set a positive width from the fixed anchor using decimal ps, ns, us, or ms, an integer tick, or cycle N"));
            if (replaceInput) goToTimeEdit_->setText(format(width));
            goToTimeRangeLabel_->setText(
                towardEnd
                    ? tr("Anchor %1 · to End").arg(format(*anchor))
                    : tr("Anchor %1 · to 0").arg(format(*anchor)));
            goToTimeRangeLabel_->setAccessibleName(
                towardEnd
                    ? tr("Fixed range anchor %1; active edge extends toward timeline end")
                          .arg(format(*anchor))
                    : tr("Fixed range anchor %1; active edge extends toward timeline start")
                          .arg(format(*anchor)));
            goToTimeGoButton_->setText(tr("Set width"));
            goToTimeGoButton_->setAccessibleName(tr("Set exact range width"));
            goToTimeGoButton_->setToolTip(
                tr("Apply this exact width from the fixed anchor (Enter)"));
            goToTimeOtherEdgeButton_->setToolTip(
                tr("Keep the range width and switch which edge is fixed"));
        } else {
            goToTimeLabel_->setText(tr("Range edge ▾"));
            goToTimeLabel_->setAccessibleName(
                tr("Range edge mode; click to edit the range width"));
            goToTimeLabel_->setToolTip(
                tr("Click to switch from exact active edge to exact range width"));
            goToTimeEdit_->setPlaceholderText(tr("Exact active edge"));
            goToTimeEdit_->setAccessibleName(tr("Exact selected range edge"));
            goToTimeEdit_->setToolTip(
                tr("Set the active range edge using decimal ps, ns, us, or ms, an integer tick, or cycle N"));
            if (replaceInput) goToTimeEdit_->setText(format(*active));
            goToTimeRangeLabel_->setText(
                tr("Anchor %1 · End %2")
                    .arg(format(*anchor))
                    .arg(format(scenario->duration)));
            goToTimeRangeLabel_->setAccessibleName(
                tr("Fixed range anchor and timeline end"));
            goToTimeGoButton_->setText(tr("Set edge"));
            goToTimeGoButton_->setAccessibleName(tr("Set exact range edge"));
            goToTimeGoButton_->setToolTip(
                tr("Apply this exact time to the active range edge (Enter)"));
            goToTimeOtherEdgeButton_->setToolTip(
                tr("Keep the range and switch the exact editor to its other edge"));
        }
        goToTimeOtherEdgeButton_->setVisible(true);
    } else {
        goToTimeLabel_->setText(tr("Go to"));
        goToTimeLabel_->setAccessibleName(tr("Go to time"));
        goToTimeLabel_->setToolTip({});
        goToTimeLabel_->setCursor(Qt::ArrowCursor);
        goToTimeEdit_->setPlaceholderText(tr("2.5 ns or cycle 25"));
        goToTimeEdit_->setAccessibleName(tr("Exact timeline position"));
        goToTimeEdit_->setToolTip(
            tr("Enter a decimal ps, ns, us, or ms value, an integer tick, or a clock cycle within the scenario"));
        if (replaceInput) goToTimeEdit_->setText(format(canvas_->cursorTick()));
        goToTimeRangeLabel_->setText(
            tr("%1–%2").arg(format(0)).arg(format(scenario->duration)));
        goToTimeRangeLabel_->setAccessibleName(tr("Available timeline range"));
        goToTimeOtherEdgeButton_->setVisible(false);
        goToTimeGoButton_->setText(tr("Go"));
        goToTimeGoButton_->setAccessibleName(tr("Go to exact time"));
        goToTimeGoButton_->setToolTip(
            tr("Move the edit cursor to this time (Enter)"));
    }
}

void MainWindow::closeGoToTime(const bool announce)
{
    if (!goToTimeWidgetAction_ || !goToTimeWidgetAction_->isVisible()) return;
    const auto wasRangeEdit = goToTimeEditsRange_;
    goToTimeWidgetAction_->setVisible(false);
    goToTimeEditsRange_ = false;
    goToTimeEditsRangeWidth_ = false;
    if (wasRangeEdit && rangeEditPaletteAction_
        && canvas_ && canvas_->hasExplicitRangeSelection()) {
        rangeEditPaletteAction_->setVisible(true);
    }
    if (canvas_ && canvas_->viewport()) {
        canvas_->viewport()->setFocus(Qt::OtherFocusReason);
    }
    if (!announce) return;

    if (wasRangeEdit && canvas_ && canvas_->selectedTimeRange()) {
        const auto [start, end] = *canvas_->selectedTimeRange();
        statusBar()->showMessage(
            tr("Exact range edit closed · selection remains %1–%2 · Ctrl+G reopens it")
                .arg(QString::fromStdString(
                    formatTick(start, project_.timeBase)))
                .arg(QString::fromStdString(
                    formatTick(end, project_.timeBase))),
            5'000);
        return;
    }
    const auto location = canvas_
        ? QString::fromStdString(formatTick(canvas_->cursorTick(), project_.timeBase))
        : QString{};
    statusBar()->showMessage(
        location.isEmpty()
            ? tr("Go to time closed · Ctrl+G opens it again")
            : tr("Go to time closed · edit cursor remains at %1 · Ctrl+G opens it again")
                  .arg(location),
        5'000);
}

void MainWindow::submitGoToTime()
{
    if (!canvas_ || !goToTimeWidgetAction_
        || !goToTimeWidgetAction_->isVisible() || !goToTimeEdit_) {
        return;
    }
    const auto* scenario = activeScenario();
    if (!scenario) return;

    if (!commitPendingEdits()) return;
    canvas_->dismissInlineValueEditor();
    const auto editingRange =
        goToTimeEditsRange_ && canvas_->hasExplicitRangeSelection();
    const ClockDomain* clock = nullptr;
    const Lane* selectedLane = nullptr;
    const auto selectedLaneId = canvas_->selectedLaneId();
    if (!selectedLaneId.isEmpty()) {
        selectedLane = findLane(*scenario, selectedLaneId.toStdString());
        if (selectedLane && !selectedLane->clockDomainId.empty()) {
            clock = findClock(project_, selectedLane->clockDomainId);
        }
    }
    if (!clock && project_.clockDomains.size() == 1) {
        clock = &project_.clockDomains.front();
    }

    const auto input = goToTimeEdit_->text().trimmed();
    std::optional<std::int64_t> cycle;
    QString error;
    const auto editingWidth = editingRange && goToTimeEditsRangeWidth_;
    const auto tick = editingWidth
        ? parseRangeWidthText(
              input,
              project_.timeBase,
              clock,
              cycle,
              error)
        : parseTimeText(
              input,
              project_.timeBase,
              clock,
              cycle,
              error);
    const auto showError = [this](const QString& message) {
        goToTimeEdit_->setStyleSheet(
            QStringLiteral("QLineEdit { border: 1px solid #c96d6d; }"));
        goToTimeEdit_->setFocus(Qt::OtherFocusReason);
        statusBar()->showMessage(message, 6'000);
    };
    if (!tick) {
        showError(
            editingRange
                ? editingWidth
                    ? tr("Cannot set range width to “%1” · %2").arg(input, error)
                    : tr("Cannot set range edge to “%1” · %2").arg(input, error)
                : tr("Cannot go to “%1” · %2").arg(input, error));
        return;
    }

    const auto format = [this](const Tick value) {
        return QString::fromStdString(formatTick(value, project_.timeBase));
    };
    if (!editingWidth && (*tick < 0 || *tick > scenario->duration)) {
        showError(
            editingRange
                ? tr("Cannot set range edge to “%1” · enter a time from %2 to %3")
                      .arg(input)
                      .arg(format(0))
                      .arg(format(scenario->duration))
                : tr("Cannot go to “%1” · enter a time from %2 to %3")
                      .arg(input)
                      .arg(format(0))
                      .arg(format(scenario->duration)));
        return;
    }

    if (editingRange) {
        const auto anchor = canvas_->explicitRangeAnchorTick();
        const auto active = canvas_->explicitRangeActiveTick();
        if (!anchor || !active) {
            showError(
                editingWidth
                    ? tr("Cannot set range width · the selected range is no longer available")
                    : tr("Cannot set range edge · the selected range is no longer available"));
            return;
        }

        Tick targetTick = *tick;
        if (editingWidth) {
            if (*tick <= 0) {
                showError(
                    tr("Cannot set range width to %1 · the width must be greater than zero")
                        .arg(format(*tick)));
                return;
            }
            const auto towardEnd = *active > *anchor;
            const auto maximumWidth = towardEnd
                ? scenario->duration - *anchor
                : *anchor;
            if (*tick > maximumWidth) {
                showError(
                    tr("Cannot set range width to %1 · maximum toward %2 from anchor %3 is %4")
                        .arg(format(*tick))
                        .arg(towardEnd ? tr("End") : tr("start"))
                        .arg(format(*anchor))
                        .arg(format(maximumWidth)));
                return;
            }
            targetTick = towardEnd
                ? *anchor + *tick
                : *anchor - *tick;
        } else {
            if (*tick == *anchor) {
                showError(
                    tr("Cannot set range edge to %1 · the range must remain non-empty")
                        .arg(format(*tick)));
                return;
            }
        }
        if (!canvas_->setExplicitRangeActiveTick(targetTick)) {
            showError(
                editingWidth
                    ? tr("Cannot set range width to %1 · keep it inside the timeline")
                          .arg(format(*tick))
                    : tr("Cannot set range edge to %1 · keep it inside the timeline and away from the anchor")
                          .arg(format(*tick)));
            return;
        }
        goToTimeEdit_->setStyleSheet({});
        syncGoToTimeEditor(true);
        goToTimeEdit_->setFocus(Qt::OtherFocusReason);
        goToTimeEdit_->selectAll();

        const auto [start, end] = *canvas_->selectedTimeRange();
        auto message = editingWidth
            ? tr("Range width set to %1 · active edge %2 · selection %3–%4 · %5 signal(s) kept")
                  .arg(format(*tick))
                  .arg(format(targetTick))
                  .arg(format(start))
                  .arg(format(end))
                  .arg(canvas_->selectedLaneIds().size())
            : tr("Range edge set to %1 · selection %2–%3 · %4 signal(s) kept")
                  .arg(format(*tick))
                  .arg(format(start))
                  .arg(format(end))
                  .arg(canvas_->selectedLaneIds().size());
        if (cycle && clock) {
            message.append(
                editingWidth
                    ? tr(" · %1 cycle(s) on %2")
                          .arg(*cycle)
                          .arg(QString::fromStdString(clock->name))
                    : tr(" · cycle %1 on %2")
                          .arg(*cycle)
                          .arg(QString::fromStdString(clock->name)));
        }
        message.append(
            editingWidth
                ? tr(" · Enter sets again · Other edge reverses direction · Esc returns")
                : tr(" · Enter sets again · Other edge switches endpoints · Esc returns"));
        statusBar()->showMessage(message);
        return;
    }

    canvas_->goToTick(*tick);
    rememberActiveScenarioLocation();
    goToTimeEdit_->setStyleSheet({});
    {
        const QSignalBlocker blocker(goToTimeEdit_);
        goToTimeEdit_->setText(format(*tick));
    }
    goToTimeEdit_->setFocus(Qt::OtherFocusReason);
    goToTimeEdit_->selectAll();

    auto message = tr("Edit cursor moved to %1").arg(format(*tick));
    if (cycle && clock) {
        message.append(
            tr(" · cycle %1 on %2")
                .arg(*cycle)
                .arg(QString::fromStdString(clock->name)));
    }
    if (selectedLane) {
        message.append(
            tr(" · %1 remains selected")
                .arg(QString::fromStdString(selectedLane->name)));
    }
    message.append(tr(" · Enter jumps again · Esc closes"));
    statusBar()->showMessage(message);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!commitPendingEdits()) {
        event->ignore();
        return;
    }
    if (!pendingQuickLaneId_.isEmpty()) cancelQuickLaneSetup(pendingQuickLaneId_);
    if (confirmDiscardChanges()) {
        if (traceCancelFlag_) traceCancelFlag_->store(true);
        if (simulationRunner_ && simulationRunner_->running()) {
            static_cast<void>(simulationRunner_->cancel());
        }
        rememberActiveScenarioLocation();
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == goToTimeLabel_ && event
        && event->type() == QEvent::MouseButtonRelease) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton
            && goToTimeEditsRange_
            && goToTimeWidgetAction_
            && goToTimeWidgetAction_->isVisible()
            && canvas_
            && canvas_->hasExplicitRangeSelection()) {
            goToTimeEditsRangeWidth_ = !goToTimeEditsRangeWidth_;
            syncGoToTimeEditor(true);
            goToTimeEdit_->setFocus(Qt::OtherFocusReason);
            goToTimeEdit_->selectAll();
            statusBar()->showMessage(
                goToTimeEditsRangeWidth_
                    ? tr("Editing exact range width from the fixed anchor · Enter applies · click Range width to return to edge entry")
                    : tr("Editing the exact active range edge · Enter applies · click Range edge to enter a width"));
            return true;
        }
    }

    if (watched == goToTimeEdit_ && event
        && event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            closeGoToTime();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return
            || keyEvent->key() == Qt::Key_Enter) {
            submitGoToTime();
            return true;
        }
    }

    if (watched == signalFindEdit_ && event
        && event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            closeSignalFind();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return
            || keyEvent->key() == Qt::Key_Enter) {
            stepSignalFind(
                keyEvent->modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
            stepSignalFind(keyEvent->key() == Qt::Key_Up ? -1 : 1);
            return true;
        }
    }
    const auto watchesCanvas = canvas_
        && (watched == canvas_ || watched == canvas_->viewport());
    if (watchesCanvas && event) {
        if (event->type() == QEvent::DragEnter) {
            auto* drag = static_cast<QDragEnterEvent*>(event);
            if (droppedProjectPath(drag->mimeData())) {
                drag->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            auto* drag = static_cast<QDragMoveEvent*>(event);
            if (droppedProjectPath(drag->mimeData())) {
                drag->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto* drop = static_cast<QDropEvent*>(event);
            const auto path = droppedProjectPath(drop->mimeData());
            if (path) {
                drop->acceptProposedAction();
                QTimer::singleShot(0, this, [this, path = *path] {
                    openProjectPath(path);
                });
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateRecentProjectsMenu()
{
    if (!recentProjectsMenu_) return;
    recentProjectsMenu_->clear();

    const auto paths = storedRecentProjectPaths();
    if (paths.isEmpty()) {
        auto* emptyAction = recentProjectsMenu_->addAction(tr("No recent projects"));
        emptyAction->setObjectName(QStringLiteral("NoRecentProjectsAction"));
        emptyAction->setEnabled(false);
        return;
    }

    qsizetype index = 0;
    for (const auto& path : paths) {
        ++index;
        const QFileInfo info(path);
        auto fileName = info.fileName();
        auto directory = QFileInfo(info.absolutePath()).fileName();
        if (directory.isEmpty()) {
            directory = QDir::toNativeSeparators(info.absolutePath());
        }
        fileName.replace(QLatin1Char('&'), QStringLiteral("&&"));
        directory.replace(QLatin1Char('&'), QStringLiteral("&&"));
        auto* action = recentProjectsMenu_->addAction(
            tr("&%1 %2 — %3").arg(index).arg(fileName, directory));
        action->setObjectName(
            QStringLiteral("RecentProjectAction%1").arg(index));
        action->setData(path);
        action->setToolTip(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path] {
            openRecentProject(path);
        });
    }
}

void MainWindow::rememberProjectPath(const QString& path)
{
    const auto normalized = normalizedProjectPath(path);
    if (normalized.isEmpty()) return;

    auto paths = storedRecentProjectPaths();
    paths.removeIf([&normalized](const QString& existing) {
        return sameProjectPath(existing, normalized);
    });
    paths.prepend(normalized);
    while (paths.size() > maximumRecentProjects) paths.removeLast();
    storeRecentProjectPaths(paths);

    QSettings settings;
    settings.setValue(
        QStringLiteral("files/lastProjectDirectory"),
        QFileInfo(normalized).absolutePath());
    QTimer::singleShot(0, this, &MainWindow::updateRecentProjectsMenu);
}

QString MainWindow::projectDialogDirectory() const
{
    if (!projectFile_.isEmpty()) {
        return QFileInfo(normalizedProjectPath(projectFile_)).absolutePath();
    }

    const auto remembered = QSettings{}.value(
        QStringLiteral("files/lastProjectDirectory")).toString();
    if (!remembered.isEmpty() && QFileInfo(remembered).isDir()) {
        return QDir::cleanPath(remembered);
    }

    const auto documents = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    return !documents.isEmpty() && QFileInfo(documents).isDir()
        ? documents
        : QDir::homePath();
}

void MainWindow::openRecentProject(const QString& path)
{
    const auto normalized = normalizedProjectPath(path);
    if (!QFileInfo(normalized).isFile()) {
        auto paths = storedRecentProjectPaths();
        paths.removeIf([&normalized](const QString& existing) {
            return sameProjectPath(existing, normalized);
        });
        storeRecentProjectPaths(paths);
        QTimer::singleShot(0, this, &MainWindow::updateRecentProjectsMenu);
        statusBar()->showMessage(
            tr("Recent project no longer exists: %1").arg(normalized),
            8'000);
        return;
    }
    openProjectPath(normalized);
}

void MainWindow::openProjectPath(const QString& path)
{
    const auto normalized = normalizedProjectPath(path);
    if (sameProjectPath(projectFile_, normalized)) {
        statusBar()->showMessage(
            tr("Already open: %1").arg(normalized),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;
    if (!pendingQuickLaneId_.isEmpty()) cancelQuickLaneSetup(pendingQuickLaneId_);
    if (!confirmDiscardChanges()) return;
    if (!loadFromPath(normalized) && dirty_) scheduleAutosave();
}

void MainWindow::openProject()
{
    if (!commitPendingEdits()) return;
    if (!pendingQuickLaneId_.isEmpty()) cancelQuickLaneSetup(pendingQuickLaneId_);
    const auto path = QFileDialog::getOpenFileName(
        this,
        tr("Open Wave Workbench project"),
        projectDialogDirectory(),
        tr("Wave Workbench project (*.wave.json);;Recovery snapshot (*.autosave);;JSON files (*.json)"));
    if (path.isEmpty() || !confirmDiscardChanges()) return;
    if (!loadFromPath(path) && dirty_) scheduleAutosave();
}

void MainWindow::saveProject()
{
    if (!commitPendingEdits()) return;
    if (!pendingQuickLaneId_.isEmpty()) {
        canvas_->showQuickLaneSetupError(tr("Press Enter to finish this signal before saving."));
        return;
    }
    if (projectFile_.isEmpty()) {
        saveProjectAs();
        return;
    }
    writeToPath(projectFile_);
}

void MainWindow::saveProjectAs()
{
    if (!commitPendingEdits()) return;
    if (!pendingQuickLaneId_.isEmpty()) {
        canvas_->showQuickLaneSetupError(tr("Press Enter to finish this signal before saving."));
        return;
    }
    const auto suggested = projectFile_.isEmpty()
        ? QDir(projectDialogDirectory()).filePath(
              QStringLiteral("project.wave.json"))
        : projectFile_;
    auto path = QFileDialog::getSaveFileName(
        this,
        tr("Save Wave Workbench project"),
        suggested,
        tr("Wave Workbench project (*.wave.json);;JSON files (*.json)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".wave.json");
    }
    writeToPath(path);
}

void MainWindow::undo()
{
    if (auto* editor = qobject_cast<QLineEdit*>(focusWidget());
        editor
        && editor->isVisibleTo(this)
        && editor->isEnabled()
        && !editor->isReadOnly()) {
        if (editor->isUndoAvailable()) editor->undo();
        updateCommandActions();
        return;
    }
    if (!pendingQuickLaneId_.isEmpty()
        && commandStack_.size() > pendingQuickCommandSize_) {
        if (commandStack_.undoLastAfter(pendingQuickCommandSize_)) {
            invalidateCompareResult();
            observedCommandStateId_ = commandStack_.stateId();
            dirty_ = commandStack_.size() > pendingQuickCommandSize_
                ? true
                : quickLaneDirtyBefore_;
            if (dirty_) scheduleAutosave();
            else autosavePending_ = false;
            canvas_->refreshModel();
            updateCommandActions();
            updateWindowTitle();
            statusBar()->showMessage(
                tr("Undid the later edit · finish the signal or press Ctrl+Z again to cancel"),
                5'000);
        } else {
            canvas_->showQuickLaneSetupError(
                tr("The later edit could not be undone; finish or cancel the signal first."));
        }
        return;
    }
    if (!pendingQuickLaneId_.isEmpty()) {
        cancelQuickLaneSetup(pendingQuickLaneId_);
        return;
    }
    const auto description =
        QString::fromStdString(commandStack_.undoDescription());
    const auto historyStateBefore = commandStack_.stateId();
    const auto changedWaveform =
        selectScenarioForHistoryState(historyStateBefore);
    if (commandStack_.undo()) {
        invalidateCompareResult();
        observedCommandStateId_ = commandStack_.stateId();
        const auto cleanupFailure = synchronizeDirtyState();
        canvas_->invalidateRangeSequenceHistoryContext();
        canvas_->refreshModel();
        const auto restoredSelection =
            canvas_->restoreSelectionForHistoryTransition(
                historyStateBefore,
                commandStack_.stateId());
        updateCommandActions();
        updateWindowTitle();
        auto message = dirty_
            ? tr("Undid %1 · Ctrl+Y to redo").arg(description)
            : projectFile_.isEmpty()
                ? tr("Undid %1 · all changes undone · Ctrl+Y to redo").arg(description)
                : tr("Undid %1 · back to saved version · Ctrl+Y to redo").arg(description);
        if (!restoredSelection.isEmpty()) {
            message += tr(" · %1").arg(restoredSelection);
        }
        if (changedWaveform) {
            message += tr(" - showing %1").arg(activeScenarioLabel());
        }
        if (!cleanupFailure.isEmpty()) {
            message += tr(" · recovery snapshot remains: %1").arg(cleanupFailure);
        }
        statusBar()->showMessage(message, cleanupFailure.isEmpty() ? 5'000 : 10'000);
    }
}

void MainWindow::redo()
{
    if (auto* editor = qobject_cast<QLineEdit*>(focusWidget());
        editor
        && editor->isVisibleTo(this)
        && editor->isEnabled()
        && !editor->isReadOnly()) {
        if (editor->isRedoAvailable()) editor->redo();
        updateCommandActions();
        return;
    }
    if (!pendingQuickLaneId_.isEmpty()) {
        canvas_->showQuickLaneSetupError(tr("Finish or cancel the current signal first."));
        return;
    }
    const auto description =
        QString::fromStdString(commandStack_.redoDescription());
    const auto historyStateBefore = commandStack_.stateId();
    if (commandStack_.redo()) {
        invalidateCompareResult();
        observedCommandStateId_ = commandStack_.stateId();
        const auto changedWaveform =
            selectScenarioForHistoryState(observedCommandStateId_);
        const auto cleanupFailure = synchronizeDirtyState();
        canvas_->invalidateRangeSequenceHistoryContext();
        canvas_->refreshModel();
        const auto restoredSelection =
            canvas_->restoreSelectionForHistoryTransition(
                historyStateBefore,
                commandStack_.stateId());
        updateCommandActions();
        updateWindowTitle();
        auto message = dirty_
            ? tr("Redid %1 · Ctrl+Z to undo").arg(description)
            : projectFile_.isEmpty()
                ? tr("Redid %1 · all changes undone · Ctrl+Z to undo").arg(description)
                : tr("Redid %1 · back to saved version · Ctrl+Z to undo").arg(description);
        if (!restoredSelection.isEmpty()) {
            message += tr(" · %1").arg(restoredSelection);
        }
        if (changedWaveform) {
            message += tr(" - showing %1").arg(activeScenarioLabel());
        }
        if (!cleanupFailure.isEmpty()) {
            message += tr(" · recovery snapshot remains: %1").arg(cleanupFailure);
        }
        statusBar()->showMessage(message, cleanupFailure.isEmpty() ? 5'000 : 10'000);
    }
}

void MainWindow::markEdited()
{
    invalidateCompareResult();
    if (simulationResultMode_) {
        if (!simulationBatchRun_.running()
            && simulationBatchRun_.state() != SimulationBatchState::Idle) {
            simulationBatchRun_.reset();
            simulationBatchReports_.clear();
            updateSimulationBatchView();
        }
        simulationStateMachine_.markStimulusEdited();
        updateSimulationControls();
    }
    const auto currentStateId = commandStack_.stateId();
    if (currentStateId == observedCommandStateId_) {
        ++externalRevision_;
    } else {
        commandScenarioIndices_[currentStateId] = activeScenarioIndex_;
    }
    observedCommandStateId_ = currentStateId;
    (void)synchronizeDirtyState();
    updateCommandActions();
    updateWindowTitle();
}

QString MainWindow::synchronizeDirtyState()
{
    const auto currentStateId = commandStack_.stateId();
    dirty_ = !cleanCommandStateId_.has_value()
        || currentStateId != *cleanCommandStateId_
        || externalRevision_ != cleanExternalRevision_;
    if (dirty_) {
        scheduleAutosave();
        return {};
    }

    if (autosaveTimer_) autosaveTimer_->stop();
    ++autosaveGeneration_;
    autosavePending_ = false;
    const auto snapshotPath = autosavePathForProject(projectFile_);
    if (snapshotPath.isEmpty()
        || snapshotPath == autosaveInFlightPath_
        || !QFileInfo::exists(snapshotPath)
        || QFile::remove(snapshotPath)) {
        return {};
    }
    return snapshotPath;
}

void MainWindow::resetEditTracking(const bool clean)
{
    observedCommandStateId_ = commandStack_.stateId();
    commandScenarioIndices_.clear();
    externalRevision_ = 0;
    cleanExternalRevision_ = 0;
    if (clean) {
        cleanCommandStateId_ = observedCommandStateId_;
        dirty_ = false;
    } else {
        cleanCommandStateId_.reset();
        dirty_ = true;
    }
}

void MainWindow::updateSelection(const QString& laneId, const qint64 tick)
{
    Q_UNUSED(laneId)
    Q_UNUSED(tick)
    updateLaneOrderActions();
    updateWaveContext();
    updateSegmentActions();
    scheduleActiveScenarioLocationMemory();
}

void MainWindow::updateCommandActions()
{
    const auto* editor = qobject_cast<QLineEdit*>(focusWidget());
    const auto textUndoAvailable = editor
        && editor->isVisibleTo(this)
        && editor->isEnabled()
        && !editor->isReadOnly()
        && editor->isUndoAvailable();
    const auto textRedoAvailable = editor
        && editor->isVisibleTo(this)
        && editor->isEnabled()
        && !editor->isReadOnly()
        && editor->isRedoAvailable();
    const auto textSessionActive = editor
        && editor->isVisibleTo(this)
        && editor->isEnabled()
        && !editor->isReadOnly();
    undoAction_->setEnabled(
        textSessionActive ? textUndoAvailable : commandStack_.canUndo());
    redoAction_->setEnabled(
        textSessionActive ? textRedoAvailable : commandStack_.canRedo());
    undoAction_->setText(
        textSessionActive
            ? textUndoAvailable ? tr("Undo text edit") : tr("Undo")
            : commandStack_.canUndo()
            ? tr("Undo %1").arg(QString::fromStdString(commandStack_.undoDescription()))
            : tr("Undo"));
    redoAction_->setText(
        textSessionActive
            ? textRedoAvailable ? tr("Redo text edit") : tr("Redo")
            : commandStack_.canRedo()
            ? tr("Redo %1").arg(QString::fromStdString(commandStack_.redoDescription()))
            : tr("Redo"));
    undoAction_->setToolTip(
        textSessionActive
            ? textUndoAvailable
                ? tr("Undo the last change in the active text field")
                : tr("Nothing to undo in the active text field")
            : commandStack_.canUndo()
                ? tr("Undo %1").arg(
                      QString::fromStdString(
                          commandStack_.undoDescription()))
                : tr("Nothing to undo"));
    redoAction_->setToolTip(
        textSessionActive
            ? textRedoAvailable
                ? tr("Redo the last reverted change in the active text field")
                : tr("Nothing to redo in the active text field")
            : commandStack_.canRedo()
                ? tr("Redo %1").arg(
                      QString::fromStdString(
                          commandStack_.redoDescription()))
                : tr("Nothing to redo"));
    if (showHiddenLanesAction_) {
        const auto* scenario = activeScenario();
        const auto hiddenCount = scenario
            ? static_cast<std::size_t>(std::count_if(
                  scenario->lanes.begin(),
                  scenario->lanes.end(),
                  [](const Lane& lane) { return !lane.visible; }))
            : std::size_t{0};
        showHiddenLanesAction_->setVisible(hiddenCount > 0);
        showHiddenLanesAction_->setEnabled(hiddenCount > 0);
        showHiddenLanesAction_->setText(
            hiddenCount == 1
                ? tr("Show 1 hidden item")
                : tr("Show %1 hidden items").arg(
                    static_cast<qulonglong>(hiddenCount)));
    }
    updateLaneOrderActions();
    updateWaveContext();
    updateSegmentActions();
}

void MainWindow::updateWaveContext()
{
    if (!canvas_) return;

    if (waveTargetLabel_) {
        waveTargetLabel_->setText(canvas_->editTargetSummary());
        waveTargetLabel_->setToolTip(canvas_->editTargetToolTip());
    }
    if (asyncTimingAction_) {
        const auto timing = canvas_->editTimingSummary();
        const auto clockSynchronized = timing.startsWith(
            tr("Sync"), Qt::CaseInsensitive);
        asyncTimingAction_->setText(
            tr("Timing: %1").arg(timing));
        asyncTimingAction_->setToolTip(
            canvas_->asynchronousEditing()
                ? tr("Current mode: %1. Left/Right and Shift+Left/Right move one tick. Click to return to clock-aligned one-beat editing.")
                      .arg(timing)
                : clockSynchronized
                    ? tr("Current mode: %1. Left/Right and Shift+Left/Right follow this associated-clock beat. Click to allow arbitrary tick offsets with light snapping.")
                          .arg(timing)
                    : tr("Current mode: %1. This signal has no associated clock, so edits use the fixed grid. Click to allow arbitrary tick offsets with light snapping.")
                          .arg(timing));
        asyncTimingAction_->setStatusTip(
            canvas_->asynchronousEditing()
                ? tr("Async timing is active; click for clock-aligned Sync editing")
                : clockSynchronized
                    ? tr("Clock-synchronized timing is active; click for arbitrary-tick Async editing")
                    : tr("Unclocked fixed-grid timing is active; click for arbitrary-tick Async editing"));
    }
}

void MainWindow::updateSegmentActions()
{
    if (!canvas_) return;
    const auto waveEdit = canvas_->tool() == WaveCanvas::Tool::WaveEdit;
    const auto* scenario = activeScenario();
    const auto selectedLaneId = canvas_->selectedLaneId().toStdString();
    const auto* selectedLane = scenario
        ? findLane(*scenario, selectedLaneId)
        : nullptr;
    const auto segmentSelected = waveEdit
        && !canvas_->selectedSegmentId().isEmpty()
        && !canvas_->selectedSegmentLaneId().isEmpty();
    const auto navigableLane = waveEdit
        && selectedLane
        && selectedLane->kind != LaneKind::Group
        && selectedLane->kind != LaneKind::Bit;

    if (segmentMenu_) {
        segmentMenu_->setEnabled(navigableLane || segmentSelected);
    }
    if (selectSegmentAtCursorAction_) {
        selectSegmentAtCursorAction_->setEnabled(navigableLane);
    }
    if (previousSegmentAction_) previousSegmentAction_->setEnabled(navigableLane);
    if (nextSegmentAction_) nextSegmentAction_->setEnabled(navigableLane);
    const auto configureSegmentAction =
        [this, segmentSelected](
            QAction* action,
            const WaveCanvas::SegmentAction segmentAction,
            const QString& baseText) {
            if (!action) return;
            const auto state =
                canvas_->selectedSegmentActionState(segmentAction);
            auto text = baseText;
            if (state.valid && state.relationRemovalCount > 0) {
                text += tr(" ⚠%1").arg(static_cast<qulonglong>(
                    state.relationRemovalCount));
            } else if (state.valid && !state.modelChanges) {
                text += tr(" · no change");
            } else if (state.applicable && !state.valid) {
                text += tr(" · unavailable");
            }
            action->setText(text);
            action->setEnabled(
                segmentSelected
                && state.valid
                && state.modelChanges);
            action->setToolTip(state.summary);
            action->setStatusTip(state.summary);
        };
    configureSegmentAction(
        duplicateSegmentBeforeAction_,
        WaveCanvas::SegmentAction::DuplicateBefore,
        tr("Duplicate Segment before"));
    configureSegmentAction(
        duplicateSegmentAfterAction_,
        WaveCanvas::SegmentAction::DuplicateAfter,
        tr("Duplicate Segment after"));
    configureSegmentAction(
        moveSegmentEarlierAction_,
        WaveCanvas::SegmentAction::MoveEarlier,
        tr("Move Segment earlier"));
    configureSegmentAction(
        moveSegmentLaterAction_,
        WaveCanvas::SegmentAction::MoveLater,
        tr("Move Segment later"));
    configureSegmentAction(
        expandSegmentStartAction_,
        WaveCanvas::SegmentAction::ExpandStart,
        tr("Expand Segment start"));
    configureSegmentAction(
        trimSegmentStartAction_,
        WaveCanvas::SegmentAction::TrimStart,
        tr("Trim Segment start"));
    configureSegmentAction(
        expandSegmentEndAction_,
        WaveCanvas::SegmentAction::ExpandEnd,
        tr("Expand Segment end"));
    configureSegmentAction(
        trimSegmentEndAction_,
        WaveCanvas::SegmentAction::TrimEnd,
        tr("Trim Segment end"));
}

bool MainWindow::commitPendingEdits()
{
    if (!canvas_ || !canvas_->commitPendingInlineEdits()) return false;
    if (pendingQuickLaneId_.isEmpty()) return true;
    canvas_->showQuickLaneSetupError(
        tr("Finish or cancel the current signal before continuing."));
    return false;
}

void MainWindow::newProject()
{
    if (!commitPendingEdits()) return;
    if (!pendingQuickLaneId_.isEmpty()) cancelQuickLaneSetup(pendingQuickLaneId_);
    if (!confirmDiscardChanges()) return;
    rememberActiveScenarioLocation();

    Project replacement;
    replacement.id = makeStableId("project");
    replacement.name = "Untitled";
    replacement.timeBase.picosecondsPerTick = 1;
    Scenario scenario;
    scenario.id = makeStableId("scenario");
    scenario.name = "Waveform";
    scenario.duration = toTicks(200, TimeUnit::Nanosecond, replacement.timeBase)
                            .value_or(Tick{200'000});
    replacement.scenarios.push_back(std::move(scenario));

    if (autosaveTimer_) autosaveTimer_->stop();
    ++autosaveGeneration_;
    autosavePending_ = false;
    traceIndex_.reset();
    activeTraceId_.clear();
    traceVisibleSignalIds_.clear();
    traceVisibilityCustomized_ = false;
    canvas_->clearDocumentContexts();
    project_ = std::move(replacement);
    activeScenarioIndex_ = 0;
    projectFile_.clear();
    recoveryLoaded_ = false;
    commandStack_.clear();
    resetEditTracking(true);
    canvas_->setDocument(&project_, activeScenario(), &commandStack_);
    populateScenarioSelector();
    closeSignalFind(false);
    closeGoToTime(false);
    canvas_->setTool(WaveCanvas::Tool::WaveEdit);
    if (markerAction_) markerAction_->setChecked(false);
    updateCommandActions();
    updateWindowTitle();
    invalidateCompareResult();
    statusBar()->showMessage(
        tr("Blank 200 ns waveform ready · add CLK, BIT or BUS"),
        5'000);
}

void MainWindow::showHiddenLanes()
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    if (!scenario) return;
    const auto hiddenCount = static_cast<std::size_t>(std::count_if(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [](const Lane& lane) { return !lane.visible; }));
    if (hiddenCount == 0) {
        statusBar()->showMessage(tr("No hidden items"), 3'000);
        updateCommandActions();
        return;
    }

    try {
        if (!commandStack_.execute(
                std::make_unique<ShowHiddenLanesCommand>(*scenario))) {
            statusBar()->showMessage(tr("No hidden items"), 3'000);
            updateCommandActions();
            return;
        }
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot show hidden items"),
            QString::fromUtf8(exception.what()));
        return;
    }

    canvas_->refreshModel();
    markEdited();
    statusBar()->showMessage(
        hiddenCount == 1
            ? tr("Restored 1 hidden item · Ctrl+Z to undo")
            : tr("Restored %1 hidden items · Ctrl+Z to undo")
                  .arg(static_cast<qulonglong>(hiddenCount)),
        5'000);
}

void MainWindow::showHiddenLane(const QString& laneId)
{
    if (!canvas_ || laneId.isEmpty()) return;
    if (canvas_->hasExplicitRangeSelection()) {
        statusBar()->showMessage(
            tr("Esc clears the selected range before restoring and selecting a hidden item"),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;

    auto* scenario = activeScenario();
    const auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) return;
    const auto laneName = QString::fromStdString(lane->name);
    const auto showingGroup = lane->kind == LaneKind::Group;
    if (lane->visible) {
        statusBar()->showMessage(
            showingGroup
                ? tr("Group %1 is already visible").arg(laneName)
                : tr("Signal %1 is already visible").arg(laneName),
            3'000);
        return;
    }

    try {
        if (!commandStack_.execute(std::make_unique<ShowLaneCommand>(
                *scenario,
                lane->id))) {
            statusBar()->showMessage(
                showingGroup
                    ? tr("Group %1 is already visible").arg(laneName)
                    : tr("Signal %1 is already visible").arg(laneName),
                3'000);
            return;
        }
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot restore item"),
            QString::fromUtf8(exception.what()));
        return;
    }

    canvas_->refreshModel();
    markEdited();
    canvas_->revealLane(laneId);
    selectLaneItem(signalTree_, laneId);
    selectLaneItem(groupTree_, laneId);
    const auto remaining = static_cast<std::size_t>(std::count_if(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [](const Lane& candidate) { return !candidate.visible; }));
    const auto remainder = remaining == 0
        ? tr("no hidden items remain")
        : remaining == 1
            ? tr("1 hidden item remains")
            : tr("%1 hidden items remain").arg(static_cast<qulonglong>(remaining));
    statusBar()->showMessage(
        tr("Restored %1 %2 in its original position · selected · %3 · Ctrl+Z to undo")
            .arg(showingGroup ? tr("group") : tr("signal"), laneName, remainder),
        8'000);
}

void MainWindow::addLane()
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    if (!scenario) return;
    Lane lane;
    lane.id = makeStableId("lane");
    lane.name = "signal";
    lane.kind = LaneKind::Bit;
    lane.color = randomReadableLaneColor(*scenario);
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

void MainWindow::addQuickLane(const LaneKind kind)
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    if (!scenario
        || (kind != LaneKind::Clock
            && kind != LaneKind::Bit
            && kind != LaneKind::Bus)) {
        return;
    }
    if (!pendingQuickLaneId_.isEmpty()) {
        canvas_->showQuickLaneSetupError(tr("Press Enter or Esc before adding another signal."));
        return;
    }

    const auto baseName = kind == LaneKind::Clock
        ? std::string_view{"clk"}
        : kind == LaneKind::Bit
            ? std::string_view{"bit"}
            : std::string_view{"bus"};
    Lane lane;
    lane.id = makeStableId("lane");
    lane.name = uniqueLaneName(*scenario, baseName);
    lane.kind = kind;
    lane.width = kind == LaneKind::Bus ? 8U : 1U;
    lane.radix = kind == LaneKind::Bit ? Radix::Binary : Radix::Hexadecimal;
    lane.color = randomReadableLaneColor(*scenario);
    lane.height = 56;
    if (kind != LaneKind::Clock && project_.clockDomains.size() == 1) {
        lane.clockDomainId = project_.clockDomains.front().id;
    }

    try {
        if (kind == LaneKind::Clock) {
            ClockDomain clock;
            clock.id = makeStableId("clock");
            clock.name = lane.name;
            clock.period = std::max<Tick>(
                1,
                static_cast<Tick>(std::llround(
                    10'000.0
                    / static_cast<double>(std::max<std::int64_t>(
                        1, project_.timeBase.picosecondsPerTick)))));
            clock.phase = 0;
            clock.dutyCycle = {1, 2};
            clock.activeEdge = ClockEdge::Rising;
            lane.clockDomainId = clock.id;
            commandStack_.execute(std::make_unique<AddLaneCommand>(
                project_,
                *scenario,
                lane,
                std::move(clock)));
        } else {
            commandStack_.execute(std::make_unique<AddLaneCommand>(*scenario, lane));
        }
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot add lane"),
            QString::fromUtf8(exception.what()));
        return;
    }

    quickLaneDirtyBefore_ = dirty_;
    pendingQuickLaneId_ = QString::fromStdString(lane.id);
    pendingQuickCommandSize_ = commandStack_.size();
    if (autosaveTimer_) autosaveTimer_->stop();
    ++autosaveGeneration_;
    if (autosaveWatcher_ && autosaveWatcher_->isRunning()) {
        autosavePending_ = quickLaneDirtyBefore_ && !projectFile_.isEmpty();
    }
    canvas_->refreshModel();
    updateCommandActions();
    canvas_->revealLocation(pendingQuickLaneId_, canvas_->cursorTick());

    QStringList clockLabels;
    QStringList clockIds;
    if (kind != LaneKind::Clock) {
        for (const auto& clock : project_.clockDomains) {
            clockLabels.push_back(QString::fromStdString(clock.name));
            clockIds.push_back(QString::fromStdString(clock.id));
        }
    }
    QString parameter;
    if (kind == LaneKind::Clock) {
        if (const auto* clock = findClock(project_, lane.clockDomainId)) {
            parameter = QString::fromStdString(formatTick(clock->period, project_.timeBase));
        }
    } else if (kind == LaneKind::Bus) {
        parameter = QString::number(lane.width);
    }
    canvas_->beginQuickLaneSetup(
        pendingQuickLaneId_,
        kind,
        QString::fromStdString(lane.name),
        parameter,
        clockLabels,
        clockIds,
        QString::fromStdString(lane.clockDomainId));
    statusBar()->showMessage(
        tr("Name the new signal, then press Enter · Esc cancels"),
        6'000);
}

void MainWindow::completeQuickLaneSetup(
    const QString& laneId,
    const QString& name,
    const QString& parameter,
    const QString& clockId)
{
    auto* scenario = activeScenario();
    if (!scenario || laneId != pendingQuickLaneId_) return;
    if (commandStack_.size() != pendingQuickCommandSize_ || !commandStack_.canUndo()) {
        canvas_->showQuickLaneSetupError(
            tr("Another edit interrupted signal creation; undo it before continuing."));
        return;
    }
    auto* lane = findLane(*scenario, laneId.toStdString());
    if (!lane) {
        cancelQuickLaneSetup(laneId);
        return;
    }
    if (name.trimmed().isEmpty()) {
        canvas_->showQuickLaneSetupError(tr("Signal name cannot be empty."));
        return;
    }
    const auto duplicateName = std::any_of(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [&laneId, &name](const Lane& candidate) {
            return QString::fromStdString(candidate.id) != laneId
                && QString::compare(
                       QString::fromStdString(candidate.name),
                       name.trimmed(),
                       Qt::CaseInsensitive)
                    == 0;
        });
    if (duplicateName) {
        canvas_->showQuickLaneSetupError(tr("A signal with this name already exists."));
        return;
    }

    auto replacement = *lane;
    replacement.name = name.trimmed().toStdString();
    try {
        if (replacement.kind == LaneKind::Clock) {
            QString error;
            std::optional<std::int64_t> unusedCycle;
            const auto period = parseTimeText(
                parameter,
                project_.timeBase,
                nullptr,
                unusedCycle,
                error);
            if (!period || *period <= 0) {
                canvas_->showQuickLaneSetupError(
                    error.isEmpty() ? tr("Clock period must be greater than zero.") : error,
                    true);
                return;
            }
            const auto* currentClock = findClock(project_, replacement.clockDomainId);
            if (!currentClock) {
                canvas_->showQuickLaneSetupError(tr("The new clock domain is unavailable."));
                return;
            }
            auto updatedClock = *currentClock;
            updatedClock.name = replacement.name;
            updatedClock.period = *period;
            commandStack_.replaceLast(std::make_unique<AddLaneCommand>(
                project_,
                *scenario,
                replacement,
                std::move(updatedClock)));
        } else {
            if (replacement.kind == LaneKind::Bus) {
                bool validWidth = false;
                const auto width = parameter.toUInt(&validWidth);
                if (!validWidth || width == 0 || width > 65'536) {
                    canvas_->showQuickLaneSetupError(
                        tr("Bus width must be from 1 to 65536."), true);
                    return;
                }
                replacement.width = width;
            }
            auto selectedClockId = clockId.toStdString();
            if (selectedClockId.empty() && project_.clockDomains.size() == 1) {
                selectedClockId = project_.clockDomains.front().id;
            }
            if (!selectedClockId.empty() && !findClock(project_, selectedClockId)) {
                canvas_->showQuickLaneSetupError(tr("Select an available clock."));
                return;
            }
            replacement.clockDomainId = std::move(selectedClockId);
            commandStack_.replaceLast(std::make_unique<AddLaneCommand>(
                *scenario,
                replacement));
        }
    } catch (const std::exception& exception) {
        canvas_->showQuickLaneSetupError(QString::fromUtf8(exception.what()));
        return;
    }

    pendingQuickLaneId_.clear();
    pendingQuickCommandSize_ = 0;
    quickLaneDirtyBefore_ = false;
    canvas_->finishQuickLaneSetup();
    canvas_->refreshModel();
    markEdited();
    canvas_->revealLocation(laneId, canvas_->cursorTick());
    statusBar()->showMessage(
        tr("Added %1 · Ctrl+Z to undo").arg(name.trimmed()),
        5'000);
}

void MainWindow::cancelQuickLaneSetup(const QString& laneId)
{
    if (laneId.isEmpty() || laneId != pendingQuickLaneId_) return;
    if (commandStack_.size() != pendingQuickCommandSize_ || !commandStack_.canUndo()) {
        canvas_->showQuickLaneSetupError(
            tr("Another edit interrupted signal creation; undo it before canceling."));
        return;
    }
    try {
        if (!commandStack_.discardLast()) {
            canvas_->showQuickLaneSetupError(tr("The pending signal cannot be canceled."));
            return;
        }
    } catch (const std::exception& exception) {
        canvas_->showQuickLaneSetupError(QString::fromUtf8(exception.what()));
        return;
    }
    pendingQuickLaneId_.clear();
    pendingQuickCommandSize_ = 0;
    quickLaneDirtyBefore_ = false;
    observedCommandStateId_ = commandStack_.stateId();
    const auto cleanupFailure = synchronizeDirtyState();
    canvas_->finishQuickLaneSetup();
    canvas_->refreshModel();
    updateCommandActions();
    updateWindowTitle();
    statusBar()->showMessage(
        cleanupFailure.isEmpty()
            ? tr("Signal creation canceled")
            : tr("Signal creation canceled · recovery snapshot remains: %1")
                  .arg(cleanupFailure),
        cleanupFailure.isEmpty() ? 3'000 : 10'000);
}

Tick MainWindow::latestContentTick(const Scenario& scenario) const noexcept
{
    Tick latest = 0;
    for (const auto& lane : scenario.lanes) {
        for (const auto& segment : lane.segments) {
            latest = std::max(latest, segment.end);
        }
    }
    for (const auto& event : scenario.events) {
        latest = std::max(
            latest,
            event.tick == std::numeric_limits<Tick>::max()
                ? event.tick
                : event.tick + 1);
    }
    for (const auto& marker : scenario.markers) {
        latest = std::max(latest, std::max(marker.start, marker.end));
    }
    return latest;
}

void MainWindow::changeScenarioDuration(const QString& value)
{
    auto* scenario = activeScenario();
    if (!scenario) return;
    QString error;
    std::optional<std::int64_t> unusedCycle;
    const auto duration = parseTimeText(
        value,
        project_.timeBase,
        nullptr,
        unusedCycle,
        error);
    if (!duration || *duration <= 0) {
        canvas_->showDurationEditError(
            error.isEmpty() ? tr("Timeline end must be greater than zero.") : error);
        return;
    }
    const auto required = latestContentTick(*scenario);
    if (*duration < required) {
        canvas_->showDurationEditError(
            tr("Timeline cannot end before existing content at %1.")
                .arg(QString::fromStdString(formatTick(required, project_.timeBase))));
        return;
    }
    if (*duration == scenario->duration) {
        canvas_->refreshModel();
        statusBar()->showMessage(tr("Timeline end is unchanged"), 2'000);
        return;
    }
    try {
        commandStack_.execute(std::make_unique<ChangeScenarioDurationCommand>(
            *scenario,
            *duration));
    } catch (const std::exception& exception) {
        canvas_->showDurationEditError(QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    canvas_->fitScenario();
    markEdited();
    statusBar()->showMessage(
        tr("Timeline end changed to %1 · Ctrl+Z to undo")
            .arg(QString::fromStdString(formatTick(*duration, project_.timeBase))),
        5'000);
}
void MainWindow::addGroup()
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    if (!scenario) return;
    const auto defaultName = uniqueLaneName(*scenario, "Group");
    const auto name = promptNewGroupName(
        this,
        *scenario,
        tr("Create group"),
        tr("Name the empty Group. Signals can be dragged onto its header after creation."),
        defaultName);
    if (!name) {
        statusBar()->showMessage(tr("Group creation cancelled"), 3'000);
        return;
    }

    Lane group;
    group.id = makeStableId("group");
    group.name = *name;
    group.kind = LaneKind::Group;
    group.color = randomReadableLaneColor(*scenario);
    group.height = 40;
    try {
        commandStack_.execute(std::make_unique<AddLaneCommand>(
            *scenario,
            group));
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
        QString::fromStdString(group.id),
        canvas_->cursorTick());
    selectLaneItem(signalTree_, QString::fromStdString(group.id));
    selectLaneItem(groupTree_, QString::fromStdString(group.id));
    statusBar()->showMessage(
        tr("Created empty group %1 · drag signals onto its header or use Move to group · Ctrl+Z to undo")
            .arg(QString::fromStdString(group.name)),
        8'000);
}

void MainWindow::editSelectedLane()
{
    editLaneById(selectedLaneIdForEditing());
}

void MainWindow::duplicateSelectedLane()
{
    if (canvas_ && canvas_->hasExplicitRangeSelection()) {
        canvas_->duplicateSelectionAfter();
        return;
    }
    if (canvas_ && canvas_->hasLaneHeaderSelection()) {
        const auto laneIds = canvas_->selectedLaneIds();
        if (laneIds.size() > 1) {
            duplicateLanesById(laneIds);
            return;
        }
    }
    duplicateLaneById(selectedLaneIdForEditing());
}

void MainWindow::duplicateLaneById(const QString& laneId)
{
    if (!canvas_ || laneId.isEmpty()) return;
    if (canvas_->hasExplicitRangeSelection()) {
        statusBar()->showMessage(
            tr("Esc clears the selected range before duplicating a whole signal"),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;

    auto* scenario = activeScenario();
    if (!scenario) return;
    const auto source = std::find_if(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [&laneId](const Lane& lane) {
            return lane.id == laneId.toStdString();
        });
    if (source == scenario->lanes.end()) return;
    if (source->kind == LaneKind::Group) {
        statusBar()->showMessage(
            tr("Select a signal, not a group, before duplicating"),
            4'000);
        return;
    }

    const auto sourceName = QString::fromStdString(source->name);
    const auto insertionIndex = static_cast<std::size_t>(
        std::distance(scenario->lanes.begin(), source)) + 1;
    Lane duplicate = *source;
    duplicate.id = makeStableId("lane");
    const auto duplicateBaseName = source->name + "_copy";
    duplicate.name = uniqueLaneName(*scenario, duplicateBaseName);
    duplicate.color = randomReadableLaneColor(*scenario, source->color);
    duplicate.visible = true;
    for (auto& segment : duplicate.segments) {
        segment.id = makeStableId("segment");
    }

    std::optional<ClockDomain> duplicateClock;
    if (duplicate.kind == LaneKind::Clock) {
        const auto* sourceClock = findClock(project_, source->clockDomainId);
        if (!sourceClock) {
            QMessageBox::warning(
                this,
                tr("Cannot duplicate clock"),
                tr("The selected clock does not reference a valid clock domain."));
            return;
        }
        duplicateClock = *sourceClock;
        duplicateClock->id = makeStableId("clock");
        duplicateClock->name = duplicate.name;
        duplicate.clockDomainId = duplicateClock->id;
    }

    try {
        if (duplicateClock) {
            commandStack_.execute(std::make_unique<DuplicateLaneCommand>(
                project_,
                *scenario,
                duplicate,
                *duplicateClock,
                insertionIndex));
        } else {
            commandStack_.execute(std::make_unique<DuplicateLaneCommand>(
                *scenario,
                duplicate,
                insertionIndex));
        }
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot duplicate signal"),
            QString::fromUtf8(exception.what()));
        return;
    }

    canvas_->refreshModel();
    markEdited();
    const auto duplicateId = QString::fromStdString(duplicate.id);
    canvas_->revealLocation(duplicateId, canvas_->cursorTick());
    selectLaneItem(signalTree_, duplicateId);
    selectLaneItem(groupTree_, duplicateId);
    auto result = tr("Duplicated %1 as %2 below the source · waveform and properties copied")
                      .arg(sourceName, QString::fromStdString(duplicate.name));
    if (duplicateClock) result.append(tr(" · independent clock settings"));
    result.append(tr(" · Ctrl+Z to undo · F2 renames"));
    statusBar()->showMessage(result, 6'000);
}

void MainWindow::duplicateLanesById(const QStringList& laneIds)
{
    if (!canvas_ || laneIds.size() < 2) {
        if (laneIds.size() == 1) duplicateLaneById(laneIds.front());
        return;
    }
    if (canvas_->hasExplicitRangeSelection()) {
        statusBar()->showMessage(
            tr("Esc clears the selected range before duplicating whole signals"),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;

    auto* scenario = activeScenario();
    if (!scenario) return;
    std::vector<const Lane*> sources;
    sources.reserve(static_cast<std::size_t>(laneIds.size()));
    std::size_t insertionIndex = 0;
    for (auto index = std::size_t{0};
         index < scenario->lanes.size();
         ++index) {
        const auto& candidate = scenario->lanes[index];
        const auto candidateId = QString::fromStdString(candidate.id);
        if (!laneIds.contains(candidateId)) continue;
        if (candidate.kind == LaneKind::Group
            || !candidate.visible
            || !canvas_->isLaneDisplayed(candidateId)) {
            statusBar()->showMessage(
                tr("The selected signals changed before they could be duplicated"),
                5'000);
            updateLaneOrderActions();
            return;
        }
        sources.push_back(&candidate);
        insertionIndex = index + 1;
    }
    if (sources.size()
        != static_cast<std::size_t>(laneIds.size())) {
        statusBar()->showMessage(
            tr("The selected signals changed before they could be duplicated"),
            5'000);
        updateLaneOrderActions();
        return;
    }

    const auto activeSourceId = canvas_->selectedLaneId().toStdString();
    auto planningScenario = *scenario;
    std::vector<Lane> duplicates;
    std::vector<ClockDomain> duplicateClocks;
    QStringList duplicateIds;
    duplicates.reserve(sources.size());
    duplicateIds.reserve(laneIds.size());
    auto activeDuplicateId = QString{};
    for (const auto* source : sources) {
        Lane duplicate = *source;
        duplicate.id = makeStableId("lane");
        duplicate.name = uniqueLaneName(
            planningScenario,
            source->name + "_copy");
        duplicate.color = randomReadableLaneColor(
            planningScenario,
            source->color);
        duplicate.visible = true;
        for (auto& segment : duplicate.segments) {
            segment.id = makeStableId("segment");
        }

        if (duplicate.kind == LaneKind::Clock) {
            const auto* sourceClock =
                findClock(project_, source->clockDomainId);
            if (!sourceClock) {
                statusBar()->showMessage(
                    tr("A selected Clock no longer references valid clock settings; no signals were duplicated"),
                    6'000);
                return;
            }
            auto duplicateClock = *sourceClock;
            duplicateClock.id = makeStableId("clock");
            duplicateClock.name = duplicate.name;
            duplicate.clockDomainId = duplicateClock.id;
            duplicateClocks.push_back(std::move(duplicateClock));
        }

        const auto duplicateId =
            QString::fromStdString(duplicate.id);
        if (source->id == activeSourceId) {
            activeDuplicateId = duplicateId;
        }
        duplicateIds.push_back(duplicateId);
        planningScenario.lanes.push_back(duplicate);
        duplicates.push_back(std::move(duplicate));
    }
    if (activeDuplicateId.isEmpty()) {
        activeDuplicateId = duplicateIds.front();
    }

    const auto beforeStateId = commandStack_.stateId();
    canvas_->beginCommandSelectionTransition(beforeStateId);
    try {
        commandStack_.execute(
            std::make_unique<DuplicateLanesCommand>(
                project_,
                *scenario,
                duplicates,
                duplicateClocks,
                insertionIndex));
    } catch (const std::exception& exception) {
        canvas_->cancelCommandSelectionTransition();
        QMessageBox::warning(
            this,
            tr("Cannot duplicate selected signals"),
            QString::fromUtf8(exception.what()));
        return;
    }

    canvas_->refreshModel();
    canvas_->selectLaneHeaders(
        duplicateIds,
        activeDuplicateId);
    canvas_->finishCommandSelectionTransition(
        commandStack_.stateId());
    markEdited();
    selectLaneItem(signalTree_, activeDuplicateId);
    selectLaneItem(groupTree_, activeDuplicateId);
    updateLaneOrderActions();

    auto result = tr("Duplicated %1 selected signals as one block below the last source · waveforms, properties, and Group memberships copied")
                      .arg(duplicateIds.size());
    if (!duplicateClocks.empty()) {
        result.append(
            tr(" · %1 Clock signal(s) use independent settings")
                .arg(static_cast<qulonglong>(
                    duplicateClocks.size())));
    }
    result.append(
        tr(" · Event, Relation, and Trace links not copied · one Undo step · Ctrl+Z to undo"));
    statusBar()->showMessage(result, 9'000);
}

void MainWindow::hideSelectedLane()
{
    if (canvas_ && canvas_->hasLaneHeaderSelection()) {
        const auto laneIds = canvas_->selectedLaneIds();
        if (laneIds.size() > 1) {
            hideLanesById(laneIds);
            return;
        }
    }
    hideLaneById(selectedLaneIdForEditing());
}

void MainWindow::hideLaneById(const QString& laneId)
{
    if (!canvas_ || laneId.isEmpty()) return;
    if (canvas_->hasExplicitRangeSelection()) {
        statusBar()->showMessage(
            tr("Esc clears the selected range before hiding a whole item"),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;

    auto* scenario = activeScenario();
    const auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) return;
    const auto laneName = QString::fromStdString(lane->name);
    const auto hidingGroup = lane->kind == LaneKind::Group;
    if (!lane->visible) {
        statusBar()->showMessage(
            hidingGroup
                ? tr("Group %1 is already hidden").arg(laneName)
                : tr("Signal %1 is already hidden").arg(laneName),
            3'000);
        updateCommandActions();
        return;
    }

    try {
        if (!commandStack_.execute(std::make_unique<HideLaneCommand>(
                *scenario,
                lane->id))) {
            statusBar()->showMessage(
                hidingGroup
                    ? tr("Group %1 is already hidden").arg(laneName)
                    : tr("Signal %1 is already hidden").arg(laneName),
                3'000);
            updateCommandActions();
            return;
        }
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot hide item"),
            QString::fromUtf8(exception.what()));
        return;
    }

    canvas_->refreshModel();
    updateSelection(QString{}, canvas_->cursorTick());
    markEdited();
    const auto hiddenCount = static_cast<std::size_t>(std::count_if(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [](const Lane& candidate) { return !candidate.visible; }));
    const auto restoreLabel = hiddenCount == 1
        ? tr("Show 1 hidden item")
        : tr("Show %1 hidden items").arg(static_cast<qulonglong>(hiddenCount));
    statusBar()->showMessage(
        tr("Hidden %1 %2 · %3 at the bottom or in Edit restores hidden items · Ctrl+Z to undo")
            .arg(hidingGroup ? tr("group") : tr("signal"), laneName, restoreLabel),
        8'000);
}

void MainWindow::hideLanesById(const QStringList& laneIds)
{
    if (!canvas_ || laneIds.size() < 2) {
        if (laneIds.size() == 1) hideLaneById(laneIds.front());
        return;
    }
    if (canvas_->hasExplicitRangeSelection()) {
        statusBar()->showMessage(
            tr("Esc clears the selected range before hiding whole signals"),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;

    auto* scenario = activeScenario();
    if (!scenario) return;
    std::vector<std::string> orderedIds;
    orderedIds.reserve(static_cast<std::size_t>(laneIds.size()));
    for (const auto& lane : scenario->lanes) {
        const auto candidateId = QString::fromStdString(lane.id);
        if (lane.kind != LaneKind::Group
            && lane.visible
            && laneIds.contains(candidateId)
            && std::find(orderedIds.begin(), orderedIds.end(), lane.id)
                == orderedIds.end()) {
            orderedIds.push_back(lane.id);
        }
    }
    if (orderedIds.size() != static_cast<std::size_t>(laneIds.size())) {
        statusBar()->showMessage(
            tr("The selected signals changed before they could be hidden"),
            5'000);
        return;
    }

    const auto beforeStateId = commandStack_.stateId();
    canvas_->beginCommandSelectionTransition(beforeStateId);
    try {
        if (!commandStack_.execute(std::make_unique<HideLanesCommand>(
                *scenario,
                orderedIds))) {
            canvas_->cancelCommandSelectionTransition();
            statusBar()->showMessage(
                tr("The selected signals are already hidden"),
                3'000);
            updateCommandActions();
            return;
        }
    } catch (const std::exception& exception) {
        canvas_->cancelCommandSelectionTransition();
        QMessageBox::warning(
            this,
            tr("Cannot hide selected signals"),
            QString::fromUtf8(exception.what()));
        return;
    }

    canvas_->refreshModel();
    updateSelection({}, canvas_->cursorTick());
    canvas_->finishCommandSelectionTransition(commandStack_.stateId());
    markEdited();
    const auto hiddenCount = static_cast<std::size_t>(std::count_if(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [](const Lane& lane) {
            return !lane.visible;
        }));
    const auto restoreLabel = hiddenCount == 1
        ? tr("Show 1 hidden item")
        : tr("Show %1 hidden items").arg(
            static_cast<qulonglong>(hiddenCount));
    statusBar()->showMessage(
        tr("Hidden %1 selected signals · %2 at the bottom or in Edit restores hidden items · Ctrl+Z to undo")
            .arg(static_cast<qulonglong>(orderedIds.size()))
            .arg(restoreLabel),
        8'000);
}

void MainWindow::renameLaneById(const QString& laneId)
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    const auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) return;
    if (!pendingQuickLaneId_.isEmpty()) {
        canvas_->showQuickLaneSetupError(tr("Press Enter or Esc before renaming another signal."));
        return;
    }
    canvas_->beginLaneRename(laneId, QString::fromStdString(lane->name));
}

void MainWindow::completeLaneRename(const QString& laneId, const QString& requestedName)
{
    auto* scenario = activeScenario();
    auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) {
        canvas_->finishLaneRename();
        statusBar()->showMessage(tr("The item being renamed is no longer available."), 4'000);
        return;
    }
    const auto renamingGroup = lane->kind == LaneKind::Group;

    const auto name = requestedName.trimmed();
    if (name.isEmpty()) {
        canvas_->showLaneRenameError(
            renamingGroup
                ? tr("Group name cannot be empty.")
                : tr("Signal name cannot be empty."));
        return;
    }
    const auto duplicate = std::any_of(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [&laneId, &name](const Lane& candidate) {
            return candidate.id != laneId.toStdString()
                && QString::compare(
                       QString::fromStdString(candidate.name),
                       name,
                       Qt::CaseInsensitive)
                    == 0;
        });
    if (duplicate) {
        canvas_->showLaneRenameError(
            renamingGroup
                ? tr("Another lane or group already uses this name.")
                : tr("Another signal already uses this name."));
        return;
    }

    const auto previousName = QString::fromStdString(lane->name);
    if (name == previousName) {
        canvas_->finishLaneRename();
        statusBar()->showMessage(
            renamingGroup ? tr("Group name unchanged") : tr("Signal name unchanged"),
            3'000);
        return;
    }

    auto replacement = *lane;
    replacement.name = name.toStdString();
    try {
        commandStack_.execute(std::make_unique<ChangeLaneCommand>(
            project_, *scenario, lane->id, std::move(replacement)));
    } catch (const std::exception& exception) {
        canvas_->showLaneRenameError(QString::fromUtf8(exception.what()));
        return;
    }

    canvas_->finishLaneRename();
    canvas_->refreshModel();
    markEdited();
    statusBar()->showMessage(
        renamingGroup
            ? tr("Renamed group %1 to %2 · Ctrl+Z to undo").arg(previousName, name)
            : tr("Renamed %1 to %2 · Ctrl+Z to undo").arg(previousName, name),
        5'000);
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
    if (canvas_ && canvas_->hasLaneHeaderSelection()) {
        const auto laneIds = canvas_->selectedLaneIds();
        if (laneIds.size() > 1) {
            removeLanesById(laneIds);
            return;
        }
    }
    removeLaneById(selectedLaneIdForEditing());
}

void MainWindow::removeLaneById(const QString& laneId)
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    const auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) return;

    const auto removedLaneId = lane->id;
    const auto removedLaneName = QString::fromStdString(lane->name);
    const auto removingGroup = lane->kind == LaneKind::Group;
    std::vector<std::string> removedEventIds;
    for (const auto& event : scenario->events) {
        if (event.laneId == removedLaneId) removedEventIds.push_back(event.id);
    }
    const auto removedRelationCount = static_cast<std::size_t>(std::count_if(
        scenario->relations.begin(),
        scenario->relations.end(),
        [&removedEventIds](const Relation& relation) {
            const auto removed = [&removedEventIds](const std::string& eventId) {
                return std::find(
                           removedEventIds.begin(),
                           removedEventIds.end(),
                           eventId)
                    != removedEventIds.end();
            };
            return removed(relation.sourceEventId)
                || removed(relation.targetEventId);
        }));
    const auto ungroupedMemberCount = removingGroup
        ? static_cast<std::size_t>(std::count_if(
              scenario->lanes.begin(),
              scenario->lanes.end(),
              [&removedLaneId](const Lane& candidate) {
                  return candidate.groupId == removedLaneId;
              }))
        : std::size_t{0};
    const auto quantity = [this](const std::size_t count, const QString& noun) {
        return tr("%1 %2%3")
            .arg(static_cast<qulonglong>(count))
            .arg(noun)
            .arg(count == 1 ? QString{} : QStringLiteral("s"));
    };
    QStringList removedEffects;
    if (!removedEventIds.empty()) {
        removedEffects.append(quantity(removedEventIds.size(), tr("event")));
    }
    if (removedRelationCount > 0) {
        removedEffects.append(quantity(removedRelationCount, tr("relation")));
    }

    auto message = removingGroup
        ? tr("Remove group \"%1\"?").arg(removedLaneName)
        : tr("Remove lane \"%1\"?").arg(removedLaneName);
    if (ungroupedMemberCount > 0) {
        message += tr(" %1 will remain and become ungrouped.")
                       .arg(quantity(ungroupedMemberCount, tr("member signal")));
    }
    if (!removedEffects.isEmpty()) {
        message += tr(" This also removes %1.")
                       .arg(removedEffects.join(QStringLiteral(", ")));
    }
    message += tr(" You can undo this with Ctrl+Z.");
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
            removedLaneId));
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

    QStringList results;
    for (const auto& effect : removedEffects) {
        results.append(tr("%1 removed").arg(effect));
    }
    if (ungroupedMemberCount > 0) {
        results.append(tr("%1 ungrouped").arg(
            quantity(ungroupedMemberCount, tr("member signal"))));
    }
    auto result = removingGroup
        ? tr("Removed group %1").arg(removedLaneName)
        : tr("Removed %1").arg(removedLaneName);
    if (!results.isEmpty()) {
        result += tr(" · %1").arg(results.join(QStringLiteral(", ")));
    }
    result += tr(" · Ctrl+Z to undo");
    statusBar()->showMessage(result, 5'000);
}

void MainWindow::removeLanesById(const QStringList& laneIds)
{
    if (!canvas_ || laneIds.size() < 2) {
        if (laneIds.size() == 1) removeLaneById(laneIds.front());
        return;
    }
    if (canvas_->hasExplicitRangeSelection()) {
        statusBar()->showMessage(
            tr("Esc clears the selected range before removing whole signals"),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;

    auto* scenario = activeScenario();
    if (!scenario) return;
    std::vector<std::string> orderedIds;
    QStringList names;
    orderedIds.reserve(static_cast<std::size_t>(laneIds.size()));
    for (const auto& lane : scenario->lanes) {
        const auto candidateId = QString::fromStdString(lane.id);
        if (lane.kind != LaneKind::Group
            && laneIds.contains(candidateId)
            && std::find(orderedIds.begin(), orderedIds.end(), lane.id)
                == orderedIds.end()) {
            orderedIds.push_back(lane.id);
            names.push_back(QString::fromStdString(lane.name));
        }
    }
    if (orderedIds.size() != static_cast<std::size_t>(laneIds.size())) {
        statusBar()->showMessage(
            tr("The selected signals changed before they could be removed"),
            5'000);
        return;
    }

    const auto selected = [&orderedIds](const std::string& laneId) {
        return std::find(orderedIds.begin(), orderedIds.end(), laneId)
            != orderedIds.end();
    };
    std::vector<std::string> removedEventIds;
    for (const auto& event : scenario->events) {
        if (selected(event.laneId)) removedEventIds.push_back(event.id);
    }
    const auto removedRelationCount = static_cast<std::size_t>(std::count_if(
        scenario->relations.begin(),
        scenario->relations.end(),
        [&removedEventIds](const Relation& relation) {
            const auto removed = [&removedEventIds](const std::string& eventId) {
                return std::find(
                           removedEventIds.begin(),
                           removedEventIds.end(),
                           eventId)
                    != removedEventIds.end();
            };
            return removed(relation.sourceEventId)
                || removed(relation.targetEventId);
        }));
    auto removedTraceMappingCount = std::size_t{0};
    for (const auto& laneId : orderedIds) {
        const auto usedByAnotherScenario = std::any_of(
            project_.scenarios.begin(),
            project_.scenarios.end(),
            [scenario, &laneId](const Scenario& candidate) {
                return &candidate != scenario
                    && findLane(candidate, laneId);
            });
        if (usedByAnotherScenario) continue;
        for (const auto& trace : project_.importedTraces) {
            if (trace.signalMapping.contains(laneId)) {
                ++removedTraceMappingCount;
            }
        }
    }
    const auto quantity = [this](const std::size_t count, const QString& noun) {
        return tr("%1 %2%3")
            .arg(static_cast<qulonglong>(count))
            .arg(noun)
            .arg(count == 1 ? QString{} : QStringLiteral("s"));
    };
    QStringList removedEffects;
    if (!removedEventIds.empty()) {
        removedEffects.append(quantity(removedEventIds.size(), tr("event")));
    }
    if (removedRelationCount > 0) {
        removedEffects.append(quantity(removedRelationCount, tr("relation")));
    }
    if (removedTraceMappingCount > 0) {
        removedEffects.append(
            quantity(removedTraceMappingCount, tr("trace mapping")));
    }

    auto message = tr("Remove %1 selected signals: %2?")
        .arg(static_cast<qulonglong>(orderedIds.size()))
        .arg(names.join(QStringLiteral(", ")));
    if (!removedEffects.isEmpty()) {
        message += tr(" This also removes %1.")
                       .arg(removedEffects.join(QStringLiteral(", ")));
    }
    message += tr(" You can undo this with Ctrl+Z.");
    if (QMessageBox::question(
            this,
            tr("Remove selected signals"),
            message,
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }

    const auto beforeStateId = commandStack_.stateId();
    canvas_->beginCommandSelectionTransition(beforeStateId);
    try {
        commandStack_.execute(std::make_unique<RemoveLanesCommand>(
            project_,
            *scenario,
            orderedIds));
    } catch (const std::exception& exception) {
        canvas_->cancelCommandSelectionTransition();
        QMessageBox::warning(
            this,
            tr("Cannot remove selected signals"),
            QString::fromUtf8(exception.what()));
        return;
    }
    canvas_->refreshModel();
    updateSelection({}, canvas_->cursorTick());
    canvas_->finishCommandSelectionTransition(commandStack_.stateId());
    markEdited();

    QStringList results;
    for (const auto& effect : removedEffects) {
        results.append(tr("%1 removed").arg(effect));
    }
    auto result = tr("Removed %1 selected signals")
        .arg(static_cast<qulonglong>(orderedIds.size()));
    if (!results.isEmpty()) {
        result += tr(" · %1").arg(results.join(QStringLiteral(", ")));
    }
    result += tr(" · Ctrl+Z to undo");
    statusBar()->showMessage(result, 8'000);
}

void MainWindow::showLaneContextMenu(
    const QString& laneId,
    const QPoint& globalPosition)
{
    const auto* scenario = activeScenario();
    const auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) return;

    QMenu menu(this);
    menu.setObjectName(QStringLiteral("LaneHeaderContextMenu"));
    menu.setToolTipsVisible(true);
    if (lane->kind == LaneKind::Group) {
        const auto collapsed = canvas_->isGroupCollapsed(laneId);
        const auto collapsible = std::any_of(
            scenario->lanes.begin(),
            scenario->lanes.end(),
            [lane](const Lane& candidate) {
                return candidate.visible
                    && candidate.kind != LaneKind::Group
                    && candidate.groupId == lane->id;
            });
        auto* toggle = menu.addAction(
            collapsed ? tr("Expand group") : tr("Collapse group"));
        toggle->setObjectName(QStringLiteral("GroupCollapseExpandAction"));
        toggle->setEnabled(collapsible);
        toggle->setToolTip(
            !collapsible
                ? tr("This group has no visible member signals")
                : collapsed
                    ? tr("Show this group's visible member signals")
                    : tr("Temporarily hide this group's member signals from the canvas"));
        connect(toggle, &QAction::triggered, this, [this, laneId, collapsed] {
            canvas_->setGroupCollapsed(laneId, !collapsed);
        });
        menu.addSeparator();
        auto* properties = menu.addAction(tr("Group properties…"));
        properties->setObjectName(QStringLiteral("GroupPropertiesAction"));
        connect(properties, &QAction::triggered, this, [this, laneId] {
            editLaneById(laneId);
        });
        menu.addSeparator();
        auto* hide = menu.addAction(tr("Hide group"));
        hide->setObjectName(QStringLiteral("HideLaneContextAction"));
        hide->setToolTip(
            tr("Hide this group; Show hidden items restores it"));
        connect(hide, &QAction::triggered, this, [this, laneId] {
            hideLaneById(laneId);
        });
        menu.exec(globalPosition);
        return;
    }
    QStringList selectedLaneIds{laneId};
    if (canvas_) {
        const auto canvasSelection = canvas_->selectedLaneIds();
        if (canvasSelection.size() > 1 && canvasSelection.contains(laneId)) {
            selectedLaneIds.clear();
            for (const auto& candidate : scenario->lanes) {
                const auto candidateId = QString::fromStdString(candidate.id);
                if (candidate.kind != LaneKind::Group
                    && candidate.visible
                    && canvasSelection.contains(candidateId)
                    && canvas_->isLaneDisplayed(candidateId)) {
                    selectedLaneIds.push_back(candidateId);
                }
            }
        }
    }
    if (selectedLaneIds.isEmpty()) selectedLaneIds = {laneId};
    const auto batchSelection = selectedLaneIds.size() > 1;

    const auto* clipboardMime = QApplication::clipboard()->mimeData();
    if (canvas_
        && clipboardMime
        && clipboardMime->hasFormat(
            QByteArrayLiteral("application/x-wave-workbench-range+json"))) {
        const auto [pasteEnabled, pasteToolTip] =
            canvas_->selectedLanePasteAvailability();
        const auto targetLabel = batchSelection
            ? tr("%1 selected signals").arg(selectedLaneIds.size())
            : QString::fromStdString(lane->name);
        const auto pasteTime = QString::fromStdString(
            formatTick(canvas_->cursorTick(), project_.timeBase));
        auto* paste = menu.addAction(
            tr("Paste copied range into %1 at %2")
                .arg(targetLabel, pasteTime));
        paste->setObjectName(
            QStringLiteral("PasteRangeIntoLaneSelectionAction"));
        paste->setShortcut(QKeySequence::Paste);
        paste->setEnabled(pasteEnabled);
        paste->setToolTip(pasteToolTip);
        auto pasteStatusTip = pasteToolTip;
        pasteStatusTip.replace(QLatin1Char('\n'), QStringLiteral(" · "));
        paste->setStatusTip(pasteStatusTip);
        connect(
            paste,
            &QAction::triggered,
            this,
            [this] { canvas_->pasteAtCursor(); });
        menu.addSeparator();
    }

    const auto label = lane->kind == LaneKind::Clock
        ? tr("Clock frequency / period...")
        : lane->kind == LaneKind::Bus
            ? tr("Bus display parameters...")
            : lane->kind == LaneKind::Bit
                ? tr("Bit display parameters...")
                : tr("Signal display parameters...");
    auto* parameters = menu.addAction(label);
    parameters->setObjectName(QStringLiteral("QuickLaneParametersAction"));
    parameters->setEnabled(!batchSelection);
    if (batchSelection) {
        parameters->setToolTip(
            tr("Display parameters require one signal; click a name without Ctrl/Shift"));
    }
    connect(parameters, &QAction::triggered, this, [this, laneId] {
        editLaneKeyParameters(laneId);
    });
    menu.addSeparator();
    auto* groupMenu = menu.addMenu(
        batchSelection
            ? tr("Move selected signals to group")
            : tr("Move to group"));
    groupMenu->setObjectName(QStringLiteral("LaneGroupMenu"));
    groupMenu->setToolTipsVisible(true);
    auto* ungroup = groupMenu->addAction(tr("No group"));
    ungroup->setObjectName(QStringLiteral("RemoveLaneFromGroupAction"));
    ungroup->setCheckable(true);
    const auto allUngrouped = std::all_of(
        selectedLaneIds.begin(),
        selectedLaneIds.end(),
        [scenario](const QString& selectedId) {
            const auto* selected = findLane(*scenario, selectedId.toStdString());
            return selected && selected->groupId.empty();
        });
    const auto anyGrouped = std::any_of(
        selectedLaneIds.begin(),
        selectedLaneIds.end(),
        [scenario](const QString& selectedId) {
            const auto* selected = findLane(*scenario, selectedId.toStdString());
            return selected && !selected->groupId.empty();
        });
    ungroup->setChecked(allUngrouped);
    ungroup->setEnabled(anyGrouped);
    ungroup->setToolTip(
        allUngrouped
            ? batchSelection
                ? tr("All selected signals are already outside every group")
                : tr("This signal is already outside every group")
            : batchSelection
                ? tr("Keep signal order and remove group membership from all selected signals")
                : tr("Keep the signal at its current position and remove its group membership"));
    connect(ungroup, &QAction::triggered, this, [this, selectedLaneIds] {
        setLanesGroupById(selectedLaneIds, {});
    });

    auto visibleGroupCount = std::size_t{0};
    for (const auto& candidate : scenario->lanes) {
        if (candidate.kind != LaneKind::Group || !candidate.visible) continue;
        if (visibleGroupCount == 0) groupMenu->addSeparator();
        ++visibleGroupCount;
        auto* assign = groupMenu->addAction(
            QString::fromStdString(candidate.name));
        assign->setObjectName(QStringLiteral("AssignLaneGroupAction"));
        assign->setData(QString::fromStdString(candidate.id));
        assign->setCheckable(true);
        const auto allInCandidate = std::all_of(
            selectedLaneIds.begin(),
            selectedLaneIds.end(),
            [scenario, &candidate](const QString& selectedId) {
                const auto* selected =
                    findLane(*scenario, selectedId.toStdString());
                return selected && selected->groupId == candidate.id;
            });
        assign->setChecked(allInCandidate);
        assign->setToolTip(
            allInCandidate
                ? batchSelection
                    ? tr("All selected signals already belong to this group")
                    : tr("This signal already belongs to this group")
                : batchSelection
                    ? tr("Move all selected signals below this group's current members in one undoable step")
                    : tr("Move the signal below this group's current members in one undoable step"));
        connect(
            assign,
            &QAction::triggered,
            this,
            [this,
             selectedLaneIds,
             groupId = QString::fromStdString(candidate.id)] {
                setLanesGroupById(selectedLaneIds, groupId);
            });
    }
    if (visibleGroupCount == 0) {
        groupMenu->addSeparator();
        auto* unavailable = groupMenu->addAction(
            tr("No visible groups"));
        unavailable->setObjectName(QStringLiteral("NoLaneGroupsAction"));
        unavailable->setEnabled(false);
        unavailable->setToolTip(
            tr("Use Edit > Add group to create or restore a group"));
    }
    groupMenu->addSeparator();
    auto* createGroup = groupMenu->addAction(
        batchSelection
            ? tr("New group with selected signals…")
            : tr("New group with this signal…"));
    createGroup->setObjectName(
        QStringLiteral("CreateGroupWithLaneAction"));
    createGroup->setToolTip(
        batchSelection
            ? tr("Name a new Group and move all selected signals into it in one undoable step")
            : tr("Name a new Group and make this signal its first member in one undoable step"));
    connect(createGroup, &QAction::triggered, this, [this, selectedLaneIds] {
        createGroupWithLanes(selectedLaneIds);
    });
    menu.addSeparator();
    auto* duplicate = menu.addAction(
        batchSelection
            ? tr("Duplicate selected signals")
            : tr("Duplicate signal"));
    duplicate->setObjectName(QStringLiteral("DuplicateLaneContextAction"));
    duplicate->setToolTip(
        batchSelection
            ? tr("Copy all selected signals and waveforms as one block below the last source")
            : tr("Copy this signal and its waveform immediately below the source"));
    connect(duplicate, &QAction::triggered, this, [this, laneId, selectedLaneIds] {
        if (selectedLaneIds.size() > 1) {
            duplicateLanesById(selectedLaneIds);
        } else {
            duplicateLaneById(laneId);
        }
    });
    auto* hide = menu.addAction(
        batchSelection
            ? tr("Hide selected signals")
            : tr("Hide signal"));
    hide->setObjectName(QStringLiteral("HideLaneContextAction"));
    hide->setToolTip(
        batchSelection
            ? tr("Hide all selected signals as one undoable edit")
            : tr("Hide this signal; Show hidden items restores it"));
    connect(hide, &QAction::triggered, this, [this, laneId, selectedLaneIds] {
        if (selectedLaneIds.size() > 1) {
            hideLanesById(selectedLaneIds);
        } else {
            hideLaneById(laneId);
        }
    });
    auto* remove = menu.addAction(
        batchSelection
            ? tr("Remove selected signals…")
            : tr("Remove signal…"));
    remove->setObjectName(QStringLiteral("RemoveLaneContextAction"));
    remove->setToolTip(
        batchSelection
            ? tr("Preview dependencies, then remove all selected signals as one undoable edit")
            : tr("Preview dependencies, then remove this signal as one undoable edit"));
    connect(remove, &QAction::triggered, this, [this, laneId, selectedLaneIds] {
        if (selectedLaneIds.size() > 1) {
            removeLanesById(selectedLaneIds);
        } else {
            removeLaneById(laneId);
        }
    });
    menu.exec(globalPosition);
}

void MainWindow::setLaneGroupById(
    const QString& laneId,
    const QString& groupId)
{
    setLanesGroupById(QStringList{laneId}, groupId);
}

void MainWindow::setLanesGroupById(
    const QStringList& laneIds,
    const QString& groupId)
{
    if (!canvas_ || laneIds.isEmpty()) return;
    if (canvas_->hasExplicitRangeSelection()) {
        statusBar()->showMessage(
            tr("Esc clears the selected range before moving whole signals"),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;

    auto* scenario = activeScenario();
    if (!scenario) return;
    std::vector<std::string> orderedIds;
    QStringList orderedLaneIds;
    orderedIds.reserve(static_cast<std::size_t>(laneIds.size()));
    for (const auto& candidate : scenario->lanes) {
        const auto candidateId = QString::fromStdString(candidate.id);
        if (candidate.kind != LaneKind::Group
            && laneIds.contains(candidateId)
            && std::find(
                   orderedIds.begin(),
                   orderedIds.end(),
                   candidate.id)
                == orderedIds.end()) {
            orderedIds.push_back(candidate.id);
            orderedLaneIds.push_back(candidateId);
        }
    }
    if (orderedIds.empty()) return;

    auto activeLaneId = canvas_->selectedLaneId();
    if (!orderedLaneIds.contains(activeLaneId)) {
        activeLaneId = orderedLaneIds.front();
    }
    const auto* activeLane =
        findLane(*scenario, activeLaneId.toStdString());
    if (!activeLane) return;
    const auto laneName = QString::fromStdString(activeLane->name);
    const auto previousGroupId = activeLane->groupId;
    const auto* previousGroup = previousGroupId.empty()
        ? nullptr
        : findLane(*scenario, previousGroupId);
    const auto previousGroupName = previousGroup
        ? QString::fromStdString(previousGroup->name)
        : tr("its previous group");

    const auto targetId = groupId.toStdString();
    const auto* targetGroup = targetId.empty()
        ? nullptr
        : findLane(*scenario, targetId);
    if (!targetId.empty()
        && (!targetGroup
            || targetGroup->kind != LaneKind::Group
            || !targetGroup->visible)) {
        statusBar()->showMessage(
            tr("The target group is no longer visible"),
            4'000);
        return;
    }
    const auto targetGroupName = targetGroup
        ? QString::fromStdString(targetGroup->name)
        : QString{};
    const auto changedSignalCount = static_cast<std::size_t>(std::count_if(
        orderedIds.begin(),
        orderedIds.end(),
        [scenario, &targetId](const std::string& selectedId) {
            const auto* selected = findLane(*scenario, selectedId);
            return selected
                && (targetId.empty()
                        ? !selected->groupId.empty()
                        : selected->groupId != targetId);
        }));
    const auto existingTargetMembers = targetGroup
        ? static_cast<std::size_t>(std::count_if(
              scenario->lanes.begin(),
              scenario->lanes.end(),
              [&targetId, &orderedIds](const Lane& candidate) {
                  return std::find(
                             orderedIds.begin(),
                             orderedIds.end(),
                             candidate.id)
                          == orderedIds.end()
                      && candidate.groupId == targetId;
              }))
        : std::size_t{0};

    bool changed = false;
    try {
        changed = commandStack_.execute(std::make_unique<SetLanesGroupCommand>(
            *scenario,
            orderedIds,
            targetId));
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            orderedIds.size() == 1
                ? tr("Cannot move signal to group")
                : tr("Cannot move selected signals to group"),
            QString::fromUtf8(exception.what()));
        return;
    }
    if (!changed) {
        statusBar()->showMessage(
            orderedIds.size() > 1
                ? targetGroup
                    ? tr("All %1 selected signals are already in group %2")
                          .arg(static_cast<qulonglong>(orderedIds.size()))
                          .arg(targetGroupName)
                    : tr("All %1 selected signals are already outside every group")
                          .arg(static_cast<qulonglong>(orderedIds.size()))
                : targetGroup
                ? tr("%1 is already in group %2")
                      .arg(laneName, targetGroupName)
                : tr("%1 is already outside every group")
                      .arg(laneName),
            4'000);
        updateCommandActions();
        return;
    }

    canvas_->refreshModel();
    markEdited();
    if (orderedLaneIds.size() > 1) {
        canvas_->selectLaneHeaders(orderedLaneIds, activeLaneId);
    } else {
        canvas_->revealLocation(activeLaneId, canvas_->cursorTick());
    }
    selectLaneItem(signalTree_, activeLaneId);
    selectLaneItem(groupTree_, activeLaneId);
    if (orderedIds.size() > 1) {
        statusBar()->showMessage(
            targetGroup
                ? tr("Placed %1 selected signals in group %2 · %3 moved · %4 already there · Ctrl+Z to undo")
                      .arg(static_cast<qulonglong>(orderedIds.size()))
                      .arg(targetGroupName)
                      .arg(static_cast<qulonglong>(changedSignalCount))
                      .arg(static_cast<qulonglong>(
                          orderedIds.size() - changedSignalCount))
                : tr("Removed group membership from %1 of %2 selected signals · signal order unchanged · Ctrl+Z to undo")
                      .arg(static_cast<qulonglong>(changedSignalCount))
                      .arg(static_cast<qulonglong>(orderedIds.size())),
            8'000);
        return;
    }
    statusBar()->showMessage(
        targetGroup
            ? tr("Moved %1 to group %2 路 placed after %3 existing member signal(s) 路 Ctrl+Z to undo")
                  .arg(laneName, targetGroupName)
                  .arg(static_cast<qulonglong>(existingTargetMembers))
            : tr("Removed %1 from group %2 路 signal order unchanged 路 Ctrl+Z to undo")
                  .arg(laneName, previousGroupName),
        7'000);
}

void MainWindow::createGroupWithLane(const QString& laneId)
{
    createGroupWithLanes(QStringList{laneId});
}

void MainWindow::createGroupWithLanes(const QStringList& laneIds)
{
    if (!canvas_ || laneIds.isEmpty()) return;
    if (canvas_->hasExplicitRangeSelection()) {
        statusBar()->showMessage(
            tr("Esc clears the selected range before grouping whole signals"),
            5'000);
        return;
    }
    if (!commitPendingEdits()) return;

    auto* scenario = activeScenario();
    if (!scenario) return;
    std::vector<std::string> orderedIds;
    QStringList orderedLaneIds;
    orderedIds.reserve(static_cast<std::size_t>(laneIds.size()));
    for (const auto& candidate : scenario->lanes) {
        const auto candidateId = QString::fromStdString(candidate.id);
        if (candidate.kind != LaneKind::Group
            && laneIds.contains(candidateId)
            && std::find(
                   orderedIds.begin(),
                   orderedIds.end(),
                   candidate.id)
                == orderedIds.end()) {
            orderedIds.push_back(candidate.id);
            orderedLaneIds.push_back(candidateId);
        }
    }
    if (orderedIds.empty()) return;

    auto activeLaneId = canvas_->selectedLaneId();
    if (!orderedLaneIds.contains(activeLaneId)) {
        activeLaneId = orderedLaneIds.front();
    }
    const auto* lane = findLane(*scenario, activeLaneId.toStdString());
    if (!lane) return;
    const auto laneName = QString::fromStdString(lane->name);
    const auto previousGroup = lane->groupId.empty()
        ? nullptr
        : findLane(*scenario, lane->groupId);
    const auto previousGroupName = previousGroup
        ? QString::fromStdString(previousGroup->name)
        : QString{};
    std::set<std::string> previousGroupIds;
    for (const auto& selectedId : orderedIds) {
        const auto* selected = findLane(*scenario, selectedId);
        if (selected && !selected->groupId.empty()) {
            previousGroupIds.insert(selected->groupId);
        }
    }
    const auto defaultName = uniqueLaneName(
        *scenario,
        lane->name + "_group");
    auto description = orderedIds.size() == 1
        ? tr("Create a Group immediately above %1 and make this signal its first member.")
              .arg(laneName)
        : tr("Create a Group immediately above the first of %1 selected signals and move all of them into it.")
              .arg(static_cast<qulonglong>(orderedIds.size()));
    if (orderedIds.size() == 1 && previousGroup) {
        description.append(
            tr(" It will leave %1.").arg(previousGroupName));
    } else if (!previousGroupIds.empty()) {
        description.append(
            tr(" The selection will leave %1 existing group(s).")
                .arg(static_cast<qulonglong>(previousGroupIds.size())));
    }
    const auto name = promptNewGroupName(
        this,
        *scenario,
        orderedIds.size() == 1
            ? tr("Create group with %1").arg(laneName)
            : tr("Create group with %1 selected signals")
                  .arg(static_cast<qulonglong>(orderedIds.size())),
        description,
        defaultName);
    if (!name) {
        statusBar()->showMessage(
            orderedIds.size() == 1
                ? tr("Group creation cancelled · %1 unchanged").arg(laneName)
                : tr("Group creation cancelled · %1 selected signals unchanged")
                      .arg(static_cast<qulonglong>(orderedIds.size())),
            3'000);
        return;
    }

    Lane group;
    group.id = makeStableId("group");
    group.name = *name;
    group.kind = LaneKind::Group;
    group.color = randomReadableLaneColor(*scenario);
    group.height = 40;
    try {
        commandStack_.execute(
            std::make_unique<CreateGroupWithLanesCommand>(
                *scenario,
                group,
                orderedIds));
    } catch (const std::exception& exception) {
        QMessageBox::warning(
            this,
            tr("Cannot create group"),
            QString::fromUtf8(exception.what()));
        return;
    }

    canvas_->refreshModel();
    markEdited();
    if (orderedLaneIds.size() > 1) {
        canvas_->selectLaneHeaders(orderedLaneIds, activeLaneId);
    } else {
        canvas_->revealLocation(activeLaneId, canvas_->cursorTick());
    }
    selectLaneItem(signalTree_, activeLaneId);
    selectLaneItem(groupTree_, QString::fromStdString(group.id));
    if (orderedIds.size() > 1) {
        auto message = tr("Created group %1 with %2 selected signals")
                           .arg(QString::fromStdString(group.name))
                           .arg(static_cast<qulonglong>(orderedIds.size()));
        if (!previousGroupIds.empty()) {
            message.append(
                tr(" · moved from %1 existing group(s)")
                    .arg(static_cast<qulonglong>(previousGroupIds.size())));
        }
        message.append(
            tr(" · one Undo step · Ctrl+Z to undo · drag more signals onto the group"));
        statusBar()->showMessage(message, 9'000);
        return;
    }
    auto message = tr("Created group %1 with %2 as its first member")
                       .arg(
                           QString::fromStdString(group.name),
                           laneName);
    if (previousGroup) {
        message.append(
            tr(" · moved from %1").arg(previousGroupName));
    }
    message.append(
        tr(" · Ctrl+Z to undo · drag more signals onto the group"));
    statusBar()->showMessage(message, 8'000);
}

void MainWindow::editLaneKeyParameters(const QString& laneId)
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane || lane->kind == LaneKind::Group) return;

    const auto laneName = QString::fromStdString(lane->name);
    bool changed = false;
    QString resultSummary;

    if (lane->kind == LaneKind::Clock) {
        const auto* original = findClock(project_, lane->clockDomainId);
        if (!original) {
            QMessageBox::warning(
                this,
                tr("Cannot edit clock"),
                tr("The clock lane does not reference a valid clock domain."));
            return;
        }
        QDialog dialog(this);
        dialog.setObjectName(QStringLiteral("QuickClockParametersDialog"));
        dialog.setWindowTitle(tr("Clock parameters"));
        auto* layout = new QFormLayout(&dialog);
        auto* mode = new QComboBox;
        mode->setObjectName(QStringLiteral("ClockRateMode"));
        mode->addItem(tr("Period"), QStringLiteral("period"));
        mode->addItem(tr("Frequency (Hz)"), QStringLiteral("frequency"));
        auto* value = new QLineEdit(
            QString::fromStdString(formatTick(original->period, project_.timeBase)));
        value->setObjectName(QStringLiteral("ClockRateValue"));
        value->setPlaceholderText(tr("For example 2.5 ns"));
        value->setToolTip(
            tr("Period accepts decimal ps, ns, us, or ms values; frequency accepts Hz"));
        auto* phase = new QLineEdit(
            QString::fromStdString(formatTick(original->phase, project_.timeBase)));
        phase->setObjectName(QStringLiteral("ClockPhaseEdit"));
        phase->setPlaceholderText(tr("For example 0 ns"));
        auto* numerator = new QLineEdit(
            QString::number(original->dutyCycle.numerator));
        numerator->setObjectName(QStringLiteral("ClockDutyNumeratorEdit"));
        numerator->setValidator(new PositiveInt64Validator(1, numerator));
        auto* denominator = new QLineEdit(
            QString::number(original->dutyCycle.denominator));
        denominator->setObjectName(QStringLiteral("ClockDutyDenominatorEdit"));
        denominator->setValidator(new PositiveInt64Validator(2, denominator));
        auto* edge = new QComboBox;
        edge->setObjectName(QStringLiteral("ClockActiveEdgeCombo"));
        edge->addItem(tr("Rising"), static_cast<int>(ClockEdge::Rising));
        edge->addItem(tr("Falling"), static_cast<int>(ClockEdge::Falling));
        edge->setCurrentIndex(original->activeEdge == ClockEdge::Rising ? 0 : 1);
        auto* reset = new QLineEdit(QString::fromStdString(original->resetRelation));
        reset->setObjectName(QStringLiteral("ClockResetConditionEdit"));
        reset->setPlaceholderText(tr("Optional reset / disable condition"));
        layout->addRow(tr("Edit as"), mode);
        layout->addRow(tr("Value"), value);
        layout->addRow(tr("Phase"), phase);
        layout->addRow(tr("Duty numerator"), numerator);
        layout->addRow(tr("Duty denominator"), denominator);
        layout->addRow(tr("Active edge"), edge);
        layout->addRow(tr("Reset / disable condition"), reset);
        auto* error = new QLabel;
        error->setObjectName(QStringLiteral("QuickLaneParameterError"));
        error->setWordWrap(true);
        error->setStyleSheet(QStringLiteral("color: #ff9d9a;"));
        error->hide();
        layout->addRow(error);
        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addRow(buttons);
        const auto ticksPerSecond = 1.0e12
            / static_cast<double>(std::max<std::int64_t>(
                1, project_.timeBase.picosecondsPerTick));
        std::optional<Tick> period;
        std::optional<Tick> parsedPhase;
        std::optional<std::int64_t> parsedNumerator;
        std::optional<std::int64_t> parsedDenominator;
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(
            buttons,
            &QDialogButtonBox::accepted,
            &dialog,
            [this, mode, value, phase, numerator, denominator, error,
             ticksPerSecond, &dialog, &period, &parsedPhase,
             &parsedNumerator, &parsedDenominator] {
                QString parseError;
                std::optional<Tick> candidate;
                if (mode->currentData().toString() == QStringLiteral("frequency")) {
                    bool valid = false;
                    const auto frequency = value->text().trimmed().toDouble(&valid);
                    const auto computed = valid
                            && std::isfinite(frequency)
                            && frequency > 0.0
                        ? ticksPerSecond / frequency
                        : 0.0;
                    if (computed >= 1.0
                        && computed
                            <= static_cast<double>(std::numeric_limits<Tick>::max())) {
                        candidate = static_cast<Tick>(std::llround(computed));
                    }
                } else {
                    std::optional<std::int64_t> unusedCycle;
                    candidate = parseTimeText(
                        value->text(),
                        project_.timeBase,
                        nullptr,
                        unusedCycle,
                        parseError);
                }
                if (!candidate || *candidate <= 0) {
                    error->setText(
                        parseError.isEmpty()
                            ? tr("Enter a positive period or a frequency that maps to at least one tick.")
                            : parseError);
                    error->show();
                    value->setFocus(Qt::OtherFocusReason);
                    value->selectAll();
                    return;
                }
                std::optional<std::int64_t> unusedCycle;
                QString phaseError;
                const auto phaseCandidate = parseTimeText(
                    phase->text(),
                    project_.timeBase,
                    nullptr,
                    unusedCycle,
                    phaseError);
                if (!phaseCandidate) {
                    error->setText(
                        phaseError.isEmpty()
                            ? tr("Phase must be an exact integer tick.")
                            : phaseError);
                    error->show();
                    phase->setFocus(Qt::OtherFocusReason);
                    phase->selectAll();
                    return;
                }
                bool numeratorValid = false;
                const auto numeratorCandidate =
                    numerator->text().trimmed().toLongLong(&numeratorValid);
                if (!numeratorValid || numeratorCandidate < 1) {
                    error->setText(
                        tr("Duty numerator must be a positive 64-bit integer."));
                    error->show();
                    numerator->setFocus(Qt::OtherFocusReason);
                    numerator->selectAll();
                    return;
                }
                bool denominatorValid = false;
                const auto denominatorCandidate =
                    denominator->text().trimmed().toLongLong(&denominatorValid);
                if (!denominatorValid || denominatorCandidate < 2) {
                    error->setText(
                        tr("Duty denominator must be a 64-bit integer of at least 2."));
                    error->show();
                    denominator->setFocus(Qt::OtherFocusReason);
                    denominator->selectAll();
                    return;
                }
                if (numeratorCandidate >= denominatorCandidate) {
                    error->setText(
                        tr("Duty numerator must be smaller than its denominator."));
                    error->show();
                    numerator->setFocus(Qt::OtherFocusReason);
                    numerator->selectAll();
                    return;
                }
                period = candidate;
                parsedPhase = phaseCandidate;
                parsedNumerator = numeratorCandidate;
                parsedDenominator = denominatorCandidate;
                error->hide();
                dialog.accept();
            });
        connect(value, &QLineEdit::textEdited, error, &QWidget::hide);
        connect(phase, &QLineEdit::textEdited, error, &QWidget::hide);
        connect(numerator, &QLineEdit::textEdited, error, &QWidget::hide);
        connect(denominator, &QLineEdit::textEdited, error, &QWidget::hide);
        connect(
            mode,
            &QComboBox::currentIndexChanged,
            &dialog,
            [this, mode, value, error, original, ticksPerSecond](int) {
                error->hide();
                value->setText(
                    mode->currentData().toString() == QStringLiteral("frequency")
                        ? QString::number(
                              ticksPerSecond / static_cast<double>(original->period),
                              'g',
                              12)
                        : QString::fromStdString(
                              formatTick(original->period, project_.timeBase)));
                value->setPlaceholderText(
                    mode->currentData().toString() == QStringLiteral("frequency")
                        ? tr("For example 100000000")
                        : tr("For example 2.5 ns"));
            });
        if (dialog.exec() != QDialog::Accepted
            || !period
            || !parsedPhase
            || !parsedNumerator
            || !parsedDenominator) {
            return;
        }
        auto replacement = *original;
        replacement.period = *period;
        replacement.phase = *parsedPhase;
        replacement.dutyCycle = {*parsedNumerator, *parsedDenominator};
        replacement.dutyCycle.normalize();
        replacement.activeEdge = static_cast<ClockEdge>(edge->currentData().toInt());
        replacement.resetRelation = reset->text().trimmed().toStdString();
        const auto periodLabel = QString::fromStdString(
            formatTick(replacement.period, project_.timeBase));
        try {
            changed = commandStack_.execute(std::make_unique<ChangeClockCommand>(
                project_, *scenario, original->id, replacement));
        } catch (const std::exception& exception) {
            QMessageBox::warning(this, tr("Cannot change clock"), QString::fromUtf8(exception.what()));
            return;
        }
        resultSummary = tr("%1 · period %2 · phase %3 · duty %4/%5 · %6 edge")
                            .arg(laneName, periodLabel)
                            .arg(QString::fromStdString(
                                formatTick(replacement.phase, project_.timeBase)))
                            .arg(replacement.dutyCycle.numerator)
                            .arg(replacement.dutyCycle.denominator)
                            .arg(replacement.activeEdge == ClockEdge::Rising
                                     ? tr("rising")
                                     : tr("falling"));
    } else {
        QDialog dialog(this);
        dialog.setObjectName(QStringLiteral("QuickLaneParametersDialog"));
        dialog.setWindowTitle(
            lane->kind == LaneKind::Bus ? tr("Bus parameters") : tr("Bit parameters"));
        auto* layout = new QFormLayout(&dialog);
        auto* color = new QLineEdit(QString::fromStdString(lane->color));
        color->setObjectName(QStringLiteral("LaneColorEdit"));
        auto* height = new QSpinBox;
        height->setObjectName(QStringLiteral("LaneHeightSpin"));
        height->setRange(30, 240);
        height->setValue(lane->height);
        auto* clock = new QComboBox;
        clock->setObjectName(QStringLiteral("LaneClockDomainCombo"));
        clock->addItem(tr("None"), QString{});
        for (const auto& domain : project_.clockDomains) {
            clock->addItem(
                QString::fromStdString(domain.name),
                QString::fromStdString(domain.id));
        }
        const auto clockIndex = clock->findData(QString::fromStdString(lane->clockDomainId));
        clock->setCurrentIndex(std::max(0, clockIndex));
        layout->addRow(tr("Color"), color);
        layout->addRow(tr("Height"), height);
        layout->addRow(tr("Clock domain"), clock);

        QSpinBox* width = nullptr;
        QCheckBox* signedValue = nullptr;
        QComboBox* radix = nullptr;
        if (lane->kind == LaneKind::Bus) {
            width = new QSpinBox;
            width->setObjectName(QStringLiteral("BusWidthSpin"));
            width->setRange(1, 65'536);
            width->setValue(static_cast<int>(std::min<std::uint32_t>(lane->width, 65'536)));
            signedValue = new QCheckBox(tr("Signed values"));
            signedValue->setObjectName(QStringLiteral("BusSignedCheck"));
            signedValue->setChecked(lane->isSigned);
            radix = new QComboBox;
            radix->setObjectName(QStringLiteral("BusRadixCombo"));
            for (const auto value : {Radix::Binary, Radix::Octal, Radix::Decimal, Radix::Hexadecimal}) {
                const auto label = toString(value);
                radix->addItem(
                    QString::fromLatin1(label.data(), static_cast<qsizetype>(label.size())),
                    static_cast<int>(value));
            }
            radix->setCurrentIndex(std::max(0, radix->findData(static_cast<int>(lane->radix))));
            layout->addRow(tr("Width"), width);
            layout->addRow(signedValue);
            layout->addRow(tr("Radix"), radix);
        }
        auto* error = new QLabel;
        error->setObjectName(QStringLiteral("QuickLaneParameterError"));
        error->setWordWrap(true);
        error->setStyleSheet(QStringLiteral("color: #ff9d9a;"));
        error->hide();
        layout->addRow(error);
        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addRow(buttons);
        QColor parsedColor;
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(
            buttons,
            &QDialogButtonBox::accepted,
            &dialog,
            [color, error, &dialog, &parsedColor] {
                const QColor candidate(color->text().trimmed());
                if (!candidate.isValid()) {
                    error->setText(
                        tr("Enter a valid HTML color, such as #4fc3f7."));
                    error->show();
                    color->setFocus(Qt::OtherFocusReason);
                    color->selectAll();
                    return;
                }
                parsedColor = candidate;
                error->hide();
                dialog.accept();
            });
        connect(color, &QLineEdit::textEdited, error, &QWidget::hide);
        if (dialog.exec() != QDialog::Accepted || !parsedColor.isValid()) return;
        auto replacement = *lane;
        replacement.color = parsedColor.name(QColor::HexRgb).toStdString();
        replacement.height = height->value();
        replacement.clockDomainId = clock->currentData().toString().toStdString();
        if (lane->kind == LaneKind::Bus) {
            replacement.width = static_cast<std::uint32_t>(width->value());
            replacement.isSigned = signedValue->isChecked();
            replacement.radix = static_cast<Radix>(radix->currentData().toInt());
        }
        const auto* selectedClock = replacement.clockDomainId.empty()
            ? nullptr
            : findClock(project_, replacement.clockDomainId);
        const auto clockLabel = selectedClock
            ? QString::fromStdString(selectedClock->name)
            : tr("none");
        const auto displaySummary = tr("color %1 · height %2 · clock %3")
                                        .arg(QString::fromStdString(replacement.color))
                                        .arg(replacement.height)
                                        .arg(clockLabel);
        if (lane->kind == LaneKind::Bus) {
            const auto radixText = toString(replacement.radix);
            resultSummary = tr("%1 · width %2 · %3 · %4 · %5")
                                .arg(laneName)
                                .arg(static_cast<qulonglong>(replacement.width))
                                .arg(replacement.isSigned ? tr("signed") : tr("unsigned"))
                                .arg(QString::fromLatin1(
                                    radixText.data(),
                                    static_cast<qsizetype>(radixText.size())))
                                .arg(displaySummary);
        } else {
            resultSummary = tr("%1 · %2").arg(laneName, displaySummary);
        }
        try {
            changed = commandStack_.execute(std::make_unique<ChangeLaneCommand>(
                project_, *scenario, lane->id, replacement));
        } catch (const std::exception& exception) {
            QMessageBox::warning(this, tr("Cannot change lane"), QString::fromUtf8(exception.what()));
            return;
        }
    }

    if (changed) {
        canvas_->refreshModel();
        markEdited();
    }
    canvas_->revealLocation(laneId, canvas_->cursorTick());
    statusBar()->showMessage(
        changed
            ? resultSummary + tr(" · Ctrl+Z to undo")
            : resultSummary + tr(" · no properties changed"),
        5'000);
}

void MainWindow::moveSelectedLaneUp()
{
    moveSelectedLaneBy(-1);
}

void MainWindow::moveSelectedLaneDown()
{
    moveSelectedLaneBy(1);
}

std::optional<std::size_t> MainWindow::batchLaneStepInsertionSlot(
    const Scenario& scenario,
    const QStringList& laneIds,
    const int offset) const
{
    if (!canvas_
        || laneIds.size() < 2
        || (offset != -1 && offset != 1)
        || scenario.lanes.empty()) {
        return std::nullopt;
    }

    std::vector<bool> selected(scenario.lanes.size(), false);
    auto selectedCount = std::size_t{0};
    for (const auto& laneId : laneIds) {
        const auto id = laneId.toStdString();
        auto matchCount = std::size_t{0};
        auto matchIndex = std::size_t{0};
        for (auto index = std::size_t{0};
             index < scenario.lanes.size();
             ++index) {
            if (scenario.lanes[index].id != id) continue;
            ++matchCount;
            matchIndex = index;
        }
        if (matchCount != 1
            || selected[matchIndex]
            || !scenario.lanes[matchIndex].visible
            || scenario.lanes[matchIndex].kind == LaneKind::Group
            || !canvas_->isLaneDisplayed(laneId)) {
            return std::nullopt;
        }
        selected[matchIndex] = true;
        ++selectedCount;
    }
    if (selectedCount != static_cast<std::size_t>(laneIds.size())) {
        return std::nullopt;
    }

    const auto first = std::find(selected.begin(), selected.end(), true);
    const auto last = std::find(selected.rbegin(), selected.rend(), true);
    if (first == selected.end() || last == selected.rend()) {
        return std::nullopt;
    }
    const auto firstIndex = static_cast<std::size_t>(
        std::distance(selected.begin(), first));
    const auto lastIndex = scenario.lanes.size() - 1
        - static_cast<std::size_t>(
            std::distance(selected.rbegin(), last));

    if (offset < 0) {
        for (auto index = firstIndex; index > 0;) {
            --index;
            if (selected[index]) continue;
            if (canvas_->isLaneDisplayed(
                    QString::fromStdString(
                        scenario.lanes[index].id))) {
                return index;
            }
        }
        return std::nullopt;
    }

    for (auto index = lastIndex + 1;
         index < scenario.lanes.size();
         ++index) {
        if (selected[index]) continue;
        if (canvas_->isLaneDisplayed(
                QString::fromStdString(
                    scenario.lanes[index].id))) {
            return index + 1;
        }
    }
    return std::nullopt;
}

void MainWindow::moveSelectedLaneBy(const int offset)
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    if (scenario
        && canvas_
        && canvas_->hasLaneHeaderSelection()
        && canvas_->selectedLaneIds().size() > 1) {
        const auto selectedLaneIds = canvas_->selectedLaneIds();
        std::vector<std::string> orderedLaneIds;
        orderedLaneIds.reserve(
            static_cast<std::size_t>(
                selectedLaneIds.size()));
        for (const auto& lane : scenario->lanes) {
            if (!selectedLaneIds.contains(
                    QString::fromStdString(lane.id))) {
                continue;
            }
            if (!lane.visible
                || lane.kind == LaneKind::Group
                || !canvas_->isLaneDisplayed(
                    QString::fromStdString(lane.id))) {
                statusBar()->showMessage(
                    tr("The selected signals changed before they could be reordered"),
                    5'000);
                updateLaneOrderActions();
                return;
            }
            orderedLaneIds.push_back(lane.id);
        }
        if (orderedLaneIds.size()
            != static_cast<std::size_t>(
                selectedLaneIds.size())) {
            statusBar()->showMessage(
                tr("The selected signals changed before they could be reordered"),
                5'000);
            updateLaneOrderActions();
            return;
        }

        const auto insertionSlot = batchLaneStepInsertionSlot(
            *scenario,
            selectedLaneIds,
            offset);
        if (!insertionSlot) {
            statusBar()->showMessage(
                offset < 0
                    ? tr("The selected signals are already at the visible top boundary")
                    : tr("The selected signals are already at the visible bottom boundary"),
                4'000);
            updateLaneOrderActions();
            return;
        }

        const auto activeLaneId = canvas_->selectedLaneId();
        const auto beforeStateId = commandStack_.stateId();
        canvas_->beginCommandSelectionTransition(beforeStateId);
        try {
            if (!commandStack_.execute(
                    std::make_unique<MoveLanesCommand>(
                        *scenario,
                        orderedLaneIds,
                        *insertionSlot))) {
                canvas_->cancelCommandSelectionTransition();
                statusBar()->showMessage(
                    tr("The selected signals already occupy that visible position"),
                    4'000);
                updateLaneOrderActions();
                return;
            }
        } catch (const std::exception& exception) {
            canvas_->cancelCommandSelectionTransition();
            QMessageBox::warning(
                this,
                tr("Cannot reorder selected signals"),
                QString::fromUtf8(exception.what()));
            return;
        }

        canvas_->refreshModel();
        QStringList reorderedIds;
        reorderedIds.reserve(selectedLaneIds.size());
        for (const auto& lane : scenario->lanes) {
            if (std::find(
                    orderedLaneIds.begin(),
                    orderedLaneIds.end(),
                    lane.id)
                != orderedLaneIds.end()) {
                reorderedIds.push_back(
                    QString::fromStdString(lane.id));
            }
        }
        canvas_->selectLaneHeaders(
            reorderedIds,
            activeLaneId);
        canvas_->finishCommandSelectionTransition(
            commandStack_.stateId());
        markEdited();
        selectLaneItem(signalTree_, activeLaneId);
        selectLaneItem(groupTree_, activeLaneId);
        updateLaneOrderActions();
        statusBar()->showMessage(
            offset < 0
                ? tr("Moved %1 selected signals one visible row up · relative order and Group memberships kept · Ctrl+Z to undo")
                      .arg(selectedLaneIds.size())
                : tr("Moved %1 selected signals one visible row down · relative order and Group memberships kept · Ctrl+Z to undo")
                      .arg(selectedLaneIds.size()),
            6'000);
        return;
    }

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
    const auto laneName = QString::fromStdString(iterator->name);
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
    statusBar()->showMessage(
        tr("Moved %1: position %2 -> %3. Ctrl+Z to undo.")
            .arg(laneName)
            .arg(currentIndex + 1)
            .arg(destinationIndex + 1),
        5'000);
}

void MainWindow::updateLaneOrderActions()
{
    if (cutRangeAction_) {
        const auto toolTip =
            tr("Copy and clear the selected time range as one undo command");
        cutRangeAction_->setToolTip(toolTip);
        cutRangeAction_->setStatusTip(toolTip);
    }
    if (moveLaneUpAction_) {
        moveLaneUpAction_->setEnabled(false);
        moveLaneUpAction_->setText(
            tr("Move selected lane &up"));
        moveLaneUpAction_->setToolTip(
            tr("Move the selected lane or group one position earlier in display order"));
    }
    if (moveLaneDownAction_) {
        moveLaneDownAction_->setEnabled(false);
        moveLaneDownAction_->setText(
            tr("Move selected lane &down"));
        moveLaneDownAction_->setToolTip(
            tr("Move the selected lane or group one position later in display order"));
    }
    if (duplicateLaneAction_) {
        duplicateLaneAction_->setEnabled(false);
        duplicateLaneAction_->setText(
            tr("&Duplicate selected signal"));
        duplicateLaneAction_->setToolTip(
            tr("Copy the selected signal, properties, and waveform below the source"));
        duplicateLaneAction_->setStatusTip(
            duplicateLaneAction_->toolTip());
    }
    if (hideLaneAction_) {
        hideLaneAction_->setEnabled(false);
        hideLaneAction_->setText(tr("&Hide selected item"));
    }
    if (removeLaneAction_) {
        removeLaneAction_->setEnabled(false);
        removeLaneAction_->setText(tr("&Remove selected lane / group…"));
    }
    const auto* scenario = activeScenario();
    if (scenario
        && canvas_
        && canvas_->hasExplicitRangeSelection()) {
        if (cutRangeAction_) {
            const auto [clearEnabled, clearToolTip] =
                canvas_->selectedRangeClearAvailability();
            const auto toolTip = clearEnabled
                ? tr("Copy the selected range, then clear its source as one undo command\n%1")
                      .arg(clearToolTip)
                : tr("Copy the selected range; the source already uses implicit values, so Cut will not remove waveform values\n%1")
                      .arg(clearToolTip);
            cutRangeAction_->setToolTip(toolTip);
            auto statusTip = toolTip;
            statusTip.replace(QLatin1Char('\n'), QStringLiteral(" · "));
            cutRangeAction_->setStatusTip(statusTip);
        }
        if (duplicateLaneAction_) {
            const auto [repeatEnabled, repeatToolTip] =
                canvas_->selectedRangeRepeatAvailability();
            const auto relationRemovalCount =
                canvas_->selectedRangeRepeatRelationRemovalCount();
            duplicateLaneAction_->setEnabled(repeatEnabled);
            duplicateLaneAction_->setText(
                relationRemovalCount > 0
                    ? tr("&Duplicate selected range after · removes %1 relation(s)")
                          .arg(static_cast<qulonglong>(
                              relationRemovalCount))
                    : tr("&Duplicate selected range after"));
            duplicateLaneAction_->setToolTip(repeatToolTip);
            auto repeatStatusTip = repeatToolTip;
            repeatStatusTip.replace(
                QLatin1Char('\n'),
                QStringLiteral(" · "));
            duplicateLaneAction_->setStatusTip(repeatStatusTip);
        }
        if (hideLaneAction_) {
            const auto laneId = selectedLaneIdForEditing();
            const auto* lane = findLane(*scenario, laneId.toStdString());
            hideLaneAction_->setEnabled(lane && lane->visible);
            hideLaneAction_->setText(
                lane && lane->kind == LaneKind::Group
                    ? tr("&Hide selected group")
                    : tr("&Hide selected signal"));
            hideLaneAction_->setToolTip(
                tr("Press Esc to clear the selected waveform range before hiding the whole item"));
        }
        return;
    }
    if (scenario
        && canvas_
        && canvas_->hasLaneHeaderSelection()
        && canvas_->selectedLaneIds().size() > 1) {
        const auto selectedLaneIds = canvas_->selectedLaneIds();
        const auto allVisibleSignals = std::all_of(
            selectedLaneIds.begin(),
            selectedLaneIds.end(),
            [scenario](const QString& selectedId) {
                const auto* lane =
                    findLane(*scenario, selectedId.toStdString());
                return lane
                    && lane->visible
                    && lane->kind != LaneKind::Group;
            });
        if (!allVisibleSignals) return;
        if (moveLaneUpAction_) {
            moveLaneUpAction_->setText(
                tr("Move %1 selected signals &up")
                    .arg(selectedLaneIds.size()));
            moveLaneUpAction_->setToolTip(
                tr("Move all selected signals together one visible row earlier; keep their relative order and Group memberships"));
            moveLaneUpAction_->setEnabled(
                batchLaneStepInsertionSlot(
                    *scenario,
                    selectedLaneIds,
                    -1)
                    .has_value());
        }
        if (moveLaneDownAction_) {
            moveLaneDownAction_->setText(
                tr("Move %1 selected signals &down")
                    .arg(selectedLaneIds.size()));
            moveLaneDownAction_->setToolTip(
                tr("Move all selected signals together one visible row later; keep their relative order and Group memberships"));
            moveLaneDownAction_->setEnabled(
                batchLaneStepInsertionSlot(
                    *scenario,
                    selectedLaneIds,
                    1)
                    .has_value());
        }
        if (duplicateLaneAction_) {
            duplicateLaneAction_->setEnabled(true);
            duplicateLaneAction_->setText(
                tr("&Duplicate %1 selected signals")
                    .arg(selectedLaneIds.size()));
            duplicateLaneAction_->setToolTip(
                tr("Copy the selected signals and waveforms as one block below the last source; one Ctrl+Z removes the block"));
        }
        if (hideLaneAction_) {
            hideLaneAction_->setEnabled(true);
            hideLaneAction_->setText(
                tr("&Hide %1 selected signals").arg(
                    selectedLaneIds.size()));
        }
        if (removeLaneAction_) {
            removeLaneAction_->setEnabled(true);
            removeLaneAction_->setText(
                tr("&Remove %1 selected signals…").arg(
                    selectedLaneIds.size()));
        }
        return;
    }
    const auto laneId = selectedLaneIdForEditing().toStdString();
    if (!scenario || laneId.empty()) return;
    const auto iterator = std::find_if(
        scenario->lanes.begin(),
        scenario->lanes.end(),
        [&laneId](const Lane& lane) {
            return lane.id == laneId;
        });
    if (iterator == scenario->lanes.end()) return;
    if (duplicateLaneAction_) {
        duplicateLaneAction_->setEnabled(
            iterator->visible && iterator->kind != LaneKind::Group);
        duplicateLaneAction_->setText(
            tr("&Duplicate selected signal"));
        duplicateLaneAction_->setToolTip(
            tr("Copy the selected signal, properties, and waveform below the source"));
    }
    if (hideLaneAction_) {
        hideLaneAction_->setEnabled(iterator->visible);
        hideLaneAction_->setText(
            iterator->kind == LaneKind::Group
                ? tr("&Hide selected group")
                : tr("&Hide selected signal"));
    }
    if (removeLaneAction_) {
        removeLaneAction_->setEnabled(true);
        removeLaneAction_->setText(
            iterator->kind == LaneKind::Group
                ? tr("&Remove selected group…")
                : tr("&Remove selected signal…"));
    }
    const auto index = std::distance(scenario->lanes.begin(), iterator);
    if (moveLaneUpAction_) moveLaneUpAction_->setEnabled(index > 0);
    if (moveLaneDownAction_) {
        moveLaneDownAction_->setEnabled(std::next(iterator) != scenario->lanes.end());
    }
}

void MainWindow::editLaneById(const QString& laneId)
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    auto* lane = scenario ? findLane(*scenario, laneId.toStdString()) : nullptr;
    if (!lane) return;
    const auto laneWasVisible = lane->visible;
    const auto replacement = promptLaneProperties(
        this,
        project_,
        *scenario,
        *lane,
        false);
    if (!replacement) return;
    const auto replacementName = QString::fromStdString(replacement->name);
    bool changed = false;
    try {
        changed = commandStack_.execute(std::make_unique<ChangeLaneCommand>(
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
    if (!changed) {
        statusBar()->showMessage(
            tr("No properties changed for %1").arg(replacementName),
            3'000);
        canvas_->revealLocation(laneId, canvas_->cursorTick());
        selectLaneItem(signalTree_, laneId);
        selectLaneItem(groupTree_, laneId);
        return;
    }
    canvas_->refreshModel();
    markEdited();
    canvas_->revealLocation(laneId, canvas_->cursorTick());
    selectLaneItem(signalTree_, laneId);
    selectLaneItem(groupTree_, laneId);
    if (laneWasVisible && !replacement->visible) {
        statusBar()->showMessage(
            tr("Hidden %1 · use Show hidden items at the bottom or in Edit · Ctrl+Z to undo")
                .arg(replacementName),
            8'000);
    } else {
        statusBar()->showMessage(
            tr("Changed properties for %1 · Ctrl+Z to undo").arg(replacementName),
            5'000);
    }
}

void MainWindow::editSelectedClock()
{
    const auto* item = clockTree_ ? clockTree_->currentItem() : nullptr;
    if (!item) return;
    const auto clockId = item->data(0, Qt::UserRole).toString().toStdString();
    editClockById(clockId);
}

void MainWindow::editClockById(const std::string& clockId)
{
    if (!commitPendingEdits()) return;
    auto* scenario = activeScenario();
    if (!scenario) return;
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
    auto* numerator = new QLineEdit(
        QString::number(original->dutyCycle.numerator));
    numerator->setObjectName(QStringLiteral("ClockDomainDutyNumeratorEdit"));
    numerator->setValidator(new PositiveInt64Validator(1, numerator));
    auto* denominator = new QLineEdit(
        QString::number(original->dutyCycle.denominator));
    denominator->setObjectName(QStringLiteral("ClockDomainDutyDenominatorEdit"));
    denominator->setValidator(new PositiveInt64Validator(2, denominator));
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
    bool numeratorValid = false;
    const auto parsedNumerator =
        numerator->text().trimmed().toLongLong(&numeratorValid);
    bool denominatorValid = false;
    const auto parsedDenominator =
        denominator->text().trimmed().toLongLong(&denominatorValid);
    if (!numeratorValid
        || parsedNumerator < 1
        || !denominatorValid
        || parsedDenominator < 2
        || parsedNumerator >= parsedDenominator) {
        QMessageBox::warning(
            this,
            tr("Invalid clock"),
            tr("Duty numerator and denominator must be positive 64-bit integers, with the numerator smaller than the denominator."));
        return;
    }

    auto replacement = *original;
    replacement.name = name->text().trimmed().toStdString();
    replacement.period = *parsedPeriod;
    replacement.phase = *parsedPhase;
    replacement.dutyCycle = {parsedNumerator, parsedDenominator};
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
    updateSimulationClockButton();
}

void MainWindow::addEvent()
{
    if (!commitPendingEdits()) return;
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
    if (!commitPendingEdits()) return;
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
    if (!eventTable_) return;
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
    if (!validationTable_ || row < 0 || !validationTable_->item(row, 0)) return;
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
    if (!commitPendingEdits()) return;
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
    if (!commitPendingEdits()) return;
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
    const auto fstAvailable = QFileInfo(wellenReaderExecutable_).isFile();
    const auto filter = fstAvailable
        ? tr("Supported traces (*.vcd *.fst *.csv);;Value Change Dump (*.vcd);;Fast Signal Trace (*.fst);;Timestamped CSV (*.csv)")
        : tr("Supported traces (*.vcd *.csv);;Value Change Dump (*.vcd);;Timestamped CSV (*.csv)");
    const auto path = QFileDialog::getOpenFileName(
        this,
        tr("Import simulation trace"),
        initialDirectory,
        filter);
    if (path.isEmpty()) return;
    const auto suffix = QFileInfo(path).suffix().toLower();
    const auto format = suffix == QStringLiteral("vcd")
        ? TraceFormat::Vcd
        : suffix == QStringLiteral("csv")
            ? TraceFormat::Csv
            : suffix == QStringLiteral("fst")
                ? TraceFormat::Fst
                : TraceFormat::Vcd;
    if (suffix != QStringLiteral("vcd") && suffix != QStringLiteral("csv")
        && suffix != QStringLiteral("fst")) {
        QMessageBox::warning(
            this,
            tr("Unsupported trace"),
            tr("Only VCD, FST, and timestamped CSV files are supported."));
        return;
    }
    startTraceImport(path, format, makeStableId("trace"), 0, true);
}

void MainWindow::cancelTraceImport()
{
    const auto metadataRunning = traceWatcher_ && traceWatcher_->isRunning();
    const auto signalsRunning = fstSignalLoadWatcher_
        && fstSignalLoadWatcher_->isRunning();
    if (!metadataRunning && !signalsRunning) return;
    if (metadataRunning && traceCancelFlag_) traceCancelFlag_->store(true);
    if (signalsRunning) cancelActiveFstSignalLoad(true);
    cancelTraceAction_->setEnabled(false);
    traceSummary_->setText(tr("Cancelling trace work…"));
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
    if (importTraceAction_) importTraceAction_->setEnabled(true);
    if (cancelTraceAction_) cancelTraceAction_->setEnabled(false);
    if (traceProgress_) traceProgress_->setVisible(false);
    const auto reloadAfter = std::exchange(reloadTraceAfterCurrent_, false);
    const auto reloadIfNeeded = [this, reloadAfter] {
        if (reloadAfter) loadFirstTraceReference();
    };
    const auto reportFailure = [this, &reloadIfNeeded](const QString& message) {
        if (traceSummary_) traceSummary_->setText(message);
        statusBar()->showMessage(message, 10'000);
        if (simulationResultMode_) {
            simulationStateMachine_.markFailed();
            updateSimulationControls(message);
            emit initialTraceReferenceLoaded(false, message);
        }
        reloadIfNeeded();
    };
    const auto parsed = traceWatcher_->result();
    if (!parsed) {
        reportFailure(tr("Trace import returned no result"));
        return;
    }
    if (parsed->cancelled) {
        reportFailure(tr("Trace import cancelled"));
        return;
    }
    if (!parsed->ok()) {
        const auto message = QString::fromStdString(parsed->errorSummary());
        if (!simulationResultMode_) {
            QMessageBox::critical(this, tr("Trace import failed"), message);
        }
        reportFailure(tr("Trace import failed: %1").arg(message));
        return;
    }
    if (parsed->index->identity.projectId != project_.id
        || parsed->index->identity.traceId != pendingTraceId_
        || parsed->index->identity.generation != traceGeneration_) {
        reportFailure(tr("Discarded an obsolete trace import result"));
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
        markEdited();
    } else if (!activeTraceReference()) {
        reportFailure(tr("Imported trace reference no longer exists"));
        return;
    }

    const auto preferredSignals = traceVisibilityCustomized_
        ? std::optional{traceVisibleSignalIds_}
        : std::nullopt;
    traceIndex_ = std::move(*parsed->index);
    activeTraceId_ = pendingTraceId_;
    initializeTraceVisibility(preferredSignals);
    refreshTraceViews();
    fstSignalLoadPaused_ = false;
    if (traceIndex_->format == TraceFormat::Fst) {
        startPendingFstSignalLoad();
    }
    if (pendingRevealTick_) {
        if (traceCanvas_) traceCanvas_->revealTick(*pendingRevealTick_);
        compareTraceCanvas_->revealTick(*pendingRevealTick_);
    }
    QStringList warnings;
    for (const auto& diagnostic : parsed->diagnostics) {
        if (diagnostic.severity == TraceDiagnosticSeverity::Warning) {
            warnings.append(QString::fromStdString(diagnostic.message));
        }
    }
    const auto successMessage = warnings.isEmpty()
        ? tr("Imported %1").arg(pendingTracePath_)
        : tr("Imported with warnings: %1")
              .arg(warnings.join(QStringLiteral("; ")));
    if (simulationResultMode_) {
        compareTraceCanvas_->show();
        compareTraceCanvas_->fitTrace();
        statusBar()->showMessage(successMessage, 10'000);
        simulationStateMachine_.markCurrent();
        updateSimulationControls(successMessage);
        emit initialTraceReferenceLoaded(true, successMessage);
        reloadIfNeeded();
        return;
    }
    invalidateCompareResult();
    populateTraceMappingTable();
    bottomTabs_->setCurrentWidget(tracePanel_);
    if (traceIndex_->format == TraceFormat::Fst) {
        updateFstLoadStatus();
    } else {
        traceSummary_->setText(
            tr("%1 signals, %2 transitions")
                .arg(traceIndex_->traceSignals.size())
                .arg(traceIndex_->transitionCount));
    }

    statusBar()->showMessage(successMessage, 10'000);
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
    cancelActiveFstSignalLoad(false);
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
    const auto pathText = pendingTracePath_;
    const auto readerExecutable = wellenReaderExecutable_;
    if (importTraceAction_) importTraceAction_->setEnabled(false);
    if (cancelTraceAction_) cancelTraceAction_->setEnabled(true);
    if (traceProgress_) traceProgress_->setVisible(true);
    if (traceSummary_) {
        traceSummary_->setText(tr("Parsing %1…").arg(QFileInfo(path).fileName()));
    }
    statusBar()->showMessage(tr("Parsing %1…").arg(QFileInfo(path).fileName()));
    const auto future = QtConcurrent::run(
        [pathValue, pathText, readerExecutable, format, options]() mutable {
            auto result = format == TraceFormat::Vcd
                ? parseVcdFile(pathValue, options)
                : format == TraceFormat::Csv
                    ? parseCsvFile(pathValue, options)
                    : readFstMetadataFile(
                          pathText, options, readerExecutable);
            return std::make_shared<TraceParseResult>(std::move(result));
        });
    traceWatcher_->setFuture(future);
}

void MainWindow::loadFirstTraceReference()
{
    const auto reportFailure = [this](const QString& message) {
        if (traceSummary_) traceSummary_->setText(message);
        statusBar()->showMessage(message, 10'000);
        if (simulationResultMode_) {
            simulationStateMachine_.markFailed();
            updateSimulationControls(message);
            emit initialTraceReferenceLoaded(false, message);
        }
    };
    traceIndex_.reset();
    activeTraceId_.clear();
    if (!traceVisibilityCustomized_) traceVisibleSignalIds_.clear();
    if (!simulationResultMode_) populateTraceMappingTable();
    refreshTraceViews();
    if (!simulationResultMode_) invalidateCompareResult();
    if (project_.importedTraces.empty()) {
        reportFailure(tr("No imported trace"));
        return;
    }
    const auto& reference = project_.importedTraces.front();
    if (reference.id.empty()) {
        reportFailure(tr("Imported trace has no stable ID"));
        return;
    }
    const auto idMatchCount =
        std::count_if(
            project_.importedTraces.begin(),
            project_.importedTraces.end(),
            [&reference](
                const ImportedTrace& candidate) {
                return candidate.id == reference.id;
            });
    if (idMatchCount != 1) {
        reportFailure(
            tr("Duplicate imported trace ID: %1")
                .arg(QString::fromStdString(
                    reference.id)));
        return;
    }
    const auto storedPath =
        QString::fromStdString(reference.path);
    if (storedPath.trimmed().isEmpty()) {
        reportFailure(tr("Imported trace path is empty"));
        return;
    }
    const auto formatText =
        QString::fromStdString(reference.format)
            .trimmed()
            .toLower();
    if (formatText != QStringLiteral("vcd") && formatText != QStringLiteral("csv")
        && formatText != QStringLiteral("fst")) {
        reportFailure(
            tr("Unsupported trace format '%1'; expected VCD, FST, or CSV")
                .arg(
                    QString::fromStdString(
                        reference.format)));
        return;
    }
    const auto path = resolvedTracePath(reference);
    if (!QFileInfo(path).isFile()) {
        reportFailure(
            tr("Trace file is missing or not a file: %1")
                .arg(path));
        return;
    }
    activeTraceId_ = reference.id;
    startTraceImport(
        path,
        formatText == QStringLiteral("vcd")
            ? TraceFormat::Vcd
            : formatText == QStringLiteral("fst")
                ? TraceFormat::Fst
                : TraceFormat::Csv,
        reference.id,
        reference.offset,
        false);
}

void MainWindow::runCompare()
{
    const auto reportUnavailable = [this](const QString& message) {
        if (!simulationResultMode_) {
            QMessageBox::warning(this, tr("Compare unavailable"), message);
            return;
        }
        compareResult_.reset();
        if (compareTable_) compareTable_->setRowCount(0);
        if (compareSummary_) {
            compareSummary_->setText(message);
            compareSummary_->setToolTip(message);
            compareSummary_->setStyleSheet(QStringLiteral("color:#815400;font-weight:600"));
        }
        setProperty("wavewidgets.comparisonStatus", QStringLiteral("unavailable"));
        setProperty("wavewidgets.compareDifferenceCount", 0);
        canvas_->setDifferenceRanges({});
        if (compareTraceCanvas_) compareTraceCanvas_->setDifferenceRanges({});
    };

    if (simulationResultMode_
        && simulationStateMachine_.state() != SimulationSessionState::Current) {
        reportUnavailable(
            tr("Run the current scenario before comparing expected and actual waveforms"));
        return;
    }
    const auto* scenario = activeScenario();
    const auto* reference = activeTraceReference();
    if (!scenario || !reference || !traceIndex_) {
        reportUnavailable(
            tr("Import and map an actual VCD, FST, or CSV trace before comparing."));
        return;
    }
    if (traceIndex_->format == TraceFormat::Fst
        && !requiredFstSignalsLoaded()) {
        fstComparePending_ = true;
        fstSignalLoadPaused_ = false;
        startPendingFstSignalLoad();
        statusBar()->showMessage(
            tr("Compare will run after the required FST signals finish loading"),
            10'000);
        return;
    }
    fstComparePending_ = false;
    QString parseError;
    std::optional<std::int64_t> cycle;
    const auto tolerance = parseTimeText(
        compareToleranceEdit_
            ? compareToleranceEdit_->text() : QStringLiteral("0 tick"),
        project_.timeBase,
        nullptr,
        cycle,
        parseError);
    if (!tolerance || *tolerance < 0) {
        const auto message = parseError.isEmpty()
            ? tr("Edge tolerance must be a non-negative exactly representable time.")
            : parseError;
        if (simulationResultMode_) {
            reportUnavailable(message);
        } else {
            QMessageBox::warning(this, tr("Invalid edge tolerance"), message);
        }
        return;
    }

    CompareOptions options;
    if (compareXCombo_) {
        options.defaultRule.xHandling = static_cast<XHandling>(
            compareXCombo_->currentData().toInt());
    }
    options.defaultRule.edgeTolerance = *tolerance;
    if (compareMaskEdit_) {
        options.defaultRule.busMask =
            compareMaskEdit_->text().trimmed().toStdString();
    }
    options.relationOnly = compareRelationOnly_
        && compareRelationOnly_->isChecked();
    if (compareSelectionOnly_ && compareSelectionOnly_->isChecked()) {
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
    if (simulationResultMode_) {
        const auto exported = exportZeroSlackStimulusScenario(
            project_, *scenario, activeSimulationViewState());
        if (!exported.ok()) {
            reportUnavailable(exported.error);
            return;
        }
        for (const auto& port : exported.scenario->ports) {
            if (port.role == StimulusPortRole::Watch
                && !port.expectedSegments.empty()) {
                options.includedLaneIds.insert(port.laneId);
            }
        }
        if (options.includedLaneIds.empty()) {
            reportUnavailable(
                tr("No expected output ranges are defined in the current scenario"));
            return;
        }
    }
    compareResult_ = compareScenario(
        project_,
        *scenario,
        *traceIndex_,
        *reference,
        options);
    populateCompareTable();
    std::vector<std::pair<Tick, Tick>> ranges;
    std::vector<WaveCanvas::DifferenceRange> expectedRanges;
    ranges.reserve(compareResult_->differences.size());
    expectedRanges.reserve(compareResult_->differences.size());
    for (const auto& difference : compareResult_->differences) {
        ranges.emplace_back(difference.start, difference.end);
        if (!difference.laneId.empty()) {
            expectedRanges.push_back({
                difference.laneId, difference.start, difference.end});
        }
    }
    canvas_->setDifferenceRanges(std::move(expectedRanges));
    if (traceCanvas_) traceCanvas_->setDifferenceRanges(ranges);
    if (compareTraceCanvas_) {
        compareTraceCanvas_->setDifferenceRanges(std::move(ranges));
        compareTraceCanvas_->setVisible(true);
    }
    if (compareModeAction_) compareModeAction_->setChecked(true);
    if (bottomTabs_ && comparePanel_) bottomTabs_->setCurrentWidget(comparePanel_);
    setProperty(
        "wavewidgets.comparisonStatus",
        compareResult_->matches()
            ? QStringLiteral("match") : QStringLiteral("mismatch"));
    setProperty(
        "wavewidgets.compareDifferenceCount",
        static_cast<qulonglong>(compareResult_->differences.size()));
}

void MainWindow::addValueSimulationCheck()
{
    addSimulationCheck(SimulationCheckKind::ValueAtTick);
}

void MainWindow::addStableSimulationCheck()
{
    addSimulationCheck(SimulationCheckKind::StableRange);
}

void MainWindow::addEdgeResponseSimulationCheck()
{
    addSimulationCheck(SimulationCheckKind::EdgeResponse);
}

void MainWindow::addSimulationCheck(const SimulationCheckKind kind)
{
    auto* scenario = activeScenario();
    if (!scenario) return;
    const auto loaded = loadSimulationChecks(*scenario);
    if (!loaded.ok()) {
        QMessageBox::warning(this, tr("Checks unavailable"), loaded.error);
        return;
    }
    const auto check = promptSimulationCheck(kind);
    if (!check) return;
    auto checks = loaded.checks;
    checks.push_back(*check);
    QString error;
    if (!storeSimulationChecks(*scenario, checks, &error)) {
        QMessageBox::warning(this, tr("Invalid check"), error);
        return;
    }
    markEdited();
    populateSimulationCheckTable();
    if (simulationReviewTabs_ && simulationCheckPanel_) {
        simulationReviewTabs_->setCurrentWidget(simulationCheckPanel_);
    }
}

std::optional<SimulationCheckDefinition> MainWindow::promptSimulationCheck(
    const SimulationCheckKind initialKind,
    const SimulationCheckDefinition* existing)
{
    const auto* scenario = activeScenario();
    if (!scenario) return std::nullopt;

    QDialog dialog(this);
    dialog.setWindowTitle(existing ? tr("Edit simulation check")
                                   : tr("Add simulation check"));
    dialog.setMinimumWidth(520);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* name = new QLineEdit(&dialog);
    auto* enabled = new QCheckBox(tr("Enabled"), &dialog);
    auto* kind = new QComboBox(&dialog);
    kind->addItem(
        tr("Value at time"), static_cast<int>(SimulationCheckKind::ValueAtTick));
    kind->addItem(
        tr("Stable range"), static_cast<int>(SimulationCheckKind::StableRange));
    kind->addItem(
        tr("Edge response"), static_cast<int>(SimulationCheckKind::EdgeResponse));
    auto* lane = new QComboBox(&dialog);
    auto* sourceLane = new QComboBox(&dialog);
    auto* targetLane = new QComboBox(&dialog);
    const auto addLanes = [scenario](QComboBox* combo) {
        for (const auto& item : scenario->lanes) {
            if (item.kind == LaneKind::Group) continue;
            combo->addItem(
                QString::fromStdString(item.name),
                QString::fromStdString(item.id));
        }
    };
    addLanes(lane);
    addLanes(sourceLane);
    addLanes(targetLane);
    if (lane->count() == 0) {
        QMessageBox::warning(
            this, tr("Checks unavailable"), tr("The scenario has no signal lanes."));
        return std::nullopt;
    }

    auto* tick = new QLineEdit(&dialog);
    auto* expected = new QLineEdit(&dialog);
    auto* start = new QLineEdit(&dialog);
    auto* end = new QLineEdit(&dialog);
    auto* sourceEdge = new QComboBox(&dialog);
    auto* targetEdge = new QComboBox(&dialog);
    auto* minimumDelay = new QLineEdit(&dialog);
    auto* maximumDelay = new QLineEdit(&dialog);
    tick->setPlaceholderText(tr("tick or time, for example 120 ns"));
    start->setPlaceholderText(tr("inclusive start"));
    end->setPlaceholderText(tr("exclusive end"));
    minimumDelay->setPlaceholderText(tr("minimum response delay"));
    maximumDelay->setPlaceholderText(tr("maximum response delay"));

    form->addRow(tr("Name"), name);
    form->addRow(QString(), enabled);
    form->addRow(tr("Kind"), kind);
    form->addRow(tr("Signal"), lane);
    form->addRow(tr("Time"), tick);
    form->addRow(tr("Expected value"), expected);
    form->addRow(tr("Range start"), start);
    form->addRow(tr("Range end"), end);
    form->addRow(tr("Source signal"), sourceLane);
    form->addRow(tr("Source edge"), sourceEdge);
    form->addRow(tr("Target signal"), targetLane);
    form->addRow(tr("Target edge"), targetEdge);
    form->addRow(tr("Minimum delay"), minimumDelay);
    form->addRow(tr("Maximum delay"), maximumDelay);
    layout->addLayout(form);

    auto* semantics = new QLabel(
        tr("Ranges use [start, end). Rising and falling edges require a one-bit signal. "
           "Each target edge satisfies at most one source edge."),
        &dialog);
    semantics->setWordWrap(true);
    semantics->setStyleSheet(QStringLiteral("color:#596579"));
    layout->addWidget(semantics);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    const auto setLane = [](QComboBox* combo, const std::string& laneId) {
        const auto index = combo->findData(QString::fromStdString(laneId));
        if (index >= 0) combo->setCurrentIndex(index);
    };
    const auto selectedLane = canvas_ ? canvas_->selectedLaneId() : QString{};
    for (auto* combo : {lane, sourceLane}) {
        const auto index = combo->findData(selectedLane);
        if (index >= 0) combo->setCurrentIndex(index);
    }
    if (targetLane->count() > 1) targetLane->setCurrentIndex(1);

    const auto configureEdgeCombo = [scenario](
                                         QComboBox* laneCombo,
                                         QComboBox* edgeCombo,
                                         const std::optional<SimulationCheckEdge> preferred) {
        const auto selectedId = laneCombo->currentData().toString().toStdString();
        const auto* selected = findLane(*scenario, selectedId);
        edgeCombo->clear();
        if (selected && selected->width == 1) {
            edgeCombo->addItem(
                QObject::tr("Rising"), static_cast<int>(SimulationCheckEdge::Rising));
            edgeCombo->addItem(
                QObject::tr("Falling"), static_cast<int>(SimulationCheckEdge::Falling));
        }
        edgeCombo->addItem(
            QObject::tr("Any change"),
            static_cast<int>(SimulationCheckEdge::AnyChange));
        if (preferred) {
            const auto index = edgeCombo->findData(static_cast<int>(*preferred));
            if (index >= 0) edgeCombo->setCurrentIndex(index);
        }
    };
    configureEdgeCombo(sourceLane, sourceEdge, std::nullopt);
    configureEdgeCombo(targetLane, targetEdge, std::nullopt);
    connect(sourceLane, &QComboBox::currentIndexChanged, &dialog, [=](const int) {
        configureEdgeCombo(sourceLane, sourceEdge, std::nullopt);
    });
    connect(targetLane, &QComboBox::currentIndexChanged, &dialog, [=](const int) {
        configureEdgeCombo(targetLane, targetEdge, std::nullopt);
    });

    const auto setFieldVisible = [form](QWidget* field, const bool visible) {
        field->setVisible(visible);
        if (auto* labelWidget = form->labelForField(field)) {
            labelWidget->setVisible(visible);
        }
    };
    const auto updateKindRows = [=] {
        const auto selectedKind = static_cast<SimulationCheckKind>(
            kind->currentData().toInt());
        const auto value = selectedKind == SimulationCheckKind::ValueAtTick;
        const auto stable = selectedKind == SimulationCheckKind::StableRange;
        const auto response = selectedKind == SimulationCheckKind::EdgeResponse;
        setFieldVisible(lane, value || stable);
        setFieldVisible(tick, value);
        setFieldVisible(expected, value);
        setFieldVisible(start, stable || response);
        setFieldVisible(end, stable || response);
        setFieldVisible(sourceLane, response);
        setFieldVisible(sourceEdge, response);
        setFieldVisible(targetLane, response);
        setFieldVisible(targetEdge, response);
        setFieldVisible(minimumDelay, response);
        setFieldVisible(maximumDelay, response);
    };
    connect(kind, &QComboBox::currentIndexChanged, &dialog, [=](const int) {
        updateKindRows();
    });

    const auto initialIndex = kind->findData(static_cast<int>(
        existing ? existing->kind : initialKind));
    kind->setCurrentIndex(std::max(0, initialIndex));
    enabled->setChecked(!existing || existing->enabled);
    if (existing) {
        name->setText(QString::fromStdString(existing->name));
        setLane(lane, existing->laneId);
        setLane(sourceLane, existing->sourceLaneId);
        setLane(targetLane, existing->targetLaneId);
        configureEdgeCombo(sourceLane, sourceEdge, existing->sourceEdge);
        configureEdgeCombo(targetLane, targetEdge, existing->targetEdge);
        tick->setText(QString::number(existing->tick));
        expected->setText(QString::fromStdString(existing->expectedValue));
        start->setText(QString::number(existing->start));
        end->setText(QString::number(existing->end));
        minimumDelay->setText(QString::number(existing->minimumDelay));
        maximumDelay->setText(QString::number(existing->maximumDelay));
    } else {
        const auto cursor = canvas_ ? canvas_->cursorTick() : Tick{0};
        const auto range = canvas_ ? canvas_->selectedTimeRange() : std::nullopt;
        name->setText(
            initialKind == SimulationCheckKind::ValueAtTick
                ? tr("Value at time")
                : initialKind == SimulationCheckKind::StableRange
                    ? tr("Stable range") : tr("Edge response"));
        tick->setText(QString::number(cursor));
        expected->setText(QStringLiteral("0"));
        start->setText(QString::number(range ? range->first : Tick{0}));
        end->setText(QString::number(
            range ? range->second : std::max<Tick>(1, scenario->duration)));
        minimumDelay->setText(QStringLiteral("0"));
        maximumDelay->setText(QStringLiteral("1"));
    }
    updateKindRows();
    name->selectAll();
    name->setFocus(Qt::OtherFocusReason);

    const auto parseTick = [this](QLineEdit* edit, const QString& field,
                                  QString& error) -> std::optional<Tick> {
        std::optional<std::int64_t> cycle;
        QString parseError;
        const auto value = parseTimeText(
            edit->text(), project_.timeBase, nullptr, cycle, parseError);
        if (!value) error = tr("%1: %2").arg(field, parseError);
        return value;
    };
    while (dialog.exec() == QDialog::Accepted) {
        SimulationCheckDefinition check;
        check.id = existing
            ? existing->id
            : "simulation-check-"
                + QUuid::createUuid().toString(QUuid::WithoutBraces)
                      .toStdString();
        check.name = name->text().trimmed().toStdString();
        check.enabled = enabled->isChecked();
        check.kind = static_cast<SimulationCheckKind>(
            kind->currentData().toInt());
        QString error;
        if (check.name.empty()) {
            error = tr("A check name is required.");
        } else if (check.kind == SimulationCheckKind::ValueAtTick) {
            check.laneId = lane->currentData().toString().toStdString();
            const auto parsed = parseTick(tick, tr("Time"), error);
            if (parsed) check.tick = *parsed;
            check.expectedValue = expected->text().trimmed().toStdString();
        } else if (check.kind == SimulationCheckKind::StableRange) {
            check.laneId = lane->currentData().toString().toStdString();
            const auto parsedStart = parseTick(start, tr("Range start"), error);
            const auto parsedEnd = error.isEmpty()
                ? parseTick(end, tr("Range end"), error) : std::nullopt;
            if (parsedStart) check.start = *parsedStart;
            if (parsedEnd) check.end = *parsedEnd;
        } else {
            check.sourceLaneId =
                sourceLane->currentData().toString().toStdString();
            check.sourceEdge = static_cast<SimulationCheckEdge>(
                sourceEdge->currentData().toInt());
            check.targetLaneId =
                targetLane->currentData().toString().toStdString();
            check.targetEdge = static_cast<SimulationCheckEdge>(
                targetEdge->currentData().toInt());
            const auto parsedStart = parseTick(start, tr("Range start"), error);
            const auto parsedEnd = error.isEmpty()
                ? parseTick(end, tr("Range end"), error) : std::nullopt;
            const auto parsedMinimum = error.isEmpty()
                ? parseTick(minimumDelay, tr("Minimum delay"), error)
                : std::nullopt;
            const auto parsedMaximum = error.isEmpty()
                ? parseTick(maximumDelay, tr("Maximum delay"), error)
                : std::nullopt;
            if (parsedStart) check.start = *parsedStart;
            if (parsedEnd) check.end = *parsedEnd;
            if (parsedMinimum) check.minimumDelay = *parsedMinimum;
            if (parsedMaximum) check.maximumDelay = *parsedMaximum;
        }
        if (error.isEmpty()) {
            auto validationScenario = *scenario;
            if (storeSimulationChecks(validationScenario, {check}, &error)) {
                return check;
            }
        }
        QMessageBox::warning(&dialog, tr("Invalid check"), error);
    }
    return std::nullopt;
}

std::optional<std::size_t> MainWindow::selectedSimulationCheckIndex() const
{
    const auto* scenario = activeScenario();
    if (!scenario || !simulationCheckTable_) return std::nullopt;
    const auto row = simulationCheckTable_->currentRow();
    if (row < 0 || !simulationCheckTable_->item(row, 0)) return std::nullopt;
    const auto id = simulationCheckTable_->item(row, 0)
                        ->data(Qt::UserRole).toString().toStdString();
    const auto loaded = loadSimulationChecks(*scenario);
    if (!loaded.ok()) return std::nullopt;
    const auto found = std::find_if(
        loaded.checks.begin(), loaded.checks.end(),
        [&id](const SimulationCheckDefinition& check) {
            return check.id == id;
        });
    return found == loaded.checks.end()
        ? std::nullopt
        : std::optional<std::size_t>{
              static_cast<std::size_t>(found - loaded.checks.begin())};
}

void MainWindow::editSelectedSimulationCheck()
{
    auto* scenario = activeScenario();
    const auto selected = selectedSimulationCheckIndex();
    if (!scenario || !selected) return;
    const auto loaded = loadSimulationChecks(*scenario);
    if (!loaded.ok() || *selected >= loaded.checks.size()) return;
    const auto replacement = promptSimulationCheck(
        loaded.checks[*selected].kind, &loaded.checks[*selected]);
    if (!replacement) return;
    auto checks = loaded.checks;
    checks[*selected] = *replacement;
    QString error;
    if (!storeSimulationChecks(*scenario, checks, &error)) {
        QMessageBox::warning(this, tr("Invalid check"), error);
        return;
    }
    markEdited();
    populateSimulationCheckTable();
}

void MainWindow::removeSelectedSimulationCheck()
{
    auto* scenario = activeScenario();
    const auto selected = selectedSimulationCheckIndex();
    if (!scenario || !selected) return;
    const auto loaded = loadSimulationChecks(*scenario);
    if (!loaded.ok() || *selected >= loaded.checks.size()) return;
    if (QMessageBox::question(
            this,
            tr("Remove check"),
            tr("Remove '%1' from this scenario?")
                .arg(QString::fromStdString(loaded.checks[*selected].name)))
        != QMessageBox::Yes) {
        return;
    }
    auto checks = loaded.checks;
    checks.erase(checks.begin() + static_cast<std::ptrdiff_t>(*selected));
    QString error;
    if (!storeSimulationChecks(*scenario, checks, &error)) {
        QMessageBox::warning(this, tr("Cannot remove check"), error);
        return;
    }
    markEdited();
    populateSimulationCheckTable();
}

void MainWindow::runSimulationChecks()
{
    const auto reportUnavailable = [this](const QString& message) {
        simulationCheckResult_.reset();
        populateSimulationCheckTable();
        if (simulationCheckSummary_) {
            simulationCheckSummary_->setText(message);
            simulationCheckSummary_->setToolTip(message);
            simulationCheckSummary_->setStyleSheet(
                QStringLiteral("color:#815400;font-weight:600"));
        }
        setProperty("wavewidgets.checkStatus", QStringLiteral("unavailable"));
        setProperty("wavewidgets.checkFailureCount", 0);
    };
    if (!simulationResultMode_) {
        QMessageBox::warning(
            this, tr("Checks unavailable"),
            tr("Lightweight checks run in a simulation-result workspace."));
        return;
    }
    if (simulationStateMachine_.state() != SimulationSessionState::Current) {
        reportUnavailable(tr("Run the current scenario before evaluating checks"));
        return;
    }
    const auto* scenario = activeScenario();
    const auto* reference = activeTraceReference();
    if (!scenario || !reference || !traceIndex_) {
        reportUnavailable(tr("No current mapped simulation trace is available"));
        return;
    }
    if (traceIndex_->format == TraceFormat::Fst
        && !requiredFstSignalsLoaded()) {
        fstChecksPending_ = true;
        fstSignalLoadPaused_ = false;
        startPendingFstSignalLoad();
        reportUnavailable(
            tr("Checks will run after the required FST signals finish loading"));
        return;
    }
    fstChecksPending_ = false;
    const auto loaded = loadSimulationChecks(*scenario);
    if (!loaded.ok()) {
        reportUnavailable(loaded.error);
        return;
    }
    if (loaded.checks.empty()) {
        reportUnavailable(tr("No lightweight checks are defined for this scenario"));
        return;
    }
    simulationCheckResult_ = evaluateSimulationChecks(
        project_, *scenario, *traceIndex_, *reference, loaded.checks);
    populateSimulationCheckTable();

    std::vector<std::pair<Tick, Tick>> traceRanges;
    std::vector<WaveCanvas::DifferenceRange> expectedRanges;
    for (const auto& outcome : simulationCheckResult_->outcomes) {
        if (outcome.status != SimulationCheckStatus::Failed
            || !outcome.focusTick) {
            continue;
        }
        const auto startTick = *outcome.focusTick;
        const auto nextTick = startTick < std::numeric_limits<Tick>::max()
            ? startTick + 1 : startTick;
        const auto endTick = std::max<Tick>(nextTick, outcome.end);
        traceRanges.emplace_back(startTick, endTick);
        const auto& laneId = outcome.kind == SimulationCheckKind::EdgeResponse
            ? outcome.sourceLaneId : outcome.laneId;
        if (!laneId.empty()) {
            expectedRanges.push_back({laneId, startTick, endTick});
        }
    }
    canvas_->setDifferenceRanges(std::move(expectedRanges));
    if (compareTraceCanvas_) {
        compareTraceCanvas_->setDifferenceRanges(std::move(traceRanges));
        compareTraceCanvas_->setVisible(true);
    }
    if (simulationReviewTabs_ && simulationCheckPanel_) {
        simulationReviewTabs_->setCurrentWidget(simulationCheckPanel_);
    }
    setProperty(
        "wavewidgets.checkStatus",
        simulationCheckResult_->allPassed()
            ? QStringLiteral("pass") : QStringLiteral("fail"));
    setProperty(
        "wavewidgets.checkFailureCount",
        static_cast<qulonglong>(simulationCheckResult_->failedCount));
    setProperty(
        "wavewidgets.checkUnavailableCount",
        static_cast<qulonglong>(simulationCheckResult_->unavailableCount));
}

void MainWindow::revealSimulationCheckOutcome(const int row, const int column)
{
    Q_UNUSED(column)
    if (!simulationCheckResult_ || row < 0
        || row >= static_cast<int>(simulationCheckResult_->outcomes.size())) {
        return;
    }
    const auto& outcome = simulationCheckResult_->outcomes[
        static_cast<std::size_t>(row)];
    if (!outcome.focusTick) return;
    const auto& laneId = outcome.kind == SimulationCheckKind::EdgeResponse
        ? outcome.sourceLaneId : outcome.laneId;
    const auto& traceSignalId = outcome.kind == SimulationCheckKind::EdgeResponse
        ? outcome.sourceTraceSignalId : outcome.traceSignalId;
    if (!laneId.empty()) {
        canvas_->revealLocation(QString::fromStdString(laneId), *outcome.focusTick);
    }
    if (compareTraceCanvas_) {
        if (!traceSignalId.empty()) {
            compareTraceCanvas_->revealSignal(
                QString::fromStdString(traceSignalId));
        }
        compareTraceCanvas_->revealTick(*outcome.focusTick);
        compareTraceCanvas_->setVisible(true);
    }
    statusBar()->showMessage(
        tr("%1 at %2")
            .arg(
                QString::fromStdString(outcome.name),
                QString::fromStdString(
                    formatTick(*outcome.focusTick, project_.timeBase))),
        5'000);
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
    if (traceCanvas_) traceCanvas_->revealTick(difference.start);
    if (compareTraceCanvas_) {
        if (!difference.traceSignalId.empty()) {
            compareTraceCanvas_->revealSignal(
                QString::fromStdString(difference.traceSignalId));
        }
        compareTraceCanvas_->revealTick(difference.start);
        compareTraceCanvas_->setVisible(true);
    }
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
    if (!pendingQuickLaneId_.isEmpty()) {
        autosavePending_ = dirty_;
        return;
    }
    if (!dirty_ || !autosaveTimer_) return;
    if (autosaveWatcher_
        && (autosaveWatcher_->isRunning() || !autosaveInFlightPath_.isEmpty())) {
        autosavePending_ = true;
        return;
    }
    autosaveTimer_->start();
}

void MainWindow::startAutosave()
{
    if (!pendingQuickLaneId_.isEmpty()) {
        autosavePending_ = dirty_;
        return;
    }
    if (!dirty_ || !autosaveWatcher_) return;
    if (autosaveWatcher_->isRunning() || !autosaveInFlightPath_.isEmpty()) {
        autosavePending_ = true;
        return;
    }
    autosavePending_ = false;
    const auto generation = autosaveGeneration_;
    auto snapshot = project_;
    const auto path = autosavePathForProject(projectFile_);
    const auto directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory)) {
        statusBar()->showMessage(
            tr("Autosave recovery directory could not be created: %1").arg(directory),
            10'000);
        return;
    }
    autosaveInFlightPath_ = path;
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
    const auto path = std::exchange(autosaveInFlightPath_, QString{});
    const auto stale = result.first != autosaveGeneration_;
    const auto explicitlyDiscarded = discardedAutosavePaths_.erase(path) > 0;
    if (stale) {
        if ((explicitlyDiscarded || !dirty_)
            && !path.isEmpty()
            && QFileInfo::exists(path)
            && !QFile::remove(path)) {
            statusBar()->showMessage(
                tr("Recovery snapshot could not be removed: %1").arg(path),
                10'000);
        }
    } else if (!result.second.isEmpty()) {
        statusBar()->showMessage(
            tr("Autosave recovery snapshot failed: %1").arg(result.second),
            10'000);
    } else {
        statusBar()->showMessage(tr("Autosaved recovery snapshot"), 3'000);
    }
    if ((autosavePending_ || stale) && !explicitlyDiscarded) {
        if (!pendingQuickLaneId_.isEmpty()) {
            autosavePending_ = dirty_;
            return;
        }
        autosavePending_ = false;
        if (dirty_) autosaveTimer_->start(0);
    } else if (explicitlyDiscarded) {
        autosavePending_ = false;
    }
}

void MainWindow::createActions()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* newAction = fileMenu->addAction(
        themedIcon(QStringLiteral("document-new"), style(), QStyle::SP_FileIcon),
        tr("&New"),
        QKeySequence::New,
        this,
        &MainWindow::newProject);
    newAction->setObjectName(QStringLiteral("NewProjectAction"));
    newAction->setToolTip(tr("Create a blank waveform project"));
    auto* openAction = fileMenu->addAction(
        themedIcon(QStringLiteral("document-open"), style(), QStyle::SP_DialogOpenButton),
        tr("&Open…"),
        QKeySequence::Open,
        this,
        &MainWindow::openProject);
    openAction->setToolTip(tr("Open a project.wave.json file"));
    recentProjectsMenu_ = fileMenu->addMenu(tr("Open &Recent"));
    recentProjectsMenu_->setObjectName(QStringLiteral("RecentProjectsMenu"));
    connect(
        recentProjectsMenu_,
        &QMenu::aboutToShow,
        this,
        &MainWindow::updateRecentProjectsMenu);
    updateRecentProjectsMenu();
    auto* saveAction = fileMenu->addAction(
        themedIcon(QStringLiteral("document-save"), style(), QStyle::SP_DialogSaveButton),
        tr("&Save"),
        QKeySequence::Save,
        this,
        &MainWindow::saveProject);
    saveAction->setToolTip(tr("Safely save the current project"));
    fileMenu->addAction(tr("Save &As…"), QKeySequence::SaveAs, this, &MainWindow::saveProjectAs);
    exportAction_ = fileMenu->addAction(
        themedIcon(QStringLiteral("document-export"), style(), QStyle::SP_DialogSaveButton),
        tr("&Export…"),
        this,
        &MainWindow::exportArtifacts);
    exportAction_->setToolTip(tr("Generate verification code and document waveform artifacts"));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    editMenu_ = menuBar()->addMenu(tr("&Edit"));
    editMenu_->setToolTipsVisible(true);
    undoAction_ = editMenu_->addAction(
        themedIcon(QStringLiteral("edit-undo"), style(), QStyle::SP_ArrowBack),
        tr("Undo"),
        QKeySequence::Undo,
        this,
        &MainWindow::undo);
    undoAction_->setObjectName(QStringLiteral("UndoAction"));
    undoAction_->setToolTip(tr("Undo the last edit"));
    redoAction_ = editMenu_->addAction(
        themedIcon(QStringLiteral("edit-redo"), style(), QStyle::SP_ArrowForward),
        tr("Redo"),
        QKeySequence::Redo,
        this,
        &MainWindow::redo);
    redoAction_->setObjectName(QStringLiteral("RedoAction"));
    redoAction_->setToolTip(tr("Redo the last reverted edit"));
    connect(
        qApp,
        &QApplication::focusChanged,
        this,
        [this](QWidget*, QWidget* now) {
            if (auto* editor = qobject_cast<QLineEdit*>(now);
                editor && (editor->window() == this || isAncestorOf(editor))) {
                connect(
                    editor,
                    &QLineEdit::textChanged,
                    this,
                    &MainWindow::updateCommandActions,
                    Qt::UniqueConnection);
            }
            updateCommandActions();
        });
    for (auto* editor : findChildren<QLineEdit*>()) {
        connect(
            editor,
            &QLineEdit::textChanged,
            this,
            &MainWindow::updateCommandActions,
            Qt::UniqueConnection);
    }
    connect(
        editMenu_,
        &QMenu::aboutToShow,
        this,
        &MainWindow::updateCommandActions);
    previousScenarioAction_ = new QAction(tr("Previous waveform"), this);
    previousScenarioAction_->setObjectName(
        QStringLiteral("PreviousWaveformAction"));
    previousScenarioAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    previousScenarioAction_->setShortcutContext(Qt::WindowShortcut);
    previousScenarioAction_->setToolTip(
        tr("Switch to the previous waveform without changing the project"));
    addAction(previousScenarioAction_);
    connect(
        previousScenarioAction_,
        &QAction::triggered,
        this,
        [this] { switchAdjacentScenario(false); });

    nextScenarioAction_ = new QAction(tr("Next waveform"), this);
    nextScenarioAction_->setObjectName(
        QStringLiteral("NextWaveformAction"));
    nextScenarioAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    nextScenarioAction_->setShortcutContext(Qt::WindowShortcut);
    nextScenarioAction_->setToolTip(
        tr("Switch to the next waveform without changing the project"));
    addAction(nextScenarioAction_);
    connect(
        nextScenarioAction_,
        &QAction::triggered,
        this,
        [this] { switchAdjacentScenario(true); });
    editMenu_->addSeparator();
    cutRangeAction_ = editMenu_->addAction(tr("Cu&t range"));
    cutRangeAction_->setObjectName(QStringLiteral("CutRangeAction"));
    cutRangeAction_->setShortcut(QKeySequence::Cut);
    cutRangeAction_->setToolTip(
        tr("Copy and clear the selected time range as one undo command"));
    connect(cutRangeAction_, &QAction::triggered, this, [this] {
        if (auto* editor = qobject_cast<QLineEdit*>(focusWidget())) {
            editor->cut();
            return;
        }
        canvas_->cutSelection();
    });
    auto* copyAction = editMenu_->addAction(tr("&Copy range"));
    copyAction->setObjectName(QStringLiteral("CopyRangeAction"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setToolTip(tr("Copy the selected time range across all selected lanes"));
    connect(copyAction, &QAction::triggered, this, [this] {
        if (auto* editor = qobject_cast<QLineEdit*>(focusWidget())) {
            editor->copy();
            return;
        }
        canvas_->copySelection();
    });
    auto* pasteAction = editMenu_->addAction(tr("&Paste range"));
    pasteAction->setObjectName(QStringLiteral("PasteRangeAction"));
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setToolTip(tr("Paste the copied range at the current cursor as one undo command"));
    connect(pasteAction, &QAction::triggered, this, [this] {
        if (auto* editor = qobject_cast<QLineEdit*>(focusWidget())) {
            editor->paste();
            return;
        }
        canvas_->pasteAtCursor();
    });
    duplicateLaneAction_ = editMenu_->addAction(
        tr("&Duplicate selected signal"));
    duplicateLaneAction_->setObjectName(
        QStringLiteral("DuplicateLaneAction"));
    duplicateLaneAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_D));
    duplicateLaneAction_->setToolTip(
        tr("Copy the selected signal, properties, and waveform below the source"));
    duplicateLaneAction_->setEnabled(false);
    connect(duplicateLaneAction_, &QAction::triggered, this, [this] {
        if (qobject_cast<QLineEdit*>(focusWidget())) {
            statusBar()->showMessage(
                tr("Finish or cancel the text edit before duplicating the current target"),
                4'000);
            return;
        }
        duplicateSelectedLane();
    });
    auto* selectAllAction = editMenu_->addAction(tr("Select &full signal range"));
    selectAllAction->setObjectName(QStringLiteral("SelectFullRangeAction"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    selectAllAction->setToolTip(
        tr("Select the complete timeline for the current signal range"));
    connect(selectAllAction, &QAction::triggered, this, [this] {
        if (auto* editor = qobject_cast<QLineEdit*>(focusWidget())) {
            editor->selectAll();
            return;
        }
        canvas_->selectEntireTimeline();
    });
    signalFindAction_ = editMenu_->addAction(tr("&Find signal…"));
    signalFindAction_->setObjectName(QStringLiteral("FindSignalAction"));
    signalFindAction_->setShortcut(QKeySequence::Find);
    signalFindAction_->setToolTip(
        tr("Find a visible signal by name or ID without changing the edit cursor"));
    connect(signalFindAction_, &QAction::triggered, this, &MainWindow::showSignalFind);

    goToTimeAction_ = editMenu_->addAction(tr("Go to &time…"));
    goToTimeAction_->setObjectName(QStringLiteral("GoToTimeAction"));
    goToTimeAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    goToTimeAction_->setToolTip(
        tr("Move the edit cursor, or set the active selected range edge or width exactly"));
    connect(goToTimeAction_, &QAction::triggered, this, &MainWindow::showGoToTime);

    segmentMenu_ = editMenu_->addMenu(tr("&Segment"));
    segmentMenu_->setObjectName(QStringLiteral("SegmentMenu"));
    segmentMenu_->setToolTipsVisible(true);

    selectSegmentAtCursorAction_ = segmentMenu_->addAction(
        tr("Select Segment at edit cursor"));
    selectSegmentAtCursorAction_->setObjectName(
        QStringLiteral("SelectSegmentAtCursorAction"));
    selectSegmentAtCursorAction_->setShortcut(QKeySequence(Qt::Key_F6));
    selectSegmentAtCursorAction_->setToolTip(
        tr("Select the explicit Bus, Enum, or Clock Segment at the edit cursor"));
    connect(selectSegmentAtCursorAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        canvas_->selectSegmentAtCursor();
    });

    previousSegmentAction_ = segmentMenu_->addAction(tr("Previous Segment"));
    previousSegmentAction_->setObjectName(QStringLiteral("PreviousSegmentAction"));
    previousSegmentAction_->setShortcut(QKeySequence(Qt::Key_F7));
    previousSegmentAction_->setToolTip(
        tr("Select the previous explicit Segment on the current signal"));
    connect(previousSegmentAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        canvas_->selectPreviousSegment();
    });

    nextSegmentAction_ = segmentMenu_->addAction(tr("Next Segment"));
    nextSegmentAction_->setObjectName(QStringLiteral("NextSegmentAction"));
    nextSegmentAction_->setShortcut(QKeySequence(Qt::Key_F8));
    nextSegmentAction_->setToolTip(
        tr("Select the next explicit Segment on the current signal"));
    connect(nextSegmentAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        canvas_->selectNextSegment();
    });

    segmentMenu_->addSeparator();
    duplicateSegmentBeforeAction_ = segmentMenu_->addAction(
        tr("Duplicate Segment before"));
    duplicateSegmentBeforeAction_->setObjectName(
        QStringLiteral("DuplicateSelectedSegmentBeforeAction"));
    duplicateSegmentBeforeAction_->setToolTip(
        tr("Copy the selected Bus or Enum Segment into the preceding interval"));
    connect(duplicateSegmentBeforeAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        static_cast<void>(canvas_->duplicateSelectedSegmentBefore());
    });
    duplicateSegmentAfterAction_ = segmentMenu_->addAction(
        tr("Duplicate Segment after"));
    duplicateSegmentAfterAction_->setObjectName(
        QStringLiteral("DuplicateSelectedSegmentAfterAction"));
    duplicateSegmentAfterAction_->setToolTip(
        tr("Copy the selected Bus or Enum Segment into the following interval"));
    connect(duplicateSegmentAfterAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        static_cast<void>(canvas_->duplicateSelectedSegmentAfter());
    });

    segmentMenu_->addSeparator();
    moveSegmentEarlierAction_ = segmentMenu_->addAction(tr("Move Segment earlier"));
    moveSegmentEarlierAction_->setObjectName(
        QStringLiteral("MoveSelectedSegmentEarlierAction"));
    moveSegmentEarlierAction_->setToolTip(
        tr("Move the selected Segment earlier by one current editing step"));
    connect(moveSegmentEarlierAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        static_cast<void>(canvas_->moveSelectedSegmentEarlier());
    });
    moveSegmentLaterAction_ = segmentMenu_->addAction(tr("Move Segment later"));
    moveSegmentLaterAction_->setObjectName(
        QStringLiteral("MoveSelectedSegmentLaterAction"));
    moveSegmentLaterAction_->setToolTip(
        tr("Move the selected Segment later by one current editing step"));
    connect(moveSegmentLaterAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        static_cast<void>(canvas_->moveSelectedSegmentLater());
    });

    segmentMenu_->addSeparator();
    expandSegmentStartAction_ = segmentMenu_->addAction(
        tr("Expand Segment start"));
    expandSegmentStartAction_->setObjectName(
        QStringLiteral("ExpandSelectedSegmentStartAction"));
    expandSegmentStartAction_->setToolTip(
        tr("Move the selected Segment start earlier by one editing step"));
    connect(expandSegmentStartAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        static_cast<void>(canvas_->expandSelectedSegmentStart());
    });
    trimSegmentStartAction_ = segmentMenu_->addAction(
        tr("Trim Segment start"));
    trimSegmentStartAction_->setObjectName(
        QStringLiteral("TrimSelectedSegmentStartAction"));
    trimSegmentStartAction_->setToolTip(
        tr("Move the selected Segment start later by one editing step"));
    connect(trimSegmentStartAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        static_cast<void>(canvas_->trimSelectedSegmentStart());
    });
    expandSegmentEndAction_ = segmentMenu_->addAction(tr("Expand Segment end"));
    expandSegmentEndAction_->setObjectName(
        QStringLiteral("ExpandSelectedSegmentEndAction"));
    expandSegmentEndAction_->setToolTip(
        tr("Move the selected Segment end later by one editing step"));
    connect(expandSegmentEndAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        static_cast<void>(canvas_->expandSelectedSegmentEnd());
    });
    trimSegmentEndAction_ = segmentMenu_->addAction(tr("Trim Segment end"));
    trimSegmentEndAction_->setObjectName(
        QStringLiteral("TrimSelectedSegmentEndAction"));
    trimSegmentEndAction_->setToolTip(
        tr("Move the selected Segment end earlier by one editing step"));
    connect(trimSegmentEndAction_, &QAction::triggered, this, [this] {
        if (!canvas_->commitPendingInlineEdits()) return;
        static_cast<void>(canvas_->trimSelectedSegmentEnd());
    });

    const auto connectSegmentPreview =
        [this](
            QAction* action,
            const WaveCanvas::SegmentAction segmentAction) {
            connect(action, &QAction::hovered, this, [this, segmentAction] {
                updateSegmentActions();
                static_cast<void>(
                    canvas_->previewSelectedSegmentAction(segmentAction));
            });
        };
    connectSegmentPreview(
        duplicateSegmentBeforeAction_,
        WaveCanvas::SegmentAction::DuplicateBefore);
    connectSegmentPreview(
        duplicateSegmentAfterAction_,
        WaveCanvas::SegmentAction::DuplicateAfter);
    connectSegmentPreview(
        moveSegmentEarlierAction_,
        WaveCanvas::SegmentAction::MoveEarlier);
    connectSegmentPreview(
        moveSegmentLaterAction_,
        WaveCanvas::SegmentAction::MoveLater);
    connectSegmentPreview(
        expandSegmentStartAction_,
        WaveCanvas::SegmentAction::ExpandStart);
    connectSegmentPreview(
        trimSegmentStartAction_,
        WaveCanvas::SegmentAction::TrimStart);
    connectSegmentPreview(
        expandSegmentEndAction_,
        WaveCanvas::SegmentAction::ExpandEnd);
    connectSegmentPreview(
        trimSegmentEndAction_,
        WaveCanvas::SegmentAction::TrimEnd);
    const auto clearSegmentPreviewOnHover = [this](QAction* action) {
        connect(action, &QAction::hovered, this, [this] {
            canvas_->clearSelectedSegmentActionPreview();
        });
    };
    clearSegmentPreviewOnHover(selectSegmentAtCursorAction_);
    clearSegmentPreviewOnHover(previousSegmentAction_);
    clearSegmentPreviewOnHover(nextSegmentAction_);
    connect(
        segmentMenu_,
        &QMenu::hovered,
        this,
        [this](QAction* hovered) {
            const auto previewsSegment =
                hovered == duplicateSegmentBeforeAction_
                || hovered == duplicateSegmentAfterAction_
                || hovered == moveSegmentEarlierAction_
                || hovered == moveSegmentLaterAction_
                || hovered == expandSegmentStartAction_
                || hovered == trimSegmentStartAction_
                || hovered == expandSegmentEndAction_
                || hovered == trimSegmentEndAction_;
            if (!previewsSegment) {
                canvas_->clearSelectedSegmentActionPreview();
            }
        });
    connect(
        segmentMenu_,
        &QMenu::aboutToShow,
        this,
        &MainWindow::updateSegmentActions);
    connect(
        segmentMenu_,
        &QMenu::aboutToHide,
        canvas_,
        &WaveCanvas::clearSelectedSegmentActionPreview);

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
    addGroupAction->setObjectName(QStringLiteral("AddGroupAction"));
    addGroupAction->setToolTip(
        tr("Create an empty Group by name; advanced properties remain available from its header"));
    auto* editLaneAction = editMenu_->addAction(
        tr("Lane / group &properties…"),
        this,
        &MainWindow::editSelectedLane);
    editLaneAction->setToolTip(tr("Edit the selected lane or group without changing its stable ID"));
    hideLaneAction_ = editMenu_->addAction(
        tr("&Hide selected item"),
        this,
        &MainWindow::hideSelectedLane);
    hideLaneAction_->setObjectName(QStringLiteral("HideLaneAction"));
    hideLaneAction_->setToolTip(
        tr("Hide the selected signal or group; Show hidden items restores it"));
    hideLaneAction_->setEnabled(false);
    showHiddenLanesAction_ = editMenu_->addAction(
        tr("Show hidden items"),
        this,
        &MainWindow::showHiddenLanes);
    showHiddenLanesAction_->setObjectName(QStringLiteral("ShowHiddenLanesAction"));
    showHiddenLanesAction_->setToolTip(
        tr("Restore every hidden signal or group as one undoable edit"));
    showHiddenLanesAction_->setVisible(false);
    showRelationsAction_ = editMenu_->addAction(
        tr("Show relation constraints"));
    showRelationsAction_->setObjectName(
        QStringLiteral("ShowRelationsAction"));
    showRelationsAction_->setCheckable(true);
    showRelationsAction_->setChecked(false);
    showRelationsAction_->setToolTip(
        tr("Reveal event-to-event constraints; edit-impact warnings remain visible when hidden"));
    connect(
        showRelationsAction_,
        &QAction::toggled,
        canvas_,
        &WaveCanvas::setRelationsVisible);
    moveLaneUpAction_ = editMenu_->addAction(
        themedIcon(QStringLiteral("go-up"), style(), QStyle::SP_ArrowUp),
        tr("Move selected lane &up"),
        this,
        &MainWindow::moveSelectedLaneUp);
    moveLaneUpAction_->setObjectName(
        QStringLiteral("MoveLaneUpAction"));
    moveLaneUpAction_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    moveLaneUpAction_->setToolTip(
        tr("Move the selected lane or group one position earlier in display order"));
    moveLaneDownAction_ = editMenu_->addAction(
        themedIcon(QStringLiteral("go-down"), style(), QStyle::SP_ArrowDown),
        tr("Move selected lane &down"),
        this,
        &MainWindow::moveSelectedLaneDown);
    moveLaneDownAction_->setObjectName(
        QStringLiteral("MoveLaneDownAction"));
    moveLaneDownAction_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Down));
    moveLaneDownAction_->setToolTip(
        tr("Move the selected lane or group one position later in display order"));
    removeLaneAction_ = editMenu_->addAction(
        tr("&Remove selected lane / group…"),
        this,
        &MainWindow::removeSelectedLane);
    removeLaneAction_->setObjectName(QStringLiteral("RemoveLaneAction"));
    removeLaneAction_->setToolTip(
        tr("Remove the selected lane and dependent scenario references as one undo command"));
    removeLaneAction_->setEnabled(false);
}

QString MainWindow::scenarioPreferenceKey() const
{
    const auto path = normalizedProjectPath(projectFile_);
    if (path.isEmpty() || project_.id.empty()) return {};
    const auto identity = path.toCaseFolded()
        + QLatin1Char('\n')
        + QString::fromStdString(project_.id);
    const auto digest = QCryptographicHash::hash(
        identity.toUtf8(),
        QCryptographicHash::Sha256).toHex();
    return QStringLiteral("waveforms/lastScenario/")
        + QString::fromLatin1(digest);
}

std::optional<std::size_t> MainWindow::rememberedActiveScenarioIndex()
{
    const auto key = scenarioPreferenceKey();
    if (key.isEmpty()) return std::nullopt;
    QSettings settings;
    const auto storedId = settings.value(key).toString();
    if (storedId.isEmpty()) return std::nullopt;

    const auto matchCount = std::count_if(
        project_.scenarios.begin(),
        project_.scenarios.end(),
        [&storedId](const Scenario& scenario) {
            return scenario.id == storedId.toStdString();
        });
    if (matchCount != 1) {
        settings.remove(key);
        settings.sync();
        return std::nullopt;
    }
    const auto match = std::find_if(
        project_.scenarios.begin(),
        project_.scenarios.end(),
        [&storedId](const Scenario& scenario) {
            return scenario.id == storedId.toStdString();
        });
    return static_cast<std::size_t>(
        std::distance(project_.scenarios.begin(), match));
}

void MainWindow::rememberActiveScenario()
{
    const auto key = scenarioPreferenceKey();
    if (key.isEmpty()) return;
    QSettings settings;
    const auto* scenario = activeScenario();
    const auto uniqueIdentity = scenario
        && !scenario->id.empty()
        && std::count_if(
               project_.scenarios.begin(),
               project_.scenarios.end(),
               [scenario](const Scenario& candidate) {
                   return candidate.id == scenario->id;
               })
            == 1;
    if (project_.scenarios.size() > 1 && uniqueIdentity) {
        settings.setValue(key, QString::fromStdString(scenario->id));
    } else {
        settings.remove(key);
    }
    settings.sync();
}

void MainWindow::scheduleActiveScenarioLocationMemory()
{
    if (!scenarioLocationMemoryTimer_
        || projectFile_.isEmpty()
        || !activeScenario()
        || !pendingQuickLaneId_.isEmpty()) {
        return;
    }
    scenarioLocationMemoryTimer_->start();
}

QString MainWindow::scenarioLocationPreferenceKey() const
{
    const auto path = normalizedProjectPath(projectFile_);
    const auto* scenario = activeScenario();
    if (path.isEmpty() || project_.id.empty()
        || !scenario || scenario->id.empty()) {
        return {};
    }
    const auto identity = path.toCaseFolded()
        + QLatin1Char('\n')
        + QString::fromStdString(project_.id)
        + QLatin1Char('\n')
        + QString::fromStdString(scenario->id);
    const auto digest = QCryptographicHash::hash(
        identity.toUtf8(),
        QCryptographicHash::Sha256).toHex();
    return QStringLiteral("waveforms/lastLocation/")
        + QString::fromLatin1(digest);
}

void MainWindow::rememberActiveScenarioLocation()
{
    if (scenarioLocationMemoryTimer_) {
        scenarioLocationMemoryTimer_->stop();
    }
    if (!canvas_ || !pendingQuickLaneId_.isEmpty()) return;
    if (simulationResultMode_ && !simulationScenarioDirectory_.isEmpty()) {
        QString error;
        if (!persistActiveSimulationScenario(&error) && !error.isEmpty()) {
            simulationStateDetail_ = error;
            updateSimulationControls(error);
        }
        return;
    }
    const auto key = scenarioLocationPreferenceKey();
    if (key.isEmpty()) return;

    const auto* scenario = activeScenario();
    QSettings settings;
    const auto uniqueScenario = scenario
        && std::count_if(
               project_.scenarios.begin(),
               project_.scenarios.end(),
               [scenario](const Scenario& candidate) {
                   return candidate.id == scenario->id;
               })
            == 1;
    if (!uniqueScenario) {
        settings.remove(key);
        settings.sync();
        return;
    }

    auto laneId = canvas_->selectedLaneId().toStdString();
    if (!laneId.empty()) {
        const auto laneMatches = std::count_if(
            scenario->lanes.begin(),
            scenario->lanes.end(),
            [&laneId](const Lane& lane) {
                return lane.id == laneId;
            });
        const auto* lane = laneMatches == 1
            ? findLane(*scenario, laneId)
            : nullptr;
        if (!lane || !lane->visible || lane->kind == LaneKind::Group) {
            laneId.clear();
        }
    }

    const auto tick = std::clamp<Tick>(
        canvas_->cursorTick(),
        0,
        std::max<Tick>(0, scenario->duration));
    settings.setValue(
        key + QStringLiteral("/scenarioId"),
        QString::fromStdString(scenario->id));
    settings.setValue(
        key + QStringLiteral("/laneId"),
        QString::fromStdString(laneId));
    settings.setValue(
        key + QStringLiteral("/tick"),
        QString::number(tick));
    const auto visibleSpan = canvas_->visibleTimeSpan();
    if (visibleSpan > 0) {
        settings.setValue(
            key + QStringLiteral("/visibleSpanTick"),
            QString::number(visibleSpan));
    } else {
        settings.remove(key + QStringLiteral("/visibleSpanTick"));
    }
    settings.sync();
}

std::optional<QString> MainWindow::restoreActiveScenarioLocation()
{
    if (!canvas_) return std::nullopt;
    const auto* scenario = activeScenario();
    if (simulationResultMode_ && scenario) {
        const auto stored = simulationScenarioViews_.find(scenario->id);
        if (stored == simulationScenarioViews_.end()) return std::nullopt;
        const auto& view = stored->second;
        const auto tick = std::clamp<Tick>(
            view.cursorTick, 0, std::max<Tick>(0, scenario->duration));
        canvas_->goToTick(tick);
        const auto lane = std::find_if(
            scenario->lanes.begin(), scenario->lanes.end(),
            [&view](const Lane& candidate) {
                return candidate.name == view.selectedPortName
                    && candidate.visible
                    && candidate.kind != LaneKind::Group;
            });
        if (lane != scenario->lanes.end()) {
            canvas_->revealLocation(QString::fromStdString(lane->id), tick);
        }
        if (view.visibleSpanTicks > 0) {
            canvas_->restoreVisibleTimeSpan(view.visibleSpanTicks, tick);
        }
        return lane == scenario->lanes.end()
            ? QString::fromStdString(formatTick(tick, project_.timeBase))
            : tr("%1 at %2")
                  .arg(QString::fromStdString(lane->name),
                       QString::fromStdString(formatTick(tick, project_.timeBase)));
    }
    const auto key = scenarioLocationPreferenceKey();
    if (key.isEmpty()) return std::nullopt;

    QSettings settings;
    const auto storedScenarioId =
        settings.value(key + QStringLiteral("/scenarioId")).toString();
    if (storedScenarioId.isEmpty()) return std::nullopt;
    const auto uniqueScenario = scenario
        && std::count_if(
               project_.scenarios.begin(),
               project_.scenarios.end(),
               [scenario](const Scenario& candidate) {
                   return candidate.id == scenario->id;
               })
            == 1;
    if (!uniqueScenario
        || storedScenarioId != QString::fromStdString(scenario->id)) {
        settings.remove(key);
        settings.sync();
        return std::nullopt;
    }

    bool tickValid = false;
    const auto storedTick =
        settings.value(key + QStringLiteral("/tick")).toString().toLongLong(
            &tickValid);
    if (!tickValid) {
        settings.remove(key);
        settings.sync();
        return std::nullopt;
    }
    const auto tick = std::clamp<Tick>(
        storedTick,
        0,
        std::max<Tick>(0, scenario->duration));
    if (tick != storedTick) {
        settings.setValue(
            key + QStringLiteral("/tick"),
            QString::number(tick));
    }

    const auto storedLaneId =
        settings.value(key + QStringLiteral("/laneId")).toString();
    const auto laneId = storedLaneId.toStdString();
    const auto laneMatches = laneId.empty()
        ? std::size_t{0}
        : static_cast<std::size_t>(std::count_if(
              scenario->lanes.begin(),
              scenario->lanes.end(),
              [&laneId](const Lane& lane) {
                  return lane.id == laneId;
              }));
    const auto* lane = laneMatches == 1
        ? findLane(*scenario, laneId)
        : nullptr;
    const auto restoreLane = lane
        && lane->visible
        && lane->kind != LaneKind::Group;

    canvas_->goToTick(tick);
    if (restoreLane) {
        canvas_->revealLocation(storedLaneId, tick);
    } else if (!storedLaneId.isEmpty()) {
        settings.setValue(key + QStringLiteral("/laneId"), QString{});
    }

    std::optional<Tick> restoredVisibleSpan;
    const auto visibleSpanKey =
        key + QStringLiteral("/visibleSpanTick");
    if (settings.contains(visibleSpanKey)) {
        bool visibleSpanValid = false;
        const auto storedVisibleSpan =
            settings.value(visibleSpanKey).toString().toLongLong(
                &visibleSpanValid);
        if (visibleSpanValid
            && storedVisibleSpan > 0
            && canvas_->restoreVisibleTimeSpan(
                storedVisibleSpan,
                tick)) {
            restoredVisibleSpan = std::clamp<Tick>(
                storedVisibleSpan,
                1,
                scenario->duration);
            settings.setValue(
                visibleSpanKey,
                QString::number(*restoredVisibleSpan));
        } else {
            settings.remove(visibleSpanKey);
        }
    }
    settings.sync();

    const auto time = QString::fromStdString(
        formatTick(tick, project_.timeBase));
    const auto view = restoredVisibleSpan
        ? tr(" · %1 view")
              .arg(QString::fromStdString(
                  formatTick(*restoredVisibleSpan, project_.timeBase)))
        : QString{};
    if (restoreLane) {
        return tr("%1 at %2")
                .arg(QString::fromStdString(lane->name), time)
            + view;
    }
    if (!storedLaneId.isEmpty()) {
        return tr("%1 (saved signal unavailable)").arg(time)
            + view;
    }
    return time + view;
}

QString MainWindow::activeScenarioLabel() const
{
    const auto* scenario = activeScenario();
    if (!scenario) return tr("No waveform");
    auto label = scenario->name.empty()
        ? tr("Waveform %1").arg(
              static_cast<qulonglong>(activeScenarioIndex_ + 1))
        : QString::fromStdString(scenario->name);
    const auto duplicateNameCount = scenario->name.empty()
        ? std::size_t{0}
        : static_cast<std::size_t>(std::count_if(
              project_.scenarios.begin(),
              project_.scenarios.end(),
              [scenario](const Scenario& candidate) {
                  return QString::compare(
                             QString::fromStdString(candidate.name),
                             QString::fromStdString(scenario->name),
                             Qt::CaseInsensitive)
                      == 0;
              }));
    if (duplicateNameCount > 1) {
        const auto duplicateIdCount = scenario->id.empty()
            ? std::size_t{0}
            : static_cast<std::size_t>(std::count_if(
                  project_.scenarios.begin(),
                  project_.scenarios.end(),
                  [scenario](const Scenario& candidate) {
                      return candidate.id == scenario->id;
                  }));
        const auto identity =
            scenario->id.empty() || duplicateIdCount > 1
            ? tr("#%1").arg(
                  static_cast<qulonglong>(activeScenarioIndex_ + 1))
            : QString::fromStdString(scenario->id);
        label = tr("%1 · %2").arg(label, identity);
    }
    return label;
}

void MainWindow::populateScenarioSelector()
{
    if (!scenarioSelector_) return;
    const QSignalBlocker blocker(scenarioSelector_);
    scenarioSelector_->clear();
    for (std::size_t index = 0; index < project_.scenarios.size(); ++index) {
        const auto& scenario = project_.scenarios.at(index);
        auto label = scenario.name.empty()
            ? tr("Waveform %1").arg(static_cast<qulonglong>(index + 1))
            : QString::fromStdString(scenario.name);
        const auto duplicateNameCount = std::count_if(
            project_.scenarios.begin(),
            project_.scenarios.end(),
            [&label](const Scenario& candidate) {
                return QString::compare(
                           QString::fromStdString(candidate.name),
                           label,
                           Qt::CaseInsensitive)
                    == 0;
            });
        if (duplicateNameCount > 1) {
            const auto duplicateIdCount = scenario.id.empty()
                ? std::size_t{0}
                : static_cast<std::size_t>(std::count_if(
                      project_.scenarios.begin(),
                      project_.scenarios.end(),
                      [&scenario](const Scenario& candidate) {
                          return candidate.id == scenario.id;
                      }));
            const auto identity =
                scenario.id.empty() || duplicateIdCount > 1
                ? tr("#%1").arg(static_cast<qulonglong>(index + 1))
                : QString::fromStdString(scenario.id);
            label = tr("%1 · %2").arg(label, identity);
        }
        scenarioSelector_->addItem(
            label,
            QVariant::fromValue<qulonglong>(
                static_cast<qulonglong>(index)));
        const auto id = scenario.id.empty()
            ? tr("(no stable ID)")
            : QString::fromStdString(scenario.id);
        scenarioSelector_->setItemData(
            static_cast<int>(index),
            tr("Waveform %1 of %2 · ID: %3")
                .arg(static_cast<qulonglong>(index + 1))
                .arg(static_cast<qulonglong>(project_.scenarios.size()))
                .arg(id),
            Qt::ToolTipRole);
    }
    if (!project_.scenarios.empty()) {
        activeScenarioIndex_ = std::min(
            activeScenarioIndex_,
            project_.scenarios.size() - 1);
        scenarioSelector_->setCurrentIndex(
            static_cast<int>(activeScenarioIndex_));
    }
    const auto multiple = project_.scenarios.size() > 1;
    if (scenarioSelectorLabel_) scenarioSelectorLabel_->setVisible(multiple);
    if (scenarioSelectorLabelAction_) {
        scenarioSelectorLabelAction_->setVisible(multiple);
    }
    scenarioSelector_->setVisible(multiple);
    if (scenarioSelectorAction_) scenarioSelectorAction_->setVisible(multiple);
    if (scenarioSelectorSeparatorAction_) {
        scenarioSelectorSeparatorAction_->setVisible(multiple);
    }
    scenarioSelector_->setToolTip(
        multiple
            ? tr("%1 waveforms in this project · Ctrl+PageUp/PageDown switches")
                  .arg(static_cast<qulonglong>(project_.scenarios.size()))
            : tr("This project contains one waveform"));
    updateScenarioNavigationActions();
    updateSimulationScenarioActions();
}

bool MainWindow::switchActiveScenario(
    const std::size_t index,
    const bool announce)
{
    const auto restoreSelector = [this] {
        if (!scenarioSelector_) return;
        const QSignalBlocker blocker(scenarioSelector_);
        scenarioSelector_->setCurrentIndex(
            static_cast<int>(activeScenarioIndex_));
    };
    if (index >= project_.scenarios.size()) {
        restoreSelector();
        return false;
    }
    if (index == activeScenarioIndex_) {
        restoreSelector();
        return true;
    }
    if (announce && !commitPendingEdits()) {
        restoreSelector();
        return false;
    }

    rememberActiveScenarioLocation();
    const auto targetHasSessionContext =
        canvas_->hasDocumentContext(&project_.scenarios.at(index));
    closeSignalFind(false);
    closeGoToTime(false);
    activeScenarioIndex_ = index;
    if (scenarioSelector_) {
        const QSignalBlocker blocker(scenarioSelector_);
        scenarioSelector_->setCurrentIndex(static_cast<int>(index));
    }
    canvas_->setDocument(&project_, activeScenario(), &commandStack_);
    const auto restoredLocation = targetHasSessionContext
        ? std::optional<QString>{}
        : restoreActiveScenarioLocation();
    refreshTraceViews();
    invalidateCompareResult();
    rememberActiveScenario();
    updateCommandActions();
    updateSimulationScenarioActions();
    updateWindowTitle();
    if (announce) {
        auto message = tr("Editing waveform %1 of %2: %3")
                           .arg(static_cast<qulonglong>(
                               activeScenarioIndex_ + 1))
                           .arg(static_cast<qulonglong>(
                               project_.scenarios.size()))
                           .arg(activeScenarioLabel());
        if (restoredLocation) {
            message.append(tr(" · resumed %1").arg(*restoredLocation));
        }
        message.append(tr(" · Ctrl+PageUp/PageDown switches"));
        statusBar()->showMessage(
            message,
            5'000);
    }
    return true;
}

void MainWindow::switchAdjacentScenario(const bool forward)
{
    if (project_.scenarios.size() <= 1) return;
    if ((!forward && activeScenarioIndex_ == 0)
        || (forward && activeScenarioIndex_ + 1 >= project_.scenarios.size())) {
        statusBar()->showMessage(
            forward
                ? tr("Already at the last waveform · Ctrl+PageUp goes back")
                : tr("Already at the first waveform · Ctrl+PageDown goes next"),
            5'000);
        return;
    }
    switchActiveScenario(
        forward ? activeScenarioIndex_ + 1 : activeScenarioIndex_ - 1);
}

void MainWindow::updateScenarioNavigationActions()
{
    const auto multiple = project_.scenarios.size() > 1;
    if (previousScenarioAction_) previousScenarioAction_->setEnabled(multiple);
    if (nextScenarioAction_) nextScenarioAction_->setEnabled(multiple);
}

bool MainWindow::selectScenarioForHistoryState(const std::uint64_t stateId)
{
    const auto owner = commandScenarioIndices_.find(stateId);
    if (owner == commandScenarioIndices_.end()
        || owner->second == activeScenarioIndex_) {
        return false;
    }
    return switchActiveScenario(owner->second, false);
}

void MainWindow::createToolBars()
{
    auto* editBar = addToolBar(tr("Waveform tools"));
    editBar->setObjectName(QStringLiteral("WaveformToolbar"));
    editBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    editBar->setMovable(false);
    editBar->setFloatable(false);
    editBar->setAllowedAreas(Qt::TopToolBarArea);
    editBar->toggleViewAction()->setEnabled(false);
    editBar->toggleViewAction()->setVisible(false);

    scenarioSelectorLabel_ = new QLabel(tr("Waveform"), editBar);
    scenarioSelectorLabel_->setObjectName(
        QStringLiteral("WaveformSelectorLabel"));
    scenarioSelectorLabel_->setAccessibleName(tr("Waveform selector label"));
    scenarioSelectorLabelAction_ = editBar->addWidget(
        scenarioSelectorLabel_);
    scenarioSelectorLabelAction_->setObjectName(
        QStringLiteral("WaveformSelectorLabelToolbarAction"));

    scenarioSelector_ = new QComboBox(editBar);
    scenarioSelector_->setObjectName(QStringLiteral("WaveformSelector"));
    scenarioSelector_->setAccessibleName(tr("Waveform to edit"));
    scenarioSelector_->setMinimumWidth(150);
    scenarioSelector_->setMaximumWidth(260);
    scenarioSelector_->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon);
    scenarioSelector_->setMinimumContentsLength(16);
    scenarioSelectorAction_ = editBar->addWidget(scenarioSelector_);
    scenarioSelectorAction_->setObjectName(
        QStringLiteral("WaveformSelectorToolbarAction"));
    connect(
        scenarioSelector_,
        &QComboBox::currentIndexChanged,
        this,
        [this](const int row) {
            if (row < 0) return;
            const auto index = scenarioSelector_->itemData(row).toULongLong();
            switchActiveScenario(static_cast<std::size_t>(index));
        });
    scenarioSelectorSeparatorAction_ = editBar->addSeparator();
    scenarioSelectorSeparatorAction_->setObjectName(
        QStringLiteral("WaveformSelectorSeparatorAction"));
    populateScenarioSelector();

    markerAction_ = editBar->addAction(
        themedIcon(QStringLiteral("flag"), style(), QStyle::SP_DialogYesButton),
        tr("Measure"));
    markerAction_->setObjectName(QStringLiteral("MeasureToolAction"));
    markerAction_->setCheckable(true);
    markerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    markerAction_->setToolTip(
        tr("Measure time and values; Ctrl creates a saved marker, Ctrl+M toggles, Esc exits"));
    connect(markerAction_, &QAction::toggled, this, [this](const bool checked) {
        if (checked && !canvas_->commitPendingInlineEdits()) {
            const QSignalBlocker blocker(markerAction_);
            markerAction_->setChecked(false);
            return;
        }
        if (checked) {
            closeSignalFind(false);
            closeGoToTime(false);
        }
        canvas_->setTool(checked ? WaveCanvas::Tool::Marker : WaveCanvas::Tool::WaveEdit);
        updateWaveContext();
        updateSegmentActions();
        statusBar()->showMessage(
            checked
                ? tr("Measure: click or drag · Ctrl locks · Shift compares · Esc exits")
                : tr("Direct waveform editing active"),
            5'000);
    });

    asyncTimingAction_ = editBar->addAction(tr("Timing: Grid"));
    asyncTimingAction_->setObjectName(QStringLiteral("AsyncTimingAction"));
    asyncTimingAction_->setCheckable(true);
    asyncTimingAction_->setChecked(false);
    asyncTimingAction_->setToolTip(
        tr("Clock-associated signals use one clock beat; unclocked signals use the visible fixed grid. Click to allow asynchronous tick offsets."));
    connect(asyncTimingAction_, &QAction::toggled, this, [this](const bool enabled) {
        if (!canvas_->commitPendingInlineEdits()) {
            const QSignalBlocker blocker(asyncTimingAction_);
            asyncTimingAction_->setChecked(!enabled);
            return;
        }
        canvas_->setAsynchronousEditing(enabled);
        updateWaveContext();
        updateSegmentActions();
    });

    editBar->addSeparator();
    waveTargetLabel_ = new QLabel(editBar);
    waveTargetLabel_->setObjectName(QStringLiteral("WaveTargetLabel"));
    waveTargetLabel_->setAccessibleName(tr("Current waveform edit target"));
    waveTargetLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    waveTargetLabel_->setMinimumWidth(190);
    waveTargetLabel_->setMaximumWidth(440);
    waveTargetLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    waveTargetLabel_->setStyleSheet(QStringLiteral(
        "QLabel#WaveTargetLabel {"
        " color: #edf2f8; background: #2d3949;"
        " border: 1px solid #65758b; border-radius: 4px;"
        " padding: 3px 8px;"
        "}"));
    waveTargetAction_ = editBar->addWidget(waveTargetLabel_);
    waveTargetAction_->setObjectName(QStringLiteral("WaveTargetToolbarAction"));
    editBar->addSeparator();

    auto* rangeEditPalette = canvas_->rangeEditPaletteWidget();
    rangeEditPaletteAction_ = editBar->addWidget(rangeEditPalette);
    rangeEditPaletteAction_->setObjectName(QStringLiteral("RangeEditToolbarAction"));
    rangeEditPalette->ensurePolished();
    editBar->ensurePolished();
    editBar->setMinimumHeight(editBar->sizeHint().height());
    rangeEditPaletteAction_->setVisible(false);
    connect(
        canvas_,
        &WaveCanvas::rangeEditPaletteVisibilityChanged,
        rangeEditPaletteAction_,
        &QAction::setVisible);

    signalFindWidget_ = new QFrame(editBar);
    signalFindWidget_->setObjectName(QStringLiteral("SignalFindBar"));
    auto* signalFindLayout = new QHBoxLayout(signalFindWidget_);
    signalFindLayout->setContentsMargins(0, 0, 0, 0);
    signalFindLayout->setSpacing(4);
    signalFindLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto* signalFindLabel = new QLabel(tr("Find signal"), signalFindWidget_);
    signalFindLabel->setObjectName(QStringLiteral("SignalFindLabel"));
    signalFindLayout->addWidget(signalFindLabel);

    signalFindEdit_ = new QLineEdit(signalFindWidget_);
    signalFindEdit_->setObjectName(QStringLiteral("SignalFindEdit"));
    signalFindEdit_->setPlaceholderText(tr("Visible signal name or ID"));
    signalFindEdit_->setAccessibleName(tr("Find visible signal"));
    signalFindEdit_->setToolTip(
        tr("Type to select a visible signal; Enter finds next, Shift+Enter finds previous"));
    signalFindEdit_->setClearButtonEnabled(true);
    signalFindEdit_->setMinimumWidth(210);
    signalFindEdit_->setMaximumWidth(300);
    signalFindEdit_->installEventFilter(this);
    signalFindLayout->addWidget(signalFindEdit_);

    signalFindResultLabel_ = new QLabel(QStringLiteral("0/0"), signalFindWidget_);
    signalFindResultLabel_->setObjectName(QStringLiteral("SignalFindResultLabel"));
    signalFindResultLabel_->setAlignment(Qt::AlignCenter);
    signalFindResultLabel_->setMinimumWidth(42);
    signalFindResultLabel_->setAccessibleName(tr("Signal search result position"));
    signalFindLayout->addWidget(signalFindResultLabel_);

    const auto makeFindButton = [signalFindLayout, this](
                                    const QString& text,
                                    const QString& objectName,
                                    const QString& accessibleName,
                                    const QString& toolTip) {
        auto* button = new QToolButton(signalFindWidget_);
        button->setText(text);
        button->setObjectName(objectName);
        button->setAccessibleName(accessibleName);
        button->setToolTip(toolTip);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        signalFindLayout->addWidget(button);
        return button;
    };
    signalFindPreviousButton_ = makeFindButton(
        QStringLiteral("↑"),
        QStringLiteral("SignalFindPreviousButton"),
        tr("Previous matching signal"),
        tr("Previous matching signal (Shift+Enter)"));
    signalFindNextButton_ = makeFindButton(
        QStringLiteral("↓"),
        QStringLiteral("SignalFindNextButton"),
        tr("Next matching signal"),
        tr("Next matching signal (Enter)"));
    signalFindCloseButton_ = makeFindButton(
        QStringLiteral("×"),
        QStringLiteral("SignalFindCloseButton"),
        tr("Close signal search"),
        tr("Close signal search (Esc)"));

    signalFindWidgetAction_ = editBar->addWidget(signalFindWidget_);
    signalFindWidgetAction_->setObjectName(QStringLiteral("SignalFindToolbarAction"));
    signalFindWidgetAction_->setVisible(false);
    connect(signalFindEdit_, &QLineEdit::textChanged, this, [this] {
        signalFindMatchIndex_ = -1;
        updateSignalFind();
    });
    connect(signalFindPreviousButton_, &QToolButton::clicked, this, [this] {
        stepSignalFind(-1);
    });
    connect(signalFindNextButton_, &QToolButton::clicked, this, [this] {
        stepSignalFind(1);
    });
    connect(signalFindCloseButton_, &QToolButton::clicked, this, [this] {
        closeSignalFind();
    });

    goToTimeWidget_ = new QFrame(editBar);
    goToTimeWidget_->setObjectName(QStringLiteral("GoToTimeBar"));
    auto* goToTimeLayout = new QHBoxLayout(goToTimeWidget_);
    goToTimeLayout->setContentsMargins(0, 0, 0, 0);
    goToTimeLayout->setSpacing(4);
    goToTimeLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    goToTimeLabel_ = new QLabel(tr("Go to"), goToTimeWidget_);
    goToTimeLabel_->setObjectName(QStringLiteral("GoToTimeLabel"));
    goToTimeLabel_->installEventFilter(this);
    goToTimeLayout->addWidget(goToTimeLabel_);

    goToTimeEdit_ = new QLineEdit(goToTimeWidget_);
    goToTimeEdit_->setObjectName(QStringLiteral("GoToTimeEdit"));
    goToTimeEdit_->setPlaceholderText(tr("2.5 ns or cycle 25"));
    goToTimeEdit_->setAccessibleName(tr("Exact timeline position"));
    goToTimeEdit_->setToolTip(
        tr("Enter a decimal ps, ns, us, or ms value, an integer tick, or a clock cycle within the scenario"));
    goToTimeEdit_->setClearButtonEnabled(true);
    goToTimeEdit_->setMinimumWidth(170);
    goToTimeEdit_->setMaximumWidth(240);
    goToTimeEdit_->installEventFilter(this);
    goToTimeLayout->addWidget(goToTimeEdit_);

    goToTimeRangeLabel_ = new QLabel(QStringLiteral("0 ps–0 ps"), goToTimeWidget_);
    goToTimeRangeLabel_->setObjectName(QStringLiteral("GoToTimeRangeLabel"));
    goToTimeRangeLabel_->setAlignment(Qt::AlignCenter);
    goToTimeRangeLabel_->setMinimumWidth(90);
    goToTimeRangeLabel_->setAccessibleName(tr("Available timeline range"));
    goToTimeLayout->addWidget(goToTimeRangeLabel_);

    const auto makeGoToTimeButton = [goToTimeLayout, this](
                                        const QString& text,
                                        const QString& objectName,
                                        const QString& accessibleName,
                                        const QString& toolTip) {
        auto* button = new QToolButton(goToTimeWidget_);
        button->setText(text);
        button->setObjectName(objectName);
        button->setAccessibleName(accessibleName);
        button->setToolTip(toolTip);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        goToTimeLayout->addWidget(button);
        return button;
    };
    goToTimeOtherEdgeButton_ = makeGoToTimeButton(
        tr("Other edge"),
        QStringLiteral("GoToTimeOtherEdgeButton"),
        tr("Edit the other selected range edge"),
        tr("Keep the range and switch the exact editor to its other edge"));
    goToTimeOtherEdgeButton_->setVisible(false);
    goToTimeGoButton_ = makeGoToTimeButton(
        tr("Go"),
        QStringLiteral("GoToTimeGoButton"),
        tr("Go to exact time"),
        tr("Move the edit cursor to this time (Enter)"));
    goToTimeCloseButton_ = makeGoToTimeButton(
        QStringLiteral("×"),
        QStringLiteral("GoToTimeCloseButton"),
        tr("Close time navigation"),
        tr("Close time navigation (Esc)"));

    goToTimeWidgetAction_ = editBar->addWidget(goToTimeWidget_);
    goToTimeWidgetAction_->setObjectName(QStringLiteral("GoToTimeToolbarAction"));
    goToTimeWidgetAction_->setVisible(false);
    connect(goToTimeEdit_, &QLineEdit::textChanged, this, [this] {
        if (!goToTimeWidgetAction_ || !goToTimeWidgetAction_->isVisible()) return;
        goToTimeEdit_->setStyleSheet({});
        statusBar()->showMessage(
            goToTimeEditsRange_
                ? goToTimeEditsRangeWidth_
                    ? tr("Enter sets the exact range width · Other edge reverses direction · Esc returns to the range")
                    : tr("Enter sets the active range edge exactly · Other edge switches endpoints · Esc returns to the range")
                : tr("Enter jumps to this exact time · Esc closes without moving"));
    });
    connect(goToTimeOtherEdgeButton_, &QToolButton::clicked, this, [this] {
        if (!goToTimeEditsRange_ || !canvas_) return;
        const auto anchor = canvas_->explicitRangeAnchorTick();
        if (!anchor) return;
        canvas_->goToTick(*anchor);
        syncGoToTimeEditor(true);
        statusBar()->showMessage(
            goToTimeEditsRangeWidth_
                ? tr("Range width direction reversed · enter an exact width · Enter applies · Esc returns")
                : tr("Editing the other range edge · enter an exact time · Enter applies · Esc returns"));
    });
    connect(goToTimeGoButton_, &QToolButton::clicked, this, [this] {
        submitGoToTime();
    });
    connect(goToTimeCloseButton_, &QToolButton::clicked, this, [this] {
        closeGoToTime();
    });
    connect(
        canvas_,
        &WaveCanvas::exactRangeTimeEditRequested,
        this,
        &MainWindow::showGoToTime);
    connect(
        canvas_,
        &WaveCanvas::busEditPaletteVisibilityChanged,
        this,
        [this](const bool visible) {
            if (visible && signalFindWidgetAction_
                && signalFindWidgetAction_->isVisible()) {
                closeSignalFind(false);
            }
            if (visible && goToTimeWidgetAction_
                && goToTimeWidgetAction_->isVisible()) {
                closeGoToTime(false);
            }
        });
    connect(
        canvas_,
        &WaveCanvas::rangeEditPaletteVisibilityChanged,
        this,
        [this](const bool visible) {
            if (waveTargetAction_) waveTargetAction_->setVisible(!visible);
            if (!visible && goToTimeEditsRange_
                && goToTimeWidgetAction_
                && goToTimeWidgetAction_->isVisible()) {
                closeGoToTime(false);
            }
            if (visible && signalFindWidgetAction_ && signalFindWidgetAction_->isVisible()) {
                closeSignalFind(false);
            }
            if (visible && goToTimeWidgetAction_ && goToTimeWidgetAction_->isVisible()) {
                closeGoToTime(false);
            }
        });
    editBar->addSeparator();
    auto* zoomInAction = editBar->addAction(
        themedIcon(QStringLiteral("zoom-in"), style(), QStyle::SP_ArrowUp),
        tr("Zoom in"));
    zoomInAction->setObjectName(QStringLiteral("ZoomInAction"));
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, canvas_, &WaveCanvas::zoomIn);
    zoomInAction->setToolTip(
        tr("Zoom in around the visible edit cursor, or the viewport center"));
    auto* zoomOutAction = editBar->addAction(
        themedIcon(QStringLiteral("zoom-out"), style(), QStyle::SP_ArrowDown),
        tr("Zoom out"));
    zoomOutAction->setObjectName(QStringLiteral("ZoomOutAction"));
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, canvas_, &WaveCanvas::zoomOut);
    zoomOutAction->setToolTip(
        tr("Zoom out around the visible edit cursor, or the viewport center"));
    auto* fitAction = editBar->addAction(
        themedIcon(QStringLiteral("zoom-fit-best"), style(), QStyle::SP_DesktopIcon),
        tr("Fit scenario"));
    fitAction->setObjectName(QStringLiteral("FitScenarioAction"));
    fitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    fitAction->setShortcutContext(Qt::WindowShortcut);
    fitAction->setToolTip(tr("Fit the complete scenario"));
    connect(fitAction, &QAction::triggered, this, [this] {
        if (canvas_->hasExplicitRangeSelection()) {
            canvas_->fitSelection();
            statusBar()->showMessage(
                tr("Fitted selected range · Esc clears selection · Fit scenario returns to overview"),
                5'000);
        } else {
            canvas_->fitScenario();
            statusBar()->showMessage(tr("Fitted complete scenario"), 3'000);
        }
    });
    connect(
        canvas_,
        &WaveCanvas::rangeEditPaletteVisibilityChanged,
        fitAction,
        [this, fitAction](const bool rangeVisible) {
            fitAction->setText(
                rangeVisible
                    ? tr("Fit selection")
                    : tr("Fit scenario"));
            fitAction->setToolTip(
                rangeVisible
                    ? tr("Fit the selected time range and keep it editable")
                    : tr("Fit the complete scenario"));
        });
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
    importTraceAction_ = traceBar->addAction(
        themedIcon(
            QStringLiteral("document-open"),
            style(),
            QStyle::SP_DialogOpenButton),
        tr("Import trace"),
        this,
        &MainWindow::importTrace);
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
    traceSummary_->setObjectName(QStringLiteral("TraceSummary"));
    traceSummary_->setMinimumWidth(220);
    traceBar->addSeparator();
    traceBar->addWidget(traceSummary_);
    traceProgress_ = new QProgressBar;
    traceProgress_->setObjectName(QStringLiteral("TraceProgress"));
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

void MainWindow::populateSimulationCheckTable()
{
    if (!simulationCheckTable_ || !simulationCheckSummary_) return;
    simulationCheckTable_->setRowCount(0);
    const auto* scenario = activeScenario();
    if (!scenario) {
        simulationCheckSummary_->setText(tr("No active scenario"));
        return;
    }
    const auto loaded = loadSimulationChecks(*scenario);
    if (!loaded.ok()) {
        simulationCheckSummary_->setText(loaded.error);
        simulationCheckSummary_->setToolTip(loaded.error);
        simulationCheckSummary_->setStyleSheet(
            QStringLiteral("color:#a52222;font-weight:600"));
        return;
    }
    const auto laneName = [scenario](const std::string& laneId) {
        const auto* lane = findLane(*scenario, laneId);
        return lane ? QString::fromStdString(lane->name)
                    : QString::fromStdString(laneId);
    };
    const auto kindText = [this](const SimulationCheckKind kind) {
        switch (kind) {
        case SimulationCheckKind::ValueAtTick: return tr("Value at time");
        case SimulationCheckKind::StableRange: return tr("Stable range");
        case SimulationCheckKind::EdgeResponse: return tr("Edge response");
        }
        return tr("Unknown");
    };
    const auto edgeText = [this](const SimulationCheckEdge edge) {
        switch (edge) {
        case SimulationCheckEdge::Rising: return tr("rising");
        case SimulationCheckEdge::Falling: return tr("falling");
        case SimulationCheckEdge::AnyChange: return tr("any change");
        }
        return tr("unknown");
    };
    simulationCheckTable_->setRowCount(
        static_cast<int>(loaded.checks.size()));
    for (int row = 0; row < simulationCheckTable_->rowCount(); ++row) {
        const auto& check = loaded.checks[static_cast<std::size_t>(row)];
        const SimulationCheckOutcome* outcome = nullptr;
        if (simulationCheckResult_) {
            const auto found = std::find_if(
                simulationCheckResult_->outcomes.begin(),
                simulationCheckResult_->outcomes.end(),
                [&check](const SimulationCheckOutcome& candidate) {
                    return candidate.checkId == check.id;
                });
            if (found != simulationCheckResult_->outcomes.end()) outcome = &*found;
        }
        QString status = check.enabled ? tr("Stale") : tr("Disabled");
        QColor statusColor(129, 84, 0);
        if (outcome) {
            status = QString::fromLatin1(toString(outcome->status).data());
            switch (outcome->status) {
            case SimulationCheckStatus::Disabled:
                statusColor = QColor(89, 101, 121);
                break;
            case SimulationCheckStatus::Passed:
                statusColor = QColor(22, 132, 91);
                break;
            case SimulationCheckStatus::Failed:
                statusColor = QColor(198, 40, 40);
                break;
            case SimulationCheckStatus::Unavailable:
                statusColor = QColor(129, 84, 0);
                break;
            }
        }
        QString source;
        QString target;
        QString window;
        switch (check.kind) {
        case SimulationCheckKind::ValueAtTick:
            source = laneName(check.laneId);
            target = tr("equals %1").arg(
                QString::fromStdString(check.expectedValue));
            window = QString::fromStdString(
                formatTick(check.tick, project_.timeBase));
            break;
        case SimulationCheckKind::StableRange:
            source = laneName(check.laneId);
            target = tr("unchanged");
            window = tr("[%1, %2)")
                         .arg(
                             QString::fromStdString(
                                 formatTick(check.start, project_.timeBase)),
                             QString::fromStdString(
                                 formatTick(check.end, project_.timeBase)));
            break;
        case SimulationCheckKind::EdgeResponse:
            source = tr("%1 · %2")
                         .arg(laneName(check.sourceLaneId), edgeText(check.sourceEdge));
            target = tr("%1 · %2")
                         .arg(laneName(check.targetLaneId), edgeText(check.targetEdge));
            window = tr("[%1, %2) · %3..%4")
                         .arg(
                             QString::fromStdString(
                                 formatTick(check.start, project_.timeBase)),
                             QString::fromStdString(
                                 formatTick(check.end, project_.timeBase)),
                             QString::fromStdString(
                                 formatTick(check.minimumDelay, project_.timeBase)),
                             QString::fromStdString(
                                 formatTick(check.maximumDelay, project_.timeBase)));
            break;
        }
        const std::array<QString, 7> values{{
            status,
            QString::fromStdString(check.name),
            kindText(check.kind),
            source,
            target,
            window,
            outcome ? QString::fromStdString(outcome->message) : tr("Run checks"),
        }};
        for (int column = 0; column < static_cast<int>(values.size()); ++column) {
            auto* item = new QTableWidgetItem(values[static_cast<std::size_t>(column)]);
            if (column == 0) {
                item->setData(Qt::UserRole, QString::fromStdString(check.id));
                item->setForeground(statusColor);
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
            }
            simulationCheckTable_->setItem(row, column, item);
        }
    }
    if (editSimulationCheckAction_) editSimulationCheckAction_->setEnabled(false);
    if (removeSimulationCheckAction_) removeSimulationCheckAction_->setEnabled(false);
    simulationCheckSummary_->setToolTip({});
    if (!simulationCheckResult_) {
        simulationCheckSummary_->setText(
            loaded.checks.empty()
                ? tr("No checks defined")
                : tr("%1 check(s) · run to evaluate").arg(loaded.checks.size()));
        simulationCheckSummary_->setStyleSheet({});
    } else if (simulationCheckResult_->allPassed()) {
        simulationCheckSummary_->setText(
            tr("%1 passed · %2 disabled")
                .arg(simulationCheckResult_->passedCount)
                .arg(simulationCheckResult_->disabledCount));
        simulationCheckSummary_->setStyleSheet(
            QStringLiteral("color:#16845b;font-weight:600"));
    } else {
        simulationCheckSummary_->setText(
            tr("%1 passed · %2 failed · %3 unavailable · %4 disabled")
                .arg(simulationCheckResult_->passedCount)
                .arg(simulationCheckResult_->failedCount)
                .arg(simulationCheckResult_->unavailableCount)
                .arg(simulationCheckResult_->disabledCount));
        simulationCheckSummary_->setStyleSheet(
            QStringLiteral("color:#c62828;font-weight:600"));
    }
}

void MainWindow::invalidateCompareResult()
{
    compareResult_.reset();
    setProperty("wavewidgets.comparisonStatus", QStringLiteral("stale"));
    setProperty("wavewidgets.compareDifferenceCount", 0);
    if (compareTable_) compareTable_->setRowCount(0);
    if (compareSummary_) {
        compareSummary_->setText(tr("Trace or scenario changed; run compare again"));
        compareSummary_->setToolTip({});
        compareSummary_->setStyleSheet({});
    }
    if (traceCanvas_) traceCanvas_->setDifferenceRanges({});
    if (compareTraceCanvas_) compareTraceCanvas_->setDifferenceRanges({});
    if (canvas_) canvas_->setDifferenceRanges({});
    invalidateSimulationCheckResult();
}

void MainWindow::invalidateSimulationCheckResult()
{
    simulationCheckResult_.reset();
    setProperty("wavewidgets.checkStatus", QStringLiteral("stale"));
    setProperty("wavewidgets.checkFailureCount", 0);
    setProperty("wavewidgets.checkUnavailableCount", 0);
    populateSimulationCheckTable();
}

void MainWindow::updateWindowTitle()
{
    const auto name = project_.name.empty()
        ? tr("Untitled")
        : QString::fromStdString(project_.name);
    const auto displayName = project_.scenarios.size() > 1
        ? tr("%1 · %2").arg(name, activeScenarioLabel())
        : name;
    setWindowTitle(
        tr("%1%2 — Wave Workbench")
            .arg(displayName)
            .arg(dirty_ ? QStringLiteral(" *") : QString{}));
    if (!saveStateLabel_) return;

    QString state;
    QString color;
    if (recoveryLoaded_) {
        state = tr("Recovery loaded · Save required");
        color = QStringLiteral("#ffca65");
    } else if (projectFile_.isEmpty()) {
        state = dirty_ ? tr("Not saved · changes") : tr("Not saved");
        color = QStringLiteral("#c4cfdd");
    } else if (dirty_) {
        state = tr("Unsaved changes");
        color = QStringLiteral("#ffca65");
    } else {
        state = tr("Saved");
        color = QStringLiteral("#8dd69a");
    }
    saveStateLabel_->setText(state);
    saveStateLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; padding: 2px 8px; }").arg(color));
    saveStateLabel_->setToolTip(
        projectFile_.isEmpty()
            ? tr("This waveform has not been saved to a file.")
            : projectFile_);
}

bool MainWindow::loadFromPath(const QString& path, const bool preferRecovery)
{
    const auto selectedPath = preferRecovery
        ? preferredProjectLoadPath(path)
        : path;
    const auto result = loadProjectFile(selectedPath);
    if (!result.ok()) {
        QMessageBox::critical(this, tr("Open failed"), result.error);
        return false;
    }
    rememberActiveScenarioLocation();
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
    canvas_->clearDocumentContexts();
    project_ = *result.project;
    const auto recoveredSnapshot = selectedPath.endsWith(
        QStringLiteral(".autosave"),
        Qt::CaseInsensitive);
    recoveryLoaded_ = recoveredSnapshot;
    projectFile_ = projectPathForLoadedFile(selectedPath);
    loadedProjectRevision_ = projectFileRevision(projectFile_);
    activeScenarioIndex_ = rememberedActiveScenarioIndex().value_or(
        std::size_t{0});
    if (!projectFile_.isEmpty() && QFileInfo(projectFile_).isFile()) {
        rememberProjectPath(projectFile_);
    }
    commandStack_.clear();
    resetEditTracking(!result.migrated && !recoveredSnapshot);
    canvas_->setDocument(&project_, activeScenario(), &commandStack_);
    populateScenarioSelector();
    rememberActiveScenario();
    const auto restoredLocation = restoreActiveScenarioLocation();
    closeSignalFind(false);
    closeGoToTime(false);
    updateCommandActions();
    updateWindowTitle();
    if (recoveredSnapshot) {
        statusBar()->showMessage(
            projectFile_.isEmpty()
                ? tr("Untitled recovery snapshot loaded; use Save to choose a project file")
                : tr("Recovery snapshot loaded; save to commit it to %1").arg(projectFile_),
            10'000);
    } else if (!result.warnings.isEmpty()) {
        statusBar()->showMessage(result.warnings.join(QStringLiteral("; ")), 10'000);
    } else {
        auto message = tr("Opened %1").arg(selectedPath);
        if (restoredLocation) {
            message.append(
                tr(" · resumed %1 · no edit range restored")
                    .arg(*restoredLocation));
        }
        statusBar()->showMessage(message, 5'000);
    }
    refreshTraceViews();
    if (compareTraceCanvas_) compareTraceCanvas_->hide();
    invalidateCompareResult();
    if (dirty_) scheduleAutosave();
    return true;
}

bool MainWindow::writeToPath(const QString& path)
{
    const auto beforeName = project_.name;
    const auto previousProjectFile = projectFile_;
    const auto overwritesLoadedProject = !projectFile_.isEmpty()
        && sameProjectPath(path, projectFile_);
    if (overwritesLoadedProject) {
        const auto diskRevision = projectFileRevision(path);
        const auto unreadableRevision =
            loadedProjectRevision_.state == ProjectFileRevision::State::Unreadable
            || diskRevision.state == ProjectFileRevision::State::Unreadable;
        const auto stateChanged =
            loadedProjectRevision_.state != diskRevision.state;
        const auto contentChanged =
            loadedProjectRevision_.state == ProjectFileRevision::State::Present
            && diskRevision.state == ProjectFileRevision::State::Present
            && loadedProjectRevision_.sha256 != diskRevision.sha256;
        if (unreadableRevision || stateChanged || contentChanged) {
            QMessageBox conflict(this);
            conflict.setIcon(QMessageBox::Warning);
            conflict.setWindowTitle(tr("Project changed on disk"));
            conflict.setText(
                tr("Another application changed this waveform after it was opened."));
            conflict.setInformativeText(
                tr("Reload it to preserve those changes, save this version under a new name, or explicitly overwrite the external changes."));
            auto* reload = conflict.addButton(
                tr("Reload"), QMessageBox::AcceptRole);
            auto* saveAs = conflict.addButton(
                tr("Save As…"), QMessageBox::ActionRole);
            auto* overwrite = conflict.addButton(
                tr("Overwrite"), QMessageBox::DestructiveRole);
            auto* cancel = conflict.addButton(QMessageBox::Cancel);
            conflict.setDefaultButton(qobject_cast<QPushButton*>(cancel));
            conflict.setEscapeButton(cancel);
            conflict.exec();
            if (conflict.clickedButton() == reload) {
                if (loadFromPath(path, false)) {
                    isolateAbandonedRecoverySnapshotsAfterReload();
                }
                return false;
            }
            if (conflict.clickedButton() == saveAs) {
                QTimer::singleShot(0, this, [this] { saveProjectAs(); });
                return false;
            }
            if (conflict.clickedButton() != overwrite) return false;
        }
    }
    if (project_.name.empty() || project_.name == "Untitled") {
        auto inferred = QFileInfo(path).fileName();
        if (inferred.endsWith(QStringLiteral(".wave.json"), Qt::CaseInsensitive)) {
            inferred.chop(QStringLiteral(".wave.json").size());
        } else {
            inferred = QFileInfo(path).completeBaseName();
        }
        if (!inferred.trimmed().isEmpty()) project_.name = inferred.trimmed().toStdString();
    }

    QString error;
    if (!saveProjectFileAtomic(project_, path, &error)) {
        project_.name = beforeName;
        QMessageBox::critical(this, tr("Save failed"), error);
        return false;
    }
    projectFile_ = path;
    loadedProjectRevision_ = projectFileRevision(projectFile_);
    rememberProjectPath(projectFile_);
    rememberActiveScenario();
    rememberActiveScenarioLocation();
    recoveryLoaded_ = false;
    observedCommandStateId_ = commandStack_.stateId();
    cleanCommandStateId_ = observedCommandStateId_;
    cleanExternalRevision_ = externalRevision_;
    dirty_ = false;
    if (autosaveTimer_) autosaveTimer_->stop();
    ++autosaveGeneration_;
    autosavePending_ = false;

    QString cleanupError;
    const auto removeRecoverySnapshot = [&cleanupError](const QString& projectPath) {
        const auto snapshotPath = autosavePathForProject(projectPath);
        if (snapshotPath.isEmpty()
            || !QFileInfo::exists(snapshotPath)
            || QFile::remove(snapshotPath)) {
            return;
        }
        if (!cleanupError.isEmpty()) cleanupError += QStringLiteral(", ");
        cleanupError += snapshotPath;
    };
    removeRecoverySnapshot(path);
    if (QString::compare(previousProjectFile, path, Qt::CaseInsensitive) != 0) {
        removeRecoverySnapshot(previousProjectFile);
    }

    updateWindowTitle();
    if (cleanupError.isEmpty()) {
        statusBar()->showMessage(tr("Saved %1").arg(path), 5'000);
    } else {
        statusBar()->showMessage(
            tr("Saved %1, but stale recovery snapshot remains: %2")
                .arg(path, cleanupError),
            10'000);
    }
    return true;
}

void MainWindow::isolateAbandonedRecoverySnapshotsAfterReload()
{
    const auto snapshotPath = autosavePathForProject(projectFile_);
    if (snapshotPath.isEmpty() || !QFileInfo::exists(snapshotPath)) return;

    const auto timestamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmsszzz"));
    const auto isolatedPath = snapshotPath
        + QStringLiteral(".discarded-") + timestamp;
    if (QFile::rename(snapshotPath, isolatedPath)) {
        statusBar()->showMessage(
            tr("Reloaded the disk version; abandoned local recovery was preserved as %1")
                .arg(isolatedPath),
            10'000);
        return;
    }
    if (QFile::remove(snapshotPath)) {
        statusBar()->showMessage(
            tr("Reloaded the disk version; abandoned local recovery was removed"),
            8'000);
        return;
    }
    statusBar()->showMessage(
        tr("Reloaded the disk version, but its abandoned recovery snapshot could not be isolated: %1")
            .arg(snapshotPath),
        10'000);
}

bool MainWindow::discardRecoverySnapshots()
{
    if (autosaveTimer_) autosaveTimer_->stop();
    ++autosaveGeneration_;
    autosavePending_ = false;

    std::set<QString> paths;
    const auto currentPath = autosavePathForProject(projectFile_);
    if (!currentPath.isEmpty()) paths.insert(currentPath);
    const auto inFlightPath = autosaveInFlightPath_;
    if (!inFlightPath.isEmpty()) {
        paths.insert(inFlightPath);
        discardedAutosavePaths_.insert(inFlightPath);
        if (autosaveWatcher_) autosaveWatcher_->waitForFinished();
    }

    QStringList failures;
    for (const auto& path : paths) {
        if (QFileInfo::exists(path) && !QFile::remove(path)) failures.push_back(path);
    }
    if (failures.isEmpty()) return true;

    if (!inFlightPath.isEmpty()) discardedAutosavePaths_.erase(inFlightPath);
    scheduleAutosave();
    QMessageBox::warning(
        this,
        tr("Cannot discard recovery snapshot"),
        tr("The unsaved changes remain open because these recovery snapshots could not be removed:\n%1")
            .arg(failures.join(QLatin1Char('\n'))));
    return false;
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
    if (choice == QMessageBox::Discard) return discardRecoverySnapshots();
    saveProject();
    return !dirty_;
}

Scenario* MainWindow::activeScenario() noexcept
{
    return activeScenarioIndex_ < project_.scenarios.size()
        ? &project_.scenarios.at(activeScenarioIndex_)
        : nullptr;
}

const Scenario* MainWindow::activeScenario() const noexcept
{
    return activeScenarioIndex_ < project_.scenarios.size()
        ? &project_.scenarios.at(activeScenarioIndex_)
        : nullptr;
}

ImportedTrace* MainWindow::activeTraceReference() noexcept
{
    if (activeTraceId_.empty()) return nullptr;
    ImportedTrace* match = nullptr;
    for (auto& trace : project_.importedTraces) {
        if (trace.id != activeTraceId_) continue;
        if (match) return nullptr;
        match = &trace;
    }
    return match;
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
