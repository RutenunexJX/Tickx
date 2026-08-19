#include "wave/simulation_batch.h"

#include <utility>

namespace wave {

bool SimulationBatchRun::begin(
    std::vector<SimulationBatchScenarioDescriptor> scenarios)
{
    if (running() || scenarios.empty()) return false;
    scenarios_.clear();
    scenarios_.reserve(scenarios.size());
    for (auto& scenario : scenarios) {
        SimulationBatchScenarioResult result;
        result.scenario = std::move(scenario);
        scenarios_.push_back(std::move(result));
    }
    state_ = SimulationBatchState::Running;
    cancelRequested_ = false;
    currentIndex_ = 0;
    scenarios_.front().state = SimulationBatchScenarioState::Running;
    return true;
}

bool SimulationBatchRun::requestCancel() noexcept
{
    if (!running() || cancelRequested_) return false;
    cancelRequested_ = true;
    return true;
}

bool SimulationBatchRun::completeCurrent(const SimulationRunReport& report)
{
    if (!running() || !currentIndex_
        || *currentIndex_ >= scenarios_.size()) {
        return false;
    }

    auto& result = scenarios_.at(*currentIndex_);
    if (result.state != SimulationBatchScenarioState::Running) return false;
    result.runStatus = report.status;
    result.diagnostic = report.diagnostic;
    result.durationMs = report.durationMs;
    result.buildCacheHit = report.buildCache.hit;
    if (report.ok()) {
        result.state = SimulationBatchScenarioState::Succeeded;
    } else if (report.status == SimulationRunStatus::Cancelled
               || report.status == SimulationRunStatus::Superseded) {
        result.state = SimulationBatchScenarioState::Cancelled;
        cancelRequested_ = true;
    } else {
        result.state = SimulationBatchScenarioState::Failed;
    }

    if (cancelRequested_) {
        cancelPending();
        currentIndex_.reset();
        state_ = SimulationBatchState::Cancelled;
        return true;
    }
    advance();
    return true;
}

void SimulationBatchRun::reset() noexcept
{
    state_ = SimulationBatchState::Idle;
    scenarios_.clear();
    currentIndex_.reset();
    cancelRequested_ = false;
}

SimulationBatchState SimulationBatchRun::state() const noexcept
{
    return state_;
}

bool SimulationBatchRun::running() const noexcept
{
    return state_ == SimulationBatchState::Running;
}

bool SimulationBatchRun::cancelRequested() const noexcept
{
    return cancelRequested_;
}

const SimulationBatchScenarioResult* SimulationBatchRun::current() const noexcept
{
    if (!currentIndex_ || *currentIndex_ >= scenarios_.size()) return nullptr;
    return &scenarios_.at(*currentIndex_);
}

const std::vector<SimulationBatchScenarioResult>&
SimulationBatchRun::scenarios() const noexcept
{
    return scenarios_;
}

SimulationBatchSummary SimulationBatchRun::summary() const noexcept
{
    SimulationBatchSummary summary;
    summary.total = scenarios_.size();
    for (const auto& scenario : scenarios_) {
        switch (scenario.state) {
        case SimulationBatchScenarioState::Pending:
            ++summary.pending;
            break;
        case SimulationBatchScenarioState::Running:
            ++summary.running;
            break;
        case SimulationBatchScenarioState::Succeeded:
            ++summary.succeeded;
            break;
        case SimulationBatchScenarioState::Failed:
            ++summary.failed;
            break;
        case SimulationBatchScenarioState::Cancelled:
            ++summary.cancelled;
            break;
        }
    }
    return summary;
}

void SimulationBatchRun::cancelPending() noexcept
{
    for (auto& scenario : scenarios_) {
        if (scenario.state == SimulationBatchScenarioState::Pending) {
            scenario.state = SimulationBatchScenarioState::Cancelled;
        }
    }
}

void SimulationBatchRun::advance() noexcept
{
    const auto next = *currentIndex_ + 1;
    if (next >= scenarios_.size()) {
        currentIndex_.reset();
        state_ = SimulationBatchState::Completed;
        return;
    }
    currentIndex_ = next;
    scenarios_.at(next).state = SimulationBatchScenarioState::Running;
}

std::string_view toString(const SimulationBatchState state) noexcept
{
    switch (state) {
    case SimulationBatchState::Idle: return "idle";
    case SimulationBatchState::Running: return "running";
    case SimulationBatchState::Completed: return "completed";
    case SimulationBatchState::Cancelled: return "cancelled";
    }
    return "idle";
}

std::string_view toString(const SimulationBatchScenarioState state) noexcept
{
    switch (state) {
    case SimulationBatchScenarioState::Pending: return "pending";
    case SimulationBatchScenarioState::Running: return "running";
    case SimulationBatchScenarioState::Succeeded: return "succeeded";
    case SimulationBatchScenarioState::Failed: return "failed";
    case SimulationBatchScenarioState::Cancelled: return "cancelled";
    }
    return "pending";
}

} // namespace wave
