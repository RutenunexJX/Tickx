#include "wave_canvas.h"

#include "wave/commands.h"
#include "wave/module_manifest.h"
#include "wave/simulation_pipeline.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>

#include <algorithm>

namespace {

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

wave::Lane* laneByName(wave::Scenario& scenario, const std::string_view name)
{
    const auto found = std::find_if(
        scenario.lanes.begin(), scenario.lanes.end(),
        [name](const wave::Lane& lane) { return lane.name == name; });
    return found == scenario.lanes.end() ? nullptr : &*found;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    if (application.arguments().size() != 2) return 2;

    const auto manifestPath = QDir(QStringLiteral(WAVE_SOURCE_DIR)).filePath(
        QStringLiteral(
            "tests/fixtures/integration/structured-module-manifest-v3.json"));
    const auto parsed = wave::parseZeroSlackModuleManifest(
        readFile(manifestPath));
    if (!parsed.ok()) return 3;
    const auto wrapper = wave::generateStructuredSimulationWrapper(
        *parsed.manifest);
    if (!wrapper.ok()
        || !wrapper.document.contains("manifest_control_if control();")
        || !wrapper.document.contains(
            "assign control.request = zs_structured_6_0;")
        || !wrapper.document.contains(
            "assign zs_structured_6_2 = control.ready;")
        || !wrapper.document.contains("logic [5:0] payload_i;")
        || !wrapper.document.contains(
            "assign payload_i[5 +: 1] = zs_structured_4_0;")
        || !wrapper.document.contains("logic [3:0] samples_i [1:0];")
        || !wrapper.document.contains(
            "assign samples_i[1] = zs_structured_5_0;")) {
        return 4;
    }
    auto imported = wave::importZeroSlackModuleManifest(*parsed.manifest);
    if (!imported.ok() || imported.project->scenarios.empty()) return 5;

    auto& project = *imported.project;
    auto& scenario = project.scenarios.front();
    auto* request = laneByName(scenario, "control.request");
    auto* command = laneByName(scenario, "control.command");
    auto* payload = laneByName(scenario, "payload_i.payload");
    auto* sample = laneByName(scenario, "samples_i[1]");
    if (!request || !command || !payload || !sample) return 6;

    request->segments.clear();
    command->segments.clear();
    payload->segments.clear();
    sample->segments.clear();
    wave::setSegmentRange(*request, 0, 50'000, "0", "request-low");
    wave::setSegmentRange(
        *request, 50'000, scenario.duration, "1", "request-high");
    wave::setSegmentRange(*command, 0, 65'000, "0x1", "command-one");
    wave::setSegmentRange(
        *command, 65'000, scenario.duration, "0x5", "command-five");
    wave::setSegmentRange(*payload, 0, 90'000, "0x03", "payload-three");
    wave::setSegmentRange(
        *payload, 90'000, scenario.duration, "0x12", "payload-eighteen");
    wave::setSegmentRange(*sample, 0, 40'000, "0x2", "sample-two");
    wave::setSegmentRange(*sample, 40'000, 120'000, "0x9", "sample-nine");
    wave::setSegmentRange(
        *sample, 120'000, scenario.duration, "0xf", "sample-fifteen");

    wave::CommandStack commands;
    wave::WaveCanvas canvas;
    canvas.resize(1'280, 720);
    canvas.setDocument(&project, &scenario, &commands);
    canvas.setTool(wave::WaveCanvas::Tool::WaveEdit);
    canvas.show();
    application.processEvents();
    canvas.fitScenario();
    application.processEvents();

    QImage image(canvas.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    canvas.render(&painter);
    painter.end();

    const auto outputPath = application.arguments().at(1);
    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    if (!image.save(outputPath)) return 7;

    const auto background = image.pixelColor(0, 0);
    qsizetype differingPixels = 0;
    for (int y = 0; y < image.height(); y += 2) {
        for (int x = 0; x < image.width(); x += 2) {
            if (image.pixelColor(x, y) != background) ++differingPixels;
        }
    }
    return differingPixels > 3'000 ? 0 : 8;
}
