#include "lane_editing.h"
#include "ui_controls.h"
#include "signal_style.h"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace wave {
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
    const std::string_view excludedColor)
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
    ui::Dialog dialog(parent);
    dialog.setObjectName(QStringLiteral("LanePropertiesDialog"));
    dialog.setWindowTitle(
        initial.kind == LaneKind::Group
            ? QObject::tr("Group properties")
            : QObject::tr("Lane properties"));
    auto* dialogLayout = new QVBoxLayout(&dialog);
    auto* columns = new QHBoxLayout;
    dialogLayout->addLayout(columns);
    auto* layout = new ui::FormLayout;
    columns->addLayout(layout,1);
    auto* stableId = ui::lineEdit(QString::fromStdString(initial.id));
    stableId->setObjectName(QStringLiteral("LanePropertiesStableId"));
    stableId->setReadOnly(true);
    auto* name = ui::lineEdit(QString::fromStdString(initial.name));
    name->setObjectName(QStringLiteral("LanePropertiesNameEdit"));
    auto* kind = ui::comboBox();
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
    auto* width = ui::lineEdit(QString::number(initial.width));
    width->setObjectName(QStringLiteral("LanePropertiesWidthEdit"));
    auto* signedValue = ui::checkBox();
    signedValue->setObjectName(QStringLiteral("LanePropertiesSignedCheck"));
    signedValue->setChecked(initial.isSigned);
    auto* radix = ui::comboBox();
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
    auto* enumMap = ui::lineEdit(enumMapText(initial.enumMap));
    enumMap->setObjectName(QStringLiteral("LanePropertiesEnumMapEdit"));
    enumMap->setPlaceholderText(QObject::tr("IDLE=0; BUSY=1"));
    auto* clock = ui::comboBox();
    clock->setObjectName(QStringLiteral("LanePropertiesClockCombo"));
    clock->addItem(QObject::tr("Asynchronous (no clock)"), QString{});
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
    auto* group = ui::comboBox();
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
    auto* color = ui::lineEdit(QString::fromStdString(initial.color));
    color->setObjectName(QStringLiteral("LanePropertiesColorEdit"));
    auto* height = ui::spinBox();
    height->setObjectName(QStringLiteral("LanePropertiesHeightSpin"));
    height->setRange(30, 240);
    height->setValue(std::clamp(initial.height, 30, 240));
    auto* visible = ui::checkBox();
    visible->setObjectName(QStringLiteral("LanePropertiesVisibleCheck"));
    visible->setChecked(initial.visible);
    auto* compatibility = ui::text(QObject::tr(
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
    layout->addRow(QObject::tr("Color"), ui::colorField(color, &dialog));
    layout->addRow(QObject::tr("Height"), height);
    layout->addRow(QObject::tr("Visible"), visible);
    auto* styleEditor = new SignalStyleEditor(signalStyleSettings(initial.extensions), true, &dialog);
    if (initial.kind != LaneKind::Group) columns->addWidget(styleEditor,1);
    else styleEditor->hide();
    dialogLayout->addWidget(compatibility);
    auto* error = new QLabel;
    error->setObjectName(QStringLiteral("LanePropertiesError"));
    error->setWordWrap(true);
    error->setProperty("waveState", QStringLiteral("error"));
    error->hide();
    dialogLayout->addWidget(error);
    auto* buttons = ui::buttonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dialogLayout->addWidget(buttons);
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
        layout->setRowVisible(kind,!groupValue || !lockKind);
        for (auto* field : std::initializer_list<QWidget*>{width,signedValue,radix,enumMap,clock,group})
            layout->setRowVisible(field,!groupValue);
        styleEditor->setVisible(!groupValue);
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
            if (result.kind != LaneKind::Group) {
                if (!styleEditor->valid()) {
                    showError(QObject::tr("Use line width 0 (preset) or 0.75–4, and a valid fill color."), styleEditor);
                    return;
                }
                if (styleEditor->settings() != signalStyleSettings(initial.extensions))
                    storeSignalStyleSettings(result.extensions, styleEditor->settings());
            }
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

std::optional<ClockDomain> promptClockParameters(QWidget* parent, const Project& project,
    const ClockDomain& clock)
{
    const auto* original = &clock;
    ui::Dialog dialog(parent);
    dialog.setObjectName(QStringLiteral("QuickClockParametersDialog"));
    dialog.setWindowTitle(QObject::tr("Clock parameters"));
    auto* layout = new ui::FormLayout(&dialog);
    auto* mode = ui::comboBox();
    mode->setObjectName(QStringLiteral("ClockRateMode"));
    mode->addItem(QObject::tr("Period"), QStringLiteral("period"));
    mode->addItem(QObject::tr("Frequency (Hz)"), QStringLiteral("frequency"));
    auto* value = ui::lineEdit(
        QString::fromStdString(formatTick(original->period, project.timeBase)));
    value->setObjectName(QStringLiteral("ClockRateValue"));
    value->setPlaceholderText(QObject::tr("For example 2.5 ns"));
    value->setToolTip(
        QObject::tr("Period accepts decimal ps, ns, us, or ms values; frequency accepts Hz"));
    auto* phase = ui::lineEdit(
        QString::fromStdString(formatTick(original->phase, project.timeBase)));
    phase->setObjectName(QStringLiteral("ClockPhaseEdit"));
    phase->setPlaceholderText(QObject::tr("For example 0 ns"));
    auto* numerator = ui::lineEdit(
        QString::number(original->dutyCycle.numerator));
    numerator->setObjectName(QStringLiteral("ClockDutyNumeratorEdit"));
    numerator->setValidator(new PositiveInt64Validator(1, numerator));
    auto* denominator = ui::lineEdit(
        QString::number(original->dutyCycle.denominator));
    denominator->setObjectName(QStringLiteral("ClockDutyDenominatorEdit"));
    denominator->setValidator(new PositiveInt64Validator(2, denominator));
    auto* edge = ui::comboBox();
    edge->setObjectName(QStringLiteral("ClockActiveEdgeCombo"));
    edge->addItem(QObject::tr("Rising"), static_cast<int>(ClockEdge::Rising));
    edge->addItem(QObject::tr("Falling"), static_cast<int>(ClockEdge::Falling));
    edge->setCurrentIndex(original->activeEdge == ClockEdge::Rising ? 0 : 1);
    auto* reset = ui::lineEdit(QString::fromStdString(original->resetRelation));
    reset->setObjectName(QStringLiteral("ClockResetConditionEdit"));
    reset->setPlaceholderText(QObject::tr("Optional reset / disable condition"));
    layout->addRow(QObject::tr("Edit as"), mode);
    layout->addRow(QObject::tr("Value"), value);
    layout->addRow(QObject::tr("Phase"), phase);
    layout->addRow(QObject::tr("Duty numerator"), numerator);
    layout->addRow(QObject::tr("Duty denominator"), denominator);
    layout->addRow(QObject::tr("Active edge"), edge);
    layout->addRow(QObject::tr("Reset / disable condition"), reset);
    auto* error = new QLabel;
    error->setObjectName(QStringLiteral("QuickLaneParameterError"));
    error->setWordWrap(true);
    error->setProperty("waveState", QStringLiteral("error"));
    error->hide();
    layout->addRow(error);
    auto* buttons = ui::buttonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    const auto ticksPerSecond = 1.0e12
        / static_cast<double>(std::max<std::int64_t>(
            1, project.timeBase.picosecondsPerTick));
    std::optional<Tick> period;
    std::optional<Tick> parsedPhase;
    std::optional<std::int64_t> parsedNumerator;
    std::optional<std::int64_t> parsedDenominator;
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(
        buttons,
        &QDialogButtonBox::accepted,
        &dialog,
        [&project, mode, value, phase, numerator, denominator, error,
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
                    project.timeBase,
                    nullptr,
                    unusedCycle,
                    parseError);
            }
            if (!candidate || *candidate <= 0) {
                error->setText(
                    parseError.isEmpty()
                        ? QObject::tr("Enter a positive period or a frequency that maps to at least one tick.")
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
                project.timeBase,
                nullptr,
                unusedCycle,
                phaseError);
            if (!phaseCandidate) {
                error->setText(
                    phaseError.isEmpty()
                        ? QObject::tr("Phase must be an exact integer tick.")
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
                    QObject::tr("Duty numerator must be a positive 64-bit integer."));
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
                    QObject::tr("Duty denominator must be a 64-bit integer of at least 2."));
                error->show();
                denominator->setFocus(Qt::OtherFocusReason);
                denominator->selectAll();
                return;
            }
            if (numeratorCandidate >= denominatorCandidate) {
                error->setText(
                    QObject::tr("Duty numerator must be smaller than its denominator."));
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
    QObject::connect(value, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(phase, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(numerator, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(denominator, &QLineEdit::textEdited, error, &QWidget::hide);
    QObject::connect(
        mode,
        &QComboBox::currentIndexChanged,
        &dialog,
        [&project, mode, value, error, original, ticksPerSecond](int) {
            error->hide();
            value->setText(
                mode->currentData().toString() == QStringLiteral("frequency")
                    ? QString::number(
                          ticksPerSecond / static_cast<double>(original->period),
                          'g',
                          12)
                    : QString::fromStdString(
                          formatTick(original->period, project.timeBase)));
            value->setPlaceholderText(
                mode->currentData().toString() == QStringLiteral("frequency")
                    ? QObject::tr("For example 100000000")
                    : QObject::tr("For example 2.5 ns"));
        });
    if (dialog.exec() != QDialog::Accepted
        || !period
        || !parsedPhase
        || !parsedNumerator
        || !parsedDenominator) {
        return std::nullopt;
    }
    auto replacement = *original;
    replacement.period = *period;
    replacement.phase = *parsedPhase;
    replacement.dutyCycle = {*parsedNumerator, *parsedDenominator};
    replacement.dutyCycle.normalize();
    replacement.activeEdge = static_cast<ClockEdge>(edge->currentData().toInt());
    replacement.resetRelation = reset->text().trimmed().toStdString();
    return replacement;
}

Project makeBlankWaveProject()
{
    Project project;
    project.id = makeStableId("project");
    project.name = "Untitled";
    project.timeBase.picosecondsPerTick = 1;
    Scenario scenario;
    scenario.id = makeStableId("scenario");
    scenario.name = "Waveform";
    scenario.duration = toTicks(200, TimeUnit::Nanosecond, project.timeBase).value_or(200'000);
    project.scenarios.push_back(std::move(scenario));
    return project;
}

QuickLaneDraft makeQuickLane(const Project& project, const Scenario& scenario, LaneKind kind)
{
    if (kind != LaneKind::Clock && kind != LaneKind::Bit && kind != LaneKind::Bus)
        throw std::invalid_argument("Use a clock, bit or bus.");
    QuickLaneDraft result;
    auto& lane = result.lane;
    lane.id = makeStableId("lane");
    lane.name = uniqueLaneName(scenario, kind == LaneKind::Clock ? "clk" : kind == LaneKind::Bit ? "bit" : "bus");
    lane.kind = kind;
    lane.width = kind == LaneKind::Bus ? 8U : 1U;
    lane.radix = kind == LaneKind::Bit ? Radix::Binary : Radix::Hexadecimal;
    lane.color = randomReadableLaneColor(scenario);
    lane.height = 56;
    if (kind == LaneKind::Clock) {
        ClockDomain clock;
        clock.id = makeStableId("clock");
        clock.name = lane.name;
        clock.period = std::max<Tick>(1, static_cast<Tick>(std::llround(
            10'000.0 / static_cast<double>(std::max<std::int64_t>(1, project.timeBase.picosecondsPerTick)))));
        clock.phase = 0;
        clock.dutyCycle = {1, 2};
        clock.activeEdge = ClockEdge::Rising;
        lane.clockDomainId = clock.id;
        result.clock = std::move(clock);
    } else if (project.clockDomains.size() == 1) {
        lane.clockDomainId = project.clockDomains.front().id;
    }
    return result;
}

std::unique_ptr<EditCommand> quickLaneCommand(Project& project, Scenario& scenario, const QuickLaneDraft& draft)
{
    if (draft.clock) return std::make_unique<AddLaneCommand>(project, scenario, draft.lane, *draft.clock);
    return std::make_unique<AddLaneCommand>(scenario, draft.lane);
}

void validateLaneName(const Scenario& scenario, const std::string& laneId, const QString& name)
{
    if (name.trimmed().isEmpty()) throw std::invalid_argument("Signal name cannot be empty.");
    for (const auto& lane : scenario.lanes) {
        if (lane.id != laneId && QString::compare(QString::fromStdString(lane.name),
                name.trimmed(), Qt::CaseInsensitive) == 0)
            throw std::invalid_argument("A signal with this name already exists.");
    }
}

std::unique_ptr<EditCommand> completeQuickLaneCommand(Project& project, Scenario& scenario,
    const Lane& lane, const QString& name, const QString& parameter,
    const QString& clockId, bool& focusParameter)
{
    focusParameter = false;
    validateLaneName(scenario, lane.id, name);
    QuickLaneDraft draft{lane, std::nullopt};
    draft.lane.name = name.trimmed().toStdString();
    if (lane.kind == LaneKind::Clock) {
        QString error;
        std::optional<std::int64_t> cycle;
        const auto period = parseTimeText(parameter, project.timeBase, nullptr, cycle, error);
        if (!period || *period <= 0) {
            focusParameter = true;
            throw std::invalid_argument(error.isEmpty() ? "Clock period must be greater than zero." : error.toStdString());
        }
        const auto* clock = findClock(project, lane.clockDomainId);
        if (!clock) throw std::invalid_argument("The new clock domain is unavailable.");
        draft.clock = *clock;
        draft.clock->name = draft.lane.name;
        draft.clock->period = *period;
    } else {
        if (lane.kind == LaneKind::Bus) {
            bool valid = false;
            const auto width = parameter.toUInt(&valid);
            if (!valid || width == 0 || width > 65'536) {
                focusParameter = true;
                throw std::invalid_argument("Bus width must be from 1 to 65536.");
            }
            draft.lane.width = width;
        }
        draft.lane.clockDomainId = clockId.toStdString();
        if (!draft.lane.clockDomainId.empty() && !findClock(project, draft.lane.clockDomainId))
            throw std::invalid_argument("Select an available clock.");
    }
    return quickLaneCommand(project, scenario, draft);
}
Tick latestWaveContentTick(const Scenario& scenario) noexcept
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

} // namespace wave
