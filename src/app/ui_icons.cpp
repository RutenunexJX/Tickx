#include "ui_controls.h"

#include "ElaIcon.h"

#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPointer>

#include <algorithm>
#include <array>
#include <cmath>

namespace wave::ui {
namespace {

// Font Awesome Free Solid 6.7.2 code points. The upstream enum also contains
// custom-font PUA assignments that are not present in our pinned Free font.
constexpr std::array<char32_t, static_cast<size_t>(Icon::Count)> glyphs{
    0xf15b, 0xf07c, 0xf0c7, 0xf56e, 0xf0e2, 0xf01e, 0xf062, 0xf063, 0xf1f8,
    0xf04b, 0xf363, 0xf04d, 0xf021, 0xf080, 0xf0ae, 0xf35d,
    0xf00e, 0xf010, 0xf065, 0xf547, 0xf0c1, 0x002b, 0xf068, 0xf00d, 0xf0c5, 0xf0ea,
    0xf058, 0xf05a, 0xf071
};

class IconEngine final : public QIconEngine {
public:
    IconEngine(char32_t glyph, QWidget* owner) : glyph_(glyph), owner_(owner) {}
    QIconEngine* clone() const override { return new IconEngine(*this); }
    QString key() const override { return QStringLiteral("WaveElaIcon"); }
    QString iconName() override { return QStringLiteral("wave-ela/%1").arg(uint(glyph_), 0, 16); }
    bool isNull() override { return false; }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State, qreal scale) override
    {
        if (size.isEmpty()) return {};
        const auto palette = owner_ ? owner_->palette() : QApplication::palette();
        const auto group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
        const auto color = palette.color(group,
            mode == QIcon::Selected ? QPalette::HighlightedText : QPalette::ButtonText);
        const QSize pixels(int(std::ceil(size.width() * scale)), int(std::ceil(size.height() * scale)));
        const auto glyphSize = std::max(1, int(std::min(pixels.width(), pixels.height()) * 0.82));
        const auto rendered = ElaIcon::getInstance()->getElaIcon(
            static_cast<ElaIconType::IconName>(glyph_), glyphSize, pixels.width(), pixels.height(), color);
        auto result = rendered.pixmap(pixels);
        result.setDevicePixelRatio(scale);
        return result;
    }
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const auto pixmap = scaledPixmap(rect.size(), mode, state, painter->device()->devicePixelRatioF());
        painter->drawPixmap(rect, pixmap);
    }
private:
    char32_t glyph_;
    QPointer<QWidget> owner_;
};

} // namespace

QIcon icon(Icon name, QWidget* owner)
{
    initialize();
    const auto index = static_cast<size_t>(name);
    return index < glyphs.size() ? QIcon(new IconEngine(glyphs[index], owner)) : QIcon{};
}

} // namespace wave::ui
