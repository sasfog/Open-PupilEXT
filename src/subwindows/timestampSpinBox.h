#ifndef PUPILEXT_TIMESTAMPSPINBOX_H
#define PUPILEXT_TIMESTAMPSPINBOX_H

/**
    @author Attila Boncsér
*/

#include <QtWidgets>
#include <QtWidgets/QSpinBox>
#include "../devices/fileCamera.h"

class TimestampSpinBox : QSpinBox {
    Q_OBJECT

public:
    explicit TimestampSpinBox(const FileCamera *fileCamera, const QWidget *parent);

    ~TimestampSpinBox() override;

    QString textFromValue(int value) const override;
    int valueFromText(const QString &text) const override;

private:
    FileCamera *fileCamera;

    uint64_t getNextTimestamp(int frameNumber);
    uint64_t getPreviousTimestamp(int frameNumber);
};
#endif //PUPILEXT_TIMESTAMPSPINBOX_H