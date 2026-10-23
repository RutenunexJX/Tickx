#include "wave_canvas.h"
#include "wave/commands.h"
#include "wave/model.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>

#include <iostream>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace {
constexpr wave::Tick Duration = 16'000;

std::uint64_t workingSetBytes()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return counters.WorkingSetSize;
#endif
    return 0;
}

wave::Project fixture(const int groups, const int members, const bool grouped)
{
    wave::Project project;
    project.id = "group-benchmark";
    project.name = "Synthetic grouped signals";
    wave::Scenario scenario;
    scenario.id = "scenario";
    scenario.name = "Synthetic signals";
    scenario.duration = Duration;
    scenario.lanes.reserve(static_cast<std::size_t>(groups * (members + 1)));
    for (int g = 0; g < groups; ++g) {
        const auto groupId = "group-" + std::to_string(g);
        for (int m = -1; m < members; ++m) {
            wave::Lane lane;
            lane.id = m < 0 ? groupId : "signal-" + std::to_string(g) + "-" + std::to_string(m);
            lane.name = lane.id;
            lane.height = 40;
            lane.kind = grouped && m < 0 ? wave::LaneKind::Group : wave::LaneKind::Bit;
            if (grouped && m >= 0) lane.groupId = groupId;
            if (lane.kind != wave::LaneKind::Group) {
                for (int s = 0; s < 16; ++s)
                    lane.segments.push_back({lane.id + "-" + std::to_string(s),
                        s * 1'000, (s + 1) * 1'000, s % 2 ? "1" : "0", {}});
            }
            scenario.lanes.push_back(std::move(lane));
        }
    }
    project.scenarios.push_back(std::move(scenario));
    return project;
}

template<class F>
double measure(F&& operation)
{
    QElapsedTimer timer;
    timer.start();
    operation();
    return static_cast<double>(timer.nsecsElapsed()) / 1'000'000.0;
}

QImage frame(wave::WaveCanvas& canvas)
{
    QApplication::processEvents();
    return canvas.viewport()->grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

QString digest(const QImage& image)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        QByteArrayView(reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes()),
        QCryptographicHash::Sha256).toHex());
}
}

int main(int argc, char** argv)
{
    QTemporaryDir profile;
    if (!profile.isValid()) return 2;
    qputenv("WAVEWORKBENCH_SETTINGS_DIR", profile.path().toUtf8());
    qputenv("WAVEWORKBENCH_RECOVERY_DIR", (profile.path() + "/recovery").toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, profile.path());
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    const auto name = app.arguments().value(1, "small-group");
    const bool small = name == "small-group";
    const bool grouped = name != "large-flat";
    const int groups = small ? 10 : 100;
    const int members = small ? 8 : 100;
    auto project = fixture(groups, members, grouped);
    auto& scenario = project.scenarios.front();
    wave::CommandStack commands;
    const auto memoryBefore = workingSetBytes();
    wave::WaveCanvas canvas;
    canvas.resize(1280, 720);
    QJsonObject result{{"case", name}, {"lanes", static_cast<int>(scenario.lanes.size())},
        {"platform", QGuiApplication::platformName()}};
    result["attach_ms"] = measure([&] { canvas.setDocument(&project, &scenario, &commands); });
    QImage first;
    result["first_frame_ms"] = measure([&] {
        canvas.show();
        QApplication::processEvents();
        canvas.restoreVisibleTimeSpan(5'000, 8'000);
        first = frame(canvas);
    });
    result["initial_image_sha256"] = digest(first);
    result["refresh_ms"] = measure([&] { canvas.refreshModel(); });
    (void)frame(canvas);
    if (grouped) {
        const auto id = QString::fromStdString("group-" + std::to_string(groups - 1));
        bool valid = true;
        result["collapse_expand_ms"] = measure([&] {
            valid &= canvas.setGroupCollapsed(id, true);
            valid &= !canvas.isLaneDisplayed(QString("signal-%1-0").arg(groups - 1));
            valid &= canvas.setGroupCollapsed(id, false);
            valid &= canvas.isLaneDisplayed(QString("signal-%1-0").arg(groups - 1));
        });
        if (!valid) return 3;
        (void)frame(canvas);
    }
    result["navigation_12_frames_ms"] = measure([&] {
        for (int step = 0; step < 12; ++step) {
            canvas.verticalScrollBar()->setValue((step * 7919) % (canvas.verticalScrollBar()->maximum() + 1));
            canvas.horizontalScrollBar()->setValue((step * 389) % (canvas.horizontalScrollBar()->maximum() + 1));
            if (step % 4 == 0) canvas.zoomIn();
            if (step % 4 == 2) canvas.zoomOut();
            (void)frame(canvas);
        }
    });
    canvas.setTool(wave::WaveCanvas::Tool::Marker);
    result["cursor_20_frames_ms"] = measure([&] {
        for (int step = 0; step < 20; ++step) {
            const QPointF local(500 + step * 10, 20);
            const QPointF global(canvas.viewport()->mapToGlobal(local.toPoint()));
            QMouseEvent press(QEvent::MouseButtonPress, local, global,
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, local, global,
                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(canvas.viewport(), &press);
            QApplication::sendEvent(canvas.viewport(), &release);
            (void)frame(canvas);
        }
    });
    result["final_image_sha256"] = digest(frame(canvas));
    result["working_set_delta_bytes"] = static_cast<double>(workingSetBytes() - memoryBefore);
    result["rendered_lanes"] = canvas.property("wavewidgets.renderedLaneCount").toInt();
    const auto evidence = qEnvironmentVariable("WAVE_GROUP_EVIDENCE_DIR");
    if (!evidence.isEmpty()) {
        QDir().mkpath(evidence);
        first.save(evidence + "/" + name + "-initial.png");
        frame(canvas).save(evidence + "/" + name + "-final.png");
    }
    canvas.close();
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << '\n';
    return 0;
}
