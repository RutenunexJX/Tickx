#include "ElaCheckBox.h"

#include "ElaApplication.h"
#include "ElaCheckBoxStyle.h"
ElaCheckBox::ElaCheckBox(QWidget* parent)
    : QCheckBox(parent)
{
    _pBorderRadius = 3;
    setMouseTracking(true);
    setObjectName("ElaCheckBox");
    _waveOwnedStyle = new ElaCheckBoxStyle();
    setStyle(_waveOwnedStyle);
    QFont font = this->font();
    font.setPixelSize(eApp->getFontPixelSize() + 2);
    setFont(font);
}

ElaCheckBox::ElaCheckBox(const QString& text, QWidget* parent)
    : ElaCheckBox(parent)
{
    setText(text);
}

ElaCheckBox::~ElaCheckBox()
{
    setStyle(nullptr);
    delete _waveOwnedStyle;
}
