#pragma once

class QWidget;

namespace wave {
class SignalStyleEditor;

QWidget* createSignalStylePreviewPage(SignalStyleEditor* editor, QWidget* parent);
}
