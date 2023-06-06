#include "timestampSpinBox.h"

TimestampSpinBox::TimestampSpinBox(FileCamera *fileCamera, QWidget *parent) :
        QSpinBox(),
        QWidget(parent),
        fileCamera(fileCamera){

}

QString TimestampSpinBox::textFromValue(int value) const {
    uint64_t timestamp = fileCamera->getTimestampForFrameNumber(value);
    return QString::Number(timestamp);
}

int TimestampSpinBox::valueFromText(const QString &text) const {
    bool success;
    uint64_t timestamp = text.toLongLong(&success);
    return fileCamera->getFrameNumberForTimestamp(timestamp);
}