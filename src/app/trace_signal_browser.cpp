#include "trace_signal_browser.h"

#include "wave/trace_hierarchy.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>

namespace wave {
namespace {

constexpr int ItemKindRole = Qt::UserRole;
constexpr int SignalIdRole = Qt::UserRole + 1;
constexpr int FullNameRole = Qt::UserRole + 2;
constexpr int ScopeItem = 1;
constexpr int SignalItem = 2;

QString widthText(const std::uint32_t width)
{
    return width == 1
        ? TraceSignalBrowser::tr("1 bit")
        : TraceSignalBrowser::tr("%1 bits").arg(width);
}

QTreeWidgetItem* appendSignal(
    QTreeWidgetItem* parent,
    QTreeWidget* tree,
    const TraceHierarchySignal& signal,
    const std::set<std::string>& visibleSignalIds)
{
    auto* item = parent
        ? new QTreeWidgetItem(parent)
        : new QTreeWidgetItem(tree);
    item->setText(0, QString::fromStdString(signal.name));
    item->setText(1, widthText(signal.width));
    item->setToolTip(0, QString::fromStdString(signal.fullName));
    item->setData(0, ItemKindRole, SignalItem);
    item->setData(0, SignalIdRole, QString::fromStdString(signal.id));
    item->setData(0, FullNameRole, QString::fromStdString(signal.fullName));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(
        0,
        visibleSignalIds.contains(signal.id) ? Qt::Checked : Qt::Unchecked);
    return item;
}

void refreshScopeState(QTreeWidgetItem* scope)
{
    if (!scope || scope->childCount() == 0) return;
    int checked = 0;
    int partial = 0;
    for (int index = 0; index < scope->childCount(); ++index) {
        const auto state = scope->child(index)->checkState(0);
        checked += state == Qt::Checked ? 1 : 0;
        partial += state == Qt::PartiallyChecked ? 1 : 0;
    }
    if (partial > 0 || (checked > 0 && checked < scope->childCount())) {
        scope->setCheckState(0, Qt::PartiallyChecked);
    } else {
        scope->setCheckState(
            0,
            checked == scope->childCount() ? Qt::Checked : Qt::Unchecked);
    }
}

QTreeWidgetItem* appendScope(
    QTreeWidgetItem* parent,
    QTreeWidget* tree,
    const TraceHierarchyScope& scope,
    const std::set<std::string>& visibleSignalIds)
{
    auto* item = parent
        ? new QTreeWidgetItem(parent)
        : new QTreeWidgetItem(tree);
    item->setText(0, QString::fromStdString(scope.name));
    item->setToolTip(0, QString::fromStdString(scope.fullName));
    item->setData(0, ItemKindRole, ScopeItem);
    item->setData(0, FullNameRole, QString::fromStdString(scope.fullName));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    for (const auto& child : scope.scopes) {
        appendScope(item, tree, child, visibleSignalIds);
    }
    for (const auto& signal : scope.leaves) {
        appendSignal(item, tree, signal, visibleSignalIds);
    }
    refreshScopeState(item);
    return item;
}

void setDescendantState(QTreeWidgetItem* item, const Qt::CheckState state)
{
    for (int index = 0; index < item->childCount(); ++index) {
        auto* child = item->child(index);
        child->setCheckState(0, state);
        setDescendantState(child, state);
    }
}

void refreshAncestors(QTreeWidgetItem* item)
{
    for (auto* parent = item ? item->parent() : nullptr;
         parent;
         parent = parent->parent()) {
        refreshScopeState(parent);
    }
}

bool filterItem(
    QTreeWidgetItem* item,
    const QString& query,
    const bool ancestorMatches)
{
    const auto ownMatch = ancestorMatches
        || item->text(0).contains(query, Qt::CaseInsensitive)
        || item->data(0, FullNameRole).toString().contains(
            query, Qt::CaseInsensitive);
    if (item->data(0, ItemKindRole).toInt() == SignalItem) {
        item->setHidden(!ownMatch);
        return ownMatch;
    }

    bool descendantMatch = false;
    for (int index = 0; index < item->childCount(); ++index) {
        descendantMatch = filterItem(
            item->child(index), query, ownMatch) || descendantMatch;
    }
    const auto visible = ownMatch || descendantMatch;
    item->setHidden(!visible);
    if (!query.isEmpty() && descendantMatch) item->setExpanded(true);
    return visible;
}

void expandSingleScopeChain(QTreeWidgetItem* item)
{
    while (item && item->data(0, ItemKindRole).toInt() == ScopeItem) {
        item->setExpanded(true);
        QTreeWidgetItem* onlyScope = nullptr;
        int scopeCount = 0;
        for (int index = 0; index < item->childCount(); ++index) {
            auto* child = item->child(index);
            if (child->data(0, ItemKindRole).toInt() == ScopeItem) {
                onlyScope = child;
                ++scopeCount;
            }
        }
        item = scopeCount == 1 ? onlyScope : nullptr;
    }
}

} // namespace

TraceSignalBrowser::TraceSignalBrowser(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("TraceSignalBrowser"));
    setMinimumWidth(210);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 6);
    layout->setSpacing(6);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setObjectName(QStringLiteral("TraceHierarchySearch"));
    searchEdit_->setPlaceholderText(tr("Filter internal signals"));
    searchEdit_->setClearButtonEnabled(true);
    layout->addWidget(searchEdit_);

    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("TraceHierarchyTree"));
    tree_->setHeaderLabels({tr("Signal"), tr("Width")});
    tree_->setRootIsDecorated(true);
    tree_->setAlternatingRowColors(true);
    tree_->setUniformRowHeights(true);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(tree_, 1);

    summaryLabel_ = new QLabel(this);
    summaryLabel_->setObjectName(QStringLiteral("TraceHierarchySummary"));
    summaryLabel_->setText(tr("No simulation result"));
    layout->addWidget(summaryLabel_);

    connect(
        searchEdit_,
        &QLineEdit::textChanged,
        this,
        &TraceSignalBrowser::applyFilter);
    connect(
        tree_,
        &QTreeWidget::itemChanged,
        this,
        [this](QTreeWidgetItem* item, int column) {
            if (column == 0) handleItemChanged(item);
        });
    connect(
        tree_,
        &QTreeWidget::itemClicked,
        this,
        [this](QTreeWidgetItem* item, int) {
            if (!item || item->data(0, ItemKindRole).toInt() != SignalItem) return;
            emit signalActivated(item->data(0, SignalIdRole).toString());
        });
}

void TraceSignalBrowser::setTrace(
    const TraceIndex* trace,
    const std::set<std::string>& visibleSignalIds)
{
    trace_ = trace;
    visibleSignalIds_ = visibleSignalIds;
    rebuild();
}

void TraceSignalBrowser::setVisibleSignalIds(
    const std::set<std::string>& signalIds)
{
    visibleSignalIds_ = signalIds;
    rebuild();
}

std::set<std::string> TraceSignalBrowser::visibleSignalIds() const
{
    return visibleSignalIds_;
}

void TraceSignalBrowser::revealSignal(const QString& signalId)
{
    if (signalId.isEmpty()) return;
    for (QTreeWidgetItemIterator iterator(tree_); *iterator; ++iterator) {
        auto* item = *iterator;
        if (item->data(0, ItemKindRole).toInt() != SignalItem
            || item->data(0, SignalIdRole).toString() != signalId) {
            continue;
        }
        tree_->setCurrentItem(item);
        tree_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        return;
    }
}

void TraceSignalBrowser::rebuild()
{
    updating_ = true;
    const QSignalBlocker blocker(tree_);
    tree_->clear();
    if (trace_) {
        const auto hierarchy = buildTraceHierarchy(*trace_);
        for (const auto& scope : hierarchy.scopes) {
            expandSingleScopeChain(appendScope(
                nullptr, tree_, scope, visibleSignalIds_));
        }
        for (const auto& signal : hierarchy.unscopedSignals) {
            appendSignal(nullptr, tree_, signal, visibleSignalIds_);
        }
    }
    updating_ = false;
    applyFilter(searchEdit_->text());
    updateSummary();
}

void TraceSignalBrowser::applyFilter(const QString& query)
{
    const auto normalized = query.trimmed();
    for (int index = 0; index < tree_->topLevelItemCount(); ++index) {
        filterItem(tree_->topLevelItem(index), normalized, normalized.isEmpty());
    }
}

void TraceSignalBrowser::updateSummary()
{
    if (!trace_) {
        summaryLabel_->setText(tr("No simulation result"));
        return;
    }
    const auto loaded = std::count_if(
        trace_->traceSignals.begin(),
        trace_->traceSignals.end(),
        [](const TraceSignal& signal) { return signal.transitionsLoaded; });
    summaryLabel_->setText(
        tr("%1 of %2 visible · %3 loaded")
            .arg(static_cast<qulonglong>(visibleSignalIds_.size()))
            .arg(static_cast<qulonglong>(trace_->traceSignals.size()))
            .arg(static_cast<qulonglong>(loaded)));
}

void TraceSignalBrowser::handleItemChanged(QTreeWidgetItem* item)
{
    if (updating_ || !item) return;
    updating_ = true;
    if (item->data(0, ItemKindRole).toInt() == ScopeItem
        && item->checkState(0) != Qt::PartiallyChecked) {
        setDescendantState(item, item->checkState(0));
    }
    refreshAncestors(item);

    visibleSignalIds_.clear();
    QStringList ids;
    for (QTreeWidgetItemIterator iterator(tree_); *iterator; ++iterator) {
        auto* candidate = *iterator;
        if (candidate->data(0, ItemKindRole).toInt() != SignalItem
            || candidate->checkState(0) != Qt::Checked) {
            continue;
        }
        const auto id = candidate->data(0, SignalIdRole).toString();
        ids.push_back(id);
        visibleSignalIds_.insert(id.toStdString());
    }
    updateSummary();
    updating_ = false;
    emit visibleSignalIdsChanged(ids);
}

} // namespace wave
