#include "ui_controls.h"

#include "ElaApplication.h"
#include "ElaAppBar.h"
#include "ElaCheckBox.h"
#include "ElaComboBox.h"
#include "ElaComboBoxStyle.h"
#include "ElaDoubleSpinBox.h"
#include "ElaLineEdit.h"
#include "ElaLineEditStyle.h"
#include "ElaListView.h"
#include "ElaMenu.h"
#include "ElaMenuBar.h"
#include "ElaProgressBar.h"
#include "ElaPushButton.h"
#include "ElaScrollBar.h"
#include "ElaSpinBox.h"
#include "ElaStatusBar.h"
#include "ElaTabBarStyle.h"
#include "ElaTableViewStyle.h"
#include "ElaTheme.h"
#include "ElaToolBar.h"
#include "ElaToolButton.h"
#include "ElaToolButtonStyle.h"
#include "ElaTreeViewStyle.h"

#include <QAbstractItemView>
#include <QActionGroup>
#include <QActionEvent>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMainWindow>
#include <QPainter>
#include <QPointer>
#include <QProxyStyle>
#include <QShowEvent>
#include <QSettings>
#include <QStyleFactory>
#include <QStyleHints>
#include <QStyleOption>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QWheelEvent>

#include <algorithm>
#include <memory>

namespace wave::ui {
namespace {

WaveformTheme colors(const QWidget* widget)
{
    return waveformTheme(waveformColorScheme(widget->palette()));
}

int controlHeight(const QWidget* widget)
{
    const auto metrics = waveformMetrics();
    const int minimum = widget->property("waveDensity") == QStringLiteral("compact")
        ? metrics.compactControlHeight : metrics.controlHeight;
    return std::max(minimum, widget->fontMetrics().height() + 12);
}

QString windowStyleSheet(const QWidget* window, WaveformColorScheme scheme)
{
    // A scoped stylesheet otherwise stops Qt's font inheritance at the root.
    // Typography is explicit; no generic border/background rules touch Ela.
    const auto font = window->parentWidget() ? window->parentWidget()->font() : window->font();
    auto family = font.family();
    family.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    family.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    const auto size = font.pointSizeF() > 0
        ? QString::number(font.pointSizeF()) + QStringLiteral("pt")
        : QString::number(font.pixelSize()) + QStringLiteral("px");
    return waveApplicationStyleSheet(scheme)
        + QStringLiteral("\nQWidget { font-family: \"%1\"; font-size: %2; }\n").arg(family, size)
        + QStringLiteral("QLineEdit#LaneRenameEdit { font-size: %1pt; font-weight: 500; }\n"
                         "QLineEdit#LaneRenameEdit[waveGroupName=\"true\"] { font-weight: 600; }\n")
              .arg(signalNameFont(font).pointSizeF());
}

void paintState(QWidget* widget)
{
    const auto theme = colors(widget);
    QColor outline;
    Qt::PenStyle penStyle = Qt::SolidLine;
    if (widget->property("waveState") == QStringLiteral("error")
        || widget->property("invalidDraft").toBool()) outline = theme.error;
    else if (widget->property("waveRole") == QStringLiteral("danger")) outline = theme.error;
    else if (widget->property("relationRisk").toBool()) outline = theme.warning;
    else if (widget->property("noEffect").toBool()
        || widget->property("loadedExisting").toBool()) {
        outline = theme.mutedText;
        penStyle = Qt::DashLine;
    } else if (widget->hasFocus()) outline = theme.focus;
    if (!outline.isValid()) return;
    QPainter painter(widget);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(outline, 2, penStyle));
    painter.drawRoundedRect(widget->rect().adjusted(2, 2, -2, -2), 5, 5);
}

template<class Control>
class AccessibleControl : public Control {
public:
    using Control::Control;
    QSize sizeHint() const override
    {
        auto size = Control::sizeHint();
        size.setHeight(std::max(size.height(), controlHeight(this)));
        return size;
    }
    QSize minimumSizeHint() const override
    {
        auto size = Control::minimumSizeHint();
        size.setHeight(std::max(size.height(), controlHeight(this)));
        return size;
    }
protected:
    void paintEvent(QPaintEvent* event) override
    {
        Control::paintEvent(event);
        paintState(this);
    }
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::DynamicPropertyChange) {
            this->updateGeometry();
            this->update();
        }
        return Control::event(event);
    }
};

template<class Control>
Control* prepare(Control* control)
{
    control->setProperty("waveEla", true);
    control->setObjectName({});
    control->setStyleSheet({});
    // An unresolved font continues to inherit when a parentless control is
    // subsequently inserted into a host layout or the host font changes.
    auto inheritedFont = control->font();
    inheritedFont.setResolveMask(0);
    control->setFont(inheritedFont);
    control->setMinimumSize(0, 0);
    control->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, control,
        [control] { control->updateGeometry(); control->update(); });
    return control;
}

void showEditMenu(QLineEdit* edit, QContextMenuEvent* event)
{
    std::unique_ptr<QMenu> standard(edit->createStandardContextMenu());
    ElaMenu popup(edit);
    popup.setFont(edit->font());
    popup.setMenuItemHeight(controlHeight(edit));
    // QAction ownership stays with the standard menu until exec returns.
    popup.addActions(standard->actions());
    popup.exec(event->globalPos());
}

class LineEdit final : public AccessibleControl<ElaLineEdit> {
public:
    using AccessibleControl::AccessibleControl;
protected:
    // Immediate native editing also honors reduced-motion preferences.
    void focusInEvent(QFocusEvent* event) override { QLineEdit::focusInEvent(event); }
    void focusOutEvent(QFocusEvent* event) override { QLineEdit::focusOutEvent(event); }
    void contextMenuEvent(QContextMenuEvent* event) override { showEditMenu(this, event); }
};

class SpinBox final : public AccessibleControl<ElaSpinBox> {
public:
    using AccessibleControl::AccessibleControl;
protected:
    void focusInEvent(QFocusEvent* event) override { QSpinBox::focusInEvent(event); }
    void focusOutEvent(QFocusEvent* event) override { QSpinBox::focusOutEvent(event); }
    void contextMenuEvent(QContextMenuEvent* event) override { showEditMenu(lineEdit(), event); }
};

class ComboBox final : public AccessibleControl<ElaComboBox> {
public:
    explicit ComboBox(QWidget* parent) : AccessibleControl(parent)
    {
        for (auto* bar : findChildren<ElaScrollBar*>()) bar->setIsAnimation(false);
    }
    void showPopup() override { QComboBox::showPopup(); }
    void hidePopup() override { QComboBox::hidePopup(); }
};

class DoubleSpinBox final : public AccessibleControl<ElaDoubleSpinBox> {
public:
    using AccessibleControl::AccessibleControl;
protected:
    void focusInEvent(QFocusEvent* event) override { QDoubleSpinBox::focusInEvent(event); }
    void focusOutEvent(QFocusEvent* event) override { QDoubleSpinBox::focusOutEvent(event); }
    void contextMenuEvent(QContextMenuEvent* event) override { showEditMenu(lineEdit(),event); }
};

class ScrollBar final : public ElaScrollBar {
public:
    ScrollBar(Qt::Orientation orientation, QAbstractScrollArea* area)
        : ElaScrollBar(orientation, area), area_(area)
    {
        setIsAnimation(false);
        setProperty("waveNativeScroll", true);
        setProperty("waveEla", true);
        QObject::connect(eTheme, &ElaTheme::themeModeChanged, this, [this] { update(); });
    }
protected:
    void wheelEvent(QWheelEvent* event) override
    {
        // Ela animates wheel values even when its range-animation flag is off.
        // Keep Qt's immediate page steps, fractional deltas and RTL semantics.
        QScrollBar::wheelEvent(event);
    }
    void paintEvent(QPaintEvent* event) override
    {
        const auto requested = area_->property("themeName").toString();
        const auto scheme = requested == QLatin1String("dark") ? WaveformColorScheme::Dark
            : requested == QLatin1String("light") ? WaveformColorScheme::Light
            : waveformColorScheme(area_->palette());
        setProperty("waveColorScheme", scheme == WaveformColorScheme::Dark ? "dark" : "light");
        ElaScrollBar::paintEvent(event);
    }
private:
    QAbstractScrollArea* area_;
};

class CompletionList final : public ElaListView {
public:
    explicit CompletionList(QLineEdit* editor) : editor_(editor)
    {
        editor_->installEventFilter(this);
        setItemHeight(controlHeight(this));
    }
    ~CompletionList() override
    {
        // The base owns its style; detach it before the base deletes that style.
        setStyle(nullptr);
    }
    void syncEditorStyle()
    {
        if (!editor_) return;
        setFont(editor_->font());
        setPalette(editor_->palette());
        setLayoutDirection(editor_->layoutDirection());
        setItemHeight(controlHeight(this));
    }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == editor_ && (event->type() == QEvent::FontChange
            || event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange
            || event->type() == QEvent::LayoutDirectionChange)) syncEditorStyle();
        return ElaListView::eventFilter(watched, event);
    }
    void showEvent(QShowEvent* event) override
    {
        syncEditorStyle();
        ElaListView::showEvent(event);
    }
    void changeEvent(QEvent* event) override
    {
        ElaListView::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange)
            setItemHeight(controlHeight(this));
    }
private:
    QPointer<QLineEdit> editor_;
};

class ToolButton final : public AccessibleControl<ElaToolButton> {
public:
    using AccessibleControl::AccessibleControl;
    void refreshSelection()
    {
        setIsSelected(isChecked() || property("waveRole") == QStringLiteral("primary"));
    }
protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::DynamicPropertyChange) refreshSelection();
        return AccessibleControl::event(event);
    }
};

class Button final : public AccessibleControl<ElaPushButton> {
public:
    using AccessibleControl::AccessibleControl;
    QSize sizeHint() const override
    {
        const auto image = icon().isNull() ? QSize(0, 0) : iconSize();
        return {fontMetrics().size(Qt::TextShowMnemonic, text()).width()
                    + image.width() + (image.isEmpty() ? 0 : 6) + 28,
                std::max(controlHeight(this), image.height() + 12)};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }
    void refreshColors()
    {
        const auto light = waveformTheme(WaveformColorScheme::Light);
        const auto dark = waveformTheme(WaveformColorScheme::Dark);
        const bool primary = property("waveRole") == QStringLiteral("primary");
        const bool danger = property("waveRole") == QStringLiteral("danger");
        setLightDefaultColor(primary ? light.selection : light.raised);
        setDarkDefaultColor(primary ? dark.selection : dark.raised);
        setLightHoverColor(light.selection);
        setDarkHoverColor(dark.selection);
        setLightPressColor(light.selection);
        setDarkPressColor(dark.selection);
        setLightTextColor(danger ? light.error : light.text);
        setDarkTextColor(danger ? dark.error : dark.text);
    }
protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::DynamicPropertyChange) refreshColors();
        return AccessibleControl::event(event);
    }
};

// Qt standard dialogs retain their private standard-button maps. The label is
// still drawn by Qt (mnemonics, icons, RTL); the surface uses the Ela tokens.
class DialogButtonStyle final : public QProxyStyle {
public:
    DialogButtonStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}
    void drawControl(ControlElement element, const QStyleOption* option,
        QPainter* painter, const QWidget* widget) const override
    {
        if (element != CE_PushButton) {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }
        const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option);
        if (!button) return;
        const auto theme = colors(widget);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const bool active = option->state & (State_MouseOver | State_Sunken | State_On);
        const bool focus = option->state & State_HasFocus;
        painter->setPen(QPen(focus ? theme.focus : theme.border, focus ? 2 : 1));
        painter->setBrush(active ? theme.selection : theme.raised);
        painter->drawRoundedRect(option->rect.adjusted(1, 1, -1, -1), 6, 6);
        if (button->features & QStyleOptionButton::DefaultButton) {
            painter->setPen(theme.accent);
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(option->rect.adjusted(3, 3, -3, -3), 4, 4);
        }
        painter->restore();
        QProxyStyle::drawControl(CE_PushButtonLabel, option, painter, widget);
    }
    QSize sizeFromContents(ContentsType type, const QStyleOption* option,
        const QSize& size, const QWidget* widget) const override
    {
        auto result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == CT_PushButton) result.setHeight(std::max(result.height(), controlHeight(widget)));
        return result;
    }
};

template<class Style>
void attachStyle(QWidget* widget, Style* style)
{
    style->setParent(widget);
    style->setProperty("waveEla", true);
    widget->setStyle(style);
    widget->setProperty("waveEla", true);
}

void styleDialogButtons(QWidget* parent)
{
    for (auto* button : parent->findChildren<QPushButton*>()) {
        if (qobject_cast<ElaPushButton*>(button)) continue;
        auto* style = button->findChild<QStyle*>(QStringLiteral("WaveDialogButtonStyle"), Qt::FindDirectChildrenOnly);
        if (!style) {
            style = new DialogButtonStyle;
            style->setObjectName(QStringLiteral("WaveDialogButtonStyle"));
        }
        attachStyle(button, style);
        button->setMinimumHeight(controlHeight(button));
    }
}

class DialogButtonObserver final : public QObject {
public:
    explicit DialogButtonObserver(QDialogButtonBox* box) : QObject(box), box_(box)
    {
        box_->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::StyleChange || event->type() == QEvent::Show) {
            // QDialogButtonBox reapplies its own style to standard buttons.
            // Restore the local surface after that event, without replacing roles.
            QTimer::singleShot(0, box_, [box = box_] { styleDialogButtons(box); });
        }
        return false;
    }
private:
    QDialogButtonBox* box_;
};

template<class ElaStyle>
class ItemStyle final : public ElaStyle {
public:
    void drawControl(QStyle::ControlElement element, const QStyleOption* option,
        QPainter* painter, const QWidget* widget) const override
    {
        if (element == QStyle::CE_ItemViewItem || element == QStyle::CE_HeaderLabel) {
            // Preserve delegate-provided font, foreground/background, check and
            // decoration roles; Ela's item text painter discards these roles.
            QProxyStyle::drawControl(element, option, painter, widget);
        } else ElaStyle::drawControl(element, option, painter, widget);
    }
    void drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption* option,
        QPainter* painter, const QWidget* widget) const override
    {
        if (element == QStyle::PE_PanelItemViewItem) {
            // ElaTableViewStyle assumes ElaTableView and skips QTableWidget.
            // Qt's panel painter retains selected and model-background colors.
            QProxyStyle::drawPrimitive(element, option, painter, widget);
        } else ElaStyle::drawPrimitive(element, option, painter, widget);
    }
    QSize sizeFromContents(QStyle::ContentsType type, const QStyleOption* option,
        const QSize& size, const QWidget* widget) const override
    {
        auto result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == QStyle::CT_ItemViewItem && widget)
            result.setHeight(std::max(result.height(), controlHeight(widget)));
        return result;
    }
};

void prepareView(QAbstractItemView* view)
{
    view->setProperty("waveEla", true);
    installScrollBars(view);
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, view,
        [view] { view->viewport()->update(); });
}

class TabStyle final : public ElaTabBarStyle {
public:
    void drawControl(ControlElement element, const QStyleOption* option,
        QPainter* painter, const QWidget* widget) const override
    {
        if (element == CE_TabBarTabLabel) QProxyStyle::drawControl(element, option, painter, widget);
        else ElaTabBarStyle::drawControl(element, option, painter, widget);
    }
    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
        QPainter* painter, const QWidget* widget) const override
    {
        if (element == PE_IndicatorArrowLeft || element == PE_IndicatorArrowRight)
            QProxyStyle::drawPrimitive(element, option, painter, widget);
        else ElaTabBarStyle::drawPrimitive(element, option, painter, widget);
    }
    QSize sizeFromContents(ContentsType type, const QStyleOption* option,
        const QSize& size, const QWidget* widget) const override
    {
        auto result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == CT_TabBarTab) result.setHeight(std::max(result.height(), controlHeight(widget)));
        return result;
    }
    QRect subElementRect(SubElement element, const QStyleOption* option,
        const QWidget* widget) const override
    {
        return QProxyStyle::subElementRect(element, option, widget);
    }
};

class ToolBar final : public ElaToolBar {
public:
    using ElaToolBar::ElaToolBar;
protected:
    void actionEvent(QActionEvent* event) override
    {
        ElaToolBar::actionEvent(event);
        auto* button = qobject_cast<QToolButton*>(widgetForAction(event->action()));
        if (!button || button->property("waveEla").toBool()) return;
        auto* style = new ElaToolButtonStyle;
        style->setBorderRadius(6);
        style->setIsSelected(button->isChecked());
        attachStyle(button, style);
        button->setMinimumHeight(controlHeight(button));
        QObject::connect(button, &QToolButton::toggled, button, [button, style](bool checked) {
            style->setIsSelected(checked); button->update();
        });
        QObject::connect(eTheme, &ElaTheme::themeModeChanged, button, [button] { button->update(); });
    }
};

class EmbeddedThemeObserver final : public QObject {
public:
    explicit EmbeddedThemeObserver(QMainWindow* window) : QObject(window), window_(window)
    {
        window_->installEventFilter(this);
        if (window_->parentWidget()) window_->parentWidget()->installEventFilter(this);
        updateTheme();
    }
protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::PaletteChange
            || event->type() == QEvent::ApplicationPaletteChange
            || event->type() == QEvent::FontChange) updateTheme();
        return false;
    }
private:
    void updateTheme()
    {
        if (qApp->property("waveworkbench.standalone").toBool()) return;
        const auto scheme = waveformColorScheme(window_->parentWidget()
            ? window_->parentWidget()->palette() : qApp->palette());
        const int key = static_cast<int>(scheme);
        const auto font = window_->parentWidget() ? window_->parentWidget()->font() : qApp->font();
        if (lastScheme_ == key && lastFont_ == font) return;
        lastScheme_ = key;
        lastFont_ = font;
        window_->setFont(font);
        applyTheme(scheme);
        window_->setStyleSheet(windowStyleSheet(window_, scheme));
    }
    QMainWindow* window_;
    int lastScheme_{-1};
    QFont lastFont_;
};

} // namespace

void initialize()
{
    static const bool initialized = [] {
        eApp->initResources();
        applyTheme(waveformColorScheme(QApplication::palette()));
        return true;
    }();
    Q_UNUSED(initialized);
}

void applyTheme(const WaveformColorScheme scheme)
{
    const auto set = [](const ElaThemeType::ThemeMode mode, const WaveformTheme& t) {
        using namespace ElaThemeType;
        const auto color = [mode](ThemeColor key, const QColor& value) {
            eTheme->setThemeColor(mode, key, value);
        };
        color(WindowBase, t.application); color(WindowCentralStackBase, t.canvas);
        color(PrimaryNormal, t.accent); color(PrimaryHover, t.accentSecondary); color(PrimaryPress, t.focus);
        color(PopupBase, t.raised); color(PopupHover, t.selection);
        color(PopupBorder, t.border); color(PopupBorderHover, t.focus);
        color(DialogBase, t.raised); color(DialogLayoutArea, t.panel);
        color(BasicText, t.text); color(BasicTextInvert, t.selectionText);
        color(BasicDetailsText, t.mutedText); color(BasicTextNoFocus, t.mutedText);
        color(BasicTextDisable, t.mutedText); color(BasicTextPress, t.text); color(BasicTextCategory, t.mutedText);
        color(BasicBorder, t.border); color(BasicBorderDeep, t.border); color(BasicBorderHover, t.accentSecondary);
        color(BasicBase, t.raised); color(BasicBaseDeep, t.panel); color(BasicDisable, t.panel);
        color(BasicHover, t.selection); color(BasicPress, t.selection); color(BasicSelectedHover, t.selection);
        color(BasicBaseLine, t.border); color(BasicHemline, t.border); color(BasicIndicator, t.accent);
        color(BasicChute, t.panel); color(BasicAlternating, t.panel); color(BasicBaseAlpha, t.raised);
        color(BasicBaseDeepAlpha, t.panel); color(BasicHoverAlpha, t.selection); color(BasicPressAlpha, t.selection);
        color(BasicSelectedAlpha, t.selection); color(BasicSelectedHoverAlpha, t.selection);
        color(ScrollBarHandle, t.mutedText); color(StatusDanger, t.error);
    };
    set(ElaThemeType::Light, waveformTheme(WaveformColorScheme::Light));
    set(ElaThemeType::Dark, waveformTheme(WaveformColorScheme::Dark));
    eTheme->setThemeMode(scheme == WaveformColorScheme::Dark ? ElaThemeType::Dark : ElaThemeType::Light);
}

void prepareWindow(QMainWindow* window)
{
    initialize();
    installToolTips(window);
    window->setProperty("waveEla", true);
    if (window->parentWidget()) window->setFont(window->parentWidget()->font());
    window->setMenuBar(prepare(new ElaMenuBar(window)));
    auto* status = prepare(new ElaStatusBar(window));
    status->setContentsMargins(4, 0, 4, 0);
    status->setSizeGripEnabled(true);
    window->setStatusBar(status);
    window->setStyleSheet(windowStyleSheet(window, waveformColorScheme(window->palette())));
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, window,
        [window](ElaThemeType::ThemeMode mode) {
            window->setStyleSheet(windowStyleSheet(window, mode == ElaThemeType::Dark
                ? WaveformColorScheme::Dark : WaveformColorScheme::Light));
        });
    new EmbeddedThemeObserver(window);
}

QFont captionFont(const QFont& base)
{
    auto font = base;
    font.setPointSizeF(std::max(9.5,base.pointSizeF()-1));
    font.setWeight(QFont::Normal);
    return font;
}

QFont signalNameFont(const QFont& base, bool group)
{
    auto font = base;
    font.setPointSizeF(std::max(11.5, base.pointSizeF()));
    font.setWeight(group ? QFont::DemiBold : QFont::Medium);
    return font;
}

void installTitleMenus(QMainWindow* window)
{
    if (window->parentWidget() || !qApp->property("waveworkbench.standalone").toBool()) return;
    auto* original = window->menuBar();
    auto* bar = new ElaAppBar(window);
    bar->setObjectName("WaveTitleBar");
    bar->setAppBarHeight(40);
    bar->setWindowButtonFlags(ElaAppBarType::MinimizeButtonHint | ElaAppBarType::MaximizeButtonHint | ElaAppBarType::CloseButtonHint);
    auto* menus = prepare(new ElaMenuBar(bar));
    menus->setObjectName("TitleMenuBar");
    menus->addActions(original->actions());
    bar->setCustomWidget(ElaAppBarType::LeftArea, menus);
    original->hide();
    bar->show();
}

namespace {
void applyApplicationTheme()
{
    const auto preference = qApp->property("waveworkbench.themePreference").toString();
    const bool dark = preference == QStringLiteral("dark")
        || (preference != QStringLiteral("light")
            && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    const auto scheme = dark ? WaveformColorScheme::Dark : WaveformColorScheme::Light;
    const auto t = waveformTheme(scheme);
    auto palette = qApp->palette();
    palette.setColor(QPalette::Window, t.application);
    palette.setColor(QPalette::WindowText, t.text);
    palette.setColor(QPalette::Base, t.canvas);
    palette.setColor(QPalette::AlternateBase, t.panel);
    palette.setColor(QPalette::Text, t.text);
    palette.setColor(QPalette::Button, t.raised);
    palette.setColor(QPalette::ButtonText, t.text);
    palette.setColor(QPalette::Highlight, t.selection);
    palette.setColor(QPalette::HighlightedText, t.selectionText);
    palette.setColor(QPalette::Link, t.accentSecondary);
    palette.setColor(QPalette::LinkVisited, t.accent);
    palette.setColor(QPalette::BrightText, t.error);
    palette.setColor(QPalette::ToolTipBase, t.raised);
    palette.setColor(QPalette::ToolTipText, t.text);
    palette.setColor(QPalette::PlaceholderText, t.mutedText);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, t.mutedText);
    qApp->setPalette(palette);
    qApp->setProperty("waveworkbench.colorScheme", dark ? "dark" : "light");
    qApp->setStyleSheet(waveApplicationStyleSheet(scheme));
    applyTheme(scheme);
}
}

void initializeApplicationTheme()
{
    initialize();
    QFont font;
    font.setFamilies({QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Noto Sans CJK SC")});
    font.setPointSizeF(10.5);
    qApp->setFont(font);
    qApp->setProperty("waveworkbench.standalone", true);
    qApp->setProperty("waveworkbench.themePreference", QSettings().value("appearance/colorScheme", "system"));
    const auto metrics = waveformMetrics();
    qApp->setProperty("waveworkbench.spacingUnit", metrics.spacingUnit);
    qApp->setProperty("waveworkbench.controlHeight", metrics.controlHeight);
    qApp->setProperty("waveworkbench.radius", metrics.radius);
    qApp->setProperty("waveworkbench.reducedMotion", waveReducedMotionEnabled());
    applyApplicationTheme();
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
        qApp, [](Qt::ColorScheme) { applyApplicationTheme(); });
}

void addAppearanceMenu(QMainWindow* window)
{
    if (!qApp->property("waveworkbench.standalone").toBool()) return;
    auto* view = addMenu(window->menuBar(), QMainWindow::tr("&View"));
    auto* appearance = addMenu(view, QMainWindow::tr("&Appearance"));
    appearance->setObjectName(QStringLiteral("AppearanceMenu"));
    auto* group = new QActionGroup(appearance);
    const QStringList names{QMainWindow::tr("Follow &system"), QMainWindow::tr("&Light"), QMainWindow::tr("&Dark")};
    const QStringList values{QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};
    for (int i = 0; i < names.size(); ++i) {
        auto* action = appearance->addAction(names[i]);
        action->setObjectName(QStringLiteral("Appearance-") + values[i]);
        action->setCheckable(true); action->setData(values[i]); group->addAction(action);
        action->setChecked(qApp->property("waveworkbench.themePreference").toString() == values[i]);
    }
    QObject::connect(group, &QActionGroup::triggered, window, [](QAction* action) {
        qApp->setProperty("waveworkbench.themePreference", action->data());
        QSettings().setValue("appearance/colorScheme", action->data());
        applyApplicationTheme();
    });
}

QPushButton* button(QWidget* parent)
{
    initialize();
    auto* result = prepare(new Button(parent));
    result->refreshColors();
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, result, [result] { result->refreshColors(); });
    return result;
}
QPushButton* button(const QString& text, QWidget* parent) { auto* w = button(parent); w->setText(text); return w; }
QToolButton* toolButton(QWidget* parent)
{
    initialize();
    auto* w = prepare(new ToolButton(parent));
    w->setBorderRadius(6); w->setIsTransparent(false);
    w->setPopupMode(QToolButton::DelayedPopup); w->setIconSize({16, 16});
    QObject::connect(w, &QToolButton::toggled, w, [w] { w->refreshSelection(); });
    return w;
}
QLineEdit* lineEdit(QWidget* parent)
{
    initialize();
    auto* w = prepare(new LineEdit(parent));
    w->setTextMargins(8, 0, 8, 0);
    w->setIsClearButtonEnable(false);
    return w;
}
QLineEdit* lineEdit(const QString& text, QWidget* parent) { auto* w = lineEdit(parent); w->setText(text); return w; }
QComboBox* comboBox(QWidget* parent) { initialize(); return prepare(new ComboBox(parent)); }
QDoubleSpinBox* doubleSpinBox(QWidget* parent) { initialize(); return prepare(new DoubleSpinBox(parent)); }
QSpinBox* spinBox(QWidget* parent) { initialize(); return prepare(new SpinBox(parent)); }
QCheckBox* checkBox(QWidget* parent) { initialize(); return prepare(new AccessibleControl<ElaCheckBox>(parent)); }
QCheckBox* checkBox(const QString& text, QWidget* parent) { auto* w = checkBox(parent); w->setText(text); return w; }
QDialogButtonBox* buttonBox(QDialogButtonBox::StandardButtons buttons, QWidget* parent)
{
    auto* w = new QDialogButtonBox(buttons, parent);
    polishDialog(w);
    return w;
}
QMenu* menu(QWidget* parent)
{
    initialize();
    auto* w = new ElaMenu(parent);
    w->setProperty("waveEla", true);
    w->setMenuItemHeight(controlHeight(w));
    return w;
}
QMenu* addMenu(QMenu* parent, const QString& title)
{
    auto* w = menu(parent); w->setTitle(title); parent->addMenu(w); return w;
}
QMenu* addMenu(QMenuBar* parent, const QString& title)
{
    for (auto* action : parent->actions())
        if (action->menu() && action->text() == title) return action->menu();
    auto* w = menu(parent); w->setTitle(title); parent->addMenu(w); return w;
}
QToolBar* toolBar(const QString& title, QWidget* parent)
{
    initialize();
    auto* w = prepare(new ToolBar(title, parent));
    w->setToolBarSpacing(4);
    return w;
}
QToolBar* addToolBar(QMainWindow* parent, const QString& title)
{
    auto* w = toolBar(title, parent); parent->addToolBar(w); return w;
}
QToolBar* toolBar(QWidget* parent) { return toolBar(QString(), parent); }
QTabWidget* tabs(QWidget* parent)
{
    initialize();
    auto* w = new QTabWidget(parent);
    w->setProperty("waveEla", true);
    attachStyle(w->tabBar(), new TabStyle);
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, w, [w] { w->tabBar()->update(); });
    return w;
}
QTreeWidget* tree(QWidget* parent)
{
    initialize();
    auto* w = new QTreeWidget(parent);
    auto* style = new ItemStyle<ElaTreeViewStyle>;
    style->setItemHeight(controlHeight(w));
    attachStyle(w, style); w->header()->setStyle(style); prepareView(w);
    return w;
}
QTableWidget* table(QWidget* parent)
{
    initialize();
    auto* w = new QTableWidget(parent);
    auto* style = new ItemStyle<ElaTableViewStyle>;
    attachStyle(w, style); w->horizontalHeader()->setStyle(style); w->verticalHeader()->setStyle(style);
    w->verticalHeader()->setDefaultSectionSize(controlHeight(w)); prepareView(w);
    return w;
}
QProgressBar* progressBar(QWidget* parent) { initialize(); return prepare(new ElaProgressBar(parent)); }

void installScrollBars(QAbstractScrollArea* area)
{
    initialize();
    area->setHorizontalScrollBar(new ScrollBar(Qt::Horizontal, area));
    area->setVerticalScrollBar(new ScrollBar(Qt::Vertical, area));
}

QAbstractItemView* completionPopup(QLineEdit* editor)
{
    initialize();
    auto* popup = prepare(new CompletionList(editor));
    prepareView(popup);
    popup->syncEditorStyle();
    return popup;
}

void polishDialog(QWidget* dialog)
{
    initialize();
    installToolTips(dialog);
    dialog->setProperty("waveEla", true);
    auto boxes = dialog->findChildren<QDialogButtonBox*>();
    if (auto* box = qobject_cast<QDialogButtonBox*>(dialog)) boxes.append(box);
    for (auto* box : boxes) {
        if (!box->property("waveElaButtonsObserved").toBool()) {
            box->setProperty("waveElaButtonsObserved", true);
            new DialogButtonObserver(box);
        }
    }
    styleDialogButtons(dialog);
    for (auto* edit : dialog->findChildren<QLineEdit*>()) {
        if (!edit->property("waveEla").toBool() && !qobject_cast<QAbstractSpinBox*>(edit->parentWidget())) {
            attachStyle(edit, new ElaLineEditStyle);
            edit->setMinimumHeight(controlHeight(edit));
            edit->setTextMargins(8, 0, 8, 0);
        }
    }
    for (auto* combo : dialog->findChildren<QComboBox*>()) {
        if (!combo->property("waveEla").toBool()) {
            attachStyle(combo, new ElaComboBoxStyle);
            combo->setMinimumHeight(controlHeight(combo));
        }
    }
}

void Dialog::showEvent(QShowEvent* event) { QDialog::showEvent(event); polishDialog(this); }
void MessageBox::showEvent(QShowEvent* event) { QMessageBox::showEvent(event); polishDialog(this); }
void InputDialog::showEvent(QShowEvent* event) { QInputDialog::showEvent(event); polishDialog(this); }

namespace {
QMessageBox::StandardButton message(QWidget* parent, const QString& title,
    const QString& text, QMessageBox::Icon icon, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    MessageBox box(icon, title, text, buttons, parent);
    box.setDefaultButton(defaultButton);
    box.exec();
    return box.standardButton(box.clickedButton());
}
}
QMessageBox::StandardButton MessageBox::warning(QWidget* p, const QString& t, const QString& s, StandardButtons b, StandardButton d)
{ return message(p, t, s, Warning, b, d); }
QMessageBox::StandardButton MessageBox::critical(QWidget* p, const QString& t, const QString& s, StandardButtons b, StandardButton d)
{ return message(p, t, s, Critical, b, d); }
QMessageBox::StandardButton MessageBox::information(QWidget* p, const QString& t, const QString& s, StandardButtons b, StandardButton d)
{ return message(p, t, s, Information, b, d); }
QMessageBox::StandardButton MessageBox::question(QWidget* p, const QString& t, const QString& s, StandardButtons b, StandardButton d)
{ return message(p, t, s, Question, b, d); }

QString InputDialog::getText(QWidget* parent, const QString& title, const QString& label,
    QLineEdit::EchoMode mode, const QString& text, bool* ok, Qt::WindowFlags flags, Qt::InputMethodHints hints)
{
    InputDialog dialog(parent, flags);
    dialog.setWindowTitle(title); dialog.setLabelText(label);
    dialog.setTextEchoMode(mode); dialog.setTextValue(text); dialog.setInputMethodHints(hints);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    if (ok) *ok = accepted;
    return accepted ? dialog.textValue() : QString();
}
QString InputDialog::getItem(QWidget* parent, const QString& title, const QString& label,
    const QStringList& items, int current, bool editable, bool* ok, Qt::WindowFlags flags, Qt::InputMethodHints hints)
{
    InputDialog dialog(parent, flags);
    dialog.setWindowTitle(title); dialog.setLabelText(label);
    dialog.setComboBoxItems(items); dialog.setComboBoxEditable(editable);
    dialog.setTextValue(items.value(current)); dialog.setInputMethodHints(hints);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    if (ok) *ok = accepted;
    return accepted ? dialog.textValue() : QString();
}

} // namespace wave::ui
