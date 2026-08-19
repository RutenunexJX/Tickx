#pragma once

#include "wave/widgets_export.h"

#include "wave/trace.h"

#include <QStringList>
#include <QWidget>

#include <set>
#include <string>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace wave {

class WAVEWIDGETS_API TraceSignalBrowser final : public QWidget {
    Q_OBJECT

public:
    explicit TraceSignalBrowser(QWidget* parent = nullptr);

    void setTrace(
        const TraceIndex* trace,
        const std::set<std::string>& visibleSignalIds);
    void setVisibleSignalIds(const std::set<std::string>& signalIds);
    [[nodiscard]] std::set<std::string> visibleSignalIds() const;

signals:
    void visibleSignalIdsChanged(const QStringList& signalIds);
    void signalActivated(const QString& signalId);

private:
    void rebuild();
    void applyFilter(const QString& query);
    void updateSummary();
    void handleItemChanged(QTreeWidgetItem* item);

    const TraceIndex* trace_{nullptr};
    std::set<std::string> visibleSignalIds_;
    QLineEdit* searchEdit_{nullptr};
    QTreeWidget* tree_{nullptr};
    QLabel* summaryLabel_{nullptr};
    bool updating_{false};
};

} // namespace wave
