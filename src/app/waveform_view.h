#pragma once

#include "wave/widgets_export.h"

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QString>

#include <memory>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace wave {

class WAVEWIDGETS_API WaveformView final : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(QString contract READ contract CONSTANT)
    Q_PROPERTY(QStringList capabilities READ capabilities CONSTANT)
    Q_PROPERTY(qulonglong previewGeneration READ previewGeneration NOTIFY previewChanged)
    Q_PROPERTY(QString previewMode READ previewMode NOTIFY previewChanged)
    Q_PROPERTY(QString presentationState READ presentationState NOTIFY presentationStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY presentationStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString themeName READ themeName WRITE setThemeName NOTIFY themeChanged)
    Q_PROPERTY(bool compact READ compact WRITE setCompact NOTIFY compactChanged)

public:
    explicit WaveformView(QWidget* parent = nullptr);
    ~WaveformView() override;

    [[nodiscard]] QString contract() const;
    [[nodiscard]] QStringList capabilities() const;
    [[nodiscard]] qulonglong previewGeneration() const noexcept;
    [[nodiscard]] QString previewMode() const;
    [[nodiscard]] QString presentationState() const;
    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QString themeName() const;
    [[nodiscard]] bool compact() const noexcept;

    Q_INVOKABLE bool replacePreviewPayload(const QByteArray& payload);
    Q_INVOKABLE bool replacePreviewJson(const QString& payload);
    Q_INVOKABLE bool setPresentationState(const QString& state,
                                          const QString& message = {});
    Q_INVOKABLE bool setThemeName(const QString& theme);
    Q_INVOKABLE void setCompact(bool compact);
    Q_INVOKABLE void clearPreview();
    Q_INVOKABLE void fitAll();
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE bool selectLane(const QString& stableId);
    Q_INVOKABLE bool revealTick(qint64 tick);

signals:
    void previewChanged(qulonglong generation);
    void presentationStateChanged(const QString& state,
                                  const QString& message);
    void lastErrorChanged(const QString& error);
    void themeChanged(const QString& theme);
    void compactChanged(bool compact);
    void cursorChanged(qint64 tick,
                       const QString& laneId,
                       const QString& value);
    void selectionChanged(const QString& laneId, qint64 tick);
    void sourceNavigationRequested(const QString& sourceFile,
                                   int sourceLine,
                                   int sourceColumn,
                                   const QString& semanticId,
                                   const QString& laneId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    class Data;
    std::unique_ptr<Data> data_;

    void setError(const QString& error);
    void updateMetrics();
    void updateScrollBars();
    void updateAccessibleSummary();
    [[nodiscard]] qint64 visibleStart() const noexcept;
    [[nodiscard]] qint64 visibleEnd() const noexcept;
    [[nodiscard]] double tickToX(qint64 tick) const noexcept;
    [[nodiscard]] qint64 xToTick(double x) const noexcept;
    void setZoom(double pixelsPerTick, double anchorX);
    void requestSourceNavigation();
};

} // namespace wave
