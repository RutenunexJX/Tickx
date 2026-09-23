#include "signal_style.h"
#include "ui_controls.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>

#include <algorithm>
#include <array>

namespace wave {
namespace {
constexpr auto key = "waveWorkbench.signalAppearance";
struct Preset {
    double stroke, bevel;
    bool trapezoid, rounded;
    const char* light[3];
    const char* dark[3];
    const char* lightFill;
    const char* darkFill;
};
const std::array<Preset, 10> presets{{
    {1.3,7,false,false,{"#34495d","#34495d","#34495d"},{"#c7d6e5","#c7d6e5","#c7d6e5"},"#f4f6f8","#232d39"},
    {2.3,4,true,true,{"#b24d72","#7954b2","#358378"},{"#eda1be","#c4a4ef","#91d3c5"},"#f0faf7","#263b38"},
    {1.2,3,false,false,{"#6a7485","#414b5d","#586478"},{"#a7b0be","#d8dfeb","#bcc7d8"},"#fafbfe","#161b23"},
    {1.8,7,false,false,{"#986518","#285f96","#327767"},{"#e7bd71","#8ebbe8","#87cbb8"},"#eff7f4","#213932"},
    {1.35,10,true,false,{"#427aa5","#275f94","#386f9d"},{"#86bce3","#b0d1ef","#94c0e4"},"#eef6ff","#1c3045"},
    {1.5,0,false,false,{"#28774c","#28774c","#28774c"},{"#83d99d","#83d99d","#83d99d"},"#f0f8f1","#183124"},
    {1.6,5,true,true,{"#916609","#237e62","#297594"},{"#f0cb74","#82dfb9","#83cce7"},"#eff8fa","#18343e"},
    {1.7,8,true,true,{"#806248","#675765","#697245"},{"#d5ba91","#d6b8c9","#c6d29b"},"#fbf7eb","#363528"},
    {2.8,8,false,false,{"#18202c","#114c9c","#66277f"},{"#edf2f9","#88c1ff","#e3a5ff"},"#ffffff","#17202a"},
    {1.7,14,true,false,{"#7952a4","#216e9e","#784598"},{"#c7a5ef","#8ac8ed","#d6a0ef"},"#f7f0fc","#32213e"},
}};

struct BackgroundPalette {
    const char *canvas, *band, *panel, *raised, *major, *minor;
    const char *text, *muted, *selection, *accent;
};
struct BackgroundPreset {
    BackgroundPalette light, dark;
    Qt::PenStyle minorGrid;
    SignalBackgroundStyle::Ruling ruling;
};
using Ruling = SignalBackgroundStyle::Ruling;
const std::array<BackgroundPreset, 10> backgrounds{{
    // Paper-white, pastel, bare, technical, blueprint, phosphor, graticule, warm paper, contrast, facet.
    {{"#fcfcfa","#f1f3f1","#e9eeec","#ffffff","#b4bfbd","#dee3e0","#263934","#51665f","#dce8e3","#416e5e"},
     {"#20282b","#252f32","#2c383b","#202a2d","#56696b","#354446","#e0ebe7","#b2c6bd","#344f47","#a2c7b6"}, Qt::SolidLine,Ruling::Ruled},
    {{"#fffafd","#f9eff8","#f2e8f1","#fffdfd","#d2bcd7","#eddfef","#4d3456","#74567e","#ebdcf4","#965ba7"},
     {"#291f30","#302638","#392d41","#302335","#695370","#44334d","#f4e4f6","#cfb2d4","#52355f","#dab1e8"}, Qt::DotLine,Ruling::None},
    {{"#ffffff","#f8f9fa","#f0f2f4","#ffffff","#d2d6dc","#eceef1","#292f38","#5c6573","#e3e8ee","#506078"},
     {"#16191e","#1c2026","#232830","#1a1e24","#424a56","#282f38","#e3e8ef","#acb6c5","#323e50","#abc1df"}, Qt::NoPen,Ruling::None},
    {{"#f7fafc","#edf2f6","#e4ecf2","#fcfdff","#b7c7d5","#d6e1e9","#233d50","#4b6578","#d5e6f3","#357ca8"},
     {"#17232d","#1e2c36","#253643","#1a2934","#466175","#2c4050","#e1edf5","#aac3d5","#2c4e66","#8dc1e6"}, Qt::SolidLine,Ruling::Graph},
    {{"#edf5fe","#e2edf9","#d8e7f5","#f3f8ff","#8daecf","#c5d8eb","#24466a","#406080","#ccdff6","#2966a8"},
     {"#112b46","#173552","#1e3e5d","#16304b","#4c759c","#284e6d","#e2f0ff","#adcce9","#284e76","#a0d0ff"}, Qt::SolidLine,Ruling::Graph},
    {{"#f3faf3","#e9f3e9","#e1ede2","#f8fdf8","#aac9ae","#d0e2d2","#284b32","#456a4e","#d0e7d5","#347c48"},
     {"#102118","#152b1d","#1c3424","#12291b","#3d6a49","#244532","#d5f2dc","#a1ccad","#285237","#8ddaa4"}, Qt::DotLine,Ruling::Ruled},
    {{"#f1f9fa","#e7f2f3","#ddecee","#f7fcfc","#9fc3c9","#cae0e4","#244b51","#41646b","#cde6e9","#287a86"},
     {"#10272b","#153136","#1d3b40","#142d32","#3e6d75","#254b52","#d6f2f4","#9dcbd0","#28535c","#83d3de"}, Qt::DotLine,Ruling::Crosses},
    {{"#fcf7ea","#f4ecda","#eae0cd","#fffbf2","#c5b497","#e2d7bf","#4c402e","#695b42","#e7dcc0","#856638"},
     {"#2c2820","#342e24","#3d3529","#312c22","#72634a","#4a402f","#f0e6d1","#cabc9e","#54452c","#d9bd85"}, Qt::SolidLine,Ruling::Ruled},
    {{"#fdfefe","#f0f2f4","#e5e9ee","#ffffff","#8793a5","#c0c9d4","#101c2c","#374860","#d3e2f5","#134fa0"},
     {"#080e17","#141d2a","#1d2a3c","#0e1724","#657d9d","#32455f","#ffffff","#cfdef0","#234b7e","#abd4ff"}, Qt::DashLine,Ruling::None},
    {{"#f6f2fc","#eee7f8","#e4dcf0","#fbf9ff","#bda7d4","#daccec","#49315f","#6c4e80","#e1d2f2","#8050a7"},
     {"#241a33","#2c213e","#362849","#2a1f3b","#685383","#443254","#eadcf7","#c5aedc","#513568","#cf9ff2"}, Qt::DashDotLine,Ruling::Crosses},
}};
}

QStringList signalStylePresetIds()
{
    return {"academic","cute","minimal","engineering","blueprint","terminal","scope","paper","contrast","facet"};
}

QStringList signalStylePresetNames()
{
    return {QObject::tr("Academic / 学术"),QObject::tr("Cute / 可爱"),QObject::tr("Minimal / 极简"),
        QObject::tr("Engineering / 工程"),QObject::tr("Blueprint / 蓝图"),QObject::tr("Retro terminal / 复古终端"),
        QObject::tr("Oscilloscope / 示波器"),QObject::tr("Paper / 纸本"),QObject::tr("High contrast / 高对比"),
        QObject::tr("Faceted tech / 棱角科技")};
}

SignalStyleSettings signalStyleSettings(const JsonExtensions& extensions)
{
    SignalStyleSettings result;
    const auto found = extensions.find(key);
    if (found == extensions.end()) return result;
    const auto object = QJsonDocument::fromJson(QByteArray::fromStdString(found->second)).object();
    const auto preset = object.value("preset").toString();
    if (signalStylePresetIds().contains(preset)) result.preset = preset;
    const auto edge = object.value("edge").toString();
    if (edge == "square" || edge == "trapezoid") result.edge = edge;
    const auto corners = object.value("corners").toString();
    if (corners == "round" || corners == "sharp") result.corners = corners;
    const auto palette = object.value("palette").toString();
    if (palette == "signal" || palette == "preset") result.palette = palette;
    const auto stroke = object.value("stroke").toDouble();
    if (stroke >= .75 && stroke <= 4) result.stroke = stroke;
    const QColor fill(object.value("fill").toString());
    if (fill.isValid()) { result.fill = fill; result.fill.setAlpha(255); }
    return result;
}

void storeSignalStyleSettings(JsonExtensions& extensions, const SignalStyleSettings& settings)
{
    QJsonObject object{{"preset",settings.preset},{"edge",settings.edge},
        {"corners",settings.corners},{"palette",settings.palette},{"stroke",settings.stroke}};
    if (settings.fill.isValid()) object.insert("fill", settings.fill.name(QColor::HexRgb));
    extensions[key] = QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
}

SignalStyle resolvedSignalStyle(const Project* project, const Lane& lane, WaveformColorScheme scheme)
{
    auto settings = signalStyleSettings(lane.extensions);
    auto defaults = project ? signalStyleSettings(project->extensions) : SignalStyleSettings{};
    const auto legacy = !lane.extensions.contains(key) && (!project || !project->extensions.contains(key));
    if (settings.preset == "inherit") {
        settings.preset = defaults.preset;
    }
    if (settings.palette == "inherit") settings.palette = defaults.palette;
    if (settings.edge == "preset") settings.edge = defaults.edge;
    if (settings.corners == "preset") settings.corners = defaults.corners;
    if (settings.stroke == 0) settings.stroke = defaults.stroke;
    if (!settings.fill.isValid()) settings.fill = defaults.fill;
    auto index = signalStylePresetIds().indexOf(settings.preset);
    if (index < 0) index = 3;
    const auto& preset = presets.at(static_cast<std::size_t>(index));
    const auto colorIndex = lane.kind == LaneKind::Clock ? 0 : lane.kind == LaneKind::Bit ? 1 : 2;
    const auto dark = scheme == WaveformColorScheme::Dark;
    SignalStyle result;
    result.color = QColor(dark ? preset.dark[colorIndex] : preset.light[colorIndex]);
    if (settings.palette == "signal" || legacy) {
        const QColor color(QString::fromStdString(lane.color));
        if (color.isValid()) result.color = color;
    }
    result.fill = settings.fill.isValid() ? settings.fill : QColor(dark ? preset.darkFill : preset.lightFill);
    result.fill.setAlpha(255);
    result.stroke = settings.stroke > 0 ? settings.stroke : preset.stroke;
    result.bevel = preset.bevel;
    result.trapezoid = settings.edge == "trapezoid" || (settings.edge == "preset" && preset.trapezoid);
    result.rounded = settings.corners == "round" || (settings.corners == "preset" && preset.rounded);
    result.doubleOutline = index == 4;
    result.facets = index == 9;
    result.halo = index == 6;
    result.pixel = index == 5;
    result.ramp = result.facets ? 4.5 : 3;
    return result;
}

SignalBackgroundStyle resolvedSignalBackground(const Project* project, WaveformColorScheme scheme)
{
    SignalBackgroundStyle result;
    result.theme = waveformTheme(scheme);
    const auto index = project ? signalStylePresetIds().indexOf(signalStyleSettings(project->extensions).preset) : -1;
    // Unstyled/invalid documents and per-lane overrides do not recolor the whole workspace.
    if (index < 0) return result;
    const auto& preset = backgrounds.at(static_cast<std::size_t>(index));
    const auto& colors = scheme == WaveformColorScheme::Dark ? preset.dark : preset.light;
    auto& theme = result.theme;
    theme.canvas = QColor(colors.canvas); theme.panel = QColor(colors.panel);
    theme.raised = QColor(colors.raised); theme.gridMajor = QColor(colors.major);
    theme.gridMinor = QColor(colors.minor); theme.border = theme.gridMajor;
    theme.text = QColor(colors.text); theme.mutedText = QColor(colors.muted);
    theme.selection = QColor(colors.selection); theme.selectionText = theme.text;
    theme.accent = QColor(colors.accent); theme.accentSecondary = theme.accent;
    theme.focus = theme.accent;
    result.cycleBand = QColor(colors.band);
    result.minorGrid = preset.minorGrid;
    result.ruling = preset.ruling;
    return result;
}

QPen SignalStyle::pen(bool preview) const
{
    return QPen(color, stroke, preview ? Qt::DashLine : Qt::SolidLine,
        rounded ? Qt::RoundCap : Qt::FlatCap, rounded ? Qt::RoundJoin : Qt::MiterJoin);
}

SignalStyleEditor::SignalStyleEditor(const SignalStyleSettings& initial, bool allowInherit, QWidget* parent)
    : QWidget(parent)
{
    setObjectName("SignalStyleEditor");
    auto* layout = new ui::FormLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    preset_ = ui::comboBox(this);
    preset_->setObjectName("SignalStylePreset");
    if (allowInherit) preset_->addItem(tr("Project default"), "inherit");
    const auto ids = signalStylePresetIds(), names = signalStylePresetNames();
    for (int i=0; i<ids.size(); ++i) preset_->addItem(names[i], ids[i]);
    preset_->setCurrentIndex(std::max(0, preset_->findData(initial.preset == "inherit" && !allowInherit ? "engineering" : initial.preset)));
    layout->addRow(allowInherit ? tr("Style") : tr("Signal + background"), preset_);
    if (!allowInherit) preset_->setToolTip(tr("Pairs the workspace background, clock-cycle shading, grid and selection with the signal style. Individual signal overrides do not change the background."));
    const auto option = [this, layout](const QString& name, const QString& objectName,
                            const QStringList& labels, const QStringList& values, const QString& current) {
        auto* combo = ui::comboBox(this);
        combo->setObjectName(objectName);
        for (int i=0; i<labels.size(); ++i) combo->addItem(labels[i], values[i]);
        combo->setCurrentIndex(std::max(0, combo->findData(current)));
        layout->addRow(name, combo);
        return combo;
    };
    const auto inheritedLabel = allowInherit ? tr("Project / style default") : tr("From style");
    edge_ = option(tr("Bit edges"), "SignalStyleEdge", {inheritedLabel,tr("Square"),tr("Trapezoid")}, {"preset","square","trapezoid"}, initial.edge);
    corners_ = option(tr("Corners"), "SignalStyleCorners", {inheritedLabel,tr("Rounded"),tr("Sharp")}, {"preset","round","sharp"}, initial.corners);
    palette_ = option(tr("Palette"), "SignalStylePalette",
        allowInherit ? QStringList{tr("Project default"),tr("Style colors"),tr("Signal color")} : QStringList{tr("Style colors"),tr("Signal color")},
        allowInherit ? QStringList{"inherit","preset","signal"} : QStringList{"preset","signal"}, initial.palette);
    stroke_ = ui::doubleSpinBox(this);
    stroke_->setObjectName("SignalStyleStroke");
    stroke_->setRange(0, 4);
    stroke_->setSingleStep(.25);
    stroke_->setSpecialValueText(inheritedLabel);
    stroke_->setValue(initial.stroke);
    layout->addRow(tr("Line width"), stroke_);
    fill_ = ui::lineEdit(this);
    fill_->setObjectName("SignalStyleFill");
    fill_->setPlaceholderText(inheritedLabel);
    if (initial.fill.isValid()) fill_->setText(initial.fill.name(QColor::HexRgb));
    layout->addRow(tr("Opaque fill"), ui::colorField(fill_, this));
}

SignalStyleSettings SignalStyleEditor::settings() const
{
    return {preset_->currentData().toString(),edge_->currentData().toString(),corners_->currentData().toString(),
        palette_->currentData().toString(),stroke_->value(),QColor(fill_->text().trimmed())};
}

bool SignalStyleEditor::valid() const
{
    return (stroke_->value() == 0 || stroke_->value() >= .75)
        && (fill_->text().trimmed().isEmpty() || QColor(fill_->text().trimmed()).isValid());
}
} // namespace wave
