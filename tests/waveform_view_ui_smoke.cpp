#include "waveform_theme.h"
#include "waveform_view.h"
#include "wave/widgets.h"

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QPixmap>
#include <QSet>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

bool visuallyPopulated(const QPixmap& pixmap)
{
    if (pixmap.isNull()) return false;
    const QImage image = pixmap.toImage();
    QSet<QRgb> colors;
    const int xStep = std::max(1, image.width() / 32);
    const int yStep = std::max(1, image.height() / 24);
    for (int y = 0; y < image.height(); y += yStep) {
        for (int x = 0; x < image.width(); x += xStep) {
            colors.insert(image.pixel(x, y));
        }
    }
    return colors.size() >= 8;
}

bool visuallyDistinct(const QPixmap& first, const QPixmap& second)
{
    if (first.isNull() || second.isNull()) return false;
    const QImage a = first.toImage();
    const QImage b = second.toImage();
    int different = 0;
    constexpr int columns = 24;
    constexpr int rows = 18;
    for (int row = 0; row < rows; ++row) {
        const int ay = std::min(a.height() - 1, row * a.height() / rows);
        const int by = std::min(b.height() - 1, row * b.height() / rows);
        for (int column = 0; column < columns; ++column) {
            const int ax = std::min(a.width() - 1, column * a.width() / columns);
            const int bx = std::min(b.width() - 1, column * b.width() / columns);
            if (a.pixel(ax, ay) != b.pixel(bx, by)) ++different;
        }
    }
    return different >= columns * rows / 3;
}

QString fullSizePath(const QString& compactPath)
{
    const QFileInfo info(compactPath);
    const QString suffix = info.suffix().isEmpty()
        ? QStringLiteral("png")
        : info.suffix();
    return info.dir().filePath(
        info.completeBaseName() + QStringLiteral("-full.") + suffix);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    if (argc != 3) return 1;

    bool scaleOk = false;
    const double expectedScale = QString::fromLocal8Bit(argv[2]).toDouble(&scaleOk);
    if (!scaleOk || expectedScale < 1.0) return 2;

    application.setStyleSheet(wave::waveApplicationStyleSheet(
        wave::WaveformColorScheme::Dark));

    QWidget host;
    host.setObjectName(QStringLiteral("WaveformViewUiSmokeHost"));
    host.setProperty("waveSurface", QStringLiteral("panel"));
    auto* layout = new QHBoxLayout(&host);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* splitter = new QSplitter(Qt::Horizontal, &host);
    splitter->setChildrenCollapsible(false);
    auto* rail = new QFrame(splitter);
    rail->setObjectName(QStringLiteral("PreviewNavigationRail"));
    rail->setProperty("waveSurface", QStringLiteral("raised"));
    rail->setAccessibleName(QStringLiteral("Preview navigation rail"));
    rail->setMinimumWidth(140);
    rail->setFrameShape(QFrame::StyledPanel);

    QWidget* created = nullptr;
    std::array<char, 1024> error{};
    if (wavewidgets_create_waveform_view_v1(
            splitter, &created, error.data(), error.size()) != 0
        || !created) {
        return 3;
    }
    auto* view = qobject_cast<wave::WaveformView*>(created);
    if (!view) return 4;
    splitter->addWidget(rail);
    splitter->addWidget(view);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter);

    const QByteArray payload = R"JSON({
      "contract":"wave-preview/v1","generation":21,"mode":"symbolic",
      "timebase":{"unit":"ns","start":0,"end":160},
      "lanes":[
        {"id":"top.clk","name":"clk","kind":"clock","width":1,
         "provenance":"zeroslack-symbolic",
         "source":{"file":"rtl/top.sv","line":4,"column":3,"semanticId":"module:top/signal:clk"},
         "segments":[
           {"start":0,"end":20,"value":"0"},{"start":20,"end":40,"value":"1"},
           {"start":40,"end":60,"value":"0"},{"start":60,"end":80,"value":"1"},
           {"start":80,"end":100,"value":"0"},{"start":100,"end":120,"value":"1"},
           {"start":120,"end":140,"value":"0"},{"start":140,"end":160,"value":"1"}]},
        {"id":"top.valid","name":"valid","kind":"bit","width":1,
         "provenance":"zeroslack-symbolic",
         "source":{"file":"rtl/top.sv","line":9,"column":3,"semanticId":"module:top/signal:valid"},
         "segments":[{"start":0,"end":48,"value":"0"},{"start":48,"end":132,"value":"1"},{"start":132,"end":160,"value":"0"}]},
        {"id":"top.data","name":"data[15:0]","kind":"bus","width":16,
         "provenance":"zeroslack-symbolic",
         "source":{"file":"rtl/top.sv","line":10,"column":3,"semanticId":"module:top/signal:data"},
         "segments":[{"start":0,"end":48,"value":"0x0000"},{"start":48,"end":96,"value":"0x12A5"},{"start":96,"end":128,"value":"X","unknown":true},{"start":128,"end":160,"value":"0xBEEF"}]},
        {"id":"top.ready","name":"ready","kind":"bit","width":1,
         "provenance":"zeroslack-symbolic","segments":[{"start":0,"end":72,"value":"0"},{"start":72,"end":160,"value":"1"}]}
      ]
    })JSON";
    if (wavewidgets_set_waveform_preview_v1(
            view, payload.constData(), static_cast<std::size_t>(payload.size()),
            error.data(), error.size()) != 0) {
        return 5;
    }

    view->setThemeName(QStringLiteral("dark"));
    view->setCompact(true);
    host.resize(960, 720);
    splitter->setSizes({190, 770});
    host.show();
    application.processEvents();
    if (host.property("waveSurface").toString() != QStringLiteral("panel")
        || rail->property("waveSurface").toString() != QStringLiteral("raised")
        || host.width() != 960 || host.height() != 720) {
        return 13;
    }
    const QList<int> firstSizes = splitter->sizes();
    splitter->setSizes({310, 650});
    application.processEvents();
    const QList<int> secondSizes = splitter->sizes();
    if (firstSizes.size() != 2 || secondSizes.size() != 2
        || secondSizes[0] <= firstSizes[0] + 50) {
        return 6;
    }

    QString navigatedLane;
    QObject::connect(
        view, &wave::WaveformView::sourceNavigationRequested,
        [&navigatedLane](const QString&, int, int, const QString&, const QString& laneId) {
            navigatedLane = laneId;
        });
    if (!view->selectLane(QStringLiteral("top.valid"))) return 7;
    view->setFocus(Qt::OtherFocusReason);
    QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(view, &down);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(view, &enter);
    if (!view->hasFocus() || navigatedLane != QStringLiteral("top.data")) return 8;

    const QPixmap darkCompactView = view->grab();
    const QPixmap darkCompact = host.grab();
    const QString compactPath = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(QFileInfo(compactPath).absolutePath())
        || !visuallyPopulated(darkCompact)
        || !darkCompact.save(compactPath)) {
        return 9;
    }
    if (std::abs(darkCompact.devicePixelRatio() - expectedScale) > 0.06) {
        return 10;
    }

    view->setCompact(false);
    view->setThemeName(QStringLiteral("light"));
    application.setStyleSheet(wave::waveApplicationStyleSheet(
        wave::WaveformColorScheme::Light));
    host.resize(1440, 900);
    splitter->setSizes({260, 1180});
    application.processEvents();
    const QPixmap lightFullView = view->grab();
    const QPixmap lightFull = host.grab();
    if (!visuallyPopulated(lightFull)
        || !lightFull.save(fullSizePath(compactPath))) {
        return 11;
    }
    if (!visuallyDistinct(darkCompactView, lightFullView)) {
        return 12;
    }
    return 0;
}
