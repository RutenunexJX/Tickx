#pragma once

#include "wave/commands.h"
#include "wave/time.h"
#include <QValidator>
#include <algorithm>

namespace wave {
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

QString laneKindText(LaneKind kind);
std::string uniqueLaneName(const Scenario& scenario, std::string_view base);
std::string randomReadableLaneColor(const Scenario& scenario, std::string_view excludedColor = {});
std::optional<Tick> parseTimeText(QString text, const TimeBase& timeBase,
    const ClockDomain* clock, std::optional<std::int64_t>& cycle, QString& error);
std::optional<Lane> promptLaneProperties(QWidget* parent, const Project& project,
    Scenario& scenario, const Lane& initial, bool lockKind);
std::optional<ClockDomain> promptClockParameters(QWidget* parent, const Project& project,
    const ClockDomain& clock);
Project makeBlankWaveProject();
Tick latestWaveContentTick(const Scenario& scenario) noexcept;
struct QuickLaneDraft {
    Lane lane;
    std::optional<ClockDomain> clock;
};
QuickLaneDraft makeQuickLane(const Project& project, const Scenario& scenario, LaneKind kind);
std::unique_ptr<EditCommand> quickLaneCommand(Project& project, Scenario& scenario,
    const QuickLaneDraft& draft);
std::unique_ptr<EditCommand> completeQuickLaneCommand(Project& project, Scenario& scenario,
    const Lane& lane, const QString& name, const QString& parameter,
    const QString& clockId, bool& focusParameter);
void validateLaneName(const Scenario& scenario, const std::string& laneId, const QString& name);
} // namespace wave
