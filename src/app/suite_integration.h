#pragma once

#include "wave/widgets_export.h"

#include <QJsonObject>
#include <QObject>

#include <memory>

namespace SuiteApp {
class Provider;
struct RuntimeStartOptions;
}

namespace wave {

class MainWindow;

class WAVEWIDGETS_API WaveSuiteIntegration final : public QObject {
public:
    explicit WaveSuiteIntegration(MainWindow* window,
                                  QObject* parent = nullptr);
    ~WaveSuiteIntegration() override;

    bool start(QString* failureReason = nullptr);
    bool start(const SuiteApp::RuntimeStartOptions& runtimeOptions,
               QString* failureReason = nullptr);
    bool isRegistered() const;

    static QJsonObject appDescriptor(const QString& version,
                                     const QString& endpoint = {});
    QJsonObject processRequestForTesting(const QJsonObject& request);

private:
    QJsonObject processRequest(const QJsonObject& request);

    MainWindow* window_ = nullptr;
    std::unique_ptr<SuiteApp::Provider> provider_;
};

} // namespace wave
