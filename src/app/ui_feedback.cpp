#include "ui_controls.h"

#include "ElaColorDialog.h"
#include "ElaMessageBar.h"
#include "ElaText.h"
#include "ElaToolTip.h"

#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QChildEvent>
#include <QFileDialog>
#include <QHelpEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QMainWindow>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSet>
#include <QTextDocument>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cmath>

namespace wave::ui {
namespace {

class ToolTipText final : public ElaText {
public:
    explicit ToolTipText(QWidget* parent) : ElaText(parent, true)
    {
        setProperty("waveInheritTextStyle", true);
        setObjectName(QStringLiteral("WaveToolTipText"));
        setStyleSheet({});
        document_.setDocumentMargin(0);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        document_.setDefaultTextOption(option);
    }
    void setContent(const QString& value, const QFont& font, QColor color, QSize maximum)
    {
        document_.setDefaultFont(font);
        document_.setDefaultStyleSheet(QStringLiteral("body { color: %1; }").arg(color.name()));
        if (Qt::mightBeRichText(value)) document_.setHtml(value);
        else document_.setPlainText(value);
        document_.setTextWidth(maximum.width());
        document_.setTextWidth(std::min(qreal(maximum.width()), std::max(1.0, document_.idealWidth())));
        color_ = color;
        setFixedSize(std::min(maximum.width(), int(std::ceil(document_.size().width()))),
            std::min(maximum.height(), int(std::ceil(document_.size().height()))));
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette.setColor(QPalette::Text, color_);
        document_.documentLayout()->draw(&painter, context);
    }
private:
    QTextDocument document_;
    QColor color_;
};

class ToolTipController final : public QObject {
public:
    explicit ToolTipController(QObject* parent) : QObject(parent)
    {
        timeout_.setSingleShot(true);
        connect(&timeout_, &QTimer::timeout, this, [this] { hide(); });
    }
    void show(const QPoint& point, const QString& value, QWidget* owner, const QRect& region, int duration)
    {
        if (!owner || value.isEmpty()) { hide(); return; }
        timeout_.stop();
        if (owner_ != owner || !popup_) {
            if (popup_) delete popup_;
            disconnect(ownerDestroyed_);
            owner_ = owner;
            // A parentless Ela tooltip does not install its uncancellable
            // Enter/Leave timers. Ownership is assigned only afterwards.
            popup_ = new ElaToolTip;
            popup_->setParent(owner, Qt::ToolTip | Qt::FramelessWindowHint);
            popup_->setObjectName(QStringLiteral("WaveElaToolTip"));
            popup_->setAttribute(Qt::WA_ShowWithoutActivating);
            popup_->setFocusPolicy(Qt::NoFocus);
            body_ = new ToolTipText(popup_);
            popup_->setCustomWidget(body_);
            ownerDestroyed_ = connect(owner, &QObject::destroyed, this, [this] { hide(); });
        }
        region_ = region;
        value_ = value;
        auto* screen = QGuiApplication::screenAt(point);
        if (!screen) screen = owner->screen();
        const QRect available = screen->availableGeometry().adjusted(4, 4, -4, -4);
        const auto theme = waveformTheme(waveformColorScheme(owner->palette()));
        body_->setContent(value, owner->font(), theme.text,
            QSize(std::max(1, std::min(520, available.width() - 48)),
                  std::max(1, available.height() - 48)));
        popup_->layout()->activate();
        popup_->adjustSize();
        popup_->setGeometry(boundedToolTipGeometry(point, popup_->sizeHint(), available));
        popup_->show();
        timeout_.start(duration < 0 ? std::clamp(2000 + int(value.size()) * 40, 5000, 12000) : duration);
    }
    void hide()
    {
        timeout_.stop();
        if (popup_) popup_->hide();
        value_.clear();
    }
    bool belongsTo(QWidget* widget) const
    {
        return owner_ && (owner_ == widget || widget->isAncestorOf(owner_));
    }
    void moved(QWidget* widget, const QPoint& point)
    {
        if (owner_ == widget && !region_.isNull() && !region_.contains(point)) hide();
    }
    QString value() const { return value_; }
    bool visible() const { return popup_ && popup_->isVisible(); }
private:
    QPointer<QWidget> owner_;
    QPointer<ElaToolTip> popup_;
    QPointer<ToolTipText> body_;
    QTimer timeout_;
    QMetaObject::Connection ownerDestroyed_;
    QRect region_;
    QString value_;
};

ToolTipController* toolTips()
{
    static QPointer<ToolTipController> controller;
    if (!controller) controller = new ToolTipController(qApp);
    return controller;
}

class ToolTipScope final : public QObject {
public:
    explicit ToolTipScope(QWidget* root) : QObject(root), root_(root) { watch(root); }
private:
    void watch(QWidget* widget)
    {
        if (!widget || qobject_cast<QFileDialog*>(widget) || watched_.contains(widget)) return;
        watched_.insert(widget);
        widget->installEventFilter(this);
        connect(widget, &QObject::destroyed, this, [this, widget] { watched_.remove(widget); });
        for (auto* child : widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) watch(child);
    }
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget) return false;
        if (event->type() == QEvent::ChildAdded || event->type() == QEvent::ChildPolished) {
            QPointer<QObject> child = static_cast<QChildEvent*>(event)->child();
            QTimer::singleShot(0, this, [this, child] { if (child) watch(qobject_cast<QWidget*>(child)); });
        } else if (event->type() == QEvent::ToolTip) {
            const auto* help = static_cast<QHelpEvent*>(event);
            auto value = widget->toolTip();
            QRect region;
            if (auto* view = qobject_cast<QAbstractItemView*>(widget->parentWidget());
                view && widget == view->viewport()) {
                const auto index = view->indexAt(help->pos());
                const auto itemTip = index.data(Qt::ToolTipRole).toString();
                if (!itemTip.isEmpty()) { value = itemTip; region = view->visualRect(index); }
            }
            if (!value.isEmpty()) {
                showToolTip(help->globalPos(), value, widget, region, widget->toolTipDuration());
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            toolTips()->moved(widget, static_cast<QMouseEvent*>(event)->position().toPoint());
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide
            || event->type() == QEvent::Close || event->type() == QEvent::WindowDeactivate) {
            if (toolTips()->belongsTo(widget)) hideToolTip();
        } else if (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::Wheel) {
            if (toolTips()->belongsTo(root_)) hideToolTip();
        }
        return false;
    }
    QWidget* root_;
    QSet<QWidget*> watched_;
};

class NotificationController final : public QObject {
public:
    explicit NotificationController(QWidget* host) : QObject(host), host_(host)
    {
        setObjectName(QStringLiteral("WaveNotificationController"));
        bar_ = ElaMessageBar::createManaged(host);
        bar_->setObjectName(QStringLiteral("WaveNotification"));
        bar_->setProperty("waveEla", true);
        auto* layout = new QHBoxLayout(bar_);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(8);
        symbol_ = text(bar_); symbol_->setFixedSize(24, 24);
        layout->addWidget(symbol_);
        auto* content = new QVBoxLayout;
        content->setSpacing(2);
        title_ = text(bar_); body_ = text(bar_);
        title_->setObjectName(QStringLiteral("WaveNotificationTitle"));
        body_->setObjectName(QStringLiteral("WaveNotificationText"));
        for (auto* label : {title_, body_}) {
            label->setTextFormat(Qt::PlainText);
            label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }
        content->addWidget(title_); content->addWidget(body_);
        layout->addLayout(content, 1);
        auto* close = toolButton(bar_);
        close->setObjectName(QStringLiteral("WaveNotificationClose"));
        close->setAccessibleName(QApplication::translate("Notification", "Dismiss notification"));
        close->setToolTip(close->accessibleName());
        close->setIcon(icon(Icon::Close, bar_));
        close->setFixedSize(28, 28);
        close->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(close);
        connect(close, &QToolButton::clicked, bar_, &ElaMessageBar::closeRequested);
        connect(bar_, &ElaMessageBar::closeRequested, this, [this] { dismiss(); });
        timer_.setSingleShot(true);
        connect(&timer_, &QTimer::timeout, this, [this] { dismiss(); });
        installToolTips(bar_);
        host->installEventFilter(this);
    }
    void show(Notice kind, const QString& title, const QString& message, int duration)
    {
        if (bar_->isVisible() && kind_ == kind && titleValue_ == title && bodyValue_ == message) return;
        timer_.stop();
        if (toolTips()->belongsTo(bar_)) hideToolTip();
        kind_ = kind; titleValue_ = title; bodyValue_ = message;
        bar_->setAccessibleName(title);
        bar_->setAccessibleDescription(message);
        body_->setAccessibleName(message);
        body_->setToolTip(Qt::convertFromPlainText(message));
        if (!refresh()) { dismiss(); return; }
        bar_->show(); bar_->raise();
        timer_.start(std::clamp(duration, 1, 30000));
    }
    void dismiss() { timer_.stop(); bar_->hide(); }
private:
    bool refresh()
    {
        const auto theme = waveformTheme(waveformColorScheme(host_->palette()));
        const auto accent = kind_ == Notice::Success ? theme.success
            : kind_ == Notice::Warning ? theme.warning : theme.information;
        const auto surface = kind_ == Notice::Success ? theme.successSurface
            : kind_ == Notice::Warning ? theme.warningSurface : theme.informationSurface;
        auto palette = host_->palette();
        palette.setColor(QPalette::Window, surface);
        palette.setColor(QPalette::WindowText, theme.text);
        palette.setColor(QPalette::ButtonText, accent);
        palette.setColor(QPalette::Mid, accent);
        bar_->setPalette(palette);
        bar_->setFont(host_->font());
        auto titleFont = host_->font(); titleFont.setBold(true); title_->setFont(titleFont);
        const int width = std::min(520, host_->width() - 24);
        const int height = title_->fontMetrics().height() + body_->fontMetrics().height() + 26;
        if (!host_->isVisible() || width < 180 || height + 24 > host_->height()) return false;
        const int textWidth = width - 92;
        title_->setText(title_->fontMetrics().elidedText(titleValue_, Qt::ElideRight, textWidth));
        body_->setText(body_->fontMetrics().elidedText(bodyValue_.simplified(), Qt::ElideMiddle, textWidth));
        symbol_->setPixmap(icon(kind_ == Notice::Success ? Icon::Success
            : kind_ == Notice::Warning ? Icon::Warning : Icon::Information, bar_)
            .pixmap(QSize(20, 20), bar_->devicePixelRatioF()));
        bar_->setFixedSize(width, height);
        const int x = host_->layoutDirection() == Qt::RightToLeft ? 12 : host_->width() - width - 12;
        bar_->move(x, host_->height() - height - 12);
        return true;
    }
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::Hide || event->type() == QEvent::Close) dismiss();
        else if (bar_->isVisible() && (event->type() == QEvent::Resize
            || event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange
            || event->type() == QEvent::LayoutDirectionChange || event->type() == QEvent::DevicePixelRatioChange)) {
            if (!refresh()) dismiss();
        }
        return false;
    }
    QWidget* host_;
    ElaMessageBar* bar_;
    QLabel* symbol_;
    QLabel* title_;
    QLabel* body_;
    QTimer timer_;
    Notice kind_{Notice::Information};
    QString titleValue_, bodyValue_;
};

QWidget* notificationHost(QWidget* owner)
{
    if (auto* window = qobject_cast<QMainWindow*>(owner); window && window->centralWidget())
        return window->centralWidget();
    return owner;
}

NotificationController* notificationController(QWidget* host)
{
    for (auto* child : host->children())
        if (auto* controller = dynamic_cast<NotificationController*>(child)) return controller;
    return nullptr;
}

} // namespace

void notify(QWidget* owner, Notice kind, const QString& title, const QString& message, int milliseconds)
{
    auto* host = notificationHost(owner);
    if (!host || !host->isVisible()) return;
    initialize();
    auto* controller = notificationController(host);
    if (!controller) controller = new NotificationController(host);
    controller->show(kind, title, message, milliseconds);
}

void dismissNotification(QWidget* owner)
{
    if (auto* host = notificationHost(owner))
        if (auto* controller = notificationController(host)) controller->dismiss();
}

QLabel* text(QWidget* parent)
{
    initialize();
    auto* label = new ElaText(parent, true);
    label->setProperty("waveEla", true);
    return label;
}
QLabel* text(const QString& value, QWidget* parent)
{
    auto* label = text(parent); label->setText(value); return label;
}

void FormLayout::addRow(const QString& label, QWidget* field)
{
    auto* caption = text(label, parentWidget());
    caption->setBuddy(field);
    QFormLayout::addRow(caption, field);
}
void FormLayout::addRow(const QString& label, QLayout* field)
{
    QFormLayout::addRow(text(label, parentWidget()), field);
}

QWidget* colorField(QLineEdit* editor, QWidget* parent)
{
    auto* field = new QWidget(parent);
    auto* layout = new QHBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(editor, 1);
    field->setFocusProxy(editor);
    auto* choose = button(QApplication::translate("ColorField", "Choose…"), field);
    choose->setObjectName(editor->objectName() + QStringLiteral("ChooseButton"));
    choose->setAccessibleName(QApplication::translate("ColorField", "Choose color"));
    choose->setAutoDefault(false);
    layout->addWidget(choose);
    QPointer<QLineEdit> safeEditor(editor);
    QObject::connect(choose, &QPushButton::clicked, field, [safeEditor] {
        if (!safeEditor || safeEditor->isReadOnly() || !safeEditor->isEnabled()) return;
        hideToolTip();
        const QColor initial(safeEditor->text().trimmed());
        QPointer<QWidget> focus = QApplication::focusWidget();
        QPointer<ElaColorDialog> dialog = new ElaColorDialog(safeEditor->window(), true);
        dialog->setObjectName(QStringLiteral("WaveColorDialog"));
        dialog->setWindowTitle(QApplication::translate("ColorField", "Choose color"));
        dialog->setWindowModality(Qt::WindowModal);
        dialog->setCurrentColor(initial.isValid() ? initial : QColor(Qt::black));
        dialog->resize(QSize(720, 680).boundedTo(safeEditor->screen()->availableGeometry().size() - QSize(48, 80)));
        for (auto* area : dialog->findChildren<QAbstractScrollArea*>()) installScrollBars(area);
        installToolTips(dialog);
        const bool accepted = dialog->exec() == QDialog::Accepted;
        if (!dialog) return;
        QColor result = dialog->getCurrentColor().toRgb();
        delete dialog;
        if (accepted && result.isValid() && safeEditor) {
            if (initial.isValid()) result.setAlpha(initial.alpha());
            safeEditor->selectAll();
            safeEditor->insert(result.name(result.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb));
        }
        if (focus && focus->isVisible()) focus->setFocus(Qt::OtherFocusReason);
    });
    return field;
}

void installToolTips(QWidget* root)
{
    if (root->property("waveToolTips").toBool()) return;
    root->setProperty("waveToolTips", true);
    new ToolTipScope(root);
}

QRect boundedToolTipGeometry(const QPoint& position, const QSize& size, const QRect& available)
{
    if (!available.isValid()) return {};
    const QSize fitted = size.expandedTo({1, 1}).boundedTo(available.size());
    QPoint origin = position + QPoint(12, 18);
    if (origin.y() + fitted.height() > available.bottom() + 1)
        origin.setY(position.y() - fitted.height() - 8);
    origin.setX(std::clamp(origin.x(), available.left(), available.right() - fitted.width() + 1));
    origin.setY(std::clamp(origin.y(), available.top(), available.bottom() - fitted.height() + 1));
    return {origin, fitted};
}

void showToolTip(const QPoint& position, const QString& value, QWidget* owner, const QRect& region, int milliseconds)
{
    initialize(); toolTips()->show(position, value, owner, region, milliseconds);
}
void hideToolTip() { toolTips()->hide(); }
QString toolTipText() { return toolTips()->value(); }
bool toolTipVisible() { return toolTips()->visible(); }

} // namespace wave::ui
