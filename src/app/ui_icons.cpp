#include "ui_controls.h"

#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>

#include <algorithm>
#include <cmath>

namespace wave::ui {
namespace {

// Shared visual language with ZeroSlack: a 24-unit grid, rounded strokes and
// palette-driven monochrome artwork. No font glyphs or raster assets are used.
void drawIcon(QPainter& painter, Icon name)
{
    const auto line = [&painter](qreal x1, qreal y1, qreal x2, qreal y2) {
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    };
    const auto outline = [&painter](std::initializer_list<QPointF> points, bool closed = false) {
        QPainterPath path;
        bool first = true;
        for (const auto& point : points) {
            if (first) path.moveTo(point);
            else path.lineTo(point);
            first = false;
        }
        if (closed) path.closeSubpath();
        painter.drawPath(path);
    };
    const auto box = [&painter](qreal x, qreal y, qreal width, qreal height) {
        painter.drawRoundedRect(QRectF(x, y, width, height), 1.8, 1.8);
    };
    const auto circle = [&painter](qreal x, qreal y, qreal radius) {
        painter.drawEllipse(QPointF(x, y), radius, radius);
    };
    const auto plus = [&line](qreal x, qreal y) {
        line(x - 3, y, x + 3, y); line(x, y - 3, x, y + 3);
    };
    switch (name) {
    case Icon::NewFile:
        outline({{5, 3}, {14, 3}, {19, 8}, {19, 21}, {5, 21}}, true);
        outline({{14, 3}, {14, 8}, {19, 8}}); plus(12, 14); break;
    case Icon::Open:
        outline({{3, 17}, {3, 5}, {9, 5}, {12, 8}, {20, 8}, {20, 10}});
        outline({{3, 20}, {6, 11}, {22, 11}, {19, 20}}, true); break;
    case Icon::Save:
        outline({{4, 3}, {17, 3}, {21, 7}, {21, 21}, {3, 21}, {3, 4}}, true);
        outline({{8, 3}, {8, 9}, {16, 9}, {16, 3}}); box(7, 14, 10, 7); break;
    case Icon::Export:
        outline({{4, 12}, {4, 21}, {20, 21}, {20, 12}});
        line(12, 3, 12, 16); outline({{7, 8}, {12, 3}, {17, 8}}); break;
    case Icon::Undo:
    case Icon::Redo: {
        if (name == Icon::Redo) { painter.translate(24, 0); painter.scale(-1, 1); }
        outline({{8, 4}, {3, 9}, {8, 14}});
        QPainterPath curve; curve.moveTo(3, 9); curve.lineTo(14, 9);
        curve.cubicTo(23, 9, 23, 20, 13, 20); painter.drawPath(curve); break;
    }
    case Icon::Up:
    case Icon::Down:
        if (name == Icon::Down) { painter.translate(0, 24); painter.scale(1, -1); }
        line(12, 21, 12, 3); outline({{6, 9}, {12, 3}, {18, 9}}); break;
    case Icon::Delete:
        line(3, 6, 21, 6); outline({{9, 6}, {9, 3}, {15, 3}, {15, 6}});
        outline({{5, 6}, {6, 21}, {18, 21}, {19, 6}});
        line(10, 10, 10, 17); line(14, 10, 14, 17); break;
    case Icon::Play: outline({{7, 3}, {21, 12}, {7, 21}}, true); break;
    case Icon::Repeat:
        outline({{3, 10}, {3, 6}, {20, 6}, {16, 2}});
        outline({{21, 14}, {21, 18}, {4, 18}, {8, 22}}); break;
    case Icon::Stop: box(5, 5, 14, 14); break;
    case Icon::Refresh:
        painter.drawArc(QRectF(4, 4, 16, 16), 55 * 16, 285 * 16);
        outline({{20, 3}, {20, 9}, {14, 9}}); break;
    case Icon::Compare:
        line(12, 3, 12, 21); outline({{3, 16}, {6, 16}, {6, 8}, {9, 8}});
        outline({{15, 16}, {18, 16}, {18, 11}, {21, 11}}); break;
    case Icon::Check:
        box(3, 3, 18, 18); outline({{7, 12}, {10, 15}, {17, 8}}); break;
    case Icon::Source:
        outline({{8, 5}, {2, 12}, {8, 19}});
        outline({{16, 5}, {22, 12}, {16, 19}}); line(14, 3, 10, 21); break;
    case Icon::ZoomIn: line(4, 12, 20, 12); line(12, 4, 12, 20); break;
    case Icon::ZoomOut: line(4, 12, 20, 12); break;
    case Icon::Fit:
        outline({{3, 9}, {3, 3}, {9, 3}}); outline({{15, 3}, {21, 3}, {21, 9}});
        outline({{21, 15}, {21, 21}, {15, 21}}); outline({{9, 21}, {3, 21}, {3, 15}}); break;
    case Icon::Measure:
        box(2, 7, 20, 10);
        for (int x = 6; x <= 18; x += 4) line(x, 7, x, x == 10 || x == 18 ? 13 : 11);
        break;
    case Icon::Link:
        circle(5, 17, 3); circle(19, 7, 3);
        outline({{5, 14}, {5, 7}, {16, 7}});
        outline({{19, 10}, {19, 17}, {8, 17}}); break;
    case Icon::Add: plus(12, 12); break;
    case Icon::Remove: line(5, 12, 19, 12); break;
    case Icon::Close: line(6, 6, 18, 18); line(18, 6, 6, 18); break;
    case Icon::Copy: box(8, 8, 13, 13); outline({{5, 16}, {3, 16}, {3, 3}, {16, 3}, {16, 5}}); break;
    case Icon::Paste:
        box(5, 5, 15, 17); box(9, 2, 7, 5); line(9, 12, 16, 12); line(9, 16, 14, 16); break;
    case Icon::Success: circle(12, 12, 9); outline({{7, 12}, {10, 15}, {17, 8}}); break;
    case Icon::Information:
        circle(12, 12, 9); line(12, 11, 12, 17); line(12, 7, 12, 7.2); break;
    case Icon::Warning:
        outline({{12, 3}, {22, 21}, {2, 21}}, true);
        line(12, 9, 12, 14); line(12, 17.5, 12, 18); break;
    case Icon::Sync:
        outline({{2, 15}, {6, 15}, {6, 6}, {12, 6}, {12, 15}, {18, 15}, {18, 6}, {22, 6}});
        line(6, 19, 6, 21); line(12, 19, 12, 21); line(18, 19, 18, 21); break;
    case Icon::Async:
        outline({{2, 15}, {8, 15}, {8, 6}, {15, 6}, {15, 15}, {22, 15}});
        outline({{4, 20}, {20, 20}, {17, 17}}); break;
    case Icon::AddClock:
        outline({{2, 14}, {5, 14}, {5, 4}, {10, 4}, {10, 14}, {15, 14}, {15, 4}, {20, 4}});
        plus(18, 19); break;
    case Icon::AddBit:
        outline({{2, 14}, {6, 14}, {6, 5}, {13, 5}, {13, 14}, {20, 14}});
        plus(18, 20); break;
    case Icon::AddBus:
        outline({{2, 5}, {6, 5}, {10, 13}, {21, 13}});
        outline({{2, 13}, {6, 13}, {10, 5}, {21, 5}});
        line(12, 9, 16, 9); plus(18, 20); break;
    case Icon::Search: circle(10, 10, 6.5); line(15, 15, 21, 21); break;
    case Icon::GoTo:
        line(20, 3, 20, 21); line(3, 12, 16, 12); outline({{11, 7}, {16, 12}, {11, 17}}); break;
    case Icon::Left: outline({{15, 5}, {8, 12}, {15, 19}}); break;
    case Icon::Right: outline({{9, 5}, {16, 12}, {9, 19}}); break;
    case Icon::Swap:
        outline({{3, 8}, {21, 8}, {17, 4}});
        outline({{21, 16}, {3, 16}, {7, 20}}); break;
    case Icon::Count: break;
    }
}

class IconEngine final : public QIconEngine {
public:
    IconEngine(Icon name, QWidget* owner) : name_(name), owner_(owner) {}
    QIconEngine* clone() const override { return new IconEngine(*this); }
    QString key() const override { return QStringLiteral("WaveRoundedIcon"); }
    QString iconName() override { return QStringLiteral("wave-rounded/%1").arg(int(name_)); }
    bool isNull() override { return false; }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        if (size.isEmpty() || scale <= 0) return {};
        QPixmap result(QSize(int(std::ceil(size.width() * scale)), int(std::ceil(size.height() * scale))));
        result.fill(Qt::transparent);
        result.setDevicePixelRatio(scale);
        QPainter painter(&result);
        paint(&painter, QRect(QPoint(), size), mode, state);
        return result;
    }
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        if (rect.isEmpty()) return;
        const auto palette = owner_ ? owner_->palette() : QApplication::palette();
        const auto group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
        const auto color = palette.color(group,
            mode == QIcon::Selected ? QPalette::HighlightedText : QPalette::ButtonText);
        const auto side = qreal(std::min(rect.width(), rect.height()));
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.x() + (rect.width() - side) / 2, rect.y() + (rect.height() - side) / 2);
        painter->scale(side / 24, side / 24);
        painter->setPen(QPen(color, state == QIcon::On ? 2.1 : 1.75,
            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        drawIcon(*painter, name_);
        painter->restore();
    }
private:
    Icon name_;
    QPointer<QWidget> owner_;
};

} // namespace

QIcon icon(Icon name, QWidget* owner)
{
    return name >= Icon::NewFile && name < Icon::Count
        ? QIcon(new IconEngine(name, owner)) : QIcon{};
}

} // namespace wave::ui
